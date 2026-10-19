#include "KAPAI.h"
#include "HAL/PlatformTime.h"

namespace KAPAI
{
	// =====================================================================
	// A*
	// =====================================================================
	bool AStar(int32 Start, int32 Goal,
		const TFunction<void(int32, TArray<TPair<int32, double>>&)>& Neighbors,
		const TFunction<double(int32)>& Heuristic,
		TArray<int32>& OutPath, double& OutCost, int32& OutExpanded)
	{
		struct FOpen { int32 Node; double F; };
		struct FLess { bool operator()(const FOpen& A, const FOpen& B) const { return A.F < B.F; } };
		TArray<FOpen> Open;
		TMap<int32, double> G;
		TMap<int32, int32> Came;
		TSet<int32> Closed;
		G.Add(Start, 0.0);
		Open.HeapPush({ Start, Heuristic(Start) }, FLess());
		OutExpanded = 0;
		TArray<TPair<int32, double>> Nb;
		while (Open.Num())
		{
			FOpen Cur;
			Open.HeapPop(Cur, FLess(), EAllowShrinking::No);
			if (Closed.Contains(Cur.Node)) continue;
			if (Cur.Node == Goal)
			{
				OutPath.Reset();
				int32 C = Goal;
				OutPath.Add(C);
				while (const int32* P = Came.Find(C)) { C = *P; OutPath.Add(C); }
				Algo::Reverse(OutPath);
				OutCost = G[Goal];
				return true;
			}
			Closed.Add(Cur.Node);
			OutExpanded++;
			const double Gc = G[Cur.Node];
			Nb.Reset();
			Neighbors(Cur.Node, Nb);
			for (const TPair<int32, double>& E : Nb)
			{
				if (Closed.Contains(E.Key)) continue;
				const double Ng = Gc + E.Value;
				const double* Old = G.Find(E.Key);
				if (!Old || Ng < *Old)
				{
					G.Add(E.Key, Ng);
					Came.Add(E.Key, Cur.Node);
					Open.HeapPush({ E.Key, Ng + Heuristic(E.Key) }, FLess());
				}
			}
		}
		return false;
	}

	int32 FRoadGraph::AddNode(FName Id, FVector2D Pos, const FString& Name)
	{
		const int32 I = Nodes.Add({ Id, Pos, Name, {} });
		Index.Add(Id, I);
		return I;
	}
	void FRoadGraph::AddEdge(FName A, FName B, double Danger)
	{
		const int32* IA = Index.Find(A);
		const int32* IB = Index.Find(B);
		if (!IA || !IB) return;
		FRoadEdge E{ *IA, *IB, FVector2D::Distance(Nodes[*IA].Pos, Nodes[*IB].Pos), Danger };
		const int32 EI = Edges.Add(E);
		Nodes[*IA].Adj.Add({ *IB, EI });
		Nodes[*IB].Adj.Add({ *IA, EI });
	}
	int32 FRoadGraph::Nearest(FVector2D P) const
	{
		int32 Best = INDEX_NONE; double Bd = TNumericLimits<double>::Max();
		for (int32 i = 0; i < Nodes.Num(); i++)
		{
			const double D = FVector2D::DistSquared(P, Nodes[i].Pos);
			if (D < Bd) { Bd = D; Best = i; }
		}
		return Best;
	}
	double FRoadGraph::EdgeDanger(const FRoadEdge& E, const FDangerFn& DangerAt) const
	{
		double D = E.BaseDanger;
		const FVector2D A = Nodes[E.A].Pos, B = Nodes[E.B].Pos;
		for (double T = 0.1; T < 1.0; T += 0.2)
		{
			const FVector2D P = FMath::Lerp(A, B, T);
			D = FMath::Max(D, DangerAt(P.X, P.Y));
		}
		return FMath::Clamp(D, 0.0, 1.0);
	}
	FRoute FRoadGraph::Route(FVector2D From, FVector2D To, bool bSafe, const FDangerFn& DangerAt) const
	{
		FRoute R;
		const int32 S = Nearest(From), T = Nearest(To);
		if (S == INDEX_NONE || T == INDEX_NONE) return R;
		const double K = bSafe ? 6.0 : 0.0;
		TArray<int32> Path; double Cost = 0;
		const FVector2D TP = Nodes[T].Pos;
		const bool bOk = AStar(S, T,
			[&](int32 N, TArray<TPair<int32, double>>& Out)
			{
				for (const TPair<int32, int32>& A : Nodes[N].Adj)
				{
					const FRoadEdge& E = Edges[A.Value];
					Out.Add({ A.Key, E.Len * (1.0 + K * EdgeDanger(E, DangerAt)) });
				}
			},
			[&](int32 N) { return FVector2D::Distance(Nodes[N].Pos, TP); }, // түзу қашықтық — рұқсатты эвристика
			Path, Cost, R.Expanded);
		if (!bOk) return R;
		R.bValid = true;
		R.Nodes = Path;
		R.Points.Add(From);
		for (int32 N : Path) R.Points.Add(Nodes[N].Pos);
		R.Points.Add(To);
		double Dsum = 0;
		for (int32 i = 0; i + 1 < Path.Num(); i++)
		{
			for (const TPair<int32, int32>& A : Nodes[Path[i]].Adj)
			{
				if (A.Key != Path[i + 1]) continue;
				const FRoadEdge& E = Edges[A.Value];
				R.Length += E.Len;
				Dsum += E.Len * EdgeDanger(E, DangerAt);
				break;
			}
		}
		R.Danger = R.Length > 0 ? Dsum / R.Length : 0;
		return R;
	}

	void FGrid::Init(double InHalf, int32 InN, const TFunction<double(double, double)>& HeightFn)
	{
		Half = InHalf; N = InN; Cell = Half * 2.0 / N;
		Slope.SetNumZeroed(N * N); Blocked.SetNumZeroed(N * N);
		for (int32 Id = 0; Id < N * N; Id++)
		{
			const FVector2D C = Center(Id);
			const double D = Cell * 0.5;
			const double Hx = FMath::Abs(HeightFn(C.X + D, C.Y) - HeightFn(C.X - D, C.Y));
			const double Hy = FMath::Abs(HeightFn(C.X, C.Y + D) - HeightFn(C.X, C.Y - D));
			Slope[Id] = (float)((Hx + Hy) / (Cell * 2.0));
			if (FMath::Max(FMath::Abs(C.X), FMath::Abs(C.Y)) > Half - Cell) Blocked[Id] = 1;
		}
	}
	int32 FGrid::Idx(FVector2D P) const
	{
		const int32 I = FMath::Clamp(FMath::FloorToInt32((P.X + Half) / Cell), 0, N - 1);
		const int32 J = FMath::Clamp(FMath::FloorToInt32((P.Y + Half) / Cell), 0, N - 1);
		return J * N + I;
	}
	FVector2D FGrid::Center(int32 Id) const
	{
		return FVector2D(-Half + (Id % N + 0.5) * Cell, -Half + (Id / N + 0.5) * Cell);
	}
	FRoute FGrid::Route(FVector2D From, FVector2D To, const FDangerFn& DangerAt, double DangerK) const
	{
		FRoute R;
		const int32 S = Idx(From), T = Idx(To);
		const FVector2D TC = Center(T);
		TArray<int32> Path; double Cost = 0;
		const bool bOk = AStar(S, T,
			[&](int32 Id, TArray<TPair<int32, double>>& Out)
			{
				const int32 I = Id % N, J = Id / N;
				for (int32 Dj = -1; Dj <= 1; Dj++)
					for (int32 Di = -1; Di <= 1; Di++)
					{
						if (!Di && !Dj) continue;
						const int32 Ni = I + Di, Nj = J + Dj;
						if (Ni < 0 || Nj < 0 || Ni >= N || Nj >= N) continue;
						const int32 Nid = Nj * N + Ni;
						if (Blocked[Nid]) continue;
						const FVector2D C = Center(Nid);
						const double Base = Cell * ((Di && Dj) ? UE_SQRT_2 : 1.0);
						Out.Add({ Nid, Base * (1.0 + 6.0 * Slope[Nid] + DangerK * DangerAt(C.X, C.Y)) });
					}
			},
			[&](int32 Id) { return FVector2D::Distance(Center(Id), TC); },
			Path, Cost, R.Expanded);
		if (!bOk) return R;
		R.bValid = true;
		R.Points.Add(From);
		for (int32 i = 1; i + 1 < Path.Num(); i++) R.Points.Add(Center(Path[i]));
		R.Points.Add(To);
		double Dsum = 0;
		for (int32 i = 1; i < R.Points.Num(); i++)
		{
			const double L = FVector2D::Distance(R.Points[i - 1], R.Points[i]);
			R.Length += L;
			Dsum += L * DangerAt(R.Points[i].X, R.Points[i].Y);
		}
		R.Danger = R.Length > 0 ? Dsum / R.Length : 0;
		return R;
	}

	// =====================================================================
	// Байес желісі — толық санау + виртуалды дәлел
	// =====================================================================
	double FBayesNet::Query(int32 Target, const TArray<FVector2D>& Lambda) const
	{
		const int32 Num = Vars.Num();
		double Acc[2] = { 0, 0 };
		TArray<int32> Val; Val.SetNumZeroed(Num);
		TArray<int32> Pv;
		TFunction<void(int32, double)> Rec = [&](int32 K, double W)
		{
			if (W == 0) return;
			if (K == Num) { Acc[Val[Target]] += W; return; }
			const FBayesVar& V = Vars[K];
			Pv.Reset();
			for (int32 P : V.Parents) Pv.Add(Val[P]);
			const double P1 = V.P1(Pv);
			const FVector2D L = Lambda.IsValidIndex(K) ? Lambda[K] : FVector2D(1, 1);
			Val[K] = 0; Rec(K + 1, W * (1 - P1) * L.X);
			Val[K] = 1; Rec(K + 1, W * P1 * L.Y);
		};
		Rec(0, 1.0);
		const double Z = Acc[0] + Acc[1];
		return Z > 0 ? Acc[1] / Z : 0;
	}

	static double Sig(double X) { return 1.0 / (1.0 + FMath::Exp(-X)); }

	// Жасы(A), Темп(T) → Тозу(W, жасырын); Тозу, Қысым(P), Көрші(N) → Бұзылу(F)
	static const FBayesNet& FailNet()
	{
		static FBayesNet Net = []()
		{
			FBayesNet B;
			B.Vars.Add({ "A", {}, [](const TArray<int32>&) { return 0.4; } });
			B.Vars.Add({ "T", {}, [](const TArray<int32>&) { return 0.3; } });
			B.Vars.Add({ "P", {}, [](const TArray<int32>&) { return 0.25; } });
			B.Vars.Add({ "N", {}, [](const TArray<int32>&) { return 0.1; } });
			B.Vars.Add({ "W", { 0, 1 }, [](const TArray<int32>& V) { static const double T[2][2] = { { 0.1, 0.3 }, { 0.45, 0.75 } }; return T[V[0]][V[1]]; } });
			B.Vars.Add({ "F", { 4, 2, 3 }, [](const TArray<int32>& V)
				{
					static const double T[2][2][2] = { { { 0.04, 0.25 }, { 0.3, 0.55 } }, { { 0.35, 0.62 }, { 0.72, 0.92 } } };
					return T[V[0]][V[1]][V[2]];
				} });
			return B;
		}();
		return Net;
	}
	FWellPrediction WellFailureProb(const FWellEvidence& E)
	{
		FWellPrediction R;
		const double Pa = Sig((E.Age - 12.0) / 2.5);
		const double Pt = Sig((E.EnvTemp - 36.0) / 3.0);
		R.LamA = FVector2D(1 - Pa, Pa);
		R.LamT = FVector2D(1 - Pt, Pt);
		R.LamN = FVector2D(1 - E.NeighborBad, E.NeighborBad);
		R.bHasP = E.bTelemetry;
		if (E.bTelemetry) { const double Pp = Sig((E.Pressure - E.BasePressure - 0.4) / 0.25); R.LamP = FVector2D(1 - Pp, Pp); }
		else R.LamP = FVector2D(1, 1); // дауылда телеметрия жоқ — приор
		TArray<FVector2D> L = { R.LamA, R.LamT, R.LamP, R.LamN, FVector2D(1, 1), FVector2D(1, 1) };
		R.P = FailNet().Query(5, L);
		return R;
	}
	double WorkerSickProb(double Dose, double Hours, bool bProtected)
	{
		static FBayesNet Net = []()
		{
			FBayesNet B;
			B.Vars.Add({ "D", {}, [](const TArray<int32>&) { return 0.2; } });
			B.Vars.Add({ "H", {}, [](const TArray<int32>&) { return 0.5; } });
			B.Vars.Add({ "Pr", {}, [](const TArray<int32>&) { return 0.5; } });
			B.Vars.Add({ "S", { 0, 1, 2 }, [](const TArray<int32>& V)
				{
					static const double T[2][2][2] = { { { 0.06, 0.02 }, { 0.12, 0.05 } }, { { 0.65, 0.35 }, { 0.85, 0.55 } } };
					return T[V[0]][V[1]][V[2]];
				} });
			return B;
		}();
		const double Pd = Sig((Dose - 180.0) / 50.0), Ph = Sig((Hours - 8.0) / 1.5);
		TArray<FVector2D> L = { FVector2D(1 - Pd, Pd), FVector2D(1 - Ph, Ph), bProtected ? FVector2D(0, 1) : FVector2D(1, 0), FVector2D(1, 1) };
		return Net.Query(3, L);
	}

	// =====================================================================
	// MDP + Q-learning
	// =====================================================================
	int32 EncodeState(int32 Hp, int32 Dist, int32 Ammo, int32 Ally) { return ((Hp * 3 + Dist) * 3 + Ammo) * 2 + Ally; }
	const TCHAR* ActionName(int32 A)
	{
		static const TCHAR* N[4] = { TEXT("Шабуыл"), TEXT("Жасырыну"), TEXT("Шегіну"), TEXT("Көмек шақыру") };
		return N[FMath::Clamp(A, 0, 3)];
	}
	struct FProf { bool bAmmo; double Escape, Death, Kill, Hurt; };
	static FProf GetProf(EQProfile P)
	{
		switch (P)
		{
		case EQProfile::Militia: return { true, 3, -12, 8, -2.5 };
		case EQProfile::Bandit: return { true, 1, -8, 12, -1.5 };
		default: return { false, 0, -4, 10, -1 };
		}
	}
	struct FSimSt { int32 Hp, Dist, Ammo, Ally, PHp; };

	void FQPolicy::Train(EQProfile InProfile, int32 Episodes, uint32 Seed)
	{
		const double T0 = FPlatformTime::Seconds();
		Profile = InProfile;
		const FProf Pr = GetProf(InProfile);
		FRandomStream R(Seed);
		Q.Init(0.f, NumStates * NumActions);
		const double Alpha = 0.12, Gamma = 0.9;
		for (int32 Ep = 0; Ep < Episodes; Ep++)
		{
			const double Eps = FMath::Max(0.05, 0.6 * (1.0 - (double)Ep / Episodes));
			FSimSt S{ R.RandRange(0, 2), R.RandRange(0, 2), Pr.bAmmo ? R.RandRange(0, 2) : 2, R.FRand() < 0.3 ? 1 : 0, 3 + R.RandRange(0, 2) };
			for (int32 Step = 0; Step < 40; Step++)
			{
				const int32 Sx = EncodeState(S.Hp, S.Dist, S.Ammo, S.Ally);
				int32 A = R.FRand() < Eps ? R.RandRange(0, 3) : Best(Sx);
				// ---- орта моделі (симулятор) ----
				double Rw = -0.1; bool bDone = false;
				const double Exposure = (S.Dist == 0 ? 0.55 : S.Dist == 1 ? 0.4 : 0.2) * (S.Ally ? 0.7 : 1.0);
				double Incoming = Exposure;
				if (A == 0)
				{
					const bool bCanHit = Pr.bAmmo ? S.Ammo > 0 : S.Dist == 0;
					if (bCanHit)
					{
						const double Hit = (S.Dist == 0 ? 0.7 : S.Dist == 1 ? 0.5 : 0.2) + (S.Ally ? 0.15 : 0.0);
						if (R.FRand() < Hit) { S.PHp--; Rw += 2; }
						if (Pr.bAmmo && R.FRand() < 0.35) S.Ammo = FMath::Max(0, S.Ammo - 1);
					}
					else if (S.Dist > 0) S.Dist--;
					if (!Pr.bAmmo && S.Dist > 0) S.Dist--;
				}
				else if (A == 1)
				{
					Incoming = 0.1;
					if (Pr.bAmmo && S.Ammo > 0 && R.FRand() < 0.15) { S.PHp--; Rw += 1; }
					if (S.Hp < 2 && R.FRand() < 0.15) S.Hp++;
					Rw -= 0.2;
				}
				else if (A == 2)
				{
					Incoming = S.Dist == 0 ? 0.3 : 0.1;
					if (S.Dist == 2 && S.Hp == 0) { bDone = true; Rw += Pr.Escape; }
					S.Dist = FMath::Min(2, S.Dist + 1);
					if (Pr.bAmmo && S.Dist == 2 && R.FRand() < 0.3) S.Ammo = FMath::Min(2, S.Ammo + 1);
				}
				else
				{
					Incoming *= 0.8;
					if (!S.Ally && R.FRand() < 0.6) S.Ally = 1; else Rw -= 1;
					Rw -= 0.5;
				}
				if (!bDone && R.FRand() < Incoming && R.FRand() < 0.6) { S.Hp--; Rw += Pr.Hurt; }
				if (S.Hp < 0) { bDone = true; Rw += Pr.Death; S.Hp = 0; }
				else if (S.PHp <= 0) { bDone = true; Rw += Pr.Kill; }
				if (!Pr.bAmmo) S.Ammo = 2;
				const int32 S2 = EncodeState(S.Hp, S.Dist, S.Ammo, S.Ally);
				float MaxQ = Q[S2 * NumActions];
				for (int32 k = 1; k < NumActions; k++) MaxQ = FMath::Max(MaxQ, Q[S2 * NumActions + k]);
				const double Target = Rw + (bDone ? 0.0 : Gamma * MaxQ);
				float& Cell = Q[Sx * NumActions + A];
				Cell += (float)(Alpha * (Target - Cell));
				if (bDone) break;
			}
		}
		TrainMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	}
	int32 FQPolicy::Best(int32 S) const
	{
		int32 A = 0;
		for (int32 k = 1; k < NumActions; k++) if (Q[S * NumActions + k] > Q[S * NumActions + A]) A = k;
		return A;
	}

	// =====================================================================
	// HMM — Forward алгоритмі
	// =====================================================================
	void FSoundHMM::Init(double InHalf, int32 InN)
	{
		Half = InHalf; N = InN; Cell = Half * 2.0 / N;
		B.Init(1.f / (N * N), N * N); Tmp.Init(0.f, N * N);
	}
	FVector2D FSoundHMM::CellCenter(int32 I, int32 J) const { return FVector2D(-Half + (I + 0.5) * Cell, -Half + (J + 0.5) * Cell); }
	double FSoundHMM::HearProb(double D) { return FMath::Clamp(0.95 - D / 260.0, 0.02, 0.95); }
	int32 FSoundHMM::SectorOf(double Dx, double Dy)
	{
		// 0 = солтүстік (−Y), сағат тілімен
		const double A = FMath::Atan2(Dx, -Dy);
		return ((FMath::RoundToInt32(A / (UE_PI / 4)) % 8) + 8) % 8;
	}
	void FSoundHMM::InitAround(FVector2D P, double Radius)
	{
		double S = 0;
		for (int32 J = 0; J < N; J++) for (int32 I = 0; I < N; I++)
		{
			const float V = FVector2D::Distance(CellCenter(I, J), P) < Radius ? 1.f : 0.f;
			B[J * N + I] = V; S += V;
		}
		if (S > 0) for (float& V : B) V /= (float)S;
	}
	void FSoundHMM::Predict(FVector2D Player)
	{
		// P(X_t | e_1:t-1) = Σ T(x→x') b(x); монстр дыбысқа қарай ығысады
		for (float& V : Tmp) V = 0;
		for (int32 J = 0; J < N; J++) for (int32 I = 0; I < N; I++)
		{
			const float V = B[J * N + I];
			if (V < 1e-7f) continue;
			const FVector2D C = CellCenter(I, J);
			const FVector2D T = Player - C;
			const double Tl = FMath::Max(1.0, T.Size());
			double W[9]; int32 Id[9]; int32 K = 0; double Ws = 0;
			for (int32 Dj = -1; Dj <= 1; Dj++) for (int32 Di = -1; Di <= 1; Di++)
			{
				const int32 Ni = I + Di, Nj = J + Dj;
				if (Ni < 0 || Nj < 0 || Ni >= N || Nj >= N) continue;
				double Wt;
				if (!Di && !Dj) Wt = 1.6;
				else { const double Cos = (Di * T.X + Dj * T.Y) / (FMath::Sqrt((double)(Di * Di + Dj * Dj)) * Tl); Wt = 0.4 + 0.9 * FMath::Max(0.0, Cos); }
				W[K] = Wt; Id[K] = Nj * N + Ni; Ws += Wt; K++;
			}
			for (int32 k = 0; k < K; k++) Tmp[Id[k]] += (float)(V * W[k] / Ws);
		}
		Swap(B, Tmp);
	}
	void FSoundHMM::Update(const FSoundObs& Obs, FVector2D Player)
	{
		// b(x) ∝ P(e_t | x) · b(x)
		double S = 0;
		for (int32 J = 0; J < N; J++) for (int32 I = 0; I < N; I++)
		{
			float& V = B[J * N + I];
			if (V < 1e-9f) { V = 0; continue; }
			const FVector2D D = CellCenter(I, J) - Player;
			const double Dist = D.Size();
			const double Ph = HearProb(Dist) * 0.7;
			double L;
			if (!Obs.bHeard) L = 1 - Ph;
			else
			{
				const int32 Sec = SectorOf(D.X, D.Y);
				const int32 Diff = FMath::Min((Sec - Obs.Sector + 8) % 8, (Obs.Sector - Sec + 8) % 8);
				const double Ps = Diff == 0 ? 0.6 : Diff == 1 ? 0.15 : 0.02;
				const double Pl = Dist < 70 ? (Obs.bLoud ? 0.8 : 0.2) : (Obs.bLoud ? 0.15 : 0.85);
				L = Ph * Ps * Pl;
			}
			V *= (float)L; S += V;
		}
		if (S > 0) for (float& V : B) V /= (float)S;
	}
	FSoundObs FSoundHMM::Sample(FVector2D Monster, FVector2D Player, FRandomStream& R)
	{
		FSoundObs O;
		const FVector2D D = Monster - Player;
		const double Dist = D.Size();
		if (R.FRand() > HearProb(Dist) * 0.7) return O;
		O.bHeard = true;
		int32 Sec = SectorOf(D.X, D.Y);
		const float X = R.FRand();
		if (X > 0.6f && X < 0.75f) Sec = (Sec + 1) % 8; else if (X >= 0.75f && X < 0.9f) Sec = (Sec + 7) % 8; else if (X >= 0.9f) Sec = R.RandRange(0, 7);
		O.Sector = Sec;
		O.bLoud = Dist < 70 ? R.FRand() < 0.8f : R.FRand() < 0.15f;
		return O;
	}

	static const double EMIT[4][4] = {
		{ 0.85, 0.12, 0.03, 0.0 }, { 0.3, 0.5, 0.17, 0.03 }, { 0.08, 0.27, 0.5, 0.15 }, { 0.02, 0.08, 0.35, 0.55 } };
	void HealthForward(double Bel[4], int32 Exposure, int32 Symptom)
	{
		const double Adv = Exposure == 0 ? 0.01 : Exposure == 1 ? 0.12 : 0.35;
		const double Heal = Exposure == 0 ? 0.06 : 0.0;
		const double T[4][4] = {
			{ 1 - Adv, Adv, 0, 0 }, { Heal, 1 - Adv - Heal, Adv, 0 }, { 0, Heal * 0.5, 1 - Adv - Heal * 0.5, Adv }, { 0, 0, 0, 1 } };
		double Nb[4] = { 0, 0, 0, 0 };
		for (int32 i = 0; i < 4; i++) for (int32 j = 0; j < 4; j++) Nb[j] += Bel[i] * T[i][j];
		if (Symptom >= 0) for (int32 j = 0; j < 4; j++) Nb[j] *= EMIT[j][Symptom];
		const double S = Nb[0] + Nb[1] + Nb[2] + Nb[3];
		for (int32 j = 0; j < 4; j++) Bel[j] = S > 0 ? Nb[j] / S : 0.25;
	}
	int32 SampleSymptom(int32 State, FRandomStream& R)
	{
		const double X = R.FRand(); double Acc = 0;
		for (int32 k = 0; k < 4; k++) { Acc += EMIT[State][k]; if (X < Acc) return k; }
		return 3;
	}

	// =====================================================================
	// CSP
	// =====================================================================
	static TArray<TArray<int32>> Domains(const TArray<FCSPBrigade>& Br, const TArray<FCSPWell>& Wells, double Hpm, double RepairH)
	{
		TArray<TArray<int32>> D;
		for (const FCSPBrigade& B : Br)
		{
			TArray<int32>& Dom = D.AddDefaulted_GetRef();
			Dom.Add(-1);
			for (const FCSPWell& W : Wells)
			{
				const double Dist = FVector2D::Distance(B.Pos, W.Pos);
				const bool bTool = B.Tools.Contains(W.Tool);
				const bool bDist = Dist <= B.Range;
				const bool bTime = Dist * Hpm + RepairH <= W.TimeLeftH;
				if (bTool && bDist && bTime) Dom.Add(W.Id);
			}
		}
		return D;
	}
	FCSPResult SolveBacktracking(const TArray<FCSPBrigade>& Br, const TArray<FCSPWell>& Wells, double Hpm, double RepairH)
	{
		const double T0 = FPlatformTime::Seconds();
		FCSPResult R; R.Method = TEXT("Backtracking (MRV + forward checking + branch & bound)");
		R.Domains = Domains(Br, Wells, Hpm, RepairH);
		TMap<int32, double> Pr;
		for (const FCSPWell& W : Wells) Pr.Add(W.Id, W.Priority);
		const int32 N = Br.Num();
		TArray<int32> Assign; Assign.Init(INT32_MIN, N);
		TArray<double> BestPossible;
		for (const TArray<int32>& D : R.Domains) { double M = 0; for (int32 V : D) if (V >= 0) M = FMath::Max(M, Pr[V]); BestPossible.Add(M); }
		double BestScore = -1; TArray<int32> BestAssign; TSet<int32> Used;
		TFunction<void(double)> Rec = [&](double Score)
		{
			R.Nodes++;
			int32 Vi = -1, Vs = INT32_MAX;
			for (int32 i = 0; i < N; i++)
			{
				if (Assign[i] != INT32_MIN) continue;
				int32 Size = 0; for (int32 V : R.Domains[i]) if (V < 0 || !Used.Contains(V)) Size++;
				if (Size < Vs) { Vs = Size; Vi = i; }
			}
			if (Vi < 0) { if (Score > BestScore) { BestScore = Score; BestAssign = Assign; } return; }
			double Ub = Score;
			for (int32 i = 0; i < N; i++) if (Assign[i] == INT32_MIN) Ub += BestPossible[i];
			if (Ub <= BestScore) return;
			TArray<int32> Vals;
			for (int32 V : R.Domains[Vi]) if (V < 0 || !Used.Contains(V)) Vals.Add(V);
			Vals.Sort([&](int32 A, int32 B) { return (A >= 0 ? Pr[A] : 0) > (B >= 0 ? Pr[B] : 0); });
			for (int32 V : Vals)
			{
				Assign[Vi] = V;
				if (V >= 0) Used.Add(V);
				Rec(Score + (V >= 0 ? Pr[V] : 0));
				if (V >= 0) Used.Remove(V);
				Assign[Vi] = INT32_MIN;
			}
		};
		Rec(0);
		R.Assign = BestAssign.Num() ? BestAssign : TArray<int32>();
		if (!R.Assign.Num()) R.Assign.Init(-1, N);
		R.Score = FMath::Max(0.0, BestScore);
		R.Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		return R;
	}
	FCSPResult SolveHillClimb(const TArray<FCSPBrigade>& Br, const TArray<FCSPWell>& Wells, double Hpm, double RepairH, int32 Restarts, uint32 Seed)
	{
		const double T0 = FPlatformTime::Seconds();
		FCSPResult R; R.Method = TEXT("Hill climbing (кездейсоқ қайта бастаулармен)");
		R.Domains = Domains(Br, Wells, Hpm, RepairH);
		TMap<int32, double> Pr;
		for (const FCSPWell& W : Wells) Pr.Add(W.Id, W.Priority);
		FRandomStream Rnd(Seed);
		const int32 N = Br.Num();
		auto ScoreOf = [&](const TArray<int32>& A)
		{
			double S = 0; TSet<int32> Seen;
			for (int32 V : A) { if (V < 0) continue; if (Seen.Contains(V)) S -= 100; else { Seen.Add(V); S += Pr[V]; } }
			return S;
		};
		double BestS = -1e9; TArray<int32> Best;
		for (int32 Rs = 0; Rs < Restarts; Rs++)
		{
			TArray<int32> Cur;
			for (const TArray<int32>& D : R.Domains) Cur.Add(D[Rnd.RandRange(0, D.Num() - 1)]);
			double Cs = ScoreOf(Cur);
			for (;;)
			{
				R.Nodes++;
				bool bImp = false; TArray<int32> Bn; double Bs = Cs;
				for (int32 i = 0; i < N; i++) for (int32 V : R.Domains[i])
				{
					if (V == Cur[i]) continue;
					TArray<int32> C2 = Cur; C2[i] = V;
					const double S2 = ScoreOf(C2);
					if (S2 > Bs) { Bs = S2; Bn = C2; bImp = true; }
				}
				if (!bImp) break;
				Cur = Bn; Cs = Bs;
			}
			if (Cs > BestS) { BestS = Cs; Best = Cur; }
		}
		R.Assign = Best; if (!R.Assign.Num()) R.Assign.Init(-1, N);
		R.Score = FMath::Max(0.0, BestS);
		R.Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		return R;
	}
}
