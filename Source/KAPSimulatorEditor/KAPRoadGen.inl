// KAPRoadGen.inl — жол текстуралары мен материалы (KAPEditorLibrary.cpp соңында қосылады)
//  • асфальт: 8 × 32 м тайл — ескі, күнге оңған жабын, бойлық/көлденең/торлы жарықшақтар, жамаулар, шұңқыр,
//    дөңгелек жолдары (жылтыр), ортаңғы үзік және жиек сызықтары (оңған), жиекте құм, сынық жиек → құмды жиек
//  • топырақ жол: 6 × 24 м тайл — екі із ±0.72 м (Нива/УАЗ/Буханка дөңгелек аралығы 1.44 м), протектор таңбасы,
//    әлсіз жүк көлігі ізі ±1.0 м, ортаңғы жал (шөп, тас), жиек үйінділері, шеті құмға сіңеді
//  • M_KAP_Road: Masked (шеті дизерленеді), TexA (түс + мөлдірлік), TexN (нормаль), TexM (R кедір-бұдыр, G AO, B биіктік),
//    әлем бойынша құм басу дақтары (ойықтарға бірінші), шеткі сөну — VertexColor.A
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVertexColor.h"

namespace KAPRoadGen
{
	static float H2(int32 X, int32 Y, uint32 Seed)
	{
		uint32 h = (uint32)X * 73856093u ^ (uint32)Y * 19349663u ^ (Seed + 1u) * 83492791u;
		h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
		return (h & 0xFFFFFFu) / 16777215.f;
	}
	static int32 Wr(int32 A, int32 N) { return ((A % N) + N) % N; }
	/** мән-шуы: Fx × Fy ұяшық, екі осьте де периодты (тайл жіксіз қайталанады) */
	static float VN(float U, float V, int32 Fx, int32 Fy, uint32 Seed)
	{
		const float X = U * Fx, Y = V * Fy;
		const int32 X0 = FMath::FloorToInt32(X), Y0 = FMath::FloorToInt32(Y);
		const float Tx = X - X0, Ty = Y - Y0;
		const float Sx = Tx * Tx * (3.f - 2.f * Tx), Sy = Ty * Ty * (3.f - 2.f * Ty);
		const float a = H2(Wr(X0, Fx), Wr(Y0, Fy), Seed), b = H2(Wr(X0 + 1, Fx), Wr(Y0, Fy), Seed);
		const float c = H2(Wr(X0, Fx), Wr(Y0 + 1, Fy), Seed), d = H2(Wr(X0 + 1, Fx), Wr(Y0 + 1, Fy), Seed);
		return FMath::Lerp(FMath::Lerp(a, b, Sx), FMath::Lerp(c, d, Sx), Sy);
	}
	static float FBm(float U, float V, int32 Fx, int32 Fy, int32 Oct, uint32 Seed)
	{
		float S = 0.f, A = 0.5f, N = 0.f;
		for (int32 i = 0; i < Oct; i++) { S += A * VN(U, V, Fx << i, Fy << i, Seed + i * 101u); N += A; A *= 0.5f; }
		return S / N;
	}
	static float Gs(float X, float S) { return FMath::Exp(-(X * X) / (S * S)); }
	static float Sat(float X) { return FMath::Clamp(X, 0.f, 1.f); }

	struct FMap
	{
		int32 W = 0, H = 0; TArray<float> P;
		void Init(int32 InW, int32 InH) { W = InW; H = InH; P.Init(0.f, W * H); }
		float& At(int32 X, int32 Y) { return P[Wr(Y, H) * W + FMath::Clamp(X, 0, W - 1)]; }
		float Get(int32 X, int32 Y) const { return P[Wr(Y, H) * W + FMath::Clamp(X, 0, W - 1)]; }
	};

	/** жарықшақ: кездейсоқ жүріс (y бойынша оралады), кейде тармақталады */
	static void Crack(FMap& C, FRandomStream& R, float X, float Y, float A, int32 Steps, float StepPx, float Width, float Wander, int32 Depth = 0)
	{
		for (int32 s = 0; s < Steps; s++)
		{
			A += R.FRandRange(-Wander, Wander);
			X += FMath::Cos(A) * StepPx; Y += FMath::Sin(A) * StepPx;
			if (X < 0 || X >= C.W) break;
			const int32 Rr = FMath::CeilToInt32(Width) + 1;
			for (int32 dy = -Rr; dy <= Rr; dy++) for (int32 dx = -Rr; dx <= Rr; dx++)
			{
				const float D = FMath::Sqrt((float)(dx * dx + dy * dy));
				const float K = Sat(Width - D + 0.5f);
				if (K <= 0.f) continue;
				float& Pv = C.At(FMath::RoundToInt32(X) + dx, FMath::RoundToInt32(Y) + dy);
				Pv = FMath::Max(Pv, K);
			}
			if (Depth < 2 && R.FRand() < 0.0035f)
				Crack(C, R, X, Y, A + R.FRandRange(0.8f, 1.6f) * (R.FRand() < 0.5f ? -1.f : 1.f), Steps / 3, StepPx, Width * 0.7f, Wander, Depth + 1);
		}
	}
	static FColor Lin(const FLinearColor& C, float A) { FColor O = FLinearColor(Sat(C.R), Sat(C.G), Sat(C.B)).ToFColor(true); O.A = (uint8)(Sat(A) * 255.f + 0.5f); return O; }
	static uint8 Q(float V) { return (uint8)(Sat(V) * 255.f + 0.5f); }

	/** биіктіктен нормаль (Str = биіктік бірлігі см / (2 × пиксель см)) */
	static void Normals(const FMap& Ht, float Str, TArray<FColor>& Out)
	{
		Out.SetNumUninitialized(Ht.W * Ht.H);
		for (int32 y = 0; y < Ht.H; y++) for (int32 x = 0; x < Ht.W; x++)
		{
			const float Dx = (Ht.Get(x + 1, y) - Ht.Get(x - 1, y)) * Str, Dy = (Ht.Get(x, y + 1) - Ht.Get(x, y - 1)) * Str;
			const FVector N = FVector(-Dx, -Dy, 1.f).GetSafeNormal();
			Out[y * Ht.W + x] = FColor(Q((float)N.X * 0.5f + 0.5f), Q((float)N.Y * 0.5f + 0.5f), Q((float)N.Z * 0.5f + 0.5f), 255);
		}
	}
}

/** Жол текстурасы: ені бойынша қысылады (U), бойымен қайталанады (V) */
static UTexture2D* SaveRoadTex(const FString& Name, int32 W, int32 H, const TArray<FColor>& Px, bool bNormal, bool bSRGB)
{
	const FString Path = TEXT("/Game/KAP/Textures/") + Name;
	UPackage* Pkg = MakePkg(Path);
	UTexture2D* T = NewObject<UTexture2D>(Pkg, FName(*Name), RF_Public | RF_Standalone);
	T->Source.Init(W, H, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Px.GetData()));
	T->SRGB = bSRGB;
	T->CompressionSettings = bNormal ? TC_Normalmap : TC_Default;
	T->LODGroup = TEXTUREGROUP_World;
	T->AddressX = TA_Clamp; T->AddressY = TA_Wrap;
	T->PostEditChange();
	T->UpdateResource();
	FTextureCompilingManager::Get().FinishAllCompilation();
	SaveAsset(T);
	return T;
}

bool UKAPEditorLibrary::CreateRoadAssets()
{
	using namespace KAPRoadGen;
	const FLinearColor SandC(0.56f, 0.43f, 0.285f);
	// =================== АСФАЛЬТ (ені 8 м: жабын ±3.1 м, жиек ±4 м; ұзындығы 32 м) ===================
	{
		const int32 W = 1024, H = 4096; const float Wm = 8.f, Lm = 32.f; const float PxCm = Wm * 100.f / W;
		FRandomStream R(7311);
		FMap Cr; Cr.Init(W, H);
		auto ToX = [&](float Xm) { return (Xm / Wm + 0.5f) * W; };
		for (int32 i = 0; i < 22; i++)
		{
			const bool bLong = R.FRand() < 0.55f;
			const float Xm = bLong ? (R.FRand() < 0.45f ? R.FRandRange(-0.25f, 0.25f) : R.FRandRange(2.2f, 3.0f) * (R.FRand() < 0.5f ? -1.f : 1.f)) : R.FRandRange(-2.9f, 2.9f);
			const float Ang = bLong ? UE_HALF_PI + R.FRandRange(-0.12f, 0.12f) : R.FRandRange(-0.3f, 0.3f) + (R.FRand() < 0.5f ? 0.f : UE_PI);
			Crack(Cr, R, ToX(Xm), R.FRandRange(0.f, (float)H), Ang, R.RandRange(bLong ? 550 : 200, bLong ? 1500 : 650), 1.0f, R.FRandRange(0.6f, 1.2f), 0.08f);
		}
		// торлы («тасбақа сауыты») аймақтар — жиекке жақын
		for (int32 p = 0; p < 5; p++)
		{
			const float Cx = ToX(R.FRandRange(1.9f, 2.9f) * (R.FRand() < 0.5f ? -1.f : 1.f)), Cy = R.FRandRange(0.f, (float)H);
			for (int32 i = 0; i < 40; i++) Crack(Cr, R, Cx + R.FRandRange(-70.f, 70.f), Cy + R.FRandRange(-200.f, 200.f), R.FRandRange(0.f, UE_TWO_PI), R.RandRange(30, 75), 1.f, 0.55f, 0.28f);
		}
		struct FPatch { float X0, X1, Y0, Y1, Tone; };
		TArray<FPatch> Patches;
		for (int32 i = 0; i < 6; i++)
		{
			const float Cx = (float)R.FRandRange(-2.3f, 2.3f), Cw = (float)R.FRandRange(0.5f, 1.7f), Cy = (float)R.FRandRange(0.f, Lm), Ch = (float)R.FRandRange(0.6f, 2.8f);
			Patches.Add({ Cx - Cw / 2, Cx + Cw / 2, Cy, Cy + Ch, (float)R.FRandRange(0.68f, 0.84f) });
		}
		struct FHole { float X, Y, Rr; };
		TArray<FHole> Holes;
		for (int32 i = 0; i < 3; i++) Holes.Add({ (float)R.FRandRange(-2.4f, 2.4f), (float)R.FRandRange(0.f, Lm), (float)R.FRandRange(0.16f, 0.34f) });
		FMap Ht; Ht.Init(W, H);
		TArray<FColor> PA, PM, PN; PA.SetNumUninitialized(W * H); PM.SetNumUninitialized(W * H);
		for (int32 y = 0; y < H; y++) for (int32 x = 0; x < W; x++)
		{
			const float U = (x + 0.5f) / W, V = (y + 0.5f) / H;
			const float Xm = (U - 0.5f) * Wm, Ym = V * Lm, Ax = FMath::Abs(Xm);
			const float Mac = FBm(U, V, 8, 32, 3, 11), Mid = FBm(U, V, 64, 256, 2, 12), Gr = H2(x, y, 13);
			const uint32 Sd = Xm < 0.f ? 14u : 24u;
			const float Edge = 3.1f + (VN(0.5f, V, 1, 64, Sd) - 0.5f) * 0.3f + (VN(0.5f, V, 1, 512, Sd + 1) - 0.5f) * 0.08f;
			const float Chunk = FBm(U, V, 48, 192, 2, 18);
			const bool bPaved = Ax < Edge && !(Ax > Edge - 0.32f && Chunk > 0.64f);
			float Hh, Rg, AO = 1.f, Op;
			FLinearColor C;
			if (bPaved)
			{
				float T = FMath::Lerp(0.075f, 0.118f, Mac * 0.7f + Mid * 0.3f) + (Gr - 0.5f) * 0.03f;
				if (Gr > 0.945f) T = 0.17f + (Gr - 0.945f) * 1.2f; else if (Gr < 0.03f) T = 0.045f;
				const float WP = FMath::Max(Gs(Ax - 0.83f, 0.24f), Gs(Ax - 2.27f, 0.24f));
				T += WP * 0.016f;
				T -= Gs(Ax - 1.55f, 0.22f) * 0.022f * Sat((FBm(U, V, 2, 64, 3, 19) - 0.35f) * 3.f);
				Rg = 0.8f + (Gr - 0.5f) * 0.1f - WP * 0.16f;
				Hh = 0.6f + (Gr - 0.5f) * 0.05f + (Mid - 0.5f) * 0.05f;
				// жамаулар — сәл қара, жігінде жарықшақ
				for (const FPatch& P : Patches)
				{
					float Yy = Ym; if (Yy < P.Y0) Yy += Lm;
					if (Xm > P.X0 && Xm < P.X1 && Yy < P.Y1)
					{
						T *= P.Tone; Rg -= 0.04f; Hh += 0.03f;
						const float Db = FMath::Min(FMath::Min(Xm - P.X0, P.X1 - Xm), FMath::Min(Yy - P.Y0, P.Y1 - Yy));
						if (Db < 0.015f) Cr.At(x, y) = FMath::Max(Cr.Get(x, y), 0.75f);
					}
				}
				C = FLinearColor(T, T * 0.985f, T * 0.955f);
				// сызықтар: ортаңғы үзік (3 м / 8 м), жиек — тұтас; күнге оңған, үгілген
				const float Wear = Sat((FBm(U, V, 32, 128, 3, 21) - 0.28f) * 2.4f);
				const float Dash = FMath::Fmod(Ym, 8.f) < 3.f ? 1.f : 0.f;
				const float CL = Sat((0.065f - Ax) / 0.012f) * Dash;
				const float EL = Sat((0.06f - FMath::Abs(Ax - 2.9f)) / 0.012f);
				const float Paint = FMath::Max(CL * 0.62f, EL * 0.42f) * Wear;
				C = FMath::Lerp(C, FLinearColor(0.58f, 0.56f, 0.5f), Paint); Rg = FMath::Lerp(Rg, 0.6f, Paint); Hh += Paint * 0.02f;
				// шұңқыр
				for (const FHole& Ho : Holes)
				{
					float Dy = FMath::Abs(Ym - Ho.Y); Dy = FMath::Min(Dy, Lm - Dy);
					const float D = FMath::Sqrt(FMath::Square(Xm - Ho.X) + Dy * Dy) / (Ho.Rr * (0.85f + Mid * 0.3f));
					if (D < 1.f)
					{
						const float K = FMath::Sqrt(1.f - D);
						Hh -= 0.55f * K; AO -= 0.35f * K;
						C = FMath::Lerp(C, FMath::Lerp(FLinearColor(0.16f, 0.14f, 0.12f), SandC * 0.8f, Sat(K * 1.3f - 0.2f)), Sat(K * 3.f)); Rg = FMath::Lerp(Rg, 0.95f, Sat(K * 3.f));
					}
				}
				// жиекке қарай құм басады
				const float SandK = Sat((Ax - (Edge - 0.7f)) / 0.7f) * (0.45f + Mac * 0.55f) + Sat((FBm(U, V, 16, 64, 3, 22) - 0.7f) * 4.f) * 0.5f;
				C = FMath::Lerp(C, SandC * (0.92f + Gr * 0.1f), Sat(SandK)); Rg = FMath::Lerp(Rg, 0.95f, Sat(SandK));
				Op = 1.f;
			}
			else
			{
				// құмды жиек: қиыршық тас + құм, шетіне қарай мөлдір
				const float Grav = Gr > 0.82f ? 1.f : 0.f;
				C = FMath::Lerp(SandC * (0.85f + Mac * 0.15f), FLinearColor(0.24f, 0.2f, 0.16f) * (0.8f + Gr * 0.4f), Grav * 0.7f);
				Hh = 0.32f + Grav * 0.06f + (Mid - 0.5f) * 0.06f; Rg = 0.95f; AO = 0.95f;
				Op = Sat((4.0f - Ax) / 0.55f + (Mid - 0.5f) * 0.8f);
				if (Ax < Edge + 0.05f) Op = 1.f;
			}
			Ht.At(x, y) = Hh;
			PA[y * W + x] = Lin(C, Op);
			PM[y * W + x] = FColor(Q(Rg), Q(AO), Q(Hh), 255);
		}
		// жарықшақтарды қолдану (құм толған — қоңырқай)
		for (int32 y = 0; y < H; y++) for (int32 x = 0; x < W; x++)
		{
			const float Cv = Cr.Get(x, y);
			if (Cv <= 0.f) continue;
			FColor& A = PA[y * W + x];
			FLinearColor C = FLinearColor::FromSRGBColor(A);
			const float Mid = FBm((x + 0.5f) / W, (y + 0.5f) / H, 64, 256, 1, 12);
			C = FMath::Lerp(C, Mid > 0.55f ? FLinearColor(0.2f, 0.15f, 0.1f) : FLinearColor(0.035f, 0.033f, 0.03f), Cv * 0.75f);
			const uint8 Al = A.A;
			A = Lin(C, Al / 255.f);
			Ht.At(x, y) -= 0.25f * Cv;
			FColor& Mk = PM[y * W + x];
			Mk.R = Q(FMath::Lerp(Mk.R / 255.f, 0.95f, Cv)); Mk.G = Q(Mk.G / 255.f - 0.5f * Cv); Mk.B = Q(Ht.Get(x, y));
		}
		Normals(Ht, 2.0f / (2.f * PxCm), PN);
		SaveRoadTex(TEXT("T_KAP_RoadAsphalt_A"), W, H, PA, false, true);
		SaveRoadTex(TEXT("T_KAP_RoadAsphalt_N"), W, H, PN, true, false);
		SaveRoadTex(TEXT("T_KAP_RoadAsphalt_M"), W, H, PM, false, false);
	}
	// =================== ТОПЫРАҚ ЖОЛ (ені 6 м; ұзындығы 24 м) ===================
	{
		const int32 W = 1024, H = 4096; const float Wm = 6.f, Lm = 24.f; const float PxCm = Wm * 100.f / W;
		FMap Ht; Ht.Init(W, H);
		TArray<FColor> PA, PM, PN; PA.SetNumUninitialized(W * H); PM.SetNumUninitialized(W * H);
		const int32 PebN = FMath::RoundToInt32(Lm * 100.f / 3.f); // 3 см ұяшық, тайл бойынша бүтін сан
		const int32 TufN = FMath::RoundToInt32(Lm * 100.f / 20.f);
		for (int32 y = 0; y < H; y++) for (int32 x = 0; x < W; x++)
		{
			const float U = (x + 0.5f) / W, V = (y + 0.5f) / H;
			const float Xm = (U - 0.5f) * Wm, Ym = V * Lm;
			const float Mac = FBm(U, V, 6, 24, 3, 41), Mid = FBm(U, V, 48, 192, 2, 42), Gr = H2(x, y, 43);
			// із сызығы сәл ирелеңдейді — екі ізге бірдей, сондықтан ара қашықтық (дөңгелек аралығы) өзгермейді
			const float Wob = (VN(0.5f, V, 1, 24, 31) - 0.5f) * 0.10f + (VN(0.5f, V, 1, 96, 32) - 0.5f) * 0.03f;
			const float Xr = Xm - Wob, Ar = FMath::Abs(Xr);
			const float Rel = Ar - 0.72f;
			const float Rut = Gs(Rel, 0.15f), RutCore = Gs(Rel, 0.085f);
			const float Wob2 = (VN(0.7f, V, 1, 24, 33) - 0.5f) * 0.12f;
			const float Truck = Gs(FMath::Abs(Xm - Wob2) - 1.0f, 0.17f) * (0.4f + 0.6f * VN(0.3f, V, 1, 48, 34));
			const float Hump = Gs(Ar, 0.36f);
			const float Berm = Gs(Ar - 1.62f, 0.28f) * (0.55f + 0.45f * FBm(U, V, 16, 64, 2, 35));
			const float TreadOn = FMath::Frac((Ym * 100.f + FMath::Abs(Rel) * 80.f) / 8.f) < 0.45f ? 1.f : 0.f; // 8 см қадам: 2400 см тайлға бүтін сан
			const float Tread = TreadOn * Gs(Rel, 0.09f) * Sat((VN(0.5f, V, 1, 192, 36) - 0.3f) * 2.f);
			float Hh = 0.55f - 0.42f * Rut - 0.15f * Truck + 0.06f * Hump + 0.22f * Berm + 0.03f * Tread + (Mid - 0.5f) * 0.07f * (1.f - Rut) + (Gr - 0.5f) * 0.015f;
			const float TrackK = Sat((1.45f - Ar) / 0.22f);
			FLinearColor C = FMath::Lerp(SandC * (0.93f + Mac * 0.12f), FLinearColor(0.47f, 0.355f, 0.235f) * (0.95f + Mid * 0.1f), TrackK);
			C = FMath::Lerp(C, FLinearColor(0.37f, 0.27f, 0.175f), Rut * 0.85f);
			C = FMath::Lerp(C, FLinearColor(0.31f, 0.225f, 0.145f), RutCore * 0.5f);
			C = FMath::Lerp(C, FLinearColor(0.43f, 0.32f, 0.21f), Truck * 0.5f);
			C *= 1.f + Berm * 0.07f + Tread * 0.06f;
			float Rg = 0.95f - Rut * 0.11f, AO = 1.f - Rut * 0.12f;
			// ұсақ тастар (ізде азырақ)
			{
				const int32 Cx = FMath::FloorToInt32(Xm * 100.f / 3.f), Cy = FMath::FloorToInt32(Ym * 100.f / 3.f);
				for (int32 oy = -1; oy <= 1; oy++) for (int32 ox = -1; ox <= 1; ox++)
				{
					const int32 Gx = Cx + ox, Gy = Wr(Cy + oy, PebN);
					const float Hs = H2(Gx, Gy, 51);
					if (Hs < 0.986f + Rut * 0.012f) continue;
					const float Px = (Gx + H2(Gx, Gy, 52)) * 3.f, Py = (Cy + oy + H2(Gx, Gy, 53)) * 3.f;
					const float Rr = 0.5f + H2(Gx, Gy, 54) * 0.9f;
					const float D = FMath::Sqrt(FMath::Square(Xm * 100.f - Px) + FMath::Square(Ym * 100.f - Py)) / Rr;
					if (D < 1.f)
					{
						const float K = FMath::Sqrt(1.f - D * D);
						Hh += 0.12f * K; Rg = FMath::Lerp(Rg, 0.8f, K); AO = FMath::Min(AO, 0.85f + 0.15f * K);
						C = FMath::Lerp(C, FLinearColor(0.26f, 0.22f, 0.18f) * (0.7f + H2(Gx, Gy, 55) * 0.6f), Sat(K * 2.f));
					}
				}
			}
			// құрғақ шөп шоқтары: ортаңғы жалда және жиекте
			{
				const int32 Cx = FMath::FloorToInt32(Xm * 100.f / 20.f), Cy = FMath::FloorToInt32(Ym * 100.f / 20.f);
				const float Where = FMath::Max(Hump * 0.9f, FMath::Max(Berm, Sat((Ar - 1.9f) * 2.f))) * (1.f - Rut);
				for (int32 oy = -1; oy <= 1; oy++) for (int32 ox = -1; ox <= 1; ox++)
				{
					const int32 Gx = Cx + ox, Gy = Wr(Cy + oy, TufN);
					if (H2(Gx, Gy, 61) < 0.95f - Where * 0.05f || Where < 0.3f) continue;
					const float Px = (Gx + H2(Gx, Gy, 62)) * 20.f, Py = (Cy + oy + H2(Gx, Gy, 63)) * 20.f;
					const float Rr = 3.f + H2(Gx, Gy, 64) * 4.f;
					const float D = FMath::Sqrt(FMath::Square(Xm * 100.f - Px) + FMath::Square(Ym * 100.f - Py)) / Rr;
					const float Strand = VN(U, V, 384, 1536, 65);
					if (D < 1.f && Strand > D * 0.7f)
					{
						C = FMath::Lerp(C, FLinearColor(0.2f, 0.18f, 0.08f) * (0.7f + Strand * 0.6f), 0.85f);
						Hh += 0.05f * (1.f - D); Rg = 0.9f; AO = FMath::Min(AO, 0.8f + D * 0.2f);
					}
				}
			}
			const float Op = Sat((2.4f - Ar + (Mac - 0.5f) * 0.45f) / 0.5f);
			Ht.At(x, y) = Hh;
			PA[y * W + x] = Lin(C, Op);
			PM[y * W + x] = FColor(Q(Rg), Q(AO), Q(Hh), 255);
		}
		Normals(Ht, 8.f / (2.f * PxCm), PN);
		SaveRoadTex(TEXT("T_KAP_RoadDirt_A"), W, H, PA, false, true);
		SaveRoadTex(TEXT("T_KAP_RoadDirt_N"), W, H, PN, true, false);
		SaveRoadTex(TEXT("T_KAP_RoadDirt_M"), W, H, PM, false, false);
	}
	return CreateRoadMaterial();
}

/** M_KAP_Road (текстуралар дайын болса) */
bool UKAPEditorLibrary::CreateRoadMaterial()
{
	using namespace KAPRoadGen;
	const FLinearColor SandC(0.56f, 0.43f, 0.285f);
	{
		UTexture2D* TA = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_RoadAsphalt_A.T_KAP_RoadAsphalt_A"));
		UTexture2D* TN = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_RoadAsphalt_N.T_KAP_RoadAsphalt_N"));
		UTexture2D* TM = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_RoadAsphalt_M.T_KAP_RoadAsphalt_M"));
		if (!TA || !TN || !TM) return false;
		UMaterial* M = NewMaterial(TEXT("M_KAP_Road"));
		M->BlendMode = BLEND_Masked;
		M->OpacityMaskClipValue = 0.45f;
		auto TP = [&](const TCHAR* N, UTexture2D* T, EMaterialSamplerType ST, int32 Y)
		{
			auto* E = Expr<UMaterialExpressionTextureSampleParameter2D>(M, -1800, Y);
			E->ParameterName = N; E->Texture = T; E->SamplerType = ST;
			return E;
		};
		auto Out = [&](UMaterialExpression* In, int32 Idx, int32 X, int32 Y)
		{
			auto* E = Expr<UMaterialExpressionComponentMask>(M, X, Y); E->R = true; E->G = false; E->B = false; E->A = false;
			E->Input.Connect(Idx, In);
			return E;
		};
		auto* SA = TP(TEXT("TexA"), TA, SAMPLERTYPE_Color, -400);
		auto* SN = TP(TEXT("TexN"), TN, SAMPLERTYPE_Normal, 0);
		auto* SM = TP(TEXT("TexM"), TM, SAMPLERTYPE_LinearColor, 400);
		UMaterialExpression* Hgt = Out(SM, 3, -1550, 520);
		// құм басу: ~11 м дақтар, ойықтарға (жарықшақ, із) бірінші жиналады
		UMaterialExpression* Nz = Noise(M, 0.0009f, 3, -1800, 800);
		UMaterialExpression* Drift = Sat(M, Op2<UMaterialExpressionAdd>(M,
			Mul(M, SubK(M, Nz, 0.64f, -1600, 800), K(M, 4.f, -1600, 860), -1450, 800),
			Mul(M, Op1<UMaterialExpressionOneMinus>(M, Mul(M, Hgt, K(M, 1.8f, -1450, 560), -1350, 520), -1250, 520), K(M, 1.2f, -1250, 600), -1150, 560), -1050, 700), -950, 700);
		Drift = Mul(M, Drift, SParam(M, TEXT("SandDrift"), 0.65f, -950, 800), -800, 700);
		UMaterialExpression* Tone = Lerp(M, K(M, 0.9f, -1300, -150), K(M, 1.06f, -1300, -100), Noise(M, 0.004f, 2, -1500, -150), -1150, -150);
		UMaterialExpression* Base = Lerp(M, Mul(M, SA, Tone, -1000, -350), VParam(M, TEXT("Sand"), SandC, -1000, -250), Drift, -700, -300);
		UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, Out(SM, 1, -1550, 300), K(M, 0.95f, -900, 360), Drift, -700, 300), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, Out(SM, 2, -1550, 420), K(M, 1.f, -900, 470), Drift, -700, 420), TEXT(""), MP_AmbientOcclusion);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.35f, -700, 440), TEXT(""), MP_Specular);
		UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, SN, Col(M, FLinearColor(0, 0, 1), -900, 100), Mul(M, Drift, K(M, 0.85f, -900, 160), -800, 160), -650, 50), TEXT(""), MP_Normal);
		// мөлдірлік: текстура альфасы × төбе түсінің альфасы (жол ұштарында сөну) → дизер
		auto* VC = Expr<UMaterialExpressionVertexColor>(M, -1550, 1000);
		UMaterialExpression* Alpha = Mul(M, Out(SA, 4, -1550, 900), Out(VC, 4, -1400, 1000), -1250, 950);
		// (DitherTemporalAA функциясы бәрін кесіп тастады — альфаның өз шуы жеткілікті)
		UMaterialExpression* OpM = Alpha;
		if (false)
		if (UMaterialFunction* DF = LoadObject<UMaterialFunction>(nullptr, TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA")))
		{
			auto* Fc = Expr<UMaterialExpressionMaterialFunctionCall>(M, -1000, 950);
			Fc->SetMaterialFunction(DF);
			for (int32 i = 0; i < Fc->FunctionInputs.Num(); i++)
			{
				const FString Nm = Fc->FunctionInputs[i].Input.InputName.ToString();
				UE_LOG(LogTemp, Display, TEXT("KAPROADMAT dither input %d: %s"), i, *Nm);
				if (Nm.Contains(TEXT("Alpha"))) if (FExpressionInput* In = Fc->GetInput(i)) In->Connect(0, Alpha);
			}
			OpM = Fc;
		}
		UMaterialEditingLibrary::ConnectMaterialProperty(OpM, TEXT(""), MP_OpacityMask);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	UE_LOG(LogTemp, Display, TEXT("KAPROAD assets done"));
	return true;
}
