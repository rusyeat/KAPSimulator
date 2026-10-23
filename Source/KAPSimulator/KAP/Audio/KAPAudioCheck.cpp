// -kapaudiocheck: дайын (пакетталған) ойында дыбыстың шынымен жеткенін тексеру — AUDIO_DEPLOY_HANDOFF кестелері бойынша.
//  • 47 банк: файл саны, loop белгісі, ұзақтығы > 0 («KAPSND bank» жолы да шығады)
//  • 203 VO asset: дайын 169-ы бар ма, ұзақтығы WAV-пен бірдей ме; дайын емес 34-тің қайсысында TTS баламасы бар
//  • vo_lines.tsv-тегі реплика мәтіндері ойынның өз хэші арқылы (LoadVo → «KAPVO ... ok/miss»)
//  • ойнату: бірнеше VO мен банк дыбысын шынымен қосып, 0.4 с кейін ойнап тұр ма — тексеру
// Деректер: KAPAudioCheckData.inl (Scripts/gen_audio_check.ps1 жасайды)
#include "KAPAudio.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "KAPAudioCheckData.inl"

void UKAPAudio::AudioCheck()
{
	int32 BankOk = 0, BankBad = 0;
	for (const FKAPBankCheck& C : GKAPBankChecks)
	{
		Pick(FName(C.Bank));
		const FKAPSoundBank* B = Banks.Find(FName(C.Bank));
		const int32 N = B ? B->W.Num() : 0;
		int32 LoopWrong = 0; float MinDur = 1e9f;
		if (B) for (const TObjectPtr<USoundWave>& W : B->W) if (W) { if ((bool)W->bLooping != C.bLoop) LoopWrong++; MinDur = FMath::Min(MinDur, W->Duration); }
		const bool bOk = N == C.Count && LoopWrong == 0 && N > 0 && MinDur > 0.01f;
		(bOk ? BankOk : BankBad)++;
		UE_LOG(LogTemp, Display, TEXT("KAPAUDIOCHECK bank %s %d/%d loop_wrong=%d min_dur=%.2f %s"), C.Bank, N, C.Count, LoopWrong, N ? MinDur : 0.f, bOk ? TEXT("ok") : TEXT("FAIL"));
	}
	int32 VoOk = 0, VoMissing = 0, DurBad = 0, Tts = 0, Silent = 0;
	for (const FKAPVoCheck& C : GKAPVoChecks)
	{
		USoundWave* W = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/KAP/Audio/VO/%s.%s"), C.Asset, C.Asset), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (C.bReady)
		{
			if (!W) { VoMissing++; UE_LOG(LogTemp, Warning, TEXT("KAPAUDIOCHECK vo %s MISSING (ElevenLabs ready)"), C.Asset); continue; }
			VoOk++;
			if (C.Dur > 0.f && FMath::Abs(W->Duration - C.Dur) > 0.15f) { DurBad++; UE_LOG(LogTemp, Warning, TEXT("KAPAUDIOCHECK vo %s duration %.2f vs wav %.2f"), C.Asset, W->Duration, C.Dur); }
		}
		else (W ? Tts : Silent)++;
	}
	int32 LineOk = 0, LineMiss = 0;
	for (const FKAPVoLine& L : GKAPVoLines) (LoadVo(L.Text, L.bFemale) ? LineOk : LineMiss)++;
	UE_LOG(LogTemp, Display, TEXT("KAPAUDIOCHECK summary banks ok=%d fail=%d | vo ready ok=%d missing=%d duration_bad=%d | not ready: tts_fallback=%d silent=%d | script lines ok=%d miss=%d"),
		BankOk, BankBad, VoOk, VoMissing, DurBad, Tts, Silent, LineOk, LineMiss);
	// шынайы ойнату: 3 VO (2D) + атыс, айқай, мотор, қадам, музыка
	UWorld* W = GetWorld();
	if (!W) return;
	TArray<TWeakObjectPtr<UAudioComponent>> Cs;
	int32 Spawned = 0;
	for (int32 i = 0, n = 0; i < (int32)UE_ARRAY_COUNT(GKAPVoChecks) && n < 3; i++)
		if (GKAPVoChecks[i].bReady)
			if (USoundWave* S = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/KAP/Audio/VO/%s.%s"), GKAPVoChecks[i].Asset, GKAPVoChecks[i].Asset), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{ Cs.Add(UGameplayStatics::SpawnSound2D(W, S, 0.25f)); Spawned++; n++; }
	for (const TCHAR* Bk : { TEXT("GunAK"), TEXT("Yell"), TEXT("Engine"), TEXT("StepSand"), TEXT("Ambient"), TEXT("WolfHowl") })
		if (USoundWave* S = Pick(FName(Bk))) { Cs.Add(UGameplayStatics::SpawnSound2D(W, S, 0.2f)); Spawned++; }
	const bool bSpoke = GKAPVoLines[0].Text ? Speak(GKAPVoLines[0].Text, FVector::ZeroVector, GKAPVoLines[0].bFemale, 1, 0.25f) : false;
	FTimerHandle H;
	W->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Cs, Spawned, bSpoke]()
	{
		int32 Playing = 0;
		for (const TWeakObjectPtr<UAudioComponent>& C : Cs) if (C.IsValid() && C->IsPlaying()) Playing++;
		UE_LOG(LogTemp, Display, TEXT("KAPAUDIOCHECK playback playing=%d/%d speak_first_line=%s"), Playing, Spawned, bSpoke ? TEXT("ok") : TEXT("miss"));
	}), 0.4f, false);
}
