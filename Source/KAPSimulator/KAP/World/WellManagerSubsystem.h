// UWellManagerSubsystem — 21 ұңғыманы басқару, Байес болжамы, жұмысшылар дозасы мен HMM, мутация, бригадалар
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KAP/KAPTypes.h"
#include "KAP/AI/KAPAI.h"
#include "WellManagerSubsystem.generated.h"

class AWellActor;
struct FKAPSession;

struct FKAPWellRuntime
{
	FKAPWellRow Row;
	float Pressure = 4.f, Flow = 10.f;
	float FailProb = 0.1f;
	KAPAI::FWellPrediction Pred;
	bool bTelemetry = true;
	TWeakObjectPtr<AWellActor> Actor;
	FVector2D PosM() const { return FVector2D(Row.X, Row.Y); }
};

UCLASS()
class KAPSIMULATOR_API UWellManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	TArray<FKAPWellRuntime> Wells; // индекс = Id-1

	void InitFromData();             // CSV-дан
	void NewGameState(FKAPSession& S);
	void CreateWorkers(FKAPSession& S);

	void Simulate(FKAPSession& S, double Dh, float EnvTemp, float Storm, float Rain);
	void UpdatePredictions(FKAPSession& S, float EnvTemp);
	void UpdateBrigades(FKAPSession& S, double DtReal, double Dh, float TimeScale);

	void StartFault(FKAPSession& S, int32 Index, EFaultType Type, const FString& Reason);
	void RepairWell(FKAPSession& S, int32 Index);
	void KillWorker(FKAPSession& S, FKAPWorker& K);

	float NeighborBad(const FKAPSession& S, int32 Index) const;
	KAPAI::FWellEvidence Evidence(const FKAPSession& S, int32 Index, float EnvTemp) const;
	double Hazard(int32 Index) const;
	double TimeToFailure(const FKAPSession& S, int32 Index) const;
	int32 DeadCount(const FKAPSession& S) const;
	int32 Exposure(const FKAPSession& S, const FKAPWorker& K) const;
	double DoseRate(const FKAPSession& S, const FKAPWorker& K) const;
	double TimeToDeath(const FKAPSession& S, const FKAPWorker& K) const;
	static int32 HealthState(float Dose, const class UKAPDataSubsystem* D);
	int32 OkCount(const FKAPSession& S) const;

	// CSP
	void BuildCSPInput(const FKAPSession& S, TArray<KAPAI::FCSPBrigade>& OutB, TArray<KAPAI::FCSPWell>& OutW, double& OutHpm) const;
	void Dispatch(FKAPSession& S, FName BrigadeId, int32 WellId);
	TArray<FString> BrigadeTools(FName Id) const;

	TFunction<double(double, double)> DangerFn;
	void RefreshVisual(const FKAPSession& S, int32 Index);
private:
	double WorkerAcc = 0;
	FRandomStream Rng;
};
