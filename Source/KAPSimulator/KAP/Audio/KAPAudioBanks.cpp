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

bool UKAPAudio::Speak(const FString& Text, FVector Loc, bool bFemale, int32 Mode, float Vol, float Pitch)
{
	if (!GetWorld() || Text.IsEmpty()) return false;
	const FString N = FString::Printf(TEXT("VO_%08x_%s"), VoHash(Text), bFemale ? TEXT("f") : TEXT("m"));
	USoundWave* W = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/KAP/Audio/VO/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UE_LOG(LogTemp, Log, TEXT("KAPVO %s %s mode=%d"), *N, W ? TEXT("ok") : TEXT("miss"), Mode);
	if (!W) return false;
	UAudioComponent* C = Mode == 0
		? UGameplayStatics::SpawnSoundAtLocation(GetWorld(), W, Loc, FRotator::ZeroRotator, Vol * 1.4f, Pitch, 0.f, Atten())
		: UGameplayStatics::SpawnSound2D(GetWorld(), W, Vol * (Mode == 2 ? 0.8f : 1.f), Pitch);
	if (!C) return false;
	if (Mode == 2)
	{
		// рация: жолақ сүзгісі (420 Гц – 3.2 кГц) + басында «тшш»
		C->SetLowPassFilterEnabled(true); C->SetLowPassFilterFrequency(3200.f);
		C->SetHighPassFilterEnabled(true); C->SetHighPassFilterFrequency(420.f);
		Impact(FVector::ZeroVector, 4, 0.4f);
		SetRadio(1.f);
		FTimerHandle H; TWeakObjectPtr<UKAPAudio> Self(this);
		GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self]() { if (Self.IsValid()) Self->SetRadio(0.f); }), FMath::Max(0.3f, W->Duration), false);
	}
	if (Mode != 0) { if (SpeechC) SpeechC->FadeOut(0.15f, 0.f); SpeechC = C; }
	Duck(W->Duration, Mode == 0 ? 0.2f : 0.4f);
	return true;
}

void UKAPAudio::StopSpeech() { if (SpeechC) { SpeechC->FadeOut(0.2f, 0.f); SpeechC = nullptr; } }

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