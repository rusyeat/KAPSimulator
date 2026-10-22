// Жазылған дыбыстар: тегін CC0/CC-BY пакеттер (CREDITS.md) және Windows TTS дауыстары.
// Банк табылмаса — бұрынғы процедуралық синтез қалады (fallback).
#include "KAPAudio.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/ScopeExit.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Sound/SoundConcurrency.h"

uint32 UKAPAudio::VoHash(const FString& S)
{
	// FNV-1a, UTF-16 бірліктері бойынша (Scripts/gen_vo.ps1 дәл осылай есептейді)
	uint32 H = 2166136261u;
	for (TCHAR C : S) { H ^= (uint32)(uint16)C; H *= 16777619u; }
	return H;
}

USoundWave* UKAPAudio::Pick(FName Bank)
{
	FKAPSoundBank& B = Banks.FindOrAdd(Bank);
	if (!B.bLoaded)
	{
		B.bLoaded = true;
		ON_SCOPE_EXIT { UE_LOG(LogTemp, Log, TEXT("KAPSND bank %s: %d"), *Bank.ToString(), B.W.Num()); };
		for (const TCHAR* Folder : { TEXT("SFX"), TEXT("Music") })
			for (int32 i = 0; i < 128; i++)
			{
				const FString N = FString::Printf(TEXT("%s_%d"), *Bank.ToString(), i);
				USoundWave* W = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/KAP/Audio/%s/%s.%s"), Folder, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (!W) break;
				B.W.Add(W);
			}
	}
	if (!B.W.Num()) return nullptr;
	int32 I = FMath::RandRange(0, B.W.Num() - 1);
	if (B.W.Num() > 1 && I == B.Last) I = (I + 1) % B.W.Num();
	B.Last = I;
	return B.W[I];
}

UAudioComponent* UKAPAudio::PlayBank(FName Bank, FVector Loc, float Vol, float Pitch, bool b2D, float StartTime, bool bFar)
{
	USoundWave* W = Pick(Bank);
	if (!W || !GetWorld()) return nullptr;
	// мылтық: бір уақытта ең көбі 6 атыс дыбысы, артығы — ең ескісі қысқа сөніп тоқтайды (автомат кезегінде жаңғырық үйілмейді)
	USoundConcurrency* Conc = nullptr;
	if (Bank.ToString().StartsWith(TEXT("Gun")))
	{
		if (!GunConc)
		{
			GunConc = NewObject<USoundConcurrency>(this);
			GunConc->Concurrency.MaxCount = 6;
			GunConc->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
			GunConc->Concurrency.VoiceStealReleaseTime = 0.06f;
		}
		Conc = GunConc;
	}
	if (b2D) return UGameplayStatics::SpawnSound2D(GetWorld(), W, Vol, Pitch, StartTime, Conc);
	return UGameplayStatics::SpawnSoundAtLocation(GetWorld(), W, Loc, FRotator::ZeroRotator, Vol, Pitch, StartTime, bFar ? AttenFar() : Atten(), Conc);
}

void UKAPAudio::SetMenuMusic(bool bOn)
{
	if (!GetWorld()) return;
	if (bOn)
	{
		if (MusMenu && MusMenu->IsPlaying()) return;
		USoundWave* W = Pick("Menu");
		if (!W) return;
		MusMenu = UGameplayStatics::CreateSound2D(GetWorld(), W, 0.55f, 1.f, 0.f, nullptr, true, false);
		if (MusMenu) MusMenu->FadeIn(2.f, 0.55f);
	}
	else if (MusMenu)
	{
		MusMenu->FadeOut(2.5f, 0.f);
		MusMenu = nullptr;
	}
}

USoundWave* UKAPAudio::LoadVo(const FString& Text, bool bFemale) const
{
	const FString N = FString::Printf(TEXT("VO_%08x_%s"), VoHash(Text), bFemale ? TEXT("f") : TEXT("m"));
	USoundWave* W = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/KAP/Audio/VO/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UE_LOG(LogTemp, Log, TEXT("KAPVO %s %s"), *N, W ? TEXT("ok") : TEXT("miss"));
	return W;
}

/** Бір дауыс файлын ойнату: 0 — 3D (NPC), 1 — 2D (диалог, диктор), 2 — рация (жолақ сүзгісі; bFirst — басында «тшш») */
UAudioComponent* UKAPAudio::PlayVoWave(USoundWave* W, FVector Loc, int32 Mode, float Vol, float Pitch, bool bFirst)
{
	if (!W || !GetWorld()) return nullptr;
	UAudioComponent* C = Mode == 0
		? UGameplayStatics::SpawnSoundAtLocation(GetWorld(), W, Loc, FRotator::ZeroRotator, Vol * 1.4f, Pitch, 0.f, Atten())
		: UGameplayStatics::SpawnSound2D(GetWorld(), W, Vol * (Mode == 2 ? 0.8f : 1.f), Pitch);
	if (!C) return nullptr;
	if (Mode == 2)
	{
		// рация: жолақ сүзгісі (420 Гц – 3.2 кГц) + басында «тшш»
		C->SetLowPassFilterEnabled(true); C->SetLowPassFilterFrequency(3200.f);
		C->SetHighPassFilterEnabled(true); C->SetHighPassFilterFrequency(420.f);
		if (bFirst) Impact(FVector::ZeroVector, 4, 0.4f);
		SetRadio(1.f);
		FTimerHandle H; TWeakObjectPtr<UKAPAudio> Self(this);
		GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self]() { if (Self.IsValid()) Self->SetRadio(0.f); }), FMath::Max(0.3f, W->Duration), false);
	}
	if (Mode != 0) { if (SpeechC && bFirst) SpeechC->FadeOut(0.15f, 0.f); SpeechC = C; }
	Duck(W->Duration, Mode == 0 ? 0.2f : 0.4f);
	return C;
}

bool UKAPAudio::Speak(const FString& Text, FVector Loc, bool bFemale, int32 Mode, float Vol, float Pitch)
{
	if (!GetWorld() || Text.IsEmpty()) return false;
	USoundWave* W = LoadVo(Text, bFemale);
	if (!W) return false;
	if (Mode != 0) ChainId++; // жаңа сөз алдыңғы тізбекті тоқтатады
	return PlayVoWave(W, Loc, Mode, Vol, Pitch, true) != nullptr;
}

bool UKAPAudio::SpeakChain(const TArray<FString>& Parts, FVector Loc, bool bFemale, int32 Mode, float Vol)
{
	if (!GetWorld() || Parts.Num() == 0) return false;
	TArray<TObjectPtr<USoundWave>> Ws;
	for (const FString& P : Parts)
	{
		USoundWave* W = LoadVo(P, bFemale);
		if (!W) return false; // бір бөлігі жоқ — тізбекті ойнатпаймыз (синтез дауыс қалады)
		Ws.Add(W);
	}
	ChainWaves = MoveTemp(Ws);
	const int32 Id = ++ChainId;
	ChainStep(Id, 0, Loc, Mode, Vol);
	return true;
}

void UKAPAudio::ChainStep(int32 Id, int32 I, FVector Loc, int32 Mode, float Vol)
{
	if (Id != ChainId || !ChainWaves.IsValidIndex(I) || !GetWorld()) return;
	USoundWave* W = ChainWaves[I];
	if (!PlayVoWave(W, Loc, Mode, Vol, 1.f, I == 0)) return;
	UE_LOG(LogTemp, Display, TEXT("KAPVO chain %d/%d %s (%.2f s)"), I + 1, ChainWaves.Num(), *W->GetName(), W->Duration);
	if (I + 1 >= ChainWaves.Num()) return;
	// келесі бөлік — осы файл біткенде (сөз арасында 60 мс тыныс)
	FTimerHandle H; TWeakObjectPtr<UKAPAudio> Self(this);
	GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self, Id, I, Loc, Mode, Vol]() { if (Self.IsValid()) Self->ChainStep(Id, I + 1, Loc, Mode, Vol); }), FMath::Max(0.05f, W->Duration + 0.06f), false);
}

FString UKAPAudio::OrdinalKk(int32 N)
{
	static const TCHAR* O[21] = { TEXT("бірінші"), TEXT("екінші"), TEXT("үшінші"), TEXT("төртінші"), TEXT("бесінші"), TEXT("алтыншы"), TEXT("жетінші"),
		TEXT("сегізінші"), TEXT("тоғызыншы"), TEXT("оныншы"), TEXT("он бірінші"), TEXT("он екінші"), TEXT("он үшінші"), TEXT("он төртінші"),
		TEXT("он бесінші"), TEXT("он алтыншы"), TEXT("он жетінші"), TEXT("он сегізінші"), TEXT("он тоғызыншы"), TEXT("жиырмасыншы"), TEXT("жиырма бірінші") };
	return (N >= 1 && N <= 21) ? FString(O[N - 1]) : FString();
}

void UKAPAudio::StopSpeech() { ChainId++; if (SpeechC) { SpeechC->FadeOut(0.2f, 0.f); SpeechC = nullptr; } }

void UKAPAudio::ShotAt(FName Weapon, FVector Loc)
{
	// алыстағы мылтық (45 м+): ашық далада жаңғырған нақты атыс жазбасы (freesound CC0, mchrpt)
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const FVector Lis = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Loc;
	if (Weapon != "pm" && Weapon != "shotgun" && FVector::Dist(Lis, Loc) > 4500.f && HasBank("GunRifle"))
	{
		PlayBank("GunRifle", Loc, 1.f, FMath::FRandRange(0.88f, 1.02f), false, 0.f, true);
		return;
	}
	const FName B = Weapon == "sniper" ? FName("GunSniper") : Weapon == "shotgun" ? FName("GunShotgun") : Weapon == "pm" ? FName("GunPM") : FName("GunAK");
	PlayBank(B, Loc, 1.f, FMath::FRandRange(0.94f, 1.04f), false, 0.f, true);
}

void UKAPAudio::Yell(FVector Loc, bool bDeath)
{
	if (!PlayBank(bDeath ? FName("Scream") : FName("Yell"), Loc, bDeath ? 1.f : 0.9f, FMath::FRandRange(0.93f, 1.07f)))
		Creature(Loc, 1, 0.6f);
}

// Двигатель (екі цикл: төмен/жоғары айналым) мен музыка (тыныш/қауіп) — әр кадрда біртіндеп
void UKAPAudio::UpdateReal(float Dt)
{
	if (!GetWorld()) return;
	// ---- двигатель ----
	if (bRealEngine)
	{
		if (!EngLo && HasBank("Engine")) { FKAPSoundBank& B = Banks.FindOrAdd("Engine");
			if (B.W.Num() >= 5)
			{
				EngLo = UGameplayStatics::CreateSound2D(GetWorld(), B.W[1], 0.f, 1.f, 0.f, nullptr, true, false);
				EngHi = UGameplayStatics::CreateSound2D(GetWorld(), B.W[4], 0.f, 1.f, 0.f, nullptr, true, false);
				if (EngLo) EngLo->Play(); if (EngHi) EngHi->Play();
			}
		}
		const float R = EngRpm;
		// жіберу (misfire): кездейсоқ қысқа құлдырау
		const float Mis = (EngMis > 0 && FMath::FRand() < EngMis * Dt * 12.f) ? 0.25f : 1.f;
		EngLoV = FMath::FInterpTo(EngLoV, EngOn * (0.55f - R * 0.3f) * Mis, Dt, 8.f);
		EngHiV = FMath::FInterpTo(EngHiV, EngOn * (0.08f + R * 0.5f) * Mis, Dt, 8.f);
		const float Cab = 1.f - IndoorK * 0.35f; // кабинада сәл тұйық
		if (EngLo) { EngLo->SetVolumeMultiplier(FMath::Max(0.001f, EngLoV * Cab)); EngLo->SetPitchMultiplier(0.62f + R * 0.85f); }
		if (EngHi) { EngHi->SetVolumeMultiplier(FMath::Max(0.001f, EngHiV * Cab)); EngHi->SetPitchMultiplier(0.55f + R * 0.75f); }
	}
	// ---- музыка ----
	if (bRealMusic)
	{
		if (!MusCalm)
		{
			if (USoundWave* W = Pick("Ambient")) { MusCalm = UGameplayStatics::CreateSound2D(GetWorld(), W, 0.f, 1.f, 0.f, nullptr, true, false); if (MusCalm) MusCalm->Play(); }
			if (USoundWave* W = Pick("Danger")) { MusDanger = UGameplayStatics::CreateSound2D(GetWorld(), W, 0.f, 1.f, 0.f, nullptr, true, false); if (MusDanger) MusDanger->Play(); }
		}
		const float DuckK = 1.f - DuckAmt * 0.6f * FMath::Clamp(DuckT, 0.f, 1.f);
		MusCalmV = FMath::FInterpTo(MusCalmV, MusV * (1.f - MusI) * 0.32f * DuckK, Dt, 0.5f);
		MusDangerV = FMath::FInterpTo(MusDangerV, MusV * MusI * 0.5f * DuckK, Dt, MusI > MusDangerV ? 2.f : 0.4f);
		if (MusCalm) MusCalm->SetVolumeMultiplier(FMath::Max(0.001f, MusCalmV));
		if (MusDanger) MusDanger->SetVolumeMultiplier(FMath::Max(0.001f, MusDangerV));
	}
}

bool UKAPAudio::HasBank(FName Bank)
{
	FKAPSoundBank* K = Banks.Find(Bank);
	if (!K || !K->bLoaded) { Pick(Bank); K = Banks.Find(Bank); }
	return K && K->W.Num() > 0;
}

// Монстр ыңырсуы: ұзын жазбаның кездейсоқ бөлігі (2–2.5 с), соңы жұмсақ өшеді
bool UKAPAudio::MoanAt(FVector Loc, float Pitch, float Vol, float Len, bool bFar)
{
	USoundWave* W = Pick("Moan");
	if (!W || !GetWorld()) return false;
	const float Start = FMath::FRandRange(0.f, FMath::Max(0.f, W->Duration - Len - 0.5f));
	UAudioComponent* C = UGameplayStatics::SpawnSoundAtLocation(GetWorld(), W, Loc, FRotator::ZeroRotator, Vol, Pitch, Start, bFar ? AttenFar() : Atten());
	if (!C) return false;
	C->FadeIn(0.08f, Vol, Start);
	TWeakObjectPtr<UAudioComponent> WC(C); FTimerHandle H;
	GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WC]() { if (WC.IsValid()) WC->FadeOut(0.5f, 0.f); }), Len, false);
	return true;
}