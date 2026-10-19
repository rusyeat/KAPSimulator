// KAP Simulator — силлабустағы AI тақырыптары (таза C++, UObject-сіз)
//  2-апта  A* іздеу және эвристикалар      → FKAPRoadGraph, FKAPGrid (навигатор, NPC жол табуы)
//  6-апта  Байес желілері                   → FKAPBayesNet (ұңғыма бұзылуы, жұмысшы ауруы)
//  5-апта  MDP + Q-learning                 → FKAPQPolicy (жасақ, бандит, монстр шешімдері)
//  7-апта  HMM (Forward алгоритмі)          → FKAPSoundHMM, KAPHealthHMM (дауылдағы монстр, жұмысшы күйі)
//  3-апта  CSP (backtracking, hill climbing) → KAPCSP (жөндеу бригадаларын бөлу)
#pragma once

#include "CoreMinimal.h"

namespace KAPAI
{
	// ---------------- A* ----------------
	struct FRoute
	{
		TArray<FVector2D> Points;   // метр, әлем XY
		TArray<int32> Nodes;
		double Length = 0.0;
		double Danger = 0.0;        // 0..1, ұзындыққа қатысты орташа
		int32 Expanded = 0;
		bool bValid = false;
	};

	using FDangerFn = TFunction<double(double X, double Y)>;

	/** Жалпы A* — Neighbors(Node, OutTo, OutCost), Heuristic(Node) */
	bool AStar(int32 Start, int32 Goal,
		const TFunction<void(int32, TArray<TPair<int32, double>>&)>& Neighbors,
		const TFunction<double(int32)>& Heuristic,
		TArray<int32>& OutPath, double& OutCost, int32& OutExpanded);

	struct FRoadEdge { int32 A = 0, B = 0; double Len = 0.0, BaseDanger = 0.0; };
	struct FRoadNode { FName Id; FVector2D Pos; FString Name; TArray<TPair<int32, int32>> Adj; /* (to, edge) */ };

	class KAPSIMULATOR_API FRoadGraph
	{
	public:
		TArray<FRoadNode> Nodes;
		TArray<FRoadEdge> Edges;
		TMap<FName, int32> Index;

		int32 AddNode(FName Id, FVector2D Pos, const FString& Name);
		void AddEdge(FName A, FName B, double Danger);
		int32 Nearest(FVector2D P) const;
		double EdgeDanger(const FRoadEdge& E, const FDangerFn& DangerAt) const;
		/** bSafe=false — ең қысқа; bSafe=true — қабырға салмағына қауіп коэффициенті қосылады */
		FRoute Route(FVector2D From, FVector2D To, bool bSafe, const FDangerFn& DangerAt) const;
	};

	class KAPSIMULATOR_API FGrid
	{
	public:
		double Half = 1200.0;
		int32 N = 60;
		double Cell = 40.0;
		TArray<float> Slope;
		TArray<uint8> Blocked;

		void Init(double InHalf, int32 InN, const TFunction<double(double, double)>& HeightFn);
		int32 Idx(FVector2D P) const;
		FVector2D Center(int32 Id) const;
		FRoute Route(FVector2D From, FVector2D To, const FDangerFn& DangerAt, double DangerK = 1.5) const;
	};

	// ---------------- Байес желісі ----------------
	struct FBayesVar
	{
		FName Name;
		TArray<int32> Parents;                          // алдыңғы айнымалылардың индекстері
		TFunction<double(const TArray<int32>&)> P1;    // P(var=1 | ата-аналар)
	};

	class KAPSIMULATOR_API FBayesNet
	{
	public:
		TArray<FBayesVar> Vars; // топологиялық ретпен
		/** Толық санау; Lambda[i] = (λ0, λ1) виртуалды дәлел, (1,1) — дәлел жоқ */
		double Query(int32 Target, const TArray<FVector2D>& Lambda) const;
	};

	struct FWellEvidence
	{
		double Age = 0, Pressure = 0, BasePressure = 0, EnvTemp = 25, NeighborBad = 0;
		bool bTelemetry = true;
	};
	struct FWellPrediction { double P = 0; FVector2D LamA, LamT, LamP, LamN; bool bHasP = true; };

	KAPSIMULATOR_API FWellPrediction WellFailureProb(const FWellEvidence& E);
	KAPSIMULATOR_API double WorkerSickProb(double Dose, double Hours, bool bProtected);

	// ---------------- MDP + Q-learning ----------------
	enum class EQProfile : uint8 { Militia, Bandit, Monster };
	static constexpr int32 NumStates = 54;
	static constexpr int32 NumActions = 4; // 0 шабуыл, 1 жасырыну, 2 шегіну, 3 көмек шақыру
	KAPSIMULATOR_API int32 EncodeState(int32 Hp, int32 Dist, int32 Ammo, int32 Ally);
	KAPSIMULATOR_API const TCHAR* ActionName(int32 A);

	class KAPSIMULATOR_API FQPolicy
	{
	public:
		EQProfile Profile = EQProfile::Monster;
		TArray<float> Q; // NumStates*NumActions
		double TrainMs = 0;
		void Train(EQProfile InProfile, int32 Episodes, uint32 Seed);
		int32 Best(int32 State) const;
		float Value(int32 S, int32 A) const { return Q[S * NumActions + A]; }
	};

	// ---------------- HMM ----------------
	struct FSoundObs { bool bHeard = false; int32 Sector = 0; bool bLoud = false; };

	class KAPSIMULATOR_API FSoundHMM
	{
	public:
		double Half = 1200.0;
		int32 N = 64;
		double Cell = 37.5;
		TArray<float> B, Tmp;
		void Init(double InHalf, int32 InN);
		void InitAround(FVector2D P, double Radius);
		void Predict(FVector2D Player);
		void Update(const FSoundObs& Obs, FVector2D Player);
		FVector2D CellCenter(int32 I, int32 J) const;
		static double HearProb(double D);
		static int32 SectorOf(double Dx, double Dy);
		static FSoundObs Sample(FVector2D Monster, FVector2D Player, FRandomStream& R);
	};

	/** Жұмысшының жасырын күйі: 0 сау, 1 уланған, 2 ауру, 3 өлім алдында. Exposure: 0 қауіпсіз, 1 ағу, 2 өрт */
	KAPSIMULATOR_API void HealthForward(double Belief[4], int32 Exposure, int32 Symptom);
	KAPSIMULATOR_API int32 SampleSymptom(int32 State, FRandomStream& R);

	// ---------------- CSP ----------------
	struct FCSPBrigade { FName Id; FVector2D Pos; TArray<FName> Tools; double Range = 1500; };
	struct FCSPWell { int32 Id = 0; FVector2D Pos; FName Tool; double TimeLeftH = 0; double Priority = 0; };
	struct FCSPResult
	{
		FString Method;
		TArray<int32> Assign;              // әр бригадаға ұңғыма id немесе -1
		TArray<TArray<int32>> Domains;
		double Score = 0;
		int32 Nodes = 0;
		double Ms = 0;
	};
	KAPSIMULATOR_API FCSPResult SolveBacktracking(const TArray<FCSPBrigade>& Br, const TArray<FCSPWell>& Wells, double HoursPerMeter, double RepairH);
	KAPSIMULATOR_API FCSPResult SolveHillClimb(const TArray<FCSPBrigade>& Br, const TArray<FCSPWell>& Wells, double HoursPerMeter, double RepairH, int32 Restarts, uint32 Seed);
}
