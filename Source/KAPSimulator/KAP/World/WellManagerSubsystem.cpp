#include "WellManagerSubsystem.h"
#include "WellActor.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Audio/KAPAudio.h"
#include "Engine/World.h"

void UWellManagerSubsystem::InitFromData()
{
	Wells.Reset();
	Rng.Initialize(FPlatformTime::Cycles());
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!D || !D->Wells) return;
	TArray<FKAPWellRow*> Rows;
	D->Wells->GetAllRows(TEXT("KAP"), Rows);
	Rows.Sort([](const FKAPWellRow& A, const FKAPWellRow& B) { return A.Id < B.Id; });
	for (FKAPWellRow* R : Rows) { FKAPWellRuntime W; W.Row = *R; W.Pressure = R->BasePressure; Wells.Add(W); }
}

void UWellManagerSubsystem::NewGameState(FKAPSession& S)
{
	S.Wells.Reset();
	for (int32 i = 0; i < Wells.Num(); i++) { FKAPWellSave W; W.Stress = Rng.FRand() * 0.3f; S.Wells.Add(W); }
	// Дауылдан қалған ақаулар: №1 (туториал), №3 (Ерланның ұңғымасы), №7, №13 — алғашқы жұмыс көлемі белгілі, кездейсоқ ақау сирек
	auto Pre = [&](int32 Id, EFaultType F, float T) { FKAPWellSave& W = S.Wells[Id - 1]; W.State = EWellState::Fault; W.Fault = F; W.Timer = T; };
	Pre(1, EFaultType::Pump, 40); Pre(3, EFaultType::Pipe, 30); Pre(7, EFaultType::Electric, 22); Pre(13, EFaultType::Acid, 26);
	CreateWorkers(S);
}

void UWellManagerSubsystem::CreateWorkers(FKAPSession& S)
{
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	FRandomStream R(2026);
	const TArray<FString> M = D->NamesOf("m"), F = D->NamesOf("f"), Sur = D->NamesOf("s"), Fam = D->NamesOf("fam");
	S.Workers.Reset();
	int32 Id = 0;
	for (const FKAPWellRuntime& W : Wells)
	{
		for (int32 i = 0; i < W.Row.Workers; i++)
		{
			FKAPWorker K;
			K.Id = Id++; K.WellId = W.Row.Id;
			K.bFemale = R.FRand() < 0.15f;
			FString Surname = Sur.Num() ? Sur[R.RandRange(0, Sur.Num() - 1)] : TEXT("Омаров");
			if (K.bFemale && (Surname.EndsWith(TEXT("ов")) || Surname.EndsWith(TEXT("ев")))) Surname += TEXT("а");
			const TArray<FString>& Pool = K.bFemale ? F : M;
			K.Name = (Pool.Num() ? Pool[R.RandRange(0, Pool.Num() - 1)] : TEXT("Асхат")) + TEXT(" ") + Surname;
			K.Age = 22 + R.RandRange(0, 37);
			K.Family = Fam.Num() ? Fam[R.RandRange(0, Fam.Num() - 1)] : TEXT("");
			K.Dose = R.FRand() * 20.f;
			K.bProtected = R.FRand() < 0.4f;
			S.Workers.Add(K);
		}
	}
	// Трагедиялық кейіпкер — жас жігіт, №3 ұңғыма
	for (FKAPWorker& K : S.Workers)
		if (K.WellId == 3)
		{
			K.Name = TEXT("Ерлан Сейітов"); K.Age = 19; K.bFemale = false; K.bSpecial = true; K.bProtected = false;
			K.Family = TEXT("Ата-анасының жалғыз баласы. Анасы Ақсай ауылында тұрады. Студент, жазғы практикада.");
			break;
		}
}

float UWellManagerSubsystem::NeighborBad(const FKAPSession& S, int32 Index) const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const float Rad = D ? D->Cfg("neighborRadius", 420) : 420;
	float V = 0.02f;
	for (int32 i = 0; i < Wells.Num(); i++)
	{
		if (i == Index) continue;
		if (FVector2D::Distance(Wells[i].PosM(), Wells[Index].PosM()) > Rad) continue;
		switch (S.Wells[i].State)
		{
		case EWellState::Fire: V += 0.5f; break;
		case EWellState::Lost: V += 0.25f; break;
		case EWellState::Leak: V += 0.25f; break;
		case EWellState::Fault: V += 0.08f; break;
		default: break;
		}
	}
	return FMath::Clamp(V, 0.f, 0.97f);
}

KAPAI::FWellEvidence UWellManagerSubsystem::Evidence(const FKAPSession& S, int32 Index, float EnvTemp) const
{
	KAPAI::FWellEvidence E;
	const FKAPWellRuntime& W = Wells[Index];
	E.Age = W.Row.Age; E.Pressure = W.Pressure; E.BasePressure = W.Row.BasePressure; E.EnvTemp = EnvTemp;
	E.NeighborBad = NeighborBad(S, Index); E.bTelemetry = W.bTelemetry;
	return E;
}

void UWellManagerSubsystem::UpdatePredictions(FKAPSession& S, float EnvTemp)
{
	for (int32 i = 0; i < Wells.Num(); i++)
	{
		FKAPWellRuntime& W = Wells[i];
		if (S.Wells[i].State != EWellState::Ok) { W.FailProb = S.Wells[i].State == EWellState::Lost ? 0.f : 1.f; continue; }
		W.Pred = KAPAI::WellFailureProb(Evidence(S, i, EnvTemp));
		W.FailProb = (float)W.Pred.P;
	}
}

double UWellManagerSubsystem::Hazard(int32 Index) const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	return (D ? D->Cfg("hazardScale", 0.06f) : 0.06) * Wells[Index].FailProb;
}

bool UWellManagerSubsystem::CanRandomFault(const FKAPSession& S, int32 Index) const
{
	// Жаңа ғана жөнделген ұңғыма біраз уақыт тыныш тұрады
	if (const double* Until = S.Flags.Find(FName(*FString::Printf(TEXT("wg%d"), Index + 1)))) if (S.Time < *Until) return false;
	// Бір мезгілде ашық тұрған ақау шектеулі: ойыншы бірінен соң бірін жөндеп үлгереді
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const int32 Cap = (int32)(S.bBrigadesUnlocked ? (D ? D->Cfg("maxOpenFaults", 3) : 3) : (D ? D->Cfg("maxOpenFaultsEarly", 2) : 2));
	int32 Open = 0;
	for (const FKAPWellSave& W : S.Wells) if (W.State == EWellState::Fault || W.State == EWellState::Leak) Open++;
	return Open < Cap;
}

double UWellManagerSubsystem::TimeToFailure(const FKAPSession& S, int32 Index) const
{
	const FKAPWellSave& W = S.Wells[Index];
	if (W.State == EWellState::Ok) return 1.0 / FMath::Max(1e-4, Hazard(Index));
	if (W.State == EWellState::Fault || W.State == EWellState::Leak) return W.Timer;
	if (W.State == EWellState::Fire) return 0;
	return TNumericLimits<double>::Max();
}

void UWellManagerSubsystem::StartFault(FKAPSession& S, int32 Index, EFaultType Type, const FString& Reason)
{
	FKAPWellSave& W = S.Wells[Index];
	if (W.State != EWellState::Ok) return;
	if (Type == EFaultType::None)
	{
		const bool bAcid = Wells[Index].Row.AcidTank;
		const float Wt[5] = { 1.f, 1.f, 1.f, bAcid ? 2.f : 0.3f, 0.6f };
		float Sum = 0; for (float X : Wt) Sum += X;
		float Rr = Rng.FRand() * Sum;
		Type = EFaultType::Pump;
		for (int32 k = 0; k < 5; k++) { Rr -= Wt[k]; if (Rr <= 0) { Type = (EFaultType)(k + 1); break; } }
	}
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	W.State = EWellState::Fault; W.Fault = Type; W.Timer = D->Cfg("stage1Hours", 6) * (0.8f + Rng.FRand() * 0.4f);
	RefreshVisual(S, Index);
	if (AKAPGameMode* GM = AKAPGameMode::Get(this))
	{
		// дауыс: «бесінші» + «ұңғымада сорғы тоқтады. Бұзылу кезеңі басталды.» (себебі тек субтитрде)
		const TCHAR* FaultVo = Type == EFaultType::Pipe ? TEXT("ұңғымада құбыр жарылды. Бұзылу кезеңі басталды.")
			: Type == EFaultType::Electric ? TEXT("ұңғымада электр ақауы. Бұзылу кезеңі басталды.")
			: Type == EFaultType::Acid ? TEXT("ұңғымада қышқыл ағып жатыр. Бұзылу кезеңі басталды.")
			: Type == EFaultType::Fire ? TEXT("ұңғымада өрт шықты. Бұзылу кезеңі басталды.")
			: TEXT("ұңғымада сорғы тоқтады. Бұзылу кезеңі басталды.");
		GM->Radio(FString::Printf(TEXT("№%d ұңғыма: %s%s. Бұзылу кезеңі басталды."), Index + 1, *KAP::FaultName(Type).ToString().ToLower(),
			Reason.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Reason)), "warn", { UKAPAudio::OrdinalKk(Index + 1), FaultVo });
	}
}

void UWellManagerSubsystem::RepairWell(FKAPSession& S, int32 Index)
{
	FKAPWellSave& W = S.Wells[Index];
	W.State = EWellState::Ok; W.Fault = EFaultType::None; W.Timer = 0; W.Stress = 0; W.Repaired++;
	Wells[Index].Pressure = Wells[Index].Row.BasePressure;
	if (const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this)) S.Flags.Add(FName(*FString::Printf(TEXT("wg%d"), Index + 1)), S.Time + D->Cfg("faultGraceHours", 10));
	S.WellsRepaired++;
	RefreshVisual(S, Index);
}

void UWellManagerSubsystem::RefreshVisual(const FKAPSession& S, int32 Index)
{
	if (AWellActor* A = Wells[Index].Actor.Get()) A->SetVisualState(S.Wells[Index].State, S.Wells[Index].Fault);
}

int32 UWellManagerSubsystem::HealthState(float Dose, const UKAPDataSubsystem* D)
{
	if (Dose >= D->Cfg("doseDying", 450)) return 3;
	if (Dose >= D->Cfg("doseSick", 250)) return 2;
	if (Dose >= D->Cfg("dosePoisoned", 100)) return 1;
	return 0;
}

int32 UWellManagerSubsystem::Exposure(const FKAPSession& S, const FKAPWorker& K) const
{
	if (!K.EvacTo.IsEmpty()) return 0;
	const EWellState St = S.Wells[K.WellId - 1].State;
	return St == EWellState::Leak ? 1 : St == EWellState::Fire ? 2 : 0;
}

double UWellManagerSubsystem::DoseRate(const FKAPSession& S, const FKAPWorker& K) const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const int32 Ex = Exposure(S, K);
	if (!Ex) return 0.02;
	double R = D->Cfg("leakDoseRate", 7) * (Ex == 2 ? 3.0 : 1.0);
	if (K.bProtected) R *= 0.35;
	if (S.Time < K.IodineUntil) R *= 0.3;
	return R;
}

double UWellManagerSubsystem::TimeToDeath(const FKAPSession& S, const FKAPWorker& K) const
{
	if (K.State >= 4) return TNumericLimits<double>::Max();
	const double R = DoseRate(S, K);
	if (R <= 0.05) return TNumericLimits<double>::Max();
	return (UKAPDataSubsystem::Get(this)->Cfg("doseDeath", 650) - K.Dose) / R;
}

int32 UWellManagerSubsystem::DeadCount(const FKAPSession& S) const
{
	int32 N = 0; for (const FKAPWorker& K : S.Workers) if (K.State == 4) N++;
	return N;
}
int32 UWellManagerSubsystem::OkCount(const FKAPSession& S) const
{
	int32 N = 0; for (const FKAPWellSave& W : S.Wells) if (W.State == EWellState::Ok) N++;
	return N;
}

void UWellManagerSubsystem::KillWorker(FKAPSession& S, FKAPWorker& K)
{
	if (K.State >= 4) return;
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	K.State = 4;
	S.Mutation = FMath::Clamp(S.Mutation + D->Cfg("mutationPerDeath", 1.2f), 0.f, 100.f);
	K.bMutated = Rng.FRand() < D->Cfg("mutateChance", 0.3f);
	if (K.bMutated) S.PendingMutants.Add(K.WellId);
	if (AKAPGameMode* GM = AKAPGameMode::Get(this))
	{
		// дауыс: аты айтылмайды (жұмысшы көп), «бесінші» + «ұңғымадағы жұмысшы ... қайтыс болды»
		GM->Radio(FString::Printf(TEXT("%s (№%d) %s"), *K.Name, K.WellId, K.bMutated ? TEXT("қайтыс болды... денесі мутацияға ұшырады.") : TEXT("сәулеленуден қайтыс болды.")), "danger",
			{ UKAPAudio::OrdinalKk(K.WellId), K.bMutated ? FString(TEXT("ұңғымадағы жұмысшы қайтыс болды... денесі мутацияға ұшырады.")) : FString(TEXT("ұңғымадағы жұмысшы сәулеленуден қайтыс болды.")) });
		GM->CheckEndings();
	}
}

void UWellManagerSubsystem::Simulate(FKAPSession& S, double Dh, float EnvTemp, float Storm, float Rain)
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!D || S.Wells.Num() != Wells.Num()) return;
	// ---- ұңғымалар ----
	for (int32 i = 0; i < Wells.Num(); i++)
	{
		FKAPWellRuntime& R = Wells[i];
		FKAPWellSave& W = S.Wells[i];
		R.bTelemetry = Storm < 0.5f;
		switch (W.State)
		{
		case EWellState::Ok:
		{
			const float Heat = FMath::Clamp((EnvTemp - 30.f) / 15.f, 0.f, 1.f);
			W.Stress = FMath::Clamp(W.Stress + (float)Dh * (0.04f * Heat + 0.06f * NeighborBad(S, i) - 0.03f) + (Rng.FRand() - 0.5f) * 0.05f * (float)Dh, 0.f, 1.2f);
			R.Pressure = R.Row.BasePressure + W.Stress * 0.9f + FMath::Sin((float)S.Time * 1.3f + i) * 0.05f;
			R.Flow = 9.f + (i % 5) - W.Stress * 2.f;
			if (S.bSimActive && Rng.FRand() < Hazard(i) * Dh && CanRandomFault(S, i))
				StartFault(S, i, Rain > 0.3f && Rng.FRand() < 0.5f ? EFaultType::Electric : EFaultType::None, TEXT(""));
			else if (S.bSimActive && R.Row.Region == 3 && !S.Flags.Contains("banditCleared") && Rng.FRand() < 0.004 * Dh && CanRandomFault(S, i))
				StartFault(S, i, EFaultType::Pipe, TEXT("Сапардың адамдары бөлшек ұрлады"));
			break;
		}
		case EWellState::Fault:
		{
			const bool bNoFlow = W.Fault == EFaultType::Pump || W.Fault == EFaultType::Electric;
			R.Pressure = bNoFlow ? R.Row.BasePressure * 0.4f : R.Row.BasePressure * (0.7f + Rng.FRand() * 0.4f);
			R.Flow = bNoFlow ? 0.f : 4.f;
			W.Timer -= (float)Dh;
			if (W.Timer <= 0)
			{
				W.State = EWellState::Leak; W.Timer = D->Cfg("stage2Hours", 5) * (0.8f + Rng.FRand() * 0.4f);
				RefreshVisual(S, i);
				if (GM) GM->Radio(FString::Printf(TEXT("№%d ұңғымада ерітінді АҒЫП ЖАТЫР! Жұмысшылар сәулеленуде."), i + 1), "danger", { UKAPAudio::OrdinalKk(i + 1), TEXT("ұңғымада ерітінді ағып жатыр! Жұмысшылар сәулеленуде.") });
			}
			break;
		}
		case EWellState::Leak:
			R.Pressure = R.Row.BasePressure * (1.3f + Rng.FRand() * 0.5f); R.Flow = 2.f;
			W.Timer -= (float)Dh;
			if (W.Timer <= 0)
			{
				W.State = EWellState::Fire; W.BurnTimer = D->Cfg("fireHours", 2); S.Burned++;
				S.Mutation = FMath::Clamp(S.Mutation + D->Cfg("mutationPerFire", 7), 0.f, 100.f);
				for (FKAPWorker& K : S.Workers)
					if (K.WellId == i + 1 && K.EvacTo.IsEmpty() && K.State < 4) K.Dose += D->Cfg("fireDoseBurst", 260) * (K.bProtected ? 0.4f : 1.f);
				RefreshVisual(S, i);
				if (GM) { GM->Radio(FString::Printf(TEXT("№%d ұңғыма ЖАНЫП ЖАТЫР! Ұңғыма жоғалды."), i + 1), "danger", { UKAPAudio::OrdinalKk(i + 1), TEXT("ұңғыма жанып жатыр! Ұңғыма жоғалды.") }); GM->CheckEndings(); }
			}
			break;
		case EWellState::Fire:
			R.Pressure = 0; R.Flow = 0;
			W.BurnTimer -= (float)Dh;
			S.Mutation = FMath::Clamp(S.Mutation + 0.4f * (float)Dh, 0.f, 100.f);
			if (W.BurnTimer <= 0) { W.State = EWellState::Lost; RefreshVisual(S, i); }
			break;
		default: R.Pressure = 0; R.Flow = 0; break;
		}
	}
	// ---- жұмысшылар ----
	const float DoseDeath = D->Cfg("doseDeath", 650);
	for (FKAPWorker& K : S.Workers)
	{
		if (K.State >= 4) continue;
		K.Dose += (float)(DoseRate(S, K) * Dh);
		K.State = HealthState(K.Dose, D);
		if (K.Dose >= DoseDeath && !K.bSpecial) KillWorker(S, K);
	}
	// HMM — әр ойын сағатында симптомдар туралы радио есеп
	WorkerAcc += Dh;
	if (WorkerAcc >= 1.0)
	{
		WorkerAcc -= 1.0;
		for (FKAPWorker& K : S.Workers)
		{
			if (K.State >= 4) continue;
			K.Symptom = KAPAI::SampleSymptom(K.State, Rng);
			double B[4] = { K.Belief[0], K.Belief[1], K.Belief[2], K.Belief[3] };
			KAPAI::HealthForward(B, Exposure(S, K), K.Symptom);
			for (int32 j = 0; j < 4; j++) K.Belief[j] = (float)B[j];
		}
	}
	// ---- мутация (UMutationSubsystem логикасы) ----
	int32 Leaks = 0; bool bActive = false;
	for (const FKAPWellSave& W : S.Wells) { if (W.State == EWellState::Leak) Leaks++; if (W.State == EWellState::Leak || W.State == EWellState::Fire) bActive = true; }
	S.Mutation = bActive ? FMath::Clamp(S.Mutation + 0.15f * (float)Dh * Leaks, 0.f, 100.f) : FMath::Clamp(S.Mutation - 0.35f * (float)Dh, 0.f, 100.f);
	if (S.Mutation >= 100 && GM) GM->CheckEndings();
}

// ---------------- CSP бригадалары ----------------
TArray<FString> UWellManagerSubsystem::BrigadeTools(FName Id) const
{
	TArray<FString> Out;
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (const FKAPBrigadeRow* R = D->Brigades->FindRow<FKAPBrigadeRow>(Id, TEXT("KAP"), false)) R->Tools.ParseIntoArray(Out, TEXT("|"));
	return Out;
}

void UWellManagerSubsystem::BuildCSPInput(const FKAPSession& S, TArray<KAPAI::FCSPBrigade>& OutB, TArray<KAPAI::FCSPWell>& OutW, double& OutHpm) const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	for (int32 i = 0; i < Wells.Num(); i++)
	{
		const FKAPWellSave& W = S.Wells[i];
		if (W.State != EWellState::Fault && W.State != EWellState::Leak) continue;
		bool bTaken = false;
		for (const FKAPBrigadeState& B : S.Brigades) if (B.Target == i + 1) bTaken = true;
		if (bTaken) continue;
		int32 Alive = 0;
		for (const FKAPWorker& K : S.Workers) if (K.WellId == i + 1 && K.State < 4 && K.EvacTo.IsEmpty()) Alive++;
		KAPAI::FCSPWell C;
		C.Id = i + 1; C.Pos = Wells[i].PosM(); C.Tool = KAP::FaultTool(W.Fault);
		C.TimeLeftH = W.State == EWellState::Fault ? W.Timer + D->Cfg("stage2Hours", 5) : W.Timer;
		C.Priority = Alive * 2 + (W.State == EWellState::Leak ? 6 : 2);
		OutW.Add(C);
	}
	for (const FKAPBrigadeState& B : S.Brigades)
	{
		if (B.State != "idle") continue;
		KAPAI::FCSPBrigade C; C.Id = B.Id; C.Pos = B.Pos; C.Range = 1500;
		for (const FString& T : BrigadeTools(B.Id)) C.Tools.Add(FName(*T));
		OutB.Add(C);
	}
	OutHpm = 24.0 / D->Cfg("dayLengthSec", 2880) / 14.0;
}

void UWellManagerSubsystem::Dispatch(FKAPSession& S, FName BrigadeId, int32 WellId)
{
	for (FKAPBrigadeState& B : S.Brigades)
	{
		if (B.Id != BrigadeId || B.State != "idle") continue;
		const FVector2D T = Wells[WellId - 1].PosM() + FVector2D(6, 6);
		UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
		KAPAI::FRoute R = D->Graph.Route(B.Pos, T, true, DangerFn ? DangerFn : [](double, double) { return 0.0; });
		B.Route = R.bValid ? R.Points : TArray<FVector2D>{ T };
		B.Ri = 0; B.State = "travel"; B.Target = WellId;
		if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Radio(FString::Printf(TEXT("Бригада №%d ұңғымаға шықты."), WellId), "info", { TEXT("Бригада"), UKAPAudio::OrdinalKk(WellId), TEXT("ұңғымаға шықты.") });
	}
}

void UWellManagerSubsystem::UpdateBrigades(FKAPSession& S, double DtReal, double Dh, float TimeScale)
{
	if (!S.bBrigadesUnlocked) return;
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	auto Return = [&](FKAPBrigadeState& B)
	{
		KAPAI::FRoute R = D->Graph.Route(B.Pos, B.Base, true, DangerFn ? DangerFn : [](double, double) { return 0.0; });
		B.Route = R.bValid ? R.Points : TArray<FVector2D>{ B.Base };
		B.Ri = 0; B.State = "return";
	};
	for (FKAPBrigadeState& B : S.Brigades)
	{
		if (B.State == "travel" || B.State == "return")
		{
			if (!B.Route.IsValidIndex(B.Ri))
			{
				if (B.State == "travel") { B.State = "repair"; B.Work = 0; } else { B.State = "idle"; B.Target = -1; }
				continue;
			}
			const FVector2D P = B.Route[B.Ri];
			const FVector2D Dv = P - B.Pos;
			const double Dist = Dv.Size(), Step = 14.0 * DtReal * TimeScale;
			if (Dist <= Step) { B.Pos = P; B.Ri++; } else B.Pos += Dv / Dist * Step;
		}
		else if (B.State == "repair")
		{
			const int32 I = B.Target - 1;
			if (!Wells.IsValidIndex(I)) { B.State = "idle"; continue; }
			const EWellState St = S.Wells[I].State;
			if (St == EWellState::Fire || St == EWellState::Lost || St == EWellState::Ok) { Return(B); continue; }
			B.Work += (float)Dh;
			if (B.Work >= 0.6f)
			{
				RepairWell(S, I);
				if (AKAPGameMode* GM = AKAPGameMode::Get(this))
				{
					GM->Radio(FString::Printf(TEXT("Бригада №%d ұңғыманы жөндеді."), I + 1), "ok", { TEXT("Бригада"), UKAPAudio::OrdinalKk(I + 1), TEXT("ұңғыманы жөндеді.") });
					GM->OnWellRepaired(I, true);
				}
				Return(B);
			}
		}
	}
}
