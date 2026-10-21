#include "KAPAudio.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/ReverbEffect.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

static constexpr int32 SR = 44100;
#include "KAPGeigerClicks.inl"
static void Noise(TArray<int16>& Out, float Dur, float Vol, float Cut, float Decay);
static void Tone(TArray<int16>& Out, float Start, float Dur, float Hz, float Vol, float SlideTo = 0, int32 Wave = 0);
static void AddEcho(TArray<int16>& B, float Delay, float Gain, float Cut);

UKAPSynthWave::UKAPSynthWave(const FObjectInitializer& OI) : Super(OI)
{
	SetSampleRate(SR);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
	SoundGroup = SOUNDGROUP_Default;
	bProcedural = true;
}

int32 UKAPSynthWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.SetNumUninitialized(NumSamples * 2);
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());
	for (int32 i = 0; i < NumSamples; i++)
	{
		Gain += (TargetGain - Gain) * 0.0002f;
		float S;
		Seed = Seed * 1664525u + 1013904223u;
		const float Wn = ((Seed >> 9) & 0xFFFF) / 32768.f - 1.f;
		if (Mode == 1)
		{
			// Гейгер: Пуассон процесі, келесі шертпеге дейін -ln(U)/λ сек
			NextClick -= 1.f / SR;
			if (NextClick <= 0 && Rate > 0.01f)
			{
				NextClick = -FMath::Loge(FMath::Max(1e-4f, ((Seed >> 8) & 0xFFFF) / 65536.f)) / Rate;
				// бос дауысқа нақты шертпе (Eberline E-120 жазбасынан), әр жолы басқасы
				for (int32 v = 0; v < 4; v++) if (ClkPos[v] < 0) { ClkPos[v] = 0; ClkIdx[v] = (int32)((Seed >> 4) % (uint32)GGeigerClickNum); break; }
			}
			S = 0.f;
			for (int32 v = 0; v < 4; v++)
				if (ClkPos[v] >= 0) { S += GGeigerClicks[ClkIdx[v]][ClkPos[v]] * 1.5f; if (++ClkPos[v] >= GGeigerClickLen) ClkPos[v] = -1; }
		}
		else if (Mode == 2)
		{
			// жүрек: «лүп-дүп» — 55/48 Гц төмен соққылар
			ModPh += Rate / 60.f / SR; if (ModPh > 1) ModPh -= 1;
			Phase += 52.f / SR; if (Phase > 1) Phase -= 1;
			const float E1 = FMath::Exp(-FMath::Square((ModPh - 0.04f) / 0.025f)), E2 = FMath::Exp(-FMath::Square((ModPh - 0.2f) / 0.03f)) * 0.7f;
			S = FMath::Sin(Phase * UE_TWO_PI) * (E1 + E2) * 1.4f;
		}
		else if (Mode == 3)
		{
			// тыныс: сүзгіленген шу, дем алу (ұзын) / шығару (қысқа)
			ModPh += Rate / 60.f / SR; if (ModPh > 1) ModPh -= 1;
			const float Env = ModPh < 0.45f ? FMath::Sin(ModPh / 0.45f * PI) * 0.7f : FMath::Sin((ModPh - 0.45f) / 0.55f * PI);
			Lp += (Wn - Lp) * 0.18f; Lp2 += (Lp - Lp2) * 0.35f;
			S = (Lp - Lp2) * Env * 3.f;
		}
		else if (Mode == 7)
		{
			// динамикалық музыка (A минор): аккорд-фон → ішекті тремоло (кіші секунда) → барабан (140 BPM)
			SampleN++;
			Int7 += (Rate - Int7) * 0.00002f;
			const float T = SampleN / (float)SR;
			static const float Ch[3][3] = { { 110.f, 130.81f, 164.81f }, { 87.31f, 110.f, 130.81f }, { 82.41f, 123.47f, 164.81f } };
			const int32 Ci = (int32)(T / 8.f) % 3;
			float Pad = 0;
			for (int32 k = 0; k < 3; k++) { Ph7[k] += Ch[Ci][k] / SR; if (Ph7[k] > 1) Ph7[k] -= 1; Pad += FMath::Sin(Ph7[k] * UE_TWO_PI) + 0.3f * FMath::Sin(Ph7[k] * 2.f * UE_TWO_PI); }
			Pad *= 0.12f * (1.f - Int7 * 0.4f) * (0.8f + 0.2f * FMath::Sin(T * 0.4f));
			float Str = 0;
			if (Int7 > 0.25f)
			{
				Ph7[3] += 220.f / SR; if (Ph7[3] > 1) Ph7[3] -= 1;
				Ph7[4] += 233.08f / SR; if (Ph7[4] > 1) Ph7[4] -= 1;
				const float Saw = (Ph7[3] * 2 - 1) + (Ph7[4] * 2 - 1);
				Lp7 += (Saw - Lp7) * 0.08f;
				const float Trem = 0.55f + 0.45f * FMath::Sin(T * UE_TWO_PI * 9.f);
				Str = Lp7 * Trem * (Int7 - 0.25f) * 0.35f;
			}
			float Drum = 0;
			if (Int7 > 0.55f)
			{
				const float Beat = 60.f / 140.f, X = FMath::Fmod(T, Beat);
				const int32 Bi = (int32)(T / Beat) % 4;
				if (Bi != 3) Drum += FMath::Sin(UE_TWO_PI * (50.f * X + 2.f * (1.f - FMath::Exp(-X * 40.f)))) * FMath::Exp(-X * 9.f);   // бас барабан
				const float Xo = FMath::Fmod(T + Beat * 0.5f, Beat);
				Lp7b += (Wn - Lp7b) * 0.3f;
				if (Bi % 2 == 1) Drum += (Wn - Lp7b) * FMath::Exp(-Xo * 45.f) * 0.7f;                                                  // металл соққы
				Drum *= (Int7 - 0.55f) * 1.5f;
			}
			S = Pad + Str + Drum;
		}
		else if (Mode == 6)
		{
			// үрей дроны: үш ара тісті тон (бір-бірінен сәл ауытқыған — соғу эффектісі), баяу ашылып-жабылатын LPF
			ModPh += 0.03f / SR; if (ModPh > 1) ModPh -= 1;
			Phase += 41.2f / SR; if (Phase > 1) Phase -= 1;
			ChirpPh += 41.55f / SR; if (ChirpPh > 1) ChirpPh -= 1;
			Sub += 61.7f / SR; if (Sub > 1) Sub -= 1;
			const float Mix = (Phase * 2 - 1) + (ChirpPh * 2 - 1) * 0.8f + (Sub * 2 - 1) * 0.35f * Rate;
			const float Cf = 0.004f + 0.012f * (0.5f + 0.5f * FMath::Sin(ModPh * UE_TWO_PI)) + 0.02f * Rate;
			Lp += (Mix - Lp) * Cf; Lp2 += (Lp - Lp2) * Cf;
			S = Lp2 * 0.95f + Wn * 0.006f;
		}
		else if (Mode == 5)
		{
			// апат сиренасы: 480↔900 Гц баяу ауытқу (~4 с), жұмсақ бұрмаланған тон + 2-гармоника
			ModPh += 0.25f / SR; if (ModPh > 1) ModPh -= 1;
			const float F = 480.f + 420.f * (0.5f - 0.5f * FMath::Cos(ModPh * UE_TWO_PI));
			Phase += F / SR; if (Phase > 1) Phase -= 1;
			const float Sn = FMath::Sin(Phase * UE_TWO_PI);
			S = FMath::Tanh(Sn * 1.4f) * 0.45f + FMath::Sin(Phase * 2.f * UE_TWO_PI) * 0.08f;
		}
		else if (Mode == 4)
		{
			// жел ысқырығы: Чемберлин SVF жолақ сүзгісі, жиілігі екпінмен баяу жылжиды (ғимарат/сым маңындағы резонанс)
			ResCur += (ResHz - ResCur) * 0.00005f;
			const float F = 2.f * FMath::Sin(PI * FMath::Clamp(ResCur, 60.f, 4000.f) / SR);
			SvL += F * SvB; const float Hp = Wn - SvL - 0.06f * SvB; SvB += F * Hp;
			S = SvB * 0.6f;
		}
		else if (ToneHz > 0)
		{
			// двигатель: ара тісі + субгармоника + шу, «жіберу» — цикл үнсіз
			Phase += ToneHz / SR;
			if (Phase > 1) { Phase -= 1; Sub = -Sub + (Sub == 0 ? 1.f : 0.f); SkipEnv = (Misfire > 0 && ((Seed >> 4) & 0xFFFF) / 65536.f < Misfire) ? 0.15f : 1.f; }
			const float Saw = Phase * 2 - 1;
			const float Pulse = FMath::Exp(-Phase * 6.f) * 1.4f - 0.3f;
			Lp += ((Saw * 0.6f + Pulse * 0.8f + Sub * 0.25f + Wn * 0.12f) * SkipEnv - Lp) * Cutoff;
			S = Lp;
		}
		else
		{
			const float N = Wn;
			Lp += (N - Lp) * Cutoff * Gust;
			Lp2 += (Lp - Lp2) * 0.5f;
			S = HighPass > 0 ? (N - Lp) * HighPass : Lp2 * 2.5f;
			if (Crackle > 0 && (Seed & 0x3FF) < 3) S += Crackle * ((Seed & 1) ? 1.f : -1.f);
			if (Chirp > 0)
			{
				// шегіртке: 4.5 кГц тон, секундына ~3 рет қысқа импульстер
				SampleN++; ChirpPh += 4500.f / SR; if (ChirpPh > 1) ChirpPh -= 1;
				const float Tt = FMath::Fmod(SampleN / (float)SR, 0.33f);
				const float Env = (Tt < 0.05f) ? FMath::Sin(Tt / 0.05f * PI) * FMath::Square(FMath::Sin(FMath::Fmod(Tt, 0.012f) / 0.012f * PI)) : 0.f;
				S = FMath::Sin(ChirpPh * 2 * PI) * Env * Chirp + S * 0.1f;
			}
		}
		const float Y = S * Gain;
		Out[i] = (int16)(FMath::Tanh(Y * 1.15f) / 1.15f * 32000.f); // қатаң кесудің орнына жұмсақ қанығу
	}
	return NumSamples;
}

UKAPAudio* UKAPAudio::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	UKAPAudio* A = W ? W->GetSubsystem<UKAPAudio>() : nullptr;
	if (A) A->EnsureInit();
	return A;
}

UKAPSynthWave* UKAPAudio::MakeLoop(UAudioComponent*& OutComp, float Cutoff, float HighPass, float Tone)
{
	UKAPSynthWave* W = NewObject<UKAPSynthWave>(this);
	W->Cutoff = Cutoff; W->HighPass = HighPass; W->ToneHz = Tone;
	OutComp = UGameplayStatics::CreateSound2D(GetWorld(), W, 1.f, 1.f, 0.f, nullptr, true, false);
	if (OutComp) { OutComp->bAutoDestroy = false; OutComp->Play(); Comps.Add(OutComp); }
	return W;
}

void UKAPAudio::EnsureInit()
{
	if (bInit || !GetWorld() || !GetWorld()->IsGameWorld()) return;
	bInit = true;
	// жазылған дыбыс банктерін алдын ала жүктеу — алғашқы атыс/қадам кезінде кідіріс болмасын
	for (const TCHAR* Bk : { TEXT("StepSand"), TEXT("StepConcrete"), TEXT("StepWood"), TEXT("StepDirt"), TEXT("StepMetal"), TEXT("ImpMetal"), TEXT("ImpWood"), TEXT("ImpBody"), TEXT("ImpSand"), TEXT("DoorOpen"), TEXT("DoorClose"), TEXT("Creak"), TEXT("Engine"), TEXT("Scream"), TEXT("Yell"), TEXT("Grunt"), TEXT("Moan"), TEXT("GunPM"), TEXT("GunAK"), TEXT("GunSniper"), TEXT("GunShotgun") }) HasBank(Bk);
	UAudioComponent* C = nullptr;
	Wind = MakeLoop(C, 0.02f, 0, 0);
	Rain = MakeLoop(C, 0.3f, 0.8f, 0);
	Fire = MakeLoop(C, 0.08f, 0, 0); Fire->Crackle = 0.8f;
	Radio = MakeLoop(C, 0.6f, 0.9f, 0);
	Engine = MakeLoop(C, 0.05f, 0, 45.f);
	Crickets = MakeLoop(C, 0.5f, 0, 0); Crickets->Chirp = 1.f;
	WindHi = MakeLoop(C, 0.25f, 0.6f, 0); WindHi->Mode = 4;   // резонанстық ысқырық қабаты
	Geiger = MakeLoop(C, 0.5f, 0, 0); Geiger->Mode = 1;
	Heart = MakeLoop(C, 0.5f, 0, 0); Heart->Mode = 2; Heart->Rate = 70;
	Breath = MakeLoop(C, 0.5f, 0, 0); Breath->Mode = 3; Breath->Rate = 16;
	Dread = MakeLoop(C, 0.5f, 0, 0); Dread->Mode = 6;
	Music = MakeLoop(C, 0.5f, 0, 0); Music->Mode = 7;
}

// Фон қабаттары: жел (төмен гул + ысқырық), жаңбыр — баспана мен ducking ескеріледі
void UKAPAudio::ApplyBeds()
{
	const float Duck = 1.f - DuckAmt * FMath::Clamp(DuckT, 0.f, 1.f);
	const float In = 1.f - IndoorK * 0.75f;
	if (Wind) { Wind->TargetGain = (0.05f + WindL * 0.45f) * GustK * In * Duck; Wind->Cutoff = (0.008f + WindL * 0.035f) * (1.f - IndoorK * 0.6f); Wind->Gust = GustK; }
	if (WindHi) { WindHi->TargetGain = (0.01f + FMath::Max(0.f, WindL - 0.15f) * 0.09f) * FMath::Square(GustK) * (1.f - IndoorK * 0.5f) * Duck; WindHi->ResHz = (IndoorK > 0.5f ? 260.f : 420.f) + GustK * 380.f + WindL * 300.f; }
	if (Rain) { Rain->TargetGain = RainL * 0.25f * (1.f - IndoorK * 0.5f) * Duck; Rain->HighPass = 0.8f - IndoorK * 0.5f; }
}

void UKAPAudio::SetWind(float L) { WindL = FMath::Clamp(L, 0.f, 1.f); ApplyBeds(); }
void UKAPAudio::SetRain(float L) { RainL = FMath::Clamp(L, 0.f, 1.f); ApplyBeds(); }
void UKAPAudio::SetIndoor(float K) { IndoorK = FMath::Clamp(K, 0.f, 1.f); ApplyBeds(); }
void UKAPAudio::Duck(float Seconds, float Amount) { DuckT = Seconds; DuckAmt = Amount; ApplyBeds(); }
void UKAPAudio::SetGeiger(float Dose, bool bOn)
{
	if (!Geiger) return;
	// фон ~0.15 мкЗв/сағ → 0.5 шертпе/с; 100 мкЗв/сағ → ~60/с
	Geiger->Rate = bOn ? FMath::Clamp(0.5f + Dose * 0.6f, 0.f, 220.f) : 0.f;
	Geiger->TargetGain = bOn ? 0.35f : 0.f;
}
void UKAPAudio::SetBody(float Exhaust, float Fear)
{
	if (!Heart || !Breath) return;
	const float A = FMath::Clamp(FMath::Max(Exhaust, Fear), 0.f, 1.f);
	Heart->Rate = 68.f + A * 75.f;
	Heart->TargetGain = FMath::Max(0.f, Fear - 0.25f) * 0.5f;
	Breath->Rate = 14.f + Exhaust * 26.f;
	Breath->TargetGain = FMath::Max(0.f, Exhaust - 0.35f) * 0.18f;
}
void UKAPAudio::SetEngineLoad(float Rpm01, float Mis) { if (bRealEngine) { EngRpm = Rpm01; EngMis = Mis; if (Engine) Engine->TargetGain = 0.f; return; } if (Engine) { Engine->Misfire = Mis; Engine->ToneHz = 32.f + Rpm01 * 95.f; Engine->Cutoff = 0.04f + Rpm01 * 0.08f; } }

void UKAPAudio::Stinger(bool bDanger)
{
	TArray<int16> B;
	if (bDanger) { Noise(B, 1.4f, 0.4f, 0.02f, 0.7f); Tone(B, 0, 1.6f, 58, 0.35f, 46, 2); Tone(B, 0.05f, 1.4f, 233, 0.12f, 220, 1); Tone(B, 0.05f, 1.4f, 247, 0.1f, 233, 1); }
	else { Tone(B, 0, 1.8f, 196, 0.12f, 0, 1); Tone(B, 0.25f, 1.8f, 294, 0.1f, 0, 1); Tone(B, 0.5f, 2.f, 392, 0.08f, 0, 1); }
	Duck(2.f, 0.6f);
	PlayBuffer(B, 0.8f);
}

void UKAPAudio::Update(float Dt, FVector L, bool bNight, float Storm)
{
	UpdateReal(Dt);
	// реверберация: баспанада — шағын металл бөлме, далада — ашық кеңістік (әлсіз, ұзақ)
	const int32 Want = IndoorK > 0.5f ? 1 : 0;
	if (Want != RevState)
	{
		RevState = Want;
		if (!RevOut) { RevOut = NewObject<UReverbEffect>(this); RevOut->Density = 0.4f; RevOut->Diffusion = 0.3f; RevOut->Gain = 0.18f; RevOut->GainHF = 0.4f; RevOut->DecayTime = 2.6f; RevOut->DecayHFRatio = 0.4f; RevOut->ReflectionsGain = 0.02f; RevOut->ReflectionsDelay = 0.12f; RevOut->LateGain = 0.6f; RevOut->LateDelay = 0.09f; RevOut->AirAbsorptionGainHF = 0.98f; }
		if (!RevIn) { RevIn = NewObject<UReverbEffect>(this); RevIn->Density = 1.f; RevIn->Diffusion = 0.85f; RevIn->Gain = 0.4f; RevIn->GainHF = 0.75f; RevIn->DecayTime = 0.7f; RevIn->DecayHFRatio = 0.7f; RevIn->ReflectionsGain = 0.9f; RevIn->ReflectionsDelay = 0.004f; RevIn->LateGain = 1.3f; RevIn->LateDelay = 0.011f; RevIn->AirAbsorptionGainHF = 0.99f; }
		UGameplayStatics::ActivateReverbEffect(GetWorld(), Want ? RevIn.Get() : RevOut.Get(), Want ? FName(TEXT("KAPIn")) : FName(TEXT("KAPOut")), 1.f, 0.8f, 0.6f);
		UGameplayStatics::DeactivateReverbEffect(GetWorld(), Want ? FName(TEXT("KAPOut")) : FName(TEXT("KAPIn")));
	}
	// жел екпіндері: баяу шу + кездейсоқ күшті серпін
	GustT += Dt;
	const float Slow = FMath::PerlinNoise1D(GustT * 0.23f) * 0.5f + 0.5f;
	const float Burst = FMath::Max(0.f, FMath::PerlinNoise1D(GustT * 0.9f + 17.f) * 2.f - 0.5f);
	GustK = 0.7f + Slow * 0.5f + Burst * (0.4f + Storm * 0.8f);
	if (DuckT > 0) DuckT -= Dt;
	ApplyBeds();
	// алыстағы «тірі» дыбыстар
	AmbT -= Dt;
	if (AmbT <= 0)
	{
		AmbT = bNight ? FMath::FRandRange(8.f, 20.f) : FMath::FRandRange(14.f, 35.f);
		const FVector P = L + FRotator(0, FMath::FRandRange(0.f, 360.f), 0).Vector() * FMath::FRandRange(2500.f, 5000.f);
		TArray<int16> B;
		const float R = FMath::FRand();
		if (Storm > 0.4f || R < 0.3f)
		{
			// металл сықыры (ескі вагон, тор)
			Tone(B, 0, 1.2f, 410, 0.18f, 360, 2); Tone(B, 0.1f, 1.f, 610, 0.08f, 560, 1);
		}
		else if (bNight && R < 0.5f)
		{
			// алыстағы металл соққысы: ауыр соғу + ұзақ «шыңылдап» өшетін резонанс
			const int32 N = FMath::RandRange(1, 3);
			for (int32 i = 0; i < N; i++) { const float T0 = i * FMath::FRandRange(0.5f, 1.1f); TArray<int16> Hit; Noise(Hit, 0.25f, 0.8f, 0.2f, 0.05f); B.SetNumZeroed(FMath::Max(B.Num(), (int32)(T0 * SR) + Hit.Num())); for (int32 k = 0; k < Hit.Num(); k++) B[(int32)(T0 * SR) + k] = (int16)FMath::Clamp((int32)B[(int32)(T0 * SR) + k] + Hit[k], -32767, 32767); Tone(B, T0, 2.2f, 147.f, 0.18f, 140.f, 1); Tone(B, T0, 1.6f, 311.f, 0.08f, 0, 0); }
			AddEcho(B, 0.6f, 0.3f, 0.05f);
		}
		else if (bNight && R < 0.62f) { Creature(P, 1, 0.55f, true); }
		else if (bNight && R < 0.7f && Storm < 0.3f) { Creature(L + FRotator(0, FMath::FRandRange(0.f, 360.f), 0).Vector() * FMath::FRandRange(250.f, 450.f), 3, 0.22f, false); }
		else if (bNight && R < 0.85f)
		{
			// ит үруі: 2-4 қысқа «ау»
			const int32 N = FMath::RandRange(2, 4);
			for (int32 i = 0; i < N; i++) { Tone(B, i * 0.38f, 0.16f, 520, 0.4f, 380, 2); Tone(B, i * 0.38f, 0.14f, 1040, 0.12f, 760, 2); }
		}
		else if (!bNight)
		{
			// құс/жыртқыш құс шақыруы
			Tone(B, 0, 0.5f, 2400, 0.12f, 1800); Tone(B, 0.6f, 0.4f, 2200, 0.1f, 1700);
		}
		if (B.Num() && GetWorld()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), MakeBuffer(B), P, 0.9f, FMath::FRandRange(0.9f, 1.1f), 0.f, AttenFar());
	}
}
void UKAPAudio::SetFire(float L) { if (Fire) Fire->TargetGain = FMath::Clamp(L, 0.f, 1.f) * 0.3f; }
void UKAPAudio::SetRadio(float L) { if (Radio) Radio->TargetGain = FMath::Clamp(L, 0.f, 1.f) * 0.12f; }
void UKAPAudio::SetEngine(bool bOn, float V) { if (HasBank("Engine")) { bRealEngine = true; EngOn = bOn ? 1.f : 0.f; EngRpm = FMath::Max(EngRpm * 0.98f, FMath::Clamp(V / 22.f, 0.f, 1.f)); if (Engine) Engine->TargetGain = 0.f; return; } if (Engine) { Engine->TargetGain = bOn ? 0.08f + FMath::Min(0.1f, V * 0.004f) : 0.f; Engine->ToneHz = 38.f + V * 3.f; } }
void UKAPAudio::StopLoops() { EngOn = 0.f; MusV = 0.f; for (UKAPSynthWave* W : { Wind.Get(), Rain.Get(), Fire.Get(), Radio.Get(), Engine.Get() }) if (W) W->TargetGain = 0; }

void UKAPAudio::PlayBuffer(const TArray<int16>& Pcm, float Vol)
{
	if (!GetWorld()) return;
	TArray<int16> P2 = Pcm; Finalize(P2);
	if (bCapture) return;
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(SR); W->NumChannels = 1; W->Duration = Pcm.Num() / (float)SR; W->SoundGroup = SOUNDGROUP_Default; W->bLooping = false;
	W->QueueAudio(reinterpret_cast<const uint8*>(P2.GetData()), P2.Num() * 2);
	UGameplayStatics::PlaySound2D(GetWorld(), W, Vol);
	OneShots.Add(W);
	if (OneShots.Num() > 32) OneShots.RemoveAt(0);
}

static void Noise(TArray<int16>& Out, float Dur, float Vol, float Cut, float Decay)
{
	const int32 N = (int32)(Dur * SR);
	Out.SetNumUninitialized(N);
	float Lp = 0; uint32 S = FMath::Rand();
	for (int32 i = 0; i < N; i++)
	{
		S = S * 1664525u + 1013904223u;
		const float X = ((S >> 9) & 0xFFFF) / 32768.f - 1.f;
		Lp += (X - Lp) * Cut;
		const float Env = FMath::Exp(-(float)i / SR / Decay);
		Out[i] = (int16)FMath::Clamp(Lp * Env * Vol * 32767.f * 2.f, -32767.f, 32767.f);
	}
}
/** Кешіккен, сүзгіленген көшірме (жаңғырық) */
static void AddEcho(TArray<int16>& B, float Delay, float Gain, float Cut)
{
	const int32 D = (int32)(Delay * SR), N = B.Num();
	if (N == 0) return;
	TArray<int16> Src = B; B.SetNumZeroed(N + D);
	float Lp = 0;
	for (int32 i = 0; i < N; i++) { Lp += (Src[i] - Lp) * Cut * 4.f; B[i + D] = (int16)FMath::Clamp((int32)B[i + D] + (int32)(Lp * Gain * 3.f), -32767, 32767); }
}
static void Tone(TArray<int16>& Out, float Start, float Dur, float Hz, float Vol, float SlideTo, int32 Wave)
{
	const int32 S0 = (int32)(Start * SR), N = (int32)(Dur * SR);
	if (Out.Num() < S0 + N) { const int32 Old = Out.Num(); Out.SetNumUninitialized(S0 + N); for (int32 i = Old; i < Out.Num(); i++) Out[i] = 0; }
	float Ph = 0;
	for (int32 i = 0; i < N; i++)
	{
		const float T = (float)i / N;
		const float F = SlideTo > 0 ? FMath::Lerp(Hz, SlideTo, T) : Hz;
		Ph += F / SR; Ph -= FMath::FloorToFloat(Ph);
		float V = Wave == 0 ? FMath::Sin(Ph * UE_TWO_PI) : Wave == 1 ? (4.f * FMath::Abs(Ph - 0.5f) - 1.f) : (Ph * 2 - 1);
		const float Env = FMath::Min(1.f, T * 30.f) * FMath::Exp(-T * 3.f);
		Out[S0 + i] = (int16)FMath::Clamp(Out[S0 + i] + V * Env * Vol * 32767.f, -32767.f, 32767.f);
	}
}

void UKAPAudio::Play(EKAPSound S, float Vol)
{
	TArray<int16> B;
	switch (S)
	{
	case EKAPSound::Swing: Noise(B, 0.15f, 0.25f, 0.15f, 0.06f); break;
	case EKAPSound::Hit: Noise(B, 0.1f, 0.5f, 0.3f, 0.03f); break;
	case EKAPSound::Hurt: if (HasBank("Grunt")) { PlayBank("Grunt", FVector::ZeroVector, Vol, FMath::FRandRange(0.95f, 1.05f), true); return; } Tone(B, 0, 0.25f, 160, 0.3f, 90, 2); break;
	case EKAPSound::Pickup: Tone(B, 0, 0.08f, 660, 0.25f); Tone(B, 0.08f, 0.1f, 990, 0.25f); break;
	case EKAPSound::Growl: Tone(B, 0, 0.7f, 75, 0.45f, 45, 2); break;
	case EKAPSound::Howl: Tone(B, 0, 1.6f, 380, 0.3f, 620, 1); Tone(B, 1.4f, 1.2f, 620, 0.2f, 300, 1); break;
	case EKAPSound::Thunder:
	{
		// найзағай: жарқыл шытыры + ұзақ, толқынды гүріл (бірнеше қайта шағылу)
		Noise(B, 0.3f, 0.9f, 0.45f, 0.06f);
		TArray<int16> Rm; Noise(Rm, 4.5f, 1.f, 0.006f, 1.6f);
		for (int32 i = 0; i < Rm.Num(); i++) { const float T = i / (float)SR; Rm[i] = (int16)(Rm[i] * (0.6f + 0.4f * FMath::Sin(T * 5.3f) * FMath::Sin(T * 1.7f + 1.f))); }
		const int32 Off = SR / 6; B.SetNumZeroed(FMath::Max(B.Num(), Off + Rm.Num()));
		for (int32 i = 0; i < Rm.Num(); i++) B[Off + i] = (int16)FMath::Clamp((int32)B[Off + i] + Rm[i], -32767, 32767);
		break;
	}
	case EKAPSound::Steam: Noise(B, 1.6f, 0.3f, 0.9f, 0.8f); break;
	case EKAPSound::ShotFar: Noise(B, 0.3f, 0.5f, 0.06f, 0.1f); break;
	case EKAPSound::Beep: Tone(B, 0, 0.12f, 880, 0.2f, 0, 2); break;
	case EKAPSound::BeepBad: Tone(B, 0, 0.15f, 220, 0.2f, 0, 2); break;
	case EKAPSound::RepairDone: { const float F[4] = { 523, 659, 784, 1046 }; for (int32 i = 0; i < 4; i++) Tone(B, i * 0.12f, 0.25f, F[i], 0.2f, 0, 1); break; }
	}
	PlayBuffer(B, Vol);
}



float UKAPAudio::Lament()
{
	// минорлық жоқтау әуені (домбыра/қобыз сарыны ретінде)
	const float N[][2] = { { 392, 1.2f }, { 440, 0.6f }, { 392, 0.6f }, { 349, 1.2f }, { 311, 1.6f }, { 349, 0.8f }, { 392, 0.8f }, { 311, 1.2f }, { 294, 2.4f }, { 262, 1.2f }, { 294, 1.2f }, { 311, 2.f }, { 294, 3.f } };
	TArray<int16> B; float T = 0;
	for (const auto& X : N) { Tone(B, T, X[1] * 1.15f, X[0], 0.16f, 0, 1); Tone(B, T, X[1] * 1.15f, X[0] / 2, 0.08f); T += X[1]; }
	PlayBuffer(B, 0.9f);
	return T;
}

void UKAPAudio::SetCrickets(float L) { if (Crickets) Crickets->TargetGain = FMath::Clamp(L, 0.f, 1.f) * 0.06f; }

USoundAttenuation* UKAPAudio::Atten()
{
	if (!Attenuation)
	{
		Attenuation = NewObject<USoundAttenuation>(this);
		Attenuation->Attenuation.bAttenuate = true;
		Attenuation->Attenuation.bSpatialize = true;
		Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Attenuation->Attenuation.AttenuationShapeExtents = FVector(250.f);
		Attenuation->Attenuation.FalloffDistance = 3500.f;
		Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	}
	return Attenuation;
}

USoundWaveProcedural* UKAPAudio::MakeBuffer(const TArray<int16>& PcmIn)
{
	TArray<int16> Pcm = PcmIn; Finalize(Pcm);
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(SR); W->NumChannels = 1; W->Duration = Pcm.Num() / (float)SR; W->SoundGroup = SOUNDGROUP_Default; W->bLooping = false;
	W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * 2);
	OneShots.Add(W);
	if (OneShots.Num() > 48) OneShots.RemoveAt(0);
	return W;
}

void UKAPAudio::PlayAt(EKAPSound S, FVector Loc, float Vol)
{
	TArray<int16> B;
	switch (S)
	{
	case EKAPSound::Footstep: Noise(B, 0.12f, 0.35f, 0.45f, 0.035f); break;
	case EKAPSound::Cough: Noise(B, 0.18f, 0.5f, 0.12f, 0.06f); { TArray<int16> B2; Noise(B2, 0.2f, 0.4f, 0.1f, 0.07f); B.AddZeroed(SR / 8); B.Append(B2); } break;
	case EKAPSound::Growl: Tone(B, 0, 0.7f, 75, 0.5f, 45, 2); break;
	case EKAPSound::Howl: Tone(B, 0, 1.6f, 380, 0.35f, 620, 1); Tone(B, 1.4f, 1.2f, 620, 0.25f, 300, 1); break;
	case EKAPSound::ShotFar: Noise(B, 0.35f, 0.9f, 0.08f, 0.12f); break;
	default: Noise(B, 0.1f, 0.4f, 0.3f, 0.04f); break;
	}
	if (GetWorld()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), MakeBuffer(B), Loc, Vol, FMath::FRandRange(0.92f, 1.08f), 0.f, Atten());
}

// Формант синтезі: глотталь импульстері → екі резонатор (F1, F2) → буындар, интонация, дауыссыздар
static void GenVoice(TArray<int16>& Out, float Seconds, bool bFemale, int32 Seed);
void UKAPAudio::Voice(FVector Loc, float Seconds, bool bFemale, float Vol, int32 Seed)
{
	TArray<int16> Out; GenVoice(Out, Seconds, bFemale, Seed);
	if (GetWorld()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), MakeBuffer(Out), Loc, Vol, 1.f, 0.f, Atten());
}
// Рация: дауыс 300–3000 Гц жолағында, бұрмаланған, шытырлақ және «тшш» басы/соңымен (2D)
void UKAPAudio::RadioVoice(float Seconds, bool bFemale)
{
	TArray<int16> V; GenVoice(V, Seconds, bFemale, FMath::Rand());
	TArray<int16> Out; Noise(Out, 0.18f, 0.25f, 0.9f, 0.5f);
	const int32 Off = Out.Num(); Out.SetNumZeroed(Off + V.Num() + SR / 4);
	float Hp = 0, Lp = 0; uint32 Ns = FMath::Rand();
	for (int32 i = 0; i < V.Num(); i++)
	{
		const float X = V[i] / 32767.f;
		Hp += (X - Hp) * 0.04f; const float B = X - Hp; Lp += (B - Lp) * 0.45f;
		Ns = Ns * 1664525u + 1013904223u; const float Nz = ((Ns >> 9) & 0xFFFF) / 32768.f - 1.f;
		const float Cr = (Ns & 0x7FF) < 3 ? Nz : 0.f;
		Out[Off + i] = (int16)FMath::Clamp((FMath::Clamp(Lp * 2.5f, -0.6f, 0.6f) + Nz * 0.035f + Cr * 0.4f) * 32767.f, -32767.f, 32767.f);
	}
	TArray<int16> Tail; Noise(Tail, 0.22f, 0.3f, 0.9f, 0.08f);
	for (int32 i = 0; i < Tail.Num() && Off + V.Num() + i < Out.Num(); i++) Out[Off + V.Num() + i] = Tail[i];
	PlayBuffer(Out, 0.55f);
}
static void GenVoice(TArray<int16>& Out, float Seconds, bool bFemale, int32 Seed)
{
	FRandomStream R(Seed ? Seed : FMath::Rand());
	static const float Vowels[7][2] = { { 750, 1250 }, { 550, 1800 }, { 320, 2300 }, { 500, 900 }, { 360, 800 }, { 650, 1600 }, { 420, 1500 } }; // а е і о ұ ә ө
	const int32 N = (int32)(Seconds * SR);
	Out.SetNumZeroed(N);
	const float Base = bFemale ? R.FRandRange(195.f, 235.f) : R.FRandRange(105.f, 135.f);
	float Ph = 0, T = 0;
	int32 Pos = 0;
	struct FBq { float b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0; float Run(float x) { const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
	auto BandPass = [](float F, float Q) { const float W0 = 2 * PI * F / SR, Al = FMath::Sin(W0) / (2 * Q), A0 = 1 + Al; FBq B; B.b0 = Al / A0; B.b1 = 0; B.b2 = -Al / A0; B.a1 = -2 * FMath::Cos(W0) / A0; B.a2 = (1 - Al) / A0; return B; };
	uint32 Ns = R.RandHelper(1 << 30);
	while (Pos < N)
	{
		const int32 Syl = (int32)(R.FRandRange(0.11f, 0.24f) * SR);
		const int32 Gap = R.FRand() < 0.18f ? (int32)(R.FRandRange(0.08f, 0.2f) * SR) : 0; // сөз арасындағы үзіліс
		const int32 Vi = R.RandRange(0, 6);
		FBq F1 = BandPass(Vowels[Vi][0] * (bFemale ? 1.15f : 1.f), 6.f), F2 = BandPass(Vowels[Vi][1] * (bFemale ? 1.12f : 1.f), 9.f);
		const bool bCons = R.FRand() < 0.75f;
		const float Fric = R.FRand() < 0.4f ? 1.f : 0.f;
		for (int32 i = 0; i < Syl && Pos < N; i++, Pos++)
		{
			T = Pos / (float)SR;
			// интонация: сөйлем соңына қарай төмендейді + шамалы діріл
			const float F0 = Base * (1.f + 0.12f * FMath::Sin(T * 2.3f + Seed) - 0.15f * (T / Seconds)) * (1.f + 0.01f * FMath::Sin(T * 37.f));
			Ph += F0 / SR; float Pulse = 0; if (Ph >= 1) { Ph -= 1; Pulse = 1; }
			const float Src = Pulse * 2.f - 0.02f;
			const float u = i / (float)Syl;
			const float Env = FMath::Min(1.f, u * 8.f) * FMath::Min(1.f, (1.f - u) * 5.f);
			float S = (F1.Run(Src) * 1.2f + F2.Run(Src) * 0.7f) * Env;
			if (bCons && u < 0.12f)
			{
				Ns = Ns * 1664525u + 1013904223u;
				const float Nz = ((Ns >> 9) & 0xFFFF) / 32768.f - 1.f;
				S = S * u * 8.f + Nz * (Fric ? 0.25f : 0.12f) * (1.f - u * 8.f);
			}
			Out[Pos] = (int16)FMath::Clamp(S * 9000.f, -32767.f, 32767.f);
		}
		Pos += Gap;
	}
	// қаттылықты қалыпқа келтіру: резонаторлар сигналды қатты әлсіретеді (бұрын шың ~0.07 — естілмейтін)
	int32 Pk = 1; for (int16 V : Out) Pk = FMath::Max(Pk, FMath::Abs((int32)V));
	const float Gn = 0.6f * 32767.f / Pk;
	for (int16& V : Out) V = (int16)FMath::Clamp(V * Gn, -32767.f, 32767.f);
}

UAudioComponent* UKAPAudio::AttachHum(USceneComponent* To, float Hz)
{
	UKAPSynthWave* W = NewObject<UKAPSynthWave>(this);
	W->Cutoff = 0.03f; W->ToneHz = Hz; W->TargetGain = 0.12f; W->Gain = 0.12f;
	UAudioComponent* C = UGameplayStatics::SpawnSoundAttached(W, To, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, Atten(), nullptr, false);
	return C;
}
USoundAttenuation* UKAPAudio::AttenFar()
{
	if (!AttenuationFar)
	{
		AttenuationFar = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& A = AttenuationFar->Attenuation;
		A.bAttenuate = true; A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(1500.f);
		A.FalloffDistance = 9000.f;
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		// алыс дыбыстың жоғары жиілігі ауада жұтылады
		A.bAttenuateWithLPF = true; A.LPFRadiusMin = 1500.f; A.LPFRadiusMax = 9000.f; A.LPFFrequencyAtMax = 1800.f;
	}
	return AttenuationFar;
}

// Беткейге қарай қадам: 0 — құм, 1 — металл, 2 — ағаш/еден, 3 — тас/бетон
void UKAPAudio::Footstep(FVector Loc, int32 Surface, float Vol, bool bRun)
{
	{
		// құм: тау етігінің нақты қадамдары (жүру 24 / жүгіру 19 түрі, freesound CC0)
		if (Surface == 0 && bRun && HasBank("StepSandRun")) { PlayBank("StepSandRun", Loc, Vol * 0.85f, FMath::FRandRange(0.94f, 1.06f)); return; }
		static const FName FB[5] = { FName("StepSand"), FName("StepMetal"), FName("StepWood"), FName("StepConcrete"), FName("StepDirt") };
		if (Surface >= 0 && Surface < 5 && HasBank(FB[Surface]))
		{
			PlayBank(FB[Surface], Loc, Vol * (Surface == 1 ? 0.5f : 1.f), FMath::FRandRange(0.93f, 1.07f));
			return;
		}
	}
	TArray<int16> B;
	switch (Surface)
	{
	case 1: Noise(B, 0.09f, 0.25f, 0.6f, 0.02f); Tone(B, 0, 0.25f, 820, 0.08f, 760, 2); Tone(B, 0, 0.2f, 1370, 0.04f, 0, 1); break;
	case 2: Noise(B, 0.08f, 0.3f, 0.2f, 0.025f); Tone(B, 0, 0.08f, 140, 0.2f, 90, 0); break;
	case 3: Noise(B, 0.07f, 0.35f, 0.5f, 0.018f); { TArray<int16> Sc; Noise(Sc, 0.09f, 0.08f, 0.9f, 0.03f); for (int32 i = 0; i < Sc.Num() && i + 900 < B.Num(); i++) B[i + 900] = (int16)FMath::Clamp((int32)B[i + 900] + Sc[i], -32767, 32767); } break; // бетон: соғу + құм «сырылы»
	case 4:
	{
		// қиыршық тас: ірірек, сирек түйіршіктер, күштірек «қытыр»
		B.SetNumZeroed((int32)(0.22f * SR));
		FRandomStream Rg(FMath::Rand());
		for (int32 g = 0; g < 28; g++)
		{
			const int32 At = (int32)(Rg.FRand() * Rg.FRand() * 0.18f * SR);
			const float A = Rg.FRandRange(0.15f, 0.45f);
			for (int32 k = 0; k < 120 && At + k < B.Num(); k++) B[At + k] = (int16)FMath::Clamp((int32)B[At + k] + (int32)(A * 32767.f * FMath::Exp(-k / 18.f) * Rg.FRandRange(-1.f, 1.f)), -32767, 32767);
		}
		float Lp = 0; for (int32 i = 0; i < B.Num(); i++) { Lp += (B[i] - Lp) * 0.4f; B[i] = (int16)Lp; }
		break;
	}
	default:
	{
		// құм: өкше соғуы (төмен тұйық) + түйіршіктердің қытырлауы (гранулалық микро-шертпелер)
		Noise(B, 0.06f, 0.3f, 0.12f, 0.02f);
		B.SetNumZeroed((int32)(0.2f * SR));
		FRandomStream Rg(FMath::Rand());
		for (int32 g = 0; g < 70; g++)
		{
			const int32 At = (int32)(FMath::Square(Rg.FRand()) * 0.17f * SR);
			const float A = Rg.FRandRange(0.05f, 0.22f) * (1.f - At / (0.2f * SR));
			for (int32 k = 0; k < 40 && At + k < B.Num(); k++) B[At + k] = (int16)FMath::Clamp((int32)B[At + k] + (int32)(A * 32767.f * FMath::Exp(-k / 6.f) * Rg.FRandRange(-1.f, 1.f)), -32767, 32767);
		}
		float Lp = 0; for (int32 i = 0; i < B.Num(); i++) { Lp += (B[i] - Lp) * 0.55f; B[i] = (int16)Lp; }
		break;
	}
	}
	if (Surface == 0 || Surface == 4) for (int16& V : B) V = (int16)FMath::Clamp(V * (Surface == 0 ? 3.f : 3.5f), -32767.f, 32767.f); // бұрын бетоннан 14 дБ тыныш еді
	if (GetWorld()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), MakeBuffer(B), Loc, Vol, FMath::FRandRange(0.9f, 1.1f), 0.f, Atten());
}

// Сирена: компонентке тіркелген 3D цикл (дауысы TargetGain арқылы)
UKAPSynthWave* UKAPAudio::AttachSiren(USceneComponent* To)
{
	UKAPSynthWave* W = NewObject<UKAPSynthWave>(this);
	W->Mode = 5; W->TargetGain = 0.f; W->Gain = 0.f;
	UAudioComponent* C = UGameplayStatics::SpawnSoundAttached(W, To, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, AttenFar(), nullptr, false);
	(void)C;
	return W;
}

// Үрей фоны: 0 — жоқ, 1 — толық (түн, қауіп). Rate — дисонанс (үшінші тон) мөлшері
void UKAPAudio::SetDread(float L, float Tension)
{
	if (!Dread) return;
	Dread->TargetGain = FMath::Clamp(L, 0.f, 1.f) * 0.16f;
	Dread->Rate = FMath::Clamp(Tension, 0.f, 1.f);
}

// Тіршілік дауыстары: 0 — сәулеленген адамның ыңырсуы, 1 — шыңғырық, 2 — қасқыр ырылы, 3 — сыбыр
void UKAPAudio::Creature(FVector Loc, int32 Type, float Vol, bool bFar)
{
	// 0 — жазылған ыңырсу; 1 — синтез шыңғырығының астына жоғары ыңырсу қабаты
	if (Type == 0 && MoanAt(Loc, FMath::FRandRange(0.82f, 0.98f), Vol, 2.4f, bFar)) return;
	if (Type == 1) MoanAt(Loc, FMath::FRandRange(1.25f, 1.45f), Vol * 0.7f, 1.3f, bFar);
	FRandomStream Rg(FMath::Rand());
	const float Dur = Type == 0 ? Rg.FRandRange(1.2f, 2.2f) : Type == 1 ? Rg.FRandRange(0.9f, 1.4f) : Type == 2 ? Rg.FRandRange(0.8f, 1.3f) : Rg.FRandRange(1.2f, 1.8f);
	const int32 N = (int32)(Dur * SR);
	TArray<int16> Out; Out.SetNumZeroed(N);
	struct FBq { float b0 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0; float Run(float x) { const float y = b0 * x - b0 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
	auto BP = [](float F, float Q) { const float W0 = 2 * PI * F / SR, Al = FMath::Sin(W0) / (2 * Q), A0 = 1 + Al; FBq B; B.b0 = Al / A0; B.a1 = -2 * FMath::Cos(W0) / A0; B.a2 = (1 - Al) / A0; return B; };
	FBq F1 = BP(Type == 1 ? 900.f : Type == 2 ? 420.f : Type == 3 ? 2400.f : 480.f, Type == 3 ? 4.f : 5.f);
	FBq F2 = BP(Type == 1 ? 2300.f : Type == 2 ? 1100.f : Type == 3 ? 5200.f : 1050.f, 6.f);
	float Ph = 0, Lp = 0; uint32 Ns = Rg.RandHelper(1 << 30);
	for (int32 i = 0; i < N; i++)
	{
		const float T = i / (float)SR, U = T / Dur;
		Ns = Ns * 1664525u + 1013904223u;
		const float Nz = ((Ns >> 9) & 0xFFFF) / 32768.f - 1.f;
		float F0 = 0, Src = 0, Env = FMath::Min(1.f, U * 10.f) * FMath::Min(1.f, (1.f - U) * 4.f);
		switch (Type)
		{
		case 0: F0 = 62.f + 18.f * FMath::Sin(T * 3.1f) + 9.f * Nz; Env *= 0.6f + 0.4f * FMath::Sin(T * 9.f + FMath::Sin(T * 2.3f) * 3.f); break; // бұзылған, дірілді ыңырсу
		case 1: F0 = FMath::Lerp(260.f, 880.f, FMath::Min(1.f, U * 2.5f)) * (1.f + 0.04f * FMath::Sin(T * 40.f)); break;                         // көтерілетін шыңғырық
		case 2: F0 = 0; Env *= 0.5f + 0.5f * FMath::Abs(FMath::Sin(T * 23.f)); break;                                                              // ырыл (дірілді шу)
		default: F0 = 0; Env *= 0.3f + 0.7f * FMath::Max(0.f, FMath::Sin(T * 7.f + FMath::Sin(T * 2.f) * 2.f)); break;                           // сыбыр (буындар)
		}
		if (F0 > 0) { Ph += F0 / SR; if (Ph >= 1) { Ph -= 1; Src = 1.f; } Src = Src * 2.f - 0.02f + Nz * (Type == 1 ? 0.5f : 0.25f); }
		else Src = Nz;
		float S = F1.Run(Src) * 1.4f + F2.Run(Src) * 0.8f;
		if (Type != 3) S = FMath::Clamp(S * 3.f, -1.f, 1.f) * 0.7f; // бұрмалау — «адам емес» реңк
		Lp += (S - Lp) * (Type == 3 ? 0.6f : 0.35f);
		Out[i] = (int16)FMath::Clamp(Lp * Env * 26000.f, -32767.f, 32767.f);
	}
	{ int32 Pk = 1; for (int16 V : Out) Pk = FMath::Max(Pk, FMath::Abs((int32)V)); const float Gn = (Type == 3 ? 0.45f : 0.65f) * 32767.f / Pk; for (int16& V : Out) V = (int16)FMath::Clamp(V * Gn, -32767.f, 32767.f); }
	if (Type == 1) AddEcho(Out, 0.4f, 0.25f, 0.06f);
	if (GetWorld()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), MakeBuffer(Out), Loc, Vol, Rg.FRandRange(0.9f, 1.08f), 0.f, bFar ? AttenFar() : Atten());
}

void UKAPAudio::SetMusic(float Intensity, float Volume)
{
	if (HasBank("Ambient")) { bRealMusic = true; MusI = FMath::Clamp(Intensity, 0.f, 1.f); MusV = FMath::Clamp(Volume, 0.f, 1.f); if (Music) Music->TargetGain = 0.f; return; }
	if (!Music) return;
	Music->Rate = FMath::Clamp(Intensity, 0.f, 1.f);
	const float Duck = 1.f - DuckAmt * 0.5f * FMath::Clamp(DuckT, 0.f, 1.f);
	Music->TargetGain = FMath::Clamp(Volume, 0.f, 1.f) * 0.22f * Duck;
}

// Есік: сықыр — «жабысып-сырғу» импульстері резонатордан өтеді (жиілігі баяу өзгереді), соңында тарс жабылу
void UKAPAudio::Door(FVector Loc, int32 Type)
{
	if (HasBank("DoorOpen") && GetWorld())
	{
		PlayBank("DoorOpen", Loc, 0.9f, FMath::FRandRange(0.92f, 1.05f));
		if (Type == 0) PlayBank("Creak", Loc, 0.45f, FMath::FRandRange(0.8f, 0.95f));
		TWeakObjectPtr<UKAPAudio> Self(this); FTimerHandle H;
		GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self, Loc]() { if (Self.IsValid()) Self->PlayBank("DoorClose", Loc, 0.9f, FMath::FRandRange(0.92f, 1.05f)); }), 0.75f, false);
		return;
	}
	FRandomStream Rg(FMath::Rand());
	const float Dur = Rg.FRandRange(0.6f, 1.1f);
	const int32 N = (int32)(Dur * SR);
	TArray<int16> B; B.SetNumZeroed(N + SR / 2);
	const float Fres = Type == 0 ? Rg.FRandRange(900.f, 1400.f) : Rg.FRandRange(500.f, 750.f);
	float SvL2 = 0, SvB2 = 0, Next = 0;
	for (int32 i = 0; i < N; i++)
	{
		const float U = i / (float)N;
		// жабысу жиілігі: 25→70→35 Гц (тұтқаның қозғалысы)
		const float Rate = 25.f + 45.f * FMath::Sin(U * PI) + 8.f * FMath::Sin(U * 31.f);
		Next -= 1.f / SR;
		float Ex = 0; if (Next <= 0) { Ex = Rg.FRandRange(0.6f, 1.f); Next = 1.f / Rate; }
		const float F = 2.f * FMath::Sin(PI * Fres * (1.f + 0.15f * FMath::Sin(U * 5.f)) / SR);
		SvL2 += F * SvB2; const float Hp = Ex - SvL2 - 0.04f * SvB2; SvB2 += F * Hp;
		const float Env = FMath::Min(1.f, U * 12.f) * FMath::Min(1.f, (1.f - U) * 6.f);
		B[i] = (int16)FMath::Clamp(SvB2 * Env * (Type == 0 ? 9000.f : 7000.f), -32767.f, 32767.f);
	}
	// тарс: төмен соққы + металл болса шыңыл
	TArray<int16> Slam; Noise(Slam, 0.35f, Type == 0 ? 0.9f : 0.7f, Type == 0 ? 0.12f : 0.07f, 0.07f);
	if (Type == 0) Tone(Slam, 0, 0.6f, 180.f, 0.15f, 170.f, 1);
	for (int32 k = 0; k < Slam.Num() && N + k < B.Num(); k++) B[N + k] = (int16)FMath::Clamp((int32)B[N + k] + Slam[k], -32767, 32767);
	if (GetWorld()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), MakeBuffer(B), Loc, 0.9f, Rg.FRandRange(0.95f, 1.05f), 0.f, Atten());
}
// Домбыра: шертілген ішек (Карплус–Стронг) — пентатоникалық минор, екі ішек (әуен + бұрдон), күй ырғағы; сирек, тыныш
float UKAPAudio::Dombra()
{
	FRandomStream Rg(FMath::Rand());
	static const float Scale[7] = { 220.f, 261.63f, 293.66f, 329.63f, 392.f, 440.f, 523.25f };
	const float Beat = 0.21f;
	TArray<int16> Out; Out.SetNumZeroed((int32)(14.f * SR));
	auto Pluck = [&](float Start, float Hz, float Vol, float Dec)
	{
		const int32 N = FMath::Max(2, (int32)(SR / Hz));
		TArray<float> Buf; Buf.SetNumUninitialized(N);
		for (int32 i = 0; i < N; i++) Buf[i] = Rg.FRandRange(-1.f, 1.f);
		const int32 S0 = (int32)(Start * SR), Len = (int32)(Dec * SR);
		int32 Idx = 0; float Prev = 0;
		for (int32 i = 0; i < Len && S0 + i < Out.Num(); i++)
		{
			const float X = Buf[Idx];
			const float Y = 0.5f * (X + Prev) * 0.996f;
			Prev = X; Buf[Idx] = Y; Idx = (Idx + 1) % N;
			const float Body = X * 0.8f + Y * 0.2f;
			Out[S0 + i] = (int16)FMath::Clamp(Out[S0 + i] + Body * Vol * 14000.f, -32767.f, 32767.f);
		}
	};
	// әуен: кішкене мотив + оның қайталануы, соңы төменге түседі
	int32 Note = 4; float T = 0.2f;
	for (int32 Bar = 0; Bar < 6; Bar++)
	{
		const int32 Pat[8] = { 0, 1, 1, 0, 1, 0, 1, 1 }; // күй ырғағы: «тарт-тарт» соққылары
		for (int32 s = 0; s < 8; s++)
		{
			if (Pat[s])
			{
				Note = FMath::Clamp(Note + Rg.RandRange(-2, 2), 0, 6);
				if (Bar == 5 && s > 4) Note = FMath::Max(0, Note - 1);
				Pluck(T, Scale[Note], 0.55f, 1.4f);
			}
			if (s % 2 == 0) Pluck(T + 0.01f, Bar % 2 ? 164.81f : 110.f, 0.3f, 0.9f); // бұрдон ішегі
			T += Beat;
		}
	}
	Pluck(T + 0.2f, 110.f, 0.6f, 3.f); Pluck(T + 0.22f, 220.f, 0.4f, 3.f);
	AddEcho(Out, 0.35f, 0.25f, 0.06f);
	PlayBuffer(Out, 0.42f);
	return T + 3.f;
}
void UKAPAudio::Land(FVector Loc, int32 Surface, float Strength)
{
	const float V = FMath::Clamp(0.35f + Strength * 0.65f, 0.f, 1.f);
	if (Surface == 0 && HasBank("LandSand")) { PlayBank("LandSand", Loc, V, FMath::FRandRange(0.95f, 1.05f)); return; }
	Footstep(Loc, Surface, V);
	if (Strength > 0.5f) Impact(Loc, Surface == 1 ? 1 : 3, 0.4f * Strength);
}

UAudioComponent* UKAPAudio::AttachGenerator(USceneComponent* To, FVector Offset, float Vol)
{
	USoundWave* W = Pick("Generator");
	if (!W || !To) return nullptr;
	// жақыннан қатты, 50 м-де фонға сіңеді; әр генератор цикліне кездейсоқ орыннан басталады (бірдей естілмесін)
	static TStrongObjectPtr<USoundAttenuation> GenAtt;
	if (!GenAtt.IsValid())
	{
		GenAtt.Reset(NewObject<USoundAttenuation>(GetTransientPackage()));
		FSoundAttenuationSettings& A = GenAtt->Attenuation;
		A.bAttenuate = true; A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere; A.AttenuationShapeExtents = FVector(250.f);
		A.FalloffDistance = 5500.f; A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A.bAttenuateWithLPF = true; A.LPFRadiusMin = 800.f; A.LPFRadiusMax = 5000.f; A.LPFFrequencyAtMax = 900.f;
	}
	UAudioComponent* C = UGameplayStatics::SpawnSoundAttached(W, To, NAME_None, Offset, EAttachLocation::KeepRelativeOffset, true, Vol, FMath::FRandRange(0.95f, 1.05f), FMath::FRandRange(0.f, 18.f), GenAtt.Get(), nullptr, false);
	return C;
}

// ---- қару дыбыстары: шытырлақ (дыбыстан жылдам оқ) + газ денесі (төмен гүріл) + механика (затвор) + дала жаңғырығы ----
void UKAPAudio::Shot(FName W)
{
	{
		const FName B = W == "sniper" ? (HasBank("GunRifle") ? FName("GunRifle") : FName("GunSniper")) : W == "shotgun" ? FName("GunShotgun") : W == "ak" ? FName("GunAK") : FName("GunPM");
		if (HasBank(B)) { PlayBank(B, FVector::ZeroVector, W == "flaregun" ? 0.55f : 0.95f, (W == "flaregun" ? 1.3f : 1.f) * FMath::FRandRange(0.96f, 1.04f), true); Duck(0.4f, 0.4f); return; }
	}
	struct FGun { float Crack, CrackCut, Body, BodyHz, BodyDec, Mech, MechHz, Echo, Len; };
	FGun G = { 0.7f, 0.5f, 0.6f, 90.f, 0.09f, 0.25f, 1900.f, 0.3f, 0.6f };                       // ПМ
	if (W == "ak") G = { 0.95f, 0.6f, 0.8f, 75.f, 0.11f, 0.3f, 1500.f, 0.35f, 0.8f };
	else if (W == "sniper") G = { 1.f, 0.7f, 1.f, 55.f, 0.2f, 0.2f, 1100.f, 0.45f, 1.4f };
	else if (W == "shotgun") G = { 0.6f, 0.3f, 1.f, 60.f, 0.18f, 0.2f, 900.f, 0.4f, 1.1f };
	else if (W == "flaregun") G = { 0.25f, 0.2f, 0.6f, 110.f, 0.12f, 0.15f, 1300.f, 0.15f, 0.7f };
	FRandomStream Rg(FMath::Rand());
	const int32 N = (int32)(G.Len * SR);
	TArray<int16> B; B.SetNumZeroed(N);
	float Lp = 0, Hp = 0, Ph = 0;
	for (int32 i = 0; i < N; i++)
	{
		const float T = i / (float)SR;
		const float Nz = Rg.FRandRange(-1.f, 1.f);
		Lp += (Nz - Lp) * G.CrackCut; Hp = Nz - Lp;
		float S = (Lp * 0.6f + Hp * 0.4f) * G.Crack * FMath::Exp(-T / 0.012f);                   // шытырлақ
		const float F = G.BodyHz * (1.f + 1.6f * FMath::Exp(-T / 0.015f));
		Ph += F / SR; S += FMath::Sin(Ph * UE_TWO_PI) * G.Body * FMath::Exp(-T / G.BodyDec);       // газ денесі
		S += Lp * 0.5f * G.Body * FMath::Exp(-T / (G.BodyDec * 1.5f));                             // ұңғы шуы
		if (T > 0.055f && T < 0.09f) S += FMath::Sin(UE_TWO_PI * G.MechHz * T) * G.Mech * FMath::Exp(-(T - 0.055f) / 0.008f); // затвор
		B[i] = (int16)FMath::Clamp(FMath::Clamp(S * 1.25f, -1.f, 1.f) * 32000.f, -32767.f, 32767.f);
	}
	AddEcho(B, 0.28f + Rg.FRand() * 0.1f, G.Echo, 0.05f); AddEcho(B, 0.9f, G.Echo * 0.45f, 0.03f); AddEcho(B, 1.7f, G.Echo * 0.2f, 0.02f);
	PlayBuffer(B, 0.95f);
	Duck(0.4f, 0.4f);
}

// Оқтау кезеңдері: 0 — магазин/патрон шығару, 1 — салу, 2 — затвор (қаруға байланысты)
void UKAPAudio::ReloadSfx(FName W, int32 Stage)
{
	TArray<int16> B;
	auto Click = [&](float At, float Hz, float Vol, float Dec) { Tone(B, At, Dec * 6.f, Hz, Vol, Hz * 0.92f, 2); TArray<int16> Nz; Noise(Nz, Dec * 3.f, Vol * 0.8f, 0.7f, Dec); const int32 O = (int32)(At * SR); B.SetNumZeroed(FMath::Max(B.Num(), O + Nz.Num())); for (int32 i = 0; i < Nz.Num(); i++) B[O + i] = (int16)FMath::Clamp((int32)B[O + i] + Nz[i], -32767, 32767); };
	auto Slide = [&](float At, float Dur, float Vol) { TArray<int16> Nz; Noise(Nz, Dur, Vol, 0.35f, Dur * 0.6f); const int32 O = (int32)(At * SR); B.SetNumZeroed(FMath::Max(B.Num(), O + Nz.Num())); for (int32 i = 0; i < Nz.Num(); i++) B[O + i] = (int16)FMath::Clamp((int32)B[O + i] + (int32)(Nz[i] * FMath::Sin(i / (float)Nz.Num() * PI)), -32767, 32767); };
	if (W == "shotgun")
	{
		if (Stage == 0) { Click(0, 700, 0.35f, 0.01f); Slide(0.02f, 0.12f, 0.2f); }           // ұңғыны сындыру
		else if (Stage == 1) { Slide(0, 0.08f, 0.25f); Click(0.08f, 520, 0.35f, 0.012f); Slide(0.25f, 0.08f, 0.25f); Click(0.33f, 500, 0.35f, 0.012f); }
		else { Click(0, 900, 0.5f, 0.012f); }
	}
	else if (W == "sniper")
	{
		if (Stage == 0) { Click(0, 1200, 0.35f, 0.008f); Slide(0.03f, 0.2f, 0.25f); }
		else if (Stage == 1) { Slide(0, 0.1f, 0.2f); Click(0.1f, 650, 0.45f, 0.015f); }
		else { Click(0, 1400, 0.35f, 0.008f); Slide(0.02f, 0.14f, 0.3f); Click(0.18f, 900, 0.45f, 0.012f); }  // затвор: көтеру, тарту, итеру
	}
	else
	{
		if (Stage == 0) { Click(0, 1500, 0.4f, 0.006f); Slide(0.02f, 0.1f, 0.22f); }         // магазин ұстағышы + шығару
		else if (Stage == 1) { Slide(0, 0.06f, 0.2f); Click(0.07f, 560, 0.55f, 0.014f); Click(0.085f, 1300, 0.3f, 0.006f); }
		else { Slide(0, 0.09f, 0.3f); Click(0.09f, 1100, 0.5f, 0.01f); }                     // затворды тарту-жіберу
	}
	PlayBuffer(B, 0.7f);
}

void UKAPAudio::Whoosh(bool bHeavy)
{
	// ауаны кесу: жолақ сүзгісі жиілігі жоғарылап-төмендейді
	const float Dur = bHeavy ? 0.32f : 0.2f;
	const int32 N = (int32)(Dur * SR);
	TArray<int16> B; B.SetNumZeroed(N);
	float L = 0, Bd = 0; FRandomStream Rg(FMath::Rand());
	for (int32 i = 0; i < N; i++)
	{
		const float U = i / (float)N;
		const float Fc = FMath::Lerp(300.f, bHeavy ? 1400.f : 2600.f, FMath::Sin(U * PI));
		const float F = 2.f * FMath::Sin(PI * Fc / SR);
		L += F * Bd; Bd += F * (Rg.FRandRange(-1.f, 1.f) - L - 0.4f * Bd);
		B[i] = (int16)FMath::Clamp(Bd * FMath::Sin(U * PI) * 9000.f, -32767.f, 32767.f);
	}
	PlayBuffer(B, 0.55f);
}

// Соққы: 0 — дене, 1 — металл (шыңыл), 2 — ағаш/жәшік, 3 — құм (оқ тигенде), 4 — рация сыртылы
void UKAPAudio::Impact(FVector Loc, int32 Type, float Vol)
{
	{
		static const FName IB[4] = { FName("ImpBody"), FName("ImpMetal"), FName("ImpWood"), FName("ImpSand") };
		if (Type >= 0 && Type < 4 && HasBank(IB[Type])) { PlayBank(IB[Type], Loc, Vol, FMath::FRandRange(0.9f, 1.1f)); return; }
	}
	TArray<int16> B;
	switch (Type)
	{
	case 0: Noise(B, 0.12f, 0.7f, 0.08f, 0.04f); Tone(B, 0, 0.12f, 90.f, 0.4f, 60.f, 0); break;
	case 1: Noise(B, 0.05f, 0.5f, 0.8f, 0.01f); Tone(B, 0, 0.5f, 1240.f + FMath::FRand() * 600.f, 0.18f, 0, 1); Tone(B, 0, 0.4f, 2870.f, 0.08f, 0, 0); if (FMath::FRand() < 0.4f) Tone(B, 0.02f, 0.35f, 3200.f, 0.12f, 1400.f, 0); break;
	case 2: Noise(B, 0.09f, 0.6f, 0.2f, 0.03f); Tone(B, 0, 0.15f, 180.f, 0.3f, 120.f, 1); break;
	case 3: Noise(B, 0.14f, 0.45f, 0.15f, 0.05f); break;
	default: Tone(B, 0, 0.05f, 2200.f, 0.15f, 0, 2); { TArray<int16> Nz; Noise(Nz, 0.25f, 0.12f, 0.9f, 0.2f); B.Append(Nz); } break;
	}
	if (GetWorld()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), MakeBuffer(B), Loc, Vol, FMath::FRandRange(0.92f, 1.08f), 0.f, Atten());
}

// Ұсақ UI/жабдық дыбыстары: 0 — фонарь сыртылы, 1 — қаруды ауыстыру (мата + металл), 2 — бос шүріппе
void UKAPAudio::Foley(int32 Type)
{
	TArray<int16> B;
	if (Type == 0) { Tone(B, 0, 0.025f, 2600.f, 0.25f, 0, 2); Tone(B, 0.06f, 0.025f, 2100.f, 0.2f, 0, 2); }
	else if (Type == 1) { Noise(B, 0.25f, 0.18f, 0.25f, 0.12f); Tone(B, 0.12f, 0.12f, 900.f, 0.12f, 850.f, 2); }
	else { Tone(B, 0, 0.04f, 1800.f, 0.3f, 1500.f, 2); }
	PlayBuffer(B, 0.6f);
}
// ---- Тексеру және тазалау ----
void UKAPAudio::WriteWav(const FString& Name, const TArray<int16>& Pcm, bool bReport)
{
	// статистика: шың, RMS (dBFS), кесілген сэмплдер, DC, басы/соңындағы шертпе
	if (bReport && Pcm.Num())
	{
		int32 Peak = 0, Clip = 0; double Sum = 0, Sq = 0;
		for (int16 S : Pcm) { const int32 A = FMath::Abs((int32)S); Peak = FMath::Max(Peak, A); if (A >= 32700) Clip++; Sum += S; Sq += (double)S * S; }
		const double Rms = FMath::Sqrt(Sq / Pcm.Num()) / 32767.0, Dc = Sum / Pcm.Num() / 32767.0;
		const float Head = FMath::Abs(Pcm[0]) / 32767.f, Tail = FMath::Abs(Pcm.Last()) / 32767.f;
		UE_LOG(LogTemp, Display, TEXT("KAPSND %-22s dur=%.2fs peak=%.2f rms=%.1fdB clip=%.2f%% dc=%.3f head=%.2f tail=%.2f"), *Name, Pcm.Num() / (float)SR, Peak / 32767.f, 20.0 * FMath::LogX(10.0, FMath::Max(Rms, 1e-6)), 100.0 * Clip / Pcm.Num(), Dc, Head, Tail);
	}
	TArray<uint8> F;
	auto U32 = [&](uint32 V) { F.Append(reinterpret_cast<uint8*>(&V), 4); };
	auto U16 = [&](uint16 V) { F.Append(reinterpret_cast<uint8*>(&V), 2); };
	F.Append((const uint8*)"RIFF", 4); U32(36 + Pcm.Num() * 2); F.Append((const uint8*)"WAVEfmt ", 8);
	U32(16); U16(1); U16(1); U32(SR); U32(SR * 2); U16(2); U16(16);
	F.Append((const uint8*)"data", 4); U32(Pcm.Num() * 2);
	F.Append(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * 2);
	FFileHelper::SaveArrayToFile(F, *(FPaths::ProjectSavedDir() / TEXT("SoundDump") / (Name + TEXT(".wav"))));
}

void UKAPAudio::Finalize(TArray<int16>& Pcm)
{
	const int32 N = Pcm.Num();
	if (N < 8) return;
	if (bCapture) WriteWav(CaptureName + TEXT("_raw"), Pcm, true);
	TArray<float> X; X.SetNumUninitialized(N);
	// DC жою (бір полюсті жоғары өткізгіш ~15 Гц)
	float Px = 0, Py = 0; const float R = 1.f - UE_TWO_PI * 15.f / SR;
	for (int32 i = 0; i < N; i++) { const float In = Pcm[i] / 32767.f; Py = In - Px + R * Py; Px = In; X[i] = Py; }
	// басы 2 мс, соңы 12 мс жұмсақ (шертпе болмасын)
	const int32 Fi = FMath::Min(N / 4, (int32)(0.002f * SR)), Fo = FMath::Min(N / 3, (int32)(0.012f * SR));
	for (int32 i = 0; i < Fi; i++) X[i] *= i / (float)Fi;
	for (int32 i = 0; i < Fo; i++) X[N - 1 - i] *= FMath::Square(i / (float)Fo);
	// жұмсақ шектегіш: 0.7-ден жоғарысы tanh арқылы басылады (қатаң кесу жоқ)
	for (int32 i = 0; i < N; i++)
	{
		float S = X[i];
		const float A = FMath::Abs(S);
		if (A > 0.7f) S = FMath::Sign(S) * (0.7f + 0.27f * FMath::Tanh((A - 0.7f) / 0.27f));
		Pcm[i] = (int16)FMath::Clamp(S * 32767.f, -32767.f, 32767.f);
	}
	if (bCapture) WriteWav(CaptureName, Pcm, true);
}

void UKAPAudio::DumpAll()
{
	bCapture = true;
	const FVector L = FVector::ZeroVector;
	struct FJob { const TCHAR* N; TFunction<void()> F; };
	TArray<FJob> Jobs = {
		{ TEXT("shot_pm"), [&] { Shot("pm"); } }, { TEXT("shot_ak"), [&] { Shot("ak"); } }, { TEXT("shot_sniper"), [&] { Shot("sniper"); } }, { TEXT("shot_shotgun"), [&] { Shot("shotgun"); } }, { TEXT("shot_flaregun"), [&] { Shot("flaregun"); } },
		{ TEXT("reload_ak_0"), [&] { ReloadSfx("ak", 0); } }, { TEXT("reload_ak_1"), [&] { ReloadSfx("ak", 1); } }, { TEXT("reload_ak_2"), [&] { ReloadSfx("ak", 2); } },
		{ TEXT("reload_sg_1"), [&] { ReloadSfx("shotgun", 1); } }, { TEXT("reload_sv_2"), [&] { ReloadSfx("sniper", 2); } },
		{ TEXT("whoosh"), [&] { Whoosh(false); } }, { TEXT("whoosh_heavy"), [&] { Whoosh(true); } },
		{ TEXT("impact_body"), [&] { Impact(L, 0); } }, { TEXT("impact_metal"), [&] { Impact(L, 1); } }, { TEXT("impact_wood"), [&] { Impact(L, 2); } }, { TEXT("impact_sand"), [&] { Impact(L, 3); } }, { TEXT("impact_radio"), [&] { Impact(L, 4); } },
		{ TEXT("foley_flash"), [&] { Foley(0); } }, { TEXT("foley_switch"), [&] { Foley(1); } }, { TEXT("foley_dry"), [&] { Foley(2); } },
		{ TEXT("step_sand"), [&] { Footstep(L, 0); } }, { TEXT("step_metal"), [&] { Footstep(L, 1); } }, { TEXT("step_wood"), [&] { Footstep(L, 2); } }, { TEXT("step_concrete"), [&] { Footstep(L, 3); } }, { TEXT("step_gravel"), [&] { Footstep(L, 4); } },
		{ TEXT("door_metal"), [&] { Door(L, 0); } }, { TEXT("door_wood"), [&] { Door(L, 1); } },
		{ TEXT("creature_groan"), [&] { Creature(L, 0); } }, { TEXT("creature_scream"), [&] { Creature(L, 1); } }, { TEXT("creature_growl"), [&] { Creature(L, 2); } }, { TEXT("creature_whisper"), [&] { Creature(L, 3); } },
		{ TEXT("voice_m"), [&] { Voice(L, 2.f, false); } }, { TEXT("voice_f"), [&] { Voice(L, 2.f, true); } }, { TEXT("radio_voice"), [&] { RadioVoice(2.f); } },
		{ TEXT("stinger_danger"), [&] { Stinger(true); } }, { TEXT("stinger_relief"), [&] { Stinger(false); } },
		{ TEXT("dombra"), [&] { Dombra(); } }, { TEXT("lament"), [&] { Lament(); } },
	};
	for (int32 i = 0; i <= (int32)EKAPSound::Shout; i++) Jobs.Add({ nullptr, [this, i] { CaptureName = FString::Printf(TEXT("play_%d"), i); Play((EKAPSound)i); } });
	for (FJob& J : Jobs) { if (J.N) CaptureName = J.N; J.F(); }
	// циклдер: генераторды тікелей 4 с айналдырамыз
	auto Loop = [&](const TCHAR* Name, TFunction<void(UKAPSynthWave*)> Setup)
	{
		UKAPSynthWave* W = NewObject<UKAPSynthWave>(this);
		Setup(W); W->Gain = W->TargetGain;
		TArray<uint8> B; W->OnGeneratePCMAudio(B, 4 * SR);
		TArray<int16> P; P.SetNumUninitialized(4 * SR); FMemory::Memcpy(P.GetData(), B.GetData(), P.Num() * 2);
		WriteWav(FString(TEXT("loop_")) + Name, P, true);
	};
	Loop(TEXT("wind_calm"), [](UKAPSynthWave* W) { W->Cutoff = 0.012f; W->TargetGain = 0.12f; });
	Loop(TEXT("wind_storm"), [](UKAPSynthWave* W) { W->Cutoff = 0.043f; W->TargetGain = 0.5f; W->Gust = 1.6f; });
	Loop(TEXT("wind_whistle"), [](UKAPSynthWave* W) { W->Mode = 4; W->ResHz = 900.f; W->TargetGain = 0.1f; });
	Loop(TEXT("rain"), [](UKAPSynthWave* W) { W->Cutoff = 0.3f; W->HighPass = 0.8f; W->TargetGain = 0.25f; });
	Loop(TEXT("fire"), [](UKAPSynthWave* W) { W->Cutoff = 0.08f; W->Crackle = 0.8f; W->TargetGain = 0.3f; });
	Loop(TEXT("radio"), [](UKAPSynthWave* W) { W->Cutoff = 0.6f; W->HighPass = 0.9f; W->TargetGain = 0.12f; });
	Loop(TEXT("engine_idle"), [](UKAPSynthWave* W) { W->ToneHz = 38.f; W->Cutoff = 0.05f; W->TargetGain = 0.1f; });
	Loop(TEXT("engine_high"), [](UKAPSynthWave* W) { W->ToneHz = 110.f; W->Cutoff = 0.11f; W->TargetGain = 0.18f; });
	Loop(TEXT("engine_misfire"), [](UKAPSynthWave* W) { W->ToneHz = 60.f; W->Cutoff = 0.07f; W->Misfire = 0.2f; W->TargetGain = 0.14f; });
	Loop(TEXT("crickets"), [](UKAPSynthWave* W) { W->Cutoff = 0.5f; W->Chirp = 1.f; W->TargetGain = 0.06f; });
	Loop(TEXT("geiger_low"), [](UKAPSynthWave* W) { W->Mode = 1; W->Rate = 2.f; W->TargetGain = 0.35f; });
	Loop(TEXT("geiger_high"), [](UKAPSynthWave* W) { W->Mode = 1; W->Rate = 60.f; W->TargetGain = 0.35f; });
	Loop(TEXT("heart"), [](UKAPSynthWave* W) { W->Mode = 2; W->Rate = 120.f; W->TargetGain = 0.4f; });
	Loop(TEXT("breath"), [](UKAPSynthWave* W) { W->Mode = 3; W->Rate = 30.f; W->TargetGain = 0.12f; });
	Loop(TEXT("siren"), [](UKAPSynthWave* W) { W->Mode = 5; W->TargetGain = 0.22f; });
	Loop(TEXT("dread"), [](UKAPSynthWave* W) { W->Mode = 6; W->Rate = 0.5f; W->TargetGain = 0.16f; });
	Loop(TEXT("music_calm"), [](UKAPSynthWave* W) { W->Mode = 7; W->Rate = 0.f; W->TargetGain = 0.22f; });
	Loop(TEXT("music_danger"), [](UKAPSynthWave* W) { W->Mode = 7; W->Rate = 1.f; W->Int7Init(1.f); W->TargetGain = 0.22f; });
	bCapture = false;
	UE_LOG(LogTemp, Display, TEXT("KAPSND done -> %s"), *(FPaths::ProjectSavedDir() / TEXT("SoundDump")));
}