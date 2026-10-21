// Көлік корпустары: қашықтық өрісі (SDF) → surface nets → аймақтар бойынша дәл кесу → Nanite мештер.
// Әр көлік — тегіс, дөңгеленген қаңылтыр пішін (қорап емес): капот, қанаттар, доңғалақ ойықтары, әйнектер, есік жіктері.
// KAPEditorLibrary.cpp ішіне қосылады (FMeshBuild, SaveMesh сонда).
#include "KAP/World/KAPVehicleSpec.h"
#include "Async/ParallelFor.h"

namespace KAPVGen
{
	using V3 = FVector3f;
	using FSdf = TFunction<float(const V3&)>;

	inline float Smin(float A, float B, float K) { if (K <= 0.f) return FMath::Min(A, B); const float H = FMath::Max(K - FMath::Abs(A - B), 0.f) / K; return FMath::Min(A, B) - H * H * K * 0.25f; }
	inline float Smax(float A, float B, float K) { return -Smin(-A, -B, K); }
	/** Шекаралары бойынша дөңгеленген қорап */
	inline float RBox(const V3& P, float X0, float X1, float Y0, float Y1, float Z0, float Z1, float R)
	{
		const V3 C((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, (Z0 + Z1) * 0.5f), H((X1 - X0) * 0.5f, (Y1 - Y0) * 0.5f, (Z1 - Z0) * 0.5f);
		const V3 Q = (P - C).GetAbs() - H + V3(R);
		const V3 M(FMath::Max(Q.X, 0.f), FMath::Max(Q.Y, 0.f), FMath::Max(Q.Z, 0.f));
		return M.Size() + FMath::Min(FMath::Max(Q.X, FMath::Max(Q.Y, Q.Z)), 0.f) - R;
	}
	/** 2D дөңгеленген тіктөртбұрыш (u, v) */
	inline float Rect2(float U, float V, float U0, float U1, float V0, float V1, float R)
	{
		const float Qu = FMath::Abs(U - (U0 + U1) * 0.5f) - ((U1 - U0) * 0.5f - R);
		const float Qv = FMath::Abs(V - (V0 + V1) * 0.5f) - ((V1 - V0) * 0.5f - R);
		return FMath::Sqrt(FMath::Square(FMath::Max(Qu, 0.f)) + FMath::Square(FMath::Max(Qv, 0.f))) + FMath::Min(FMath::Max(Qu, Qv), 0.f) - R;
	}
	/** Жазықтық: P0 нүктесі, сыртқы нормаль N (нормалданады) */
	inline float Plane(const V3& P, const V3& P0, V3 N) { N.Normalize(); return V3::DotProduct(P - P0, N); }
	/** 2D жазықтық (сызық): (U0,V0) арқылы, сыртқы нормаль (Nu,Nv) */
	inline float Line2(float U, float V, float U0, float V0, float Nu, float Nv) { const float L = FMath::Sqrt(Nu * Nu + Nv * Nv); return ((U - U0) * Nu + (V - V0) * Nv) / L; }
	/** Доңғалақ ойығы: Y бойынша цилиндр, |y| > Yin бөлігі */
	inline float Arch(const V3& P, float Xa, float Za, float Ra, float Yin)
	{
		return FMath::Max(FMath::Sqrt(FMath::Square(P.X - Xa) + FMath::Square(P.Z - Za)) - Ra, Yin - FMath::Abs(P.Y));
	}
	/** Жіңішке жік сызығы: тіктөртбұрыштың шекарасы (W — жарты ені) */
	inline float Seam(float U, float V, float U0, float U1, float V0, float V1, float R, float W = 0.45f) { return FMath::Abs(Rect2(U, V, U0, U1, V0, V1, R)) - W; }

	struct FMask { int32 Region; FSdf F; };

	struct FTV { V3 P; };
	inline bool Less(const V3& A, const V3& B) { return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z; }

	struct FOut
	{
		FMeshBuild B;
		TMap<FVector3f, int32> Map;
	};

	static V3 Grad(const FSdf& F, const V3& P, float E)
	{
		return V3(F(P + V3(E, 0, 0)) - F(P - V3(E, 0, 0)), F(P + V3(0, E, 0)) - F(P - V3(0, E, 0)), F(P + V3(0, 0, E)) - F(P - V3(0, 0, E))) / (2.f * E);
	}

	static int32 AddV(FOut& O, const FSdf& F, const V3& P, float E)
	{
		if (const int32* I = O.Map.Find(P)) return *I;
		V3 N = Grad(F, P, E).GetSafeNormal();
		if (N.IsNearlyZero()) N = V3(0, 0, 1);
		const int32 I = O.B.P.Num();
		O.B.P.Add(P); O.B.N.Add(N);
		const V3 A = N.GetAbs();
		O.B.UV.Add(A.Z >= A.X && A.Z >= A.Y ? FVector2f(P.X, P.Y) * 0.01f : A.Y >= A.X ? FVector2f(P.X, P.Z) * 0.01f : FVector2f(P.Y, P.Z) * 0.01f);
		O.Map.Add(P, I);
		return I;
	}

	/** Үшбұрышты маскалар тізбегі бойынша кесу: бірінші теріс маска — аймақ; ешқайсысы болмаса — 0 (бояу) */
	static void Clip(const V3* T, int32 N, int32 Mi, const TArray<FMask>& Masks, TArray<FOut>& Outs, const FSdf& F, float E)
	{
		if (Mi >= Masks.Num())
		{
			for (int32 k = 1; k + 1 < N; k++) { FMeshBuild& B = Outs[0].B; const int32 a = AddV(Outs[0], F, T[0], E), b = AddV(Outs[0], F, T[k], E), c = AddV(Outs[0], F, T[k + 1], E); B.Idx.Append({ a, b, c }); }
			return;
		}
		float S[8]; bool bAnyIn = false, bAnyOut = false;
		for (int32 k = 0; k < N; k++) { S[k] = Masks[Mi].F(T[k]); (S[k] < 0.f ? bAnyIn : bAnyOut) = true; }
		const int32 R = Masks[Mi].Region;
		if (!bAnyIn) { Clip(T, N, Mi + 1, Masks, Outs, F, E); return; }
		if (!bAnyOut)
		{
			for (int32 k = 1; k + 1 < N; k++) { FMeshBuild& B = Outs[R].B; const int32 a = AddV(Outs[R], F, T[0], E), b = AddV(Outs[R], F, T[k], E), c = AddV(Outs[R], F, T[k + 1], E); B.Idx.Append({ a, b, c }); }
			return;
		}
		V3 In[8], Ou[8]; int32 Ni = 0, No = 0;
		for (int32 k = 0; k < N; k++)
		{
			const int32 k2 = (k + 1) % N;
			if (S[k] < 0.f) In[Ni++] = T[k]; else Ou[No++] = T[k];
			if ((S[k] < 0.f) != (S[k2] < 0.f))
			{
				// кесу нүктесі — ұштардың тұрақты ретімен (көрші үшбұрыштарда бит-дәл бірдей)
				const bool bSw = Less(T[k2], T[k]);
				const V3& A = bSw ? T[k2] : T[k]; const V3& Bv = bSw ? T[k] : T[k2];
				const float Sa = bSw ? S[k2] : S[k], Sb = bSw ? S[k] : S[k2];
				const float t = FMath::Clamp(Sa / (Sa - Sb), 0.f, 1.f);
				const V3 X = A + (Bv - A) * t;
				if (Ni < 8) In[Ni++] = X;
				if (No < 8) Ou[No++] = X;
			}
		}
		if (Ni >= 3) for (int32 k = 1; k + 1 < Ni; k++) { FMeshBuild& B = Outs[R].B; const int32 a = AddV(Outs[R], F, In[0], E), b = AddV(Outs[R], F, In[k], E), c = AddV(Outs[R], F, In[k + 1], E); B.Idx.Append({ a, b, c }); }
		if (No >= 3) Clip(Ou, No, Mi + 1, Masks, Outs, F, E);
	}

	/** SDF → меш(тер): SM_KAP_Veh_<Name>_<аймақ> */
	static int32 BuildShell(const FString& Name, const FSdf& F, const TArray<FMask>& Masks, V3 Lo, V3 Hi, float H)
	{
		const int32 Nx = FMath::CeilToInt((Hi.X - Lo.X) / H) + 1, Ny = FMath::CeilToInt((Hi.Y - Lo.Y) / H) + 1, Nz = FMath::CeilToInt((Hi.Z - Lo.Z) / H) + 1;
		auto Id = [&](int32 i, int32 j, int32 k) { return (k * Ny + j) * Nx + i; };
		auto Pt = [&](int32 i, int32 j, int32 k) { return Lo + V3(i * H, j * H, k * H); };
		TArray<float> G; G.SetNumUninitialized(Nx * Ny * Nz);
		ParallelFor(Nz, [&](int32 k) { for (int32 j = 0; j < Ny; j++) for (int32 i = 0; i < Nx; i++) G[Id(i, j, k)] = F(Pt(i, j, k)); });
		// торды шекарада «сыртқа» мәжбүрлеу (тұйық бет)
		for (int32 k = 0; k < Nz; k++) for (int32 j = 0; j < Ny; j++) for (int32 i = 0; i < Nx; i++)
			if (i == 0 || j == 0 || k == 0 || i == Nx - 1 || j == Ny - 1 || k == Nz - 1) G[Id(i, j, k)] = FMath::Max(G[Id(i, j, k)], 0.01f);
		const int32 Cx = Nx - 1, Cy = Ny - 1, Cz = Nz - 1;
		auto CId = [&](int32 i, int32 j, int32 k) { return (k * Cy + j) * Cx + i; };
		TArray<int32> CellV; CellV.Init(-1, Cx * Cy * Cz);
		TArray<V3> VP;
		static const int32 Ed[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
		for (int32 k = 0; k < Cz; k++) for (int32 j = 0; j < Cy; j++) for (int32 i = 0; i < Cx; i++)
		{
			float C[8]; int32 Neg = 0;
			for (int32 c = 0; c < 8; c++) { C[c] = G[Id(i + (c & 1), j + ((c >> 1) & 1), k + ((c >> 2) & 1))]; Neg += C[c] < 0.f; }
			if (Neg == 0 || Neg == 8) continue;
			V3 Acc(0); int32 Cnt = 0;
			for (int32 e = 0; e < 12; e++)
			{
				const float A = C[Ed[e][0]], B = C[Ed[e][1]];
				if ((A < 0.f) == (B < 0.f)) continue;
				const float t = A / (A - B);
				const V3 Pa((float)(Ed[e][0] & 1), (float)((Ed[e][0] >> 1) & 1), (float)((Ed[e][0] >> 2) & 1)), Pb((float)(Ed[e][1] & 1), (float)((Ed[e][1] >> 1) & 1), (float)((Ed[e][1] >> 2) & 1));
				Acc += Pa + (Pb - Pa) * t; Cnt++;
			}
			CellV[CId(i, j, k)] = VP.Num();
			VP.Add(Pt(i, j, k) + Acc / (float)Cnt * H);
		}
		// төбелерді бетке жобалау (Ньютон, ұяшықтан шықпай)
		const float E = H * 0.3f;
		ParallelFor(VP.Num(), [&](int32 v)
		{
			const V3 P0 = VP[v]; V3 P = P0;
			for (int32 it = 0; it < 4; it++)
			{
				const float f = F(P); const V3 Gr = Grad(F, P, E); const float GG = Gr.SizeSquared();
				if (GG < 1e-6f) break;
				P -= Gr * (f / GG);
			}
			const V3 D = P - P0;
			VP[v] = D.GetAbsMax() > H * 0.9f ? P0 + D.GetClampedToMaxSize(H * 0.9f) : P;
		});
		// төртбұрыштар: таңбасы ауысқан әр тор қырына — оны қоршаған 4 ұяшық
		TArray<FIntVector> Tris; TArray<V3> TriOut;
		auto Quad = [&](int32 a, int32 b, int32 c, int32 d, const V3& Out)
		{
			if (a < 0 || b < 0 || c < 0 || d < 0) return;
			const float D1 = (VP[a] - VP[c]).SizeSquared(), D2 = (VP[b] - VP[d]).SizeSquared();
			auto Tri = [&](int32 x, int32 y, int32 z)
			{
				// Rock конвенциясы: Cross(P2-P0, P1-P0) сыртқа
				if (V3::DotProduct(V3::CrossProduct(VP[z] - VP[x], VP[y] - VP[x]), Out) < 0) Swap(y, z);
				Tris.Add(FIntVector(x, y, z));
			};
			if (D1 < D2) { Tri(a, b, c); Tri(a, c, d); } else { Tri(a, b, d); Tri(b, c, d); }
		};
		for (int32 k = 1; k < Nz - 1; k++) for (int32 j = 1; j < Ny - 1; j++) for (int32 i = 1; i < Nx - 1; i++)
		{
			const float g0 = G[Id(i, j, k)];
			const bool In0 = g0 < 0.f;
			if (i < Cx && (G[Id(i + 1, j, k)] < 0.f) != In0) Quad(CellV[CId(i, j - 1, k - 1)], CellV[CId(i, j, k - 1)], CellV[CId(i, j, k)], CellV[CId(i, j - 1, k)], V3(In0 ? 1.f : -1.f, 0, 0));
			if (j < Cy && (G[Id(i, j + 1, k)] < 0.f) != In0) Quad(CellV[CId(i - 1, j, k - 1)], CellV[CId(i, j, k - 1)], CellV[CId(i, j, k)], CellV[CId(i - 1, j, k)], V3(0, In0 ? 1.f : -1.f, 0));
			if (k < Cz && (G[Id(i, j, k + 1)] < 0.f) != In0) Quad(CellV[CId(i - 1, j - 1, k)], CellV[CId(i, j - 1, k)], CellV[CId(i, j, k)], CellV[CId(i - 1, j, k)], V3(0, 0, In0 ? 1.f : -1.f));
		}
		// аймақтар бойынша кесу
		TArray<FOut> Outs; Outs.SetNum(KAPVehSpec::NumRegions);
		for (const FIntVector& T : Tris) { const V3 Tv[3] = { VP[T.X], VP[T.Y], VP[T.Z] }; Clip(Tv, 3, 0, Masks, Outs, F, E); }
		int32 Total = 0;
		for (int32 r = 0; r < Outs.Num(); r++)
		{
			const FString N = FString::Printf(TEXT("SM_KAP_Veh_%s_%d"), *Name, r);
			FMeshBuild& B = Outs[r].B;
			UE_LOG(LogTemp, Display, TEXT("KAPVEH %s: %d verts, %d tris"), *N, B.P.Num(), B.Idx.Num() / 3);
			if (B.Idx.Num() < 3) continue;
			Total += B.Idx.Num() / 3;
			SaveMesh(N, B, true, false);
		}
		return Total;
	}

	/** Бағытталған қорап (әр бетке өз нормалі): C — центр, X/Y/Z — осьтер, H — жарты өлшем */
	static void AddBox(FMeshBuild& B, const V3& C, const V3& X, const V3& Y, const V3& Z, const V3& H)
	{
		const V3 Ax[3] = { X, Y, Z };
		for (int32 a = 0; a < 3; a++) for (int32 sg = -1; sg <= 1; sg += 2)
		{
			const V3 N = Ax[a] * (float)sg, U = Ax[(a + 1) % 3], W = Ax[(a + 2) % 3];
			const float Hn = H[a], Hu = H[(a + 1) % 3], Hw = H[(a + 2) % 3];
			const int32 Base = B.P.Num();
			for (int32 c = 0; c < 4; c++)
			{
				const float Su = (c == 1 || c == 2) ? 1.f : -1.f, Sw = c >= 2 ? 1.f : -1.f;
				B.P.Add(C + N * Hn + U * (Su * Hu) + W * (Sw * Hw)); B.N.Add(N); B.UV.Add(FVector2f(Su, Sw) * 0.5f);
			}
			B.Idx.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}
	}

	/** Айналу денесі (Y осі): профиль (y, ρ) — сыртқы жағы солға қарай қозғалғанда сол жақта */
	static void Lathe(FMeshBuild& B, const TArray<FVector2f>& Prof, int32 Seg)
	{
		const int32 Base = B.P.Num(), Np = Prof.Num();
		for (int32 s = 0; s <= Seg; s++)
		{
			const float A = s * 2.f * PI / Seg; const float Ca = FMath::Cos(A), Sa = FMath::Sin(A);
			for (int32 p = 0; p < Np; p++)
			{
				const FVector2f T = (Prof[FMath::Min(p + 1, Np - 1)] - Prof[FMath::Max(p - 1, 0)]).GetSafeNormal();
				const FVector2f N2(-T.Y, T.X); // (ny, nρ)
				B.P.Add(V3(Ca * Prof[p].Y, Prof[p].X, Sa * Prof[p].Y));
				B.N.Add(V3(Ca * N2.Y, N2.X, Sa * N2.Y).GetSafeNormal());
				B.UV.Add(FVector2f(s / (float)Seg * 8.f, p / (float)(Np - 1)));
			}
		}
		for (int32 s = 0; s < Seg; s++) for (int32 p = 0; p + 1 < Np; p++)
		{
			const int32 a = Base + s * Np + p, b = a + 1, c = a + Np, d = c + 1;
			B.Idx.Append({ a, b, c, b, d, c });
		}
	}
}

bool UKAPEditorLibrary::CreateVehicleMeshes()
{
	using namespace KAPVGen;
	using KAPVehSpec::Paint; using KAPVehSpec::Second; using KAPVehSpec::Glass; using KAPVehSpec::Dark; using KAPVehSpec::Trim;
	const double T0 = FPlatformTime::Seconds();

	// ---------------- дөңгелек: шина (иықтары дөңгеленген, бойлық ойықтары бар) және диск ----------------
	{
		// бірлік шина: сыртқы радиус 50, ені 100 (Y), ішкі (дискке) радиус 31
		TArray<FVector2f> P;
		const float Half[] = { -40, -46, -49.5f, -50, -48.5f, -45, -40, -34 };
		const float Rho[] = { 31, 34, 38, 42, 45.4f, 47.2f, 48.0f, 48.3f };
		for (int32 i = 0; i < 8; i++) P.Add(FVector2f(Half[i], Rho[i]));
		const float Grv[][2] = { { -16, 48.3f }, { -15, 47.2f }, { -9, 47.2f }, { -8, 48.3f }, { 8, 48.3f }, { 9, 47.2f }, { 15, 47.2f }, { 16, 48.3f } };
		for (auto& G2 : Grv) P.Add(FVector2f(G2[0], G2[1]));
		for (int32 i = 7; i >= 0; i--) P.Add(FVector2f(-Half[i], Rho[i]));
		FMeshBuild B; Lathe(B, P, 64);
		// протектор бұдырлары: екі қатар, жарты қадамға ығысқан, «шырша» бұрышымен
		const int32 NL = 22;
		for (int32 k = 0; k < NL; k++) for (int32 s = 0; s < 2; s++)
		{
			const float A = (k + s * 0.5f) / NL * 2.f * PI, Ph = FMath::DegreesToRadians(s ? 18.f : -18.f);
			const V3 Rd(FMath::Cos(A), 0, FMath::Sin(A)), Tg(-FMath::Sin(A), 0, FMath::Cos(A)), Ay(0, 1, 0);
			const V3 T2 = Tg * FMath::Cos(Ph) + Ay * FMath::Sin(Ph), Y2 = Ay * FMath::Cos(Ph) - Tg * FMath::Sin(Ph);
			AddBox(B, Rd * 48.95f + Ay * (s ? 20.f : -20.f), T2, Y2, Rd, V3(3.2f, 14.f, 1.05f));
		}
		B.FixWinding();
		SaveMesh(TEXT("SM_KAP_Tire"), B, false, false);
	}
	{
		// бірлік диск (шинамен бірге масштабталады): сыртқы беті +Y, ойыңқы «тарелка», күпшек
		const float Pr[][2] = { { -36, 31.5f }, { -36, 28 }, { -32, 27 }, { 24, 27 }, { 30, 28.5f }, { 34, 31.5f }, { 36, 30.5f }, { 33, 27.5f }, { 29, 25 }, { 24, 21 }, { 22, 16 }, { 23, 12 }, { 27, 10.5f }, { 30, 8 }, { 31, 4 }, { 31, 0.01f } };
		TArray<FVector2f> P; for (auto& X : Pr) P.Add(FVector2f(X[0], X[1]));
		FMeshBuild B; Lathe(B, P, 48);
		// ішкі жағы (тұйық диск)
		TArray<FVector2f> In = { FVector2f(-36, 0.01f), FVector2f(-36, 27) };
		Lathe(B, In, 24);
		// бұрандалар (5)
		for (int32 k = 0; k < 5; k++) { const float A = k / 5.f * 2.f * PI; const V3 Rd(FMath::Cos(A), 0, FMath::Sin(A)), Tg(-FMath::Sin(A), 0, FMath::Cos(A)); AddBox(B, Rd * 7.6f + V3(0, 30.6f, 0), Rd, V3(0, 1, 0), Tg, V3(1.3f, 1.1f, 1.3f)); }
		B.FixWinding();
		SaveMesh(TEXT("SM_KAP_Rim"), B, false, false);
	}

	auto Sp = [](const TCHAR* Id) { return *KAPVehSpec::Find(Id); };
	int32 Tot = 0;

	// ================= УАЗ-469 «козлик» =================
	{
		const FKAPVehSpec S = Sp(TEXT("uaz469"));
		const float Xa = S.WB * 0.5f, Zw = S.R + 2.f, Ra = S.R + 6.f, Yin = S.Track - S.TireW * 0.5f - 5.f;
		FSdf F = [=](const V3& P) -> float
		{
			const float Ya = FMath::Abs(P.Y); const V3 Q(P.X, Ya, P.Z);
			float Tub = RBox(P, -195, 62, -85, 85, 46, 118, 5);                                      // кузов «ваннасы»
			float Hood = RBox(P, 48, 186, -64, 64, 70, 122, 5);
			Hood = Smax(Hood, Plane(P, V3(186, 0, 116.5f), V3(0.042f, 0, 1)), 4);                    // капот алға қарай сәл төмендейді
			float Fen = RBox(Q, 50, 191, 57, 89, 64, 114.5f, 5);                                       // жалпақ алдыңғы қанаттар
			Fen = Smax(Fen, Plane(P, V3(187, 0, 76), V3(1, 0, -1)), 9);                                // қанаттың алдыңғы-төменгі иілісі
			Fen = Smax(Fen, Plane(P, V3(191, 0, 112), V3(1, 0, 0.55f)), 4);
			float Gr = RBox(P, 150, 192, -64, 64, 60, 117, 4);                                         // радиатор панелі
			float Ws = FMath::Max(FMath::Max(Plane(P, V3(57, 0, 118), V3(1, 0, 0.12f)), Plane(P, V3(51, 0, 118), V3(-1, 0, -0.12f))), FMath::Max(Ya - 80.f, FMath::Max(116.f - P.Z, P.Z - 187.f)));
			float Tent = RBox(P, -194, 50, -84, 84, 112, 201, 9);                                       // брезент шатыр
			Tent = Smax(Tent, Plane(Q, V3(0, 84, 150), V3(0, 1, 0.1f)), 6);
			float B = Smin(Tub, Hood, 4.f); B = Smin(B, Fen, 4.f); B = Smin(B, Gr, 3.f); B = Smin(B, Ws, 1.5f); B = Smin(B, Tent, 2.f);
			B = Smax(B, -Arch(P, Xa, Zw, Ra, Yin), 3.f);
			B = Smax(B, -Arch(P, -Xa, Zw, Ra, Yin), 3.f);
			return B;
		};
		TArray<FMask> M;
		// әйнектер: алдыңғы, брезенттегі мөлдір терезелер, артқы
		M.Add({ Glass, [](const V3& P) { return FMath::Max(Rect2(P.Y, P.Z, -73, 73, 125, 181, 3), FMath::Abs(Plane(P, V3(57, 0, 118), V3(1, 0, 0.12f))) - 2.5f); } });
		M.Add({ Glass, [](const V3& P) { const float Ya = FMath::Abs(P.Y); return FMath::Max(FMath::Min(FMath::Min(Rect2(P.X, P.Z, 6, 44, 132, 184, 6), Rect2(P.X, P.Z, -74, -8, 132, 184, 6)), Rect2(P.X, P.Z, -176, -96, 132, 184, 6)), 78.f - Ya); } });
		M.Add({ Glass, [](const V3& P) { return FMath::Max(Rect2(P.Y, P.Z, -46, 46, 140, 186, 6), P.X + 188.f); } });
		// радиатор саңылаулары (7 тік), доңғалақ ойығының іші, есік жіктері, капот жігі, асты
		M.Add({ Dark, [](const V3& P) { float D = 1e9f; for (int32 i = 0; i < 7; i++) { const float Y = -42.f + i * 14.f; D = FMath::Min(D, Rect2(P.Y, P.Z, Y - 3.2f, Y + 3.2f, 72, 108, 1.5f)); } return FMath::Max(D, 188.f - P.X); } });
		M.Add({ Dark, [Xa, Ra](const V3& P) { const float Ya = FMath::Abs(P.Y); const float Dw = FMath::Min(FMath::Sqrt(FMath::Square(P.X - Xa) + FMath::Square(P.Z - 41.f)), FMath::Sqrt(FMath::Square(P.X + Xa) + FMath::Square(P.Z - 41.f))); return FMath::Max(Dw - (Ra + 2.5f), FMath::Max(50.f - Ya, Ya - 88.f)); } });
		M.Add({ Dark, [](const V3& P) { const float Ya = FMath::Abs(P.Y); float D = 1e9f; for (float Xs : { 47.f, -3.f, -82.f }) D = FMath::Min(D, FMath::Abs(P.X - Xs) - 0.45f); return FMath::Max(D, FMath::Max(80.f - Ya, FMath::Max(50.f - P.Z, P.Z - 117.f))); } });
		M.Add({ Dark, [](const V3& P) { return FMath::Max(FMath::Abs(FMath::Abs(P.Y) - 63.f) - 0.45f, FMath::Max(116.f - P.Z, 54.f - P.X)); } });
		M.Add({ Dark, [](const V3& P) { return P.Z - 49.f; } });
		// брезент (екінші материал)
		M.Add({ Second, [](const V3& P) { return FMath::Max(119.5f - P.Z, P.X - 49.f); } });
		Tot += BuildShell(TEXT("uaz469"), F, M, V3(-205, -96, 36), V3(200, 96, 210), 2.2f);
	}

	// ================= УАЗ-452 «Буханка» =================
	{
		const FKAPVehSpec S = Sp(TEXT("bukhanka"));
		const float Xa = S.WB * 0.5f, Zw = S.R + 2.f, Ra = S.R + 7.f, Yin = S.Track - S.TireW * 0.5f - 4.f;
		FSdf F = [=](const V3& P) -> float
		{
			const float Ya = FMath::Abs(P.Y); const V3 Q(P.X, Ya, P.Z);
			float B = RBox(P, -222, 216, -97, 97, 40, 206, 12);
			B = FMath::Max(B, Rect2(P.X, P.Y, -222, 216, -97, 97, P.X > 0 ? 34.f : 22.f));            // жоспардағы «нан» бұрыштары
			B = FMath::Max(B, Rect2(P.Y, P.Z, -97, 97, -40, 206, 26));                                  // шатыр жиектері
			B = FMath::Max(B, Rect2(P.X, P.Z, -222, 216, -40, 206, P.X > 0 ? 30.f : 18.f));
			B = Smax(B, Plane(Q, V3(0, 97, 112), V3(0, 1, 0.075f)), 4);                                 // белдеуден жоғары ішке көлбеу
			B = Smax(B, Plane(P, V3(216, 0, 116), V3(1, 0, 0.16f)), 8);                                  // алдыңғы әйнек көлбеуі
			B = Smax(B, Plane(P, V3(216, 0, 56), V3(1, 0, -0.5f)), 10);                                  // алдыңғы төменгі иіліс
			B = Smax(B, -Arch(P, Xa, Zw, Ra, Yin), 3.f);
			B = Smax(B, -Arch(P, -Xa, Zw, Ra, Yin), 3.f);
			return B;
		};
		TArray<FMask> M;
		// белдеу молдингі (хром)
		M.Add({ Trim, [](const V3& P) { return FMath::Abs(P.Z - 111.f) - 1.3f; } });
		// әйнектер
		M.Add({ Glass, [](const V3& P) { return FMath::Max(Rect2(FMath::Abs(P.Y), P.Z, 3.5f, 84, 126, 192, 8), 194.f - P.X); } });
		M.Add({ Glass, [](const V3& P) { const float Ya = FMath::Abs(P.Y); float D = Rect2(P.X, P.Z, 128, 188, 124, 190, 7);
			for (float X0 : { -204.f, -130.f, -56.f, 18.f }) D = FMath::Min(D, Rect2(P.X, P.Z, X0, X0 + 64.f, 126, 188, 7));
			return FMath::Max(D, 85.f - Ya); } });
		M.Add({ Glass, [](const V3& P) { return FMath::Max(Rect2(FMath::Abs(P.Y), P.Z, 8, 76, 128, 186, 7), P.X + 214.f); } });
		// тор саңылаулары, есік жіктері, артқы есіктер, доңғалақ ойықтары, асты
		M.Add({ Dark, [](const V3& P) { float D = 1e9f; for (int32 i = 0; i < 5; i++) { const float Z = 66.f + i * 8.f; D = FMath::Min(D, Rect2(P.Y, P.Z, -32, 32, Z - 2.f, Z + 2.f, 1.f)); } return FMath::Max(D, 206.f - P.X); } });
		M.Add({ Dark, [](const V3& P) { const float Ya = FMath::Abs(P.Y); float D = Seam(P.X, P.Z, 122, 194, 40, 197, 6);
			if (P.Y > 0) D = FMath::Min(D, Seam(P.X, P.Z, 12, 90, 36, 195, 6));
			return FMath::Max(D, 84.f - Ya); } });
		M.Add({ Dark, [](const V3& P) { return FMath::Max(FMath::Min(FMath::Abs(P.Y) - 0.45f, Seam(P.Y, P.Z, -84, 84, 36, 196, 6)), P.X + 217.f); } });
		M.Add({ Dark, [Xa, Ra](const V3& P) { const float Ya = FMath::Abs(P.Y); const float Dw = FMath::Min(FMath::Sqrt(FMath::Square(P.X - Xa) + FMath::Square(P.Z - 41.f)), FMath::Sqrt(FMath::Square(P.X + Xa) + FMath::Square(P.Z - 41.f))); return FMath::Max(Dw - (Ra + 2.5f), FMath::Max(50.f - Ya, Ya - 96.f)); } });
		M.Add({ Dark, [](const V3& P) { return P.Z - 43.f; } });
		// жоғарғы бөлік — екінші түс (ақшыл)
		M.Add({ Second, [](const V3& P) { return 112.f - P.Z; } });
		Tot += BuildShell(TEXT("bukhanka"), F, M, V3(-228, -102, 24), V3(222, 102, 212), 2.3f);
	}

	// ================= ВАЗ-2121 «Нива» =================
	{
		const FKAPVehSpec S = Sp(TEXT("niva"));
		const float Xa = S.WB * 0.5f, Zw = S.R + 2.f, Ra = S.R + 6.f, Yin = S.Track - S.TireW * 0.5f - 4.f;
		const V3 WsP(58, 0, 101), WsN(60, 0, 74), HtP(-189, 0, 103), HtN(-58, 0, 13);
		FSdf F = [=](const V3& P) -> float
		{
			const float Ya = FMath::Abs(P.Y); const V3 Q(P.X, Ya, P.Z);
			float Lo = RBox(P, -190, 184, -84, 84, 28, 102, 6);
			Lo = FMath::Max(Lo, Rect2(P.X, P.Y, -190, 184, -84, 84, P.X > 0 ? 14.f : 9.f));
			Lo = Smax(Lo, Plane(P, V3(184, 0, 94), V3(0.062f, 0, 1)), 4);
			Lo = Smax(Lo, Plane(P, V3(184, 0, 46), V3(1, 0, -0.6f)), 5);
			float Cab = RBox(P, -189, 70, -81, 81, 90, 163, 7);
			Cab = Smax(Cab, Plane(P, WsP, WsN), 6);
			Cab = Smax(Cab, Plane(P, HtP, HtN), 6);
			Cab = Smax(Cab, Plane(Q, V3(0, 81, 102), V3(0, 61, 10)), 5);
			float B = Smin(Lo, Cab, 2.5f);
			// доңғалақ ойығының жиегі (қалқа)
			for (float Xw : { Xa, -Xa })
			{
				const float Dr = FMath::Sqrt(FMath::Square(P.X - Xw) + FMath::Square(P.Z - Zw));
				B = Smin(B, FMath::Max(FMath::Abs(Dr - (Ra + 2.f)) - 3.5f, FMath::Max(FMath::Abs(Ya - 82.f) - 4.f, Zw - 4.f - P.Z)), 2.f);
			}
			B = Smax(B, -Arch(P, Xa, Zw, Ra, Yin), 2.5f);
			B = Smax(B, -Arch(P, -Xa, Zw, Ra, Yin), 2.5f);
			return B;
		};
		TArray<FMask> M;
		M.Add({ Glass, [=](const V3& P) { return FMath::Max(FMath::Abs(Plane(P, WsP, WsN)) - 2.5f, Rect2(FMath::Abs(P.Y), P.Z, -1, 66, 105, 158, 6)); } });
		M.Add({ Glass, [=](const V3& P) { const float Ya = FMath::Abs(P.Y);
			const float Fr = FMath::Max(Rect2(P.X, P.Z, -54, 60, 106, 156, 5), Plane(P, WsP, WsN) + 7.f);
			const float Rr = FMath::Max(Rect2(P.X, P.Z, -186, -64, 106, 156, 5), Plane(P, HtP, HtN) + 7.f);
			return FMath::Max(FMath::Min(Fr, Rr), 66.f - Ya); } });
		M.Add({ Glass, [=](const V3& P) { return FMath::Max(FMath::Abs(Plane(P, HtP, HtN)) - 2.5f, Rect2(FMath::Abs(P.Y), P.Z, -1, 60, 108, 156, 6)); } });
		M.Add({ Dark, [](const V3& P) { return FMath::Max(Rect2(P.Y, P.Z, -44, 44, 66, 88, 2), 178.f - P.X); } });
		M.Add({ Dark, [](const V3& P) { const float Ya = FMath::Abs(P.Y); float D = FMath::Min(FMath::Abs(P.X - 54.f), FMath::Abs(P.X + 60.f)) - 0.45f; return FMath::Max(D, FMath::Max(70.f - Ya, FMath::Max(34.f - P.Z, P.Z - 160.f))); } });
		M.Add({ Dark, [](const V3& P) { return FMath::Max(FMath::Abs(P.Z - 70.f) - 2.2f, 79.f - FMath::Abs(P.Y)); } });   // бүйір молдинг
		M.Add({ Dark, [Xa, Ra](const V3& P) { const float Ya = FMath::Abs(P.Y); const float Dw = FMath::Min(FMath::Sqrt(FMath::Square(P.X - Xa) + FMath::Square(P.Z - 36.f)), FMath::Sqrt(FMath::Square(P.X + Xa) + FMath::Square(P.Z - 36.f))); return FMath::Max(Dw - (Ra + 6.f), FMath::Max(50.f - Ya, Ya - 87.f)); } });
		M.Add({ Dark, [](const V3& P) { return P.Z - 31.f; } });
		Tot += BuildShell(TEXT("niva"), F, M, V3(-198, -92, 22), V3(192, 92, 170), 2.0f);
	}

	// ================= КамАЗ кабинасы (су тасушымен ортақ) =================
	{
		const FKAPVehSpec S = Sp(TEXT("kamaz"));
		const float Xa = S.WB * 0.5f, Zw = S.R, Ra = S.R + 10.f, Yin = S.Track - S.TireW * 0.5f - 8.f;
		FSdf F = [=](const V3& P) -> float
		{
			const float Ya = FMath::Abs(P.Y);
			float Cab = RBox(P, 86, 282, -124, 124, 118, 312, 14);
			Cab = Smax(Cab, Plane(P, V3(282, 0, 205), V3(1, 0, 0.07f)), 10);
			// алдыңғы қанат (қара): доңғалақ үстіндегі доға
			const float Dr = FMath::Sqrt(FMath::Square(P.X - Xa) + FMath::Square(P.Z - Zw));
			const float Fen = FMath::Max(FMath::Abs(Dr - (Ra + 3.f)) - 2.5f, FMath::Max(FMath::Abs(Ya - 103.f) - 19.f, Zw + 4.f - P.Z));
			float B = Smin(Cab, Fen, 3.f);
			B = Smax(B, -Arch(P, Xa, Zw, Ra, Yin), 3.f);
			return B;
		};
		TArray<FMask> M;
		M.Add({ Glass, [](const V3& P) { return FMath::Max(Rect2(FMath::Abs(P.Y), P.Z, 3.5f, 114, 208, 292, 8), 274.f - P.X); } });
		M.Add({ Glass, [](const V3& P) { return FMath::Max(Rect2(P.X, P.Z, 172, 268, 210, 290, 6), 116.f - FMath::Abs(P.Y)); } });
		M.Add({ Glass, [](const V3& P) { return FMath::Max(Rect2(P.Y, P.Z, -50, 50, 240, 290, 6), P.X - 90.f); } });
		M.Add({ Dark, [](const V3& P) { float D = 1e9f; for (int32 i = 0; i < 5; i++) { const float Z = 146.f + i * 11.f; D = FMath::Min(D, Rect2(P.Y, P.Z, -92, 92, Z - 3.f, Z + 3.f, 1.5f)); } return FMath::Max(D, 272.f - P.X); } });
		M.Add({ Dark, [](const V3& P) { return FMath::Max(Seam(P.X, P.Z, 162, 276, 122, 300, 6), 118.f - FMath::Abs(P.Y)); } });
		M.Add({ Dark, [](const V3& P) { return P.Z - 121.f; } });
		Tot += BuildShell(TEXT("kamaz"), F, M, V3(70, -132, 40), V3(296, 132, 322), 3.0f);
	}
	// ================= су цистернасы =================
	{
		FSdf F = [](const V3& P) -> float
		{
			float T = RBox(P, -330, 70, -108, 108, 128, 282, 62);
			for (float Xr : { -262.f, -130.f, 2.f }) T = Smin(T, RBox(P, Xr - 3.f, Xr + 3.f, -111, 111, 125, 285, 64), 1.5f);
			T = Smin(T, FMath::Max(FMath::Sqrt(FMath::Square(P.X + 130.f) + FMath::Square(P.Y)) - 24.f, FMath::Max(268.f - P.Z, P.Z - 292.f)), 3.f);
			return T;
		};
		TArray<FMask> M;
		M.Add({ Trim, [](const V3& P) { float D = 1e9f; for (float Xr : { -262.f, -130.f, 2.f }) D = FMath::Min(D, FMath::Abs(P.X - Xr) - 3.6f); return FMath::Min(D, 284.5f - P.Z); } });
		Tot += BuildShell(TEXT("watertank"), F, M, V3(-342, -122, 114), V3(82, 122, 300), 3.0f);
	}

	// ================= БТР-80 тектес бронетранспортер =================
	{
		const FKAPVehSpec S = Sp(TEXT("btr"));
		const float Ra = S.R + 7.f, Yin = S.Track - S.TireW * 0.5f - 5.f;
		const float Ax[4] = { S.WB * 0.5f, S.WB * 0.5f - 135.f, -S.WB * 0.5f, -S.WB * 0.5f - 135.f };
		const V3 GlP(311, 0, 120), GlN(84, 0, 115);
		FSdf F = [=](const V3& P) -> float
		{
			const float Ya = FMath::Abs(P.Y); const V3 Q(P.X, Ya, P.Z);
			float H = RBox(P, -446, 311, -146, 146, 46, 206, 3);
			H = Smax(H, Plane(Q, V3(0, 146, 150), V3(0, 0.94f, 0.34f)), 4);
			H = Smax(H, Plane(Q, V3(0, 146, 150), V3(0, 65, -46)), 4);
			H = Smax(H, Plane(P, GlP, GlN), 4);
			H = Smax(H, Plane(P, V3(311, 0, 120), V3(74, 0, -49)), 4);
			H = Smax(H, Line2(P.X, Ya, 311, 50, 96, 121), 4);
			H = Smax(H, Plane(P, V3(-446, 0, 170), V3(-36, 0, 26)), 4);
			H = Smax(H, Plane(P, V3(-446, 0, 120), V3(-74, 0, -26)), 4);
			H = Smax(H, Line2(P.X, Ya, -446, 110, -36, 46), 4);
			for (int32 i = 0; i < 4; i++) H = Smax(H, -Arch(P, Ax[i], S.R, Ra, Yin), 3.f);
			// мұнара: кесік конус
			const float Rt = FMath::Lerp(62.f, 47.f, FMath::Clamp((P.Z - 200.f) / 40.f, 0.f, 1.f));
			const float Tur = FMath::Max(FMath::Sqrt(FMath::Square(P.X - 80.f) + FMath::Square(P.Y)) - Rt, FMath::Max(196.f - P.Z, P.Z - 240.f));
			H = Smin(H, Tur, 3.f);
			return H;
		};
		TArray<FMask> M;
		M.Add({ Glass, [=](const V3& P) { return FMath::Max(FMath::Abs(Plane(P, GlP, GlN)) - 3.f, Rect2(FMath::Abs(P.Y), P.Z, 12, 62, 176, 196, 3)); } });
		M.Add({ Dark, [](const V3& P) { float D = 1e9f; for (float Xp : { -60.f, -150.f, -240.f, -330.f }) D = FMath::Min(D, FMath::Sqrt(FMath::Square(P.X - Xp) + FMath::Square(P.Z - 172.f)) - 5.f); return FMath::Max(D, 120.f - FMath::Abs(P.Y)); } });
		M.Add({ Dark, [](const V3& P) { return FMath::Max(Seam(P.X, P.Z, -128, -42, 88, 146, 6), 90.f - FMath::Abs(P.Y)); } });
		M.Add({ Dark, [](const V3& P) { return FMath::Max(FMath::Abs(P.Z - 202.f) - 1.5f, FMath::Sqrt(FMath::Square(P.X - 80.f) + FMath::Square(P.Y)) - 70.f); } });
		M.Add({ Dark, [=](const V3& P) { const float Ya = FMath::Abs(P.Y); float Dw = 1e9f; for (int32 i = 0; i < 4; i++) Dw = FMath::Min(Dw, FMath::Sqrt(FMath::Square(P.X - Ax[i]) + FMath::Square(P.Z - S.R))); return FMath::Max(Dw - (Ra + 2.5f), FMath::Max(Yin - 3.f - Ya, Ya - 150.f)); } });
		M.Add({ Dark, [](const V3& P) { return P.Z - 49.f; } });
		Tot += BuildShell(TEXT("btr"), F, M, V3(-456, -155, 36), V3(321, 155, 250), 3.0f);
	}
	UE_LOG(LogTemp, Display, TEXT("KAPVEH done: %d triangles, %.1f s"), Tot, FPlatformTime::Seconds() - T0);
	return Tot > 0;
}
