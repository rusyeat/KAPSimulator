// ---------------------------------------------------------------------
// Табиғат активтері: желмен тербелетін өсімдік материалы (MPC_KAPEnv), шөп/бидай/жас егін/қамыс,
// сексеуіл (діңі + бұтақшалары + сынған бөлігі), терек/тал/қарағаш, ландшафт шөп түрлері (LGT_KAP_*).
// KAPEditorLibrary.cpp ішіне KAPTerrainGen.inl-ден кейін қосылады.
// ---------------------------------------------------------------------
#include "LandscapeGrassType.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"

namespace KAPNGen
{
	using V3 = FVector3f;
	static V3 Perp(const V3& D) { V3 S = V3::CrossProduct(D, FMath::Abs(D.Z) < 0.95f ? V3(0, 0, 1) : V3(1, 0, 0)); return S.GetSafeNormal(); }
	/** Жолақ (жапырақ, сабақ): Grow бағытымен H ұзындық, ені W (ұшына қарай жіңішкереді), BendDir бойынша иіледі */
	static void Blade(FMeshBuild& B, V3 Base, V3 Grow, V3 Side, float H, float W, V3 BendDir, float Bend, int32 Seg, float Taper = 0.85f)
	{
		Grow.Normalize(); Side.Normalize();
		const V3 Face = V3::CrossProduct(Side, Grow).GetSafeNormal();
		const V3 N = (Face * 0.35f + V3(0, 0, 1) * 0.65f).GetSafeNormal();
		const int32 S = B.P.Num();
		for (int32 i = 0; i <= Seg; i++)
		{
			const float T = i / (float)Seg;
			const V3 C = Base + Grow * (H * T) + BendDir * (Bend * T * T);
			const float Wi = W * (1.f - Taper * T);
			B.P.Add(C - Side * (Wi * 0.5f)); B.N.Add(N); B.UV.Add(FVector2f(0, 0));
			B.P.Add(C + Side * (Wi * 0.5f)); B.N.Add(N); B.UV.Add(FVector2f(1, 0));
		}
		for (int32 i = 0; i < Seg; i++) { const int32 a = S + i * 2; B.Idx.Append({ a, a + 2, a + 1, a + 1, a + 2, a + 3 }); }
	}
	/** Айқас екі жолақ (жан-жақтан көлемді көрінеді) */
	static void Cross(FMeshBuild& B, V3 Base, V3 Grow, float H, float W, V3 BendDir, float Bend, int32 Seg, float Taper = 0.85f)
	{
		const V3 S1 = Perp(Grow.GetSafeNormal()), S2 = V3::CrossProduct(Grow.GetSafeNormal(), S1);
		Blade(B, Base, Grow, S1, H, W, BendDir, Bend, Seg, Taper);
		Blade(B, Base, Grow, S2, H, W, BendDir, Bend, Seg, Taper);
	}
	/** Тербелу салмағы (UV.y): биіктікке пропорционал (түбі 0, ұшы 1) — материал WPO осыны қолданады */
	static void SwayWeights(FMeshBuild& B, float MaxZ = -1.f)
	{
		if (MaxZ <= 0.f) for (const V3& P : B.P) MaxZ = FMath::Max(MaxZ, P.Z);
		for (int32 i = 0; i < B.P.Num(); i++) B.UV[i].Y = FMath::Clamp(B.P[i].Z / FMath::Max(1.f, MaxZ), 0.f, 1.f);
	}
	/** Жапырақ шоғыры: C маңында N төртбұрыш жапырақ */
	static void Leaves(FMeshBuild& B, FRandomStream& R, V3 C, float Rad, int32 N, float L, float W, float Droop)
	{
		for (int32 i = 0; i < N; i++)
		{
			const V3 D = V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(-0.6f, 1.f)).GetSafeNormal();
			const V3 P = C + D * (Rad * FMath::Sqrt(R.FRand()));
			const V3 G = (D + V3(0, 0, -Droop)).GetSafeNormal();
			Blade(B, P, G, Perp(G), L * R.FRandRange(0.8f, 1.2f), W * R.FRandRange(0.8f, 1.2f), V3(0, 0, -1), L * 0.15f, 1, 0.55f);
		}
	}

	/** Жапырақ шоғыры (крона «бұлты»): шумен бұдырланған эллипсоид — алыстан тұтас жасыл крона */
	static void Blob(FMeshBuild& B, FRandomStream& R, V3 C, V3 Rad, int32 Seg = 6)
	{
		const V3 O(R.FRandRange(0.f, 50.f), R.FRandRange(0.f, 50.f), R.FRandRange(0.f, 50.f));
		B.Ellipsoid(C, Rad, Seg, [O](V3 D) { return 0.78f + 0.4f * (FMath::PerlinNoise3D(FVector(D * 2.6f + O)) * 0.5f + 0.5f); });
	}
	struct FTreeP { float TrunkH, TrunkR, Lean, BranchAng, Droop, Gravity, LeafRad, LeafL, LeafW; int32 Depth, Kids, LeafN; float Twist; };
	/** Бұтақтанатын ағаш: Wood — діңі мен бұтақтары, Leaf — жапырақтар/бұтақшалар */
	static void Branch(FMeshBuild& Wood, FMeshBuild& Leaf, FRandomStream& R, const FTreeP& T, V3 P, V3 Dir, float Len, float Rad, int32 Depth, TFunctionRef<void(const V3&, const V3&, float)> Tip)
	{
		const int32 Segs = FMath::Max(2, FMath::CeilToInt32(Len / 35.f));
		const float SegL = Len / Segs;
		for (int32 s = 0; s < Segs; s++)
		{
			const float R0 = FMath::Lerp(Rad, Rad * 0.62f, s / (float)Segs), R1 = FMath::Lerp(Rad, Rad * 0.62f, (s + 1) / (float)Segs);
			Dir = (Dir + V3(R.FRandRange(-T.Twist, T.Twist), R.FRandRange(-T.Twist, T.Twist), R.FRandRange(-T.Twist, T.Twist) * 0.5f) + V3(0, 0, -T.Gravity * (1.f - Depth / (float)FMath::Max(1, T.Depth)))).GetSafeNormal();
			const V3 Q = P + Dir * SegL;
			Wood.Tube(P, Q, FMath::Max(R0, 0.6f), FMath::Max(R1, 0.5f), Rad > 6.f ? 8 : 5);
			// жанама бұтақтар
			if (Depth > 0 && s > 0 && R.FRand() < (float)T.Kids / Segs)
			{
				const V3 Sd = Perp(Dir);
				const float A = R.FRandRange(0.f, UE_TWO_PI);
				const V3 Out = (Sd * FMath::Cos(A) + V3::CrossProduct(Dir, Sd) * FMath::Sin(A)).GetSafeNormal();
				const float Ang = FMath::DegreesToRadians(T.BranchAng * R.FRandRange(0.7f, 1.2f));
				const V3 Kd = (Dir * FMath::Cos(Ang) + Out * FMath::Sin(Ang)).GetSafeNormal();
				Branch(Wood, Leaf, R, T, Q, Kd, Len * R.FRandRange(0.45f, 0.7f), R1 * 0.7f, Depth - 1, Tip);
			}
			P = Q;
		}
		Tip(P, Dir, Rad);
		if (Depth > 0)
			for (int32 k = 0; k < 2; k++)
			{
				const V3 Sd = Perp(Dir);
				const float A = R.FRandRange(0.f, UE_TWO_PI);
				const V3 Out = (Sd * FMath::Cos(A) + V3::CrossProduct(Dir, Sd) * FMath::Sin(A)).GetSafeNormal();
				const float Ang = FMath::DegreesToRadians(T.BranchAng * 0.6f * R.FRandRange(0.6f, 1.2f));
				Branch(Wood, Leaf, R, T, P, (Dir * FMath::Cos(Ang) + Out * FMath::Sin(Ang)).GetSafeNormal(), Len * R.FRandRange(0.5f, 0.72f), Rad * 0.62f * 0.75f, Depth - 1, Tip);
			}
	}

	static UMaterialInstanceConstant* MakeMIC(const FString& Name, UMaterialInterface* Parent, FLinearColor C, float Sway)
	{
		UPackage* P = MakePkg(TEXT("/Game/KAP/Materials/") + Name);
		UMaterialInstanceConstantFactoryNew* F = NewObject<UMaterialInstanceConstantFactoryNew>();
		F->InitialParent = Parent;
		UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(F->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), P, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
		MI->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")), C);
		MI->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("SwayAmp")), Sway);
		MI->PostEditChange();
		SaveAsset(MI);
		return MI;
	}
	static UStaticMesh* SaveWithMat(const FString& Name, FMeshBuild& B, UMaterialInterface* Mat)
	{
		if (B.Idx.Num() < 3) return nullptr;
		SaveMesh(Name, B, false, false);
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/KAP/Meshes/%s.%s"), *Name, *Name));
		if (SM && Mat)
		{
			SM->SetMaterial(0, Mat);
			if (UBodySetup* BS = SM->GetBodySetup()) BS->CollisionTraceFlag = CTF_UseSimpleAsComplex; // өсімдіктің күрделі соқтығысы керек емес
			SM->PostEditChange();
			SaveAsset(SM);
		}
		UE_LOG(LogTemp, Display, TEXT("KAPNATURE mesh %s: %d verts %d tris"), *Name, B.P.Num(), B.Idx.Num() / 3);
		return SM;
	}
	struct FVar { UStaticMesh* Mesh; float PerM2; int32 Cull0, Cull1; float S0, S1; bool bShadow; };
	static ULandscapeGrassType* MakeGrass(const FString& Name, const TArray<FVar>& Vars)
	{
		UPackage* P = MakePkg(TEXT("/Game/KAP/Foliage/") + Name);
		ULandscapeGrassType* G = NewObject<ULandscapeGrassType>(P, FName(*Name), RF_Public | RF_Standalone);
		G->GrassVarieties.Reset();
		for (const FVar& V : Vars)
		{
			if (!V.Mesh) continue;
			FGrassVariety Gv;
			Gv.GrassMesh = V.Mesh;
			Gv.GrassDensity.Default = V.PerM2 * 10.f; Gv.GrassDensityQuality.Default = V.PerM2 * 10.f;
			Gv.StartCullDistance.Default = V.Cull0; Gv.StartCullDistanceQuality.Default = V.Cull0;
			Gv.EndCullDistance.Default = V.Cull1; Gv.EndCullDistanceQuality.Default = V.Cull1;
			Gv.bUseGrid = true; Gv.PlacementJitter = 1.f;
			Gv.Scaling = EGrassScaling::Uniform; Gv.ScaleX = FFloatInterval(V.S0, V.S1);
			Gv.RandomRotation = true; Gv.AlignToSurface = true;
			Gv.bCastDynamicShadow = V.bShadow; Gv.bCastContactShadow = true; Gv.bReceivesDecals = false;
			Gv.bAffectDistanceFieldLighting = false;
			G->GrassVarieties.Add(Gv);
		}
		G->PostEditChange();
		SaveAsset(G);
		return G;
	}
}

bool UKAPEditorLibrary::CreateNatureAssets()
{
	using namespace KAPNGen;
	UMaterialParameterCollection* Env = KAPEnsureEnvMPC();
	// ================= M_KAP_Foliage: екі жақты өсімдік, жел бағытымен тербеледі, жаңбырда қараяды =================
	UMaterial* M = NewMaterial(TEXT("M_KAP_Foliage"));
	{
		M->TwoSided = true;
		M->SetShadingModel(MSM_TwoSidedFoliage);
		bool bD = false; M->SetMaterialUsage(bD, MATUSAGE_InstancedStaticMeshes);
		auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -2600, 0);
		auto* PIR = Expr<UMaterialExpressionPerInstanceRandom>(M, -2600, 200);
		auto* UV = Expr<UMaterialExpressionTextureCoordinate>(M, -2600, 300);
		UMaterialExpression* H = MaskC(M, UV, false, true, false, -2450, 300);
		UMaterialExpression* Color = VParam(M, TEXT("Color"), FLinearColor(0.42f, 0.4f, 0.2f), -2600, -200);
		auto* Var = Noise(M, 0.0015f, 2, -2600, 450);
		UMaterialExpression* Wet = KAPEnvParam(M, Env, TEXT("Wetness"), -2600, 650);
		UMaterialExpression* C = Mul(M, Color, Op2<UMaterialExpressionAdd>(M, Mul(M, Var, K(M, 0.38f, -2300, 500), -2150, 450), K(M, 0.8f, -2150, 520), -2000, 450), -1850, -150);
		C = Mul(M, C, Op2<UMaterialExpressionAdd>(M, Mul(M, PIR, K(M, 0.3f, -2300, 260), -2150, 200), K(M, 0.85f, -2150, 260), -2000, 200), -1700, -100);
		C = Mul(M, C, Lerp(M, K(M, 0.42f, -2000, 330), K(M, 1.f, -2000, 390), H, -1850, 330), -1550, -50);
		C = Mul(M, C, Op1<UMaterialExpressionOneMinus>(M, Mul(M, Wet, K(M, 0.3f, -2300, 700), -2150, 650), -2000, 650), -1400, 0);
		UMaterialEditingLibrary::ConnectMaterialProperty(C, TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, C, K(M, 0.75f, -1400, 120), -1250, 100), TEXT(""), MP_SubsurfaceColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, K(M, 0.78f, -1400, 200), K(M, 0.4f, -1400, 260), Wet, -1250, 200), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.35f, -1250, 300), TEXT(""), MP_Specular);
		// жел: (WindX, WindY) бағытымен иілу + діріл; күші жел жылдамдығына және биіктік квадратына пропорционал
		auto* T = Expr<UMaterialExpressionTime>(M, -2600, 900);
		UMaterialExpression* WX = KAPEnvParam(M, Env, TEXT("WindX"), -2600, 1000), *WY = KAPEnvParam(M, Env, TEXT("WindY"), -2600, 1060), *WS = KAPEnvParam(M, Env, TEXT("WindSpeed"), -2600, 1120);
		UMaterialExpression* Sway = SParam(M, TEXT("SwayAmp"), 10.f, -2600, 1200);
		UMaterialExpression* Ph = Op2<UMaterialExpressionAdd>(M,
			Op2<UMaterialExpressionAdd>(M, Mul(M, MaskC(M, WP, true, false, false, -2450, 1300), K(M, 0.0011f, -2450, 1360), -2300, 1300), Mul(M, MaskC(M, WP, false, true, false, -2450, 1420), K(M, 0.0007f, -2450, 1480), -2300, 1420), -2150, 1350),
			Mul(M, PIR, K(M, 6.28f, -2300, 1540), -2150, 1500), -2000, 1400);
		UMaterialExpression* Freq = Op2<UMaterialExpressionAdd>(M, K(M, 1.4f, -2450, 1600), Mul(M, WS, K(M, 0.12f, -2450, 1660), -2300, 1620), -2150, 1600);
		UMaterialExpression* Arg = Op2<UMaterialExpressionAdd>(M, Mul(M, T, Freq, -2000, 1550), Ph, -1850, 1450);
		UMaterialExpression* Sw = Op2<UMaterialExpressionAdd>(M, Op2<UMaterialExpressionAdd>(M, Mul(M, Op1<UMaterialExpressionSine>(M, Arg, -1700, 1450), K(M, 0.45f, -1700, 1510), -1550, 1450), K(M, 0.55f, -1550, 1510), -1400, 1450), Mul(M, WS, K(M, 0.03f, -1550, 1570), -1400, 1550), -1250, 1450);
		UMaterialExpression* Arg2 = Op2<UMaterialExpressionAdd>(M, Mul(M, Arg, K(M, 3.7f, -1700, 1700), -1550, 1700), Mul(M, Ph, K(M, 2.3f, -1700, 1760), -1550, 1760), -1400, 1700);
		UMaterialExpression* Fl = Mul(M, Op1<UMaterialExpressionSine>(M, Arg2, -1250, 1700), K(M, 0.2f, -1250, 1760), -1100, 1700);
		UMaterialExpression* Amp = Mul(M, Sway, Op2<UMaterialExpressionAdd>(M, K(M, 0.15f, -2300, 1250), Mul(M, WS, K(M, 0.1f, -2450, 1250), -2300, 1200), -2150, 1200), -2000, 1200);
		UMaterialExpression* Kk = Mul(M, Mul(M, Amp, Op2<UMaterialExpressionAdd>(M, Sw, Fl, -950, 1550), -800, 1300), Mul(M, H, H, -950, 1300), -650, 1300);
		auto* Off = Expr<UMaterialExpressionAppendVector>(M, -500, 1200);
		auto* OffXY = Expr<UMaterialExpressionAppendVector>(M, -650, 1100);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, WX, Kk, -800, 1050), TEXT(""), OffXY, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, WY, Kk, -800, 1150), TEXT(""), OffXY, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OffXY, TEXT(""), Off, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, Op1<UMaterialExpressionAbs>(M, Kk, -650, 1400), K(M, -0.25f, -650, 1460), -500, 1400), TEXT(""), Off, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(Off, TEXT(""), MP_WorldPositionOffset);
		UMaterialEditingLibrary::RecompileMaterial(M);
		KAPLogMatErrors(M);
		SaveAsset(M);
	}
	UMaterialInstanceConstant* MiGrass = MakeMIC(TEXT("MI_KAP_Grass"), M, FLinearColor(0.4f, 0.39f, 0.19f), 9.f);
	UMaterialInstanceConstant* MiDry = MakeMIC(TEXT("MI_KAP_GrassDry"), M, FLinearColor(0.56f, 0.48f, 0.29f), 11.f);
	UMaterialInstanceConstant* MiWheat = MakeMIC(TEXT("MI_KAP_Wheat"), M, FLinearColor(0.7f, 0.57f, 0.29f), 13.f);
	UMaterialInstanceConstant* MiGreen = MakeMIC(TEXT("MI_KAP_GreenCrop"), M, FLinearColor(0.2f, 0.34f, 0.11f), 7.f);
	UMaterialInstanceConstant* MiReed = MakeMIC(TEXT("MI_KAP_Reed"), M, FLinearColor(0.44f, 0.45f, 0.25f), 28.f);
	UMaterialInstanceConstant* MiTwig = MakeMIC(TEXT("MI_KAP_SaxaulTwig"), M, FLinearColor(0.43f, 0.46f, 0.33f), 5.f);
	UMaterialInstanceConstant* MiPoplar = MakeMIC(TEXT("MI_KAP_PoplarLeaf"), M, FLinearColor(0.24f, 0.34f, 0.14f), 22.f);
	UMaterialInstanceConstant* MiWillow = MakeMIC(TEXT("MI_KAP_WillowLeaf"), M, FLinearColor(0.31f, 0.41f, 0.18f), 30.f);
	UMaterialInstanceConstant* MiElm = MakeMIC(TEXT("MI_KAP_ElmLeaf"), M, FLinearColor(0.19f, 0.3f, 0.11f), 18.f);
	UMaterialInstanceConstant* MiTamarisk = MakeMIC(TEXT("MI_KAP_Tamarisk"), M, FLinearColor(0.55f, 0.36f, 0.42f), 12.f);
	FRandomStream R(20261023);
	// ---- дала шөбі (селеу/бетеге): жіңішке жапырақтар шоғыры ----
	auto Tuft = [&](FMeshBuild& B, int32 N, float H0, float H1, float Spread, float W)
	{
		for (int32 i = 0; i < N; i++)
		{
			const float A = R.FRandRange(0.f, UE_TWO_PI), Rr = FMath::Sqrt(R.FRand()) * Spread;
			const V3 Out(FMath::Cos(A), FMath::Sin(A), 0);
			const V3 Base = Out * Rr + V3(0, 0, -2);
			const V3 Grow = (Out * R.FRandRange(0.12f, 0.5f) + V3(0, 0, 1)).GetSafeNormal();
			const float Hh = R.FRandRange(H0, H1);
			Blade(B, Base, Grow, Perp(Grow), Hh, W * R.FRandRange(0.7f, 1.25f), Out + V3(0, 0, -0.35f), Hh * R.FRandRange(0.12f, 0.38f), 3);
		}
	};
	UStaticMesh *SmGrass = nullptr, *SmDry = nullptr, *SmSandGrass = nullptr, *SmWheat = nullptr, *SmGreen = nullptr, *SmReed = nullptr;
	{ FMeshBuild B; Tuft(B, 16, 22.f, 48.f, 9.f, 1.3f); SwayWeights(B); SmGrass = SaveWithMat(TEXT("SM_KAP_GrassTuft"), B, MiGrass); }
	{ FMeshBuild B; Tuft(B, 12, 35.f, 70.f, 7.f, 0.8f); SwayWeights(B); SmDry = SaveWithMat(TEXT("SM_KAP_GrassFeather"), B, MiDry); } // селеу (ұзын, жіңішке)
	{ FMeshBuild B; Tuft(B, 9, 18.f, 40.f, 6.f, 0.9f); SwayWeights(B); SmSandGrass = SaveWithMat(TEXT("SM_KAP_SandGrass"), B, MiDry); }
	// ---- бидай: сабақ + масақ + 1–2 жапырақ ----
	{
		FMeshBuild B;
		for (int32 i = 0; i < 18; i++)
		{
			const V3 Base(R.FRandRange(-24.f, 24.f), R.FRandRange(-24.f, 24.f), -2.f);
			const V3 Grow = V3(R.FRandRange(-0.08f, 0.08f), R.FRandRange(-0.08f, 0.08f), 1).GetSafeNormal();
			const float Hh = R.FRandRange(74.f, 94.f);
			const V3 Bd = V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0).GetSafeNormal();
			Blade(B, Base, Grow, Perp(Grow), Hh, 0.8f, Bd, 6.f, 3, 0.3f);
			const V3 Top = Base + Grow * Hh + Bd * 6.f;
			Cross(B, Top - Grow * 1.f, (Grow + Bd * 0.25f).GetSafeNormal(), 9.f, 1.9f, Bd, 1.5f, 2, 0.6f); // масақ
			for (int32 l = 0; l < 1; l++)
			{
				const V3 Lp = Base + Grow * R.FRandRange(18.f, 45.f);
				const V3 O = V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0).GetSafeNormal();
				Blade(B, Lp, (O + V3(0, 0, 0.8f)).GetSafeNormal(), Perp(O), R.FRandRange(20.f, 30.f), 1.4f, V3(0, 0, -1), 9.f, 3);
			}
		}
		SwayWeights(B, 95.f); SmWheat = SaveWithMat(TEXT("SM_KAP_Wheat"), B, MiWheat);
	}
	// ---- жас егін (жасыл): кең жапырақтар ----
	{ FMeshBuild B; Tuft(B, 22, 24.f, 44.f, 20.f, 2.2f); SwayWeights(B); SmGreen = SaveWithMat(TEXT("SM_KAP_GreenCrop"), B, MiGreen); }
	// ---- қамыс: биік сабақтар, ұзын жапырақтар, басындағы шашақ ----
	{
		FMeshBuild B;
		for (int32 i = 0; i < 16; i++)
		{
			const V3 Base(R.FRandRange(-40.f, 40.f), R.FRandRange(-40.f, 40.f), -5.f);
			const V3 Grow = V3(R.FRandRange(-0.07f, 0.07f), R.FRandRange(-0.07f, 0.07f), 1).GetSafeNormal();
			const float Hh = R.FRandRange(180.f, 280.f);
			const V3 Bd = V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0).GetSafeNormal();
			Cross(B, Base, Grow, Hh, 1.4f, Bd, Hh * 0.05f, 5, 0.4f);
			for (int32 l = 0; l < 4; l++)
			{
				const V3 Lp = Base + Grow * (Hh * R.FRandRange(0.15f, 0.75f));
				const V3 O = V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0).GetSafeNormal();
				Blade(B, Lp, (O + V3(0, 0, 1.1f)).GetSafeNormal(), Perp(O), R.FRandRange(35.f, 55.f), 2.6f, V3(0, 0, -1) + O * 0.3f, 18.f, 3);
			}
			const V3 Top = Base + Grow * Hh + Bd * (Hh * 0.05f);
			for (int32 p = 0; p < 4; p++)
			{
				const V3 O = (Bd + V3(R.FRandRange(-0.6f, 0.6f), R.FRandRange(-0.6f, 0.6f), 0)).GetSafeNormal();
				Blade(B, Top, (O * 0.35f + V3(0, 0, 1)).GetSafeNormal(), Perp(O), R.FRandRange(22.f, 32.f), 5.f, O + V3(0, 0, -0.6f), 10.f, 2, 0.5f);
			}
		}
		SwayWeights(B, 280.f); SmReed = SaveWithMat(TEXT("SM_KAP_Reeds"), B, MiReed);
	}
	// ---- сексеуіл: 3 нұсқа (діңі — қабық, бұтақшалары — жапырақсыз жасыл-сұр бұтақшалар) ----
	for (int32 v = 0; v < 3; v++)
	{
		FMeshBuild Wood, Twig;
		FTreeP T; T.TrunkH = 0; T.TrunkR = 0; T.Lean = 0; T.BranchAng = 42.f; T.Droop = 0; T.Gravity = 0.04f; T.LeafRad = 0; T.LeafL = 0; T.LeafW = 0; T.Depth = 2; T.Kids = 2; T.LeafN = 0; T.Twist = 0.32f;
		auto TwigTip = [&](const V3& P, const V3& D, float Rad)
		{
			// сексеуілдің бұлдыр сұр-жасыл «бұлты» (алыстан бұта болып көрінеді)
			for (int32 b = 0; b < 3; b++) Blob(Twig, R, P + V3(R.FRandRange(-22.f, 22.f), R.FRandRange(-22.f, 22.f), R.FRandRange(-14.f, 4.f)), V3(R.FRandRange(13.f, 22.f), R.FRandRange(13.f, 22.f), R.FRandRange(10.f, 16.f)), 4);
			// салбыраған буынды бұтақшалар (сексеуілдің «жапырағы»)
			const int32 N = 14 + R.RandRange(0, 6);
			for (int32 k = 0; k < N; k++)
			{
				const V3 O = (D * 0.4f + V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(-0.2f, 0.6f))).GetSafeNormal();
				const V3 S = P + V3(R.FRandRange(-12.f, 12.f), R.FRandRange(-12.f, 12.f), R.FRandRange(-10.f, 6.f));
				Cross(Twig, S, O, R.FRandRange(22.f, 42.f), 0.9f, V3(0, 0, -1), R.FRandRange(10.f, 22.f), 3, 0.5f);
			}
		};
		const int32 Stems = 2 + v;
		for (int32 s = 0; s < Stems; s++)
		{
			const float A = R.FRandRange(0.f, UE_TWO_PI);
			const V3 D = (V3(FMath::Cos(A), FMath::Sin(A), 0) * R.FRandRange(0.25f, 0.6f) + V3(0, 0, 1)).GetSafeNormal();
			Branch(Wood, Twig, R, T, V3(FMath::Cos(A) * 6.f, FMath::Sin(A) * 6.f, -8.f), D, R.FRandRange(120.f, 190.f) + v * 25.f, R.FRandRange(5.5f, 8.f), 2, TwigTip);
		}
		Wood.RecomputeNormals(); Wood.FixWinding(); Wood.RecomputeNormals();
		SwayWeights(Twig, 330.f);
		SaveMesh(FString::Printf(TEXT("SM_KAP_SaxaulWood_%d"), v), Wood, false, false);
		SaveWithMat(FString::Printf(TEXT("SM_KAP_SaxaulTwig_%d"), v), Twig, MiTwig);
		UE_LOG(LogTemp, Display, TEXT("KAPNATURE saxaul %d: wood %d tris, twig %d tris"), v, Wood.Idx.Num() / 3, Twig.Idx.Num() / 3);
	}
	// ---- сынған сексеуіл бөлігі (физикалық сынық, отын) ----
	{
		FMeshBuild B;
		V3 P(-35, 0, 0), D(1, 0, 0);
		for (int32 s = 0; s < 5; s++)
		{
			D = (D + V3(0, R.FRandRange(-0.35f, 0.35f), R.FRandRange(-0.25f, 0.25f))).GetSafeNormal();
			const V3 Q = P + D * 15.f;
			B.Tube(P, Q, 5.f - s * 0.5f, 4.5f - s * 0.5f, 7);
			if (s == 2) B.Tube(Q, Q + V3(4, 10, 6), 2.2f, 1.f, 5);
			P = Q;
		}
		B.Ellipsoid(V3(-35, 0, 0), V3(1.5f, 5.f, 5.f), 6); B.Ellipsoid(P, V3(1.5f, 2.6f, 2.6f), 6); // кесік беттері
		B.RecomputeNormals(); B.FixWinding(); B.RecomputeNormals();
		SaveMesh(TEXT("SM_KAP_SaxaulLog"), B, false, false);
		// физика үшін қарапайым капсула (X бойымен 75 см, R 5 см)
		if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/KAP/Meshes/SM_KAP_SaxaulLog.SM_KAP_SaxaulLog")))
			if (UBodySetup* BS = SM->GetBodySetup())
			{
				BS->AggGeom.EmptyElements();
				FKSphylElem Cap(5.f, 66.f); Cap.Center = FVector(2.f, 0, 0); Cap.Rotation = FRotator(90.f, 0, 0);
				BS->AggGeom.SphylElems.Add(Cap);
				BS->CollisionTraceFlag = CTF_UseSimpleAsComplex;
				BS->InvalidatePhysicsData(); BS->CreatePhysicsMeshes();
				SM->PostEditChange();
				SaveAsset(SM);
			}
	}
	// ---- ағаштар: терек (биік, тік), тал (салбыраған), қарағаш (дөңгелек басты), жыңғыл (бұта) ----
	struct FTreeDef { const TCHAR* Name; UMaterialInterface* Mi; float H, R; int32 Depth, Kids, LeafN; float Ang, Grav, Twist, LeafRad, LeafL, LeafW, Droop; int32 Trunks; };
	const FTreeDef Trees[] = {
		{ TEXT("Poplar"), MiPoplar, 820.f, 24.f, 3, 3, 26, 28.f, 0.01f, 0.12f, 70.f, 11.f, 8.f, 0.2f, 1 },
		{ TEXT("Willow"), MiWillow, 380.f, 30.f, 2, 3, 30, 55.f, 0.16f, 0.16f, 90.f, 18.f, 6.f, 1.2f, 1 },
		{ TEXT("Elm"), MiElm, 520.f, 26.f, 3, 3, 30, 48.f, 0.03f, 0.18f, 95.f, 9.f, 7.f, 0.3f, 1 },
		{ TEXT("Tamarisk"), MiTamarisk, 95.f, 4.f, 2, 2, 18, 30.f, 0.02f, 0.25f, 40.f, 16.f, 1.2f, 0.2f, 5 },
	};
	for (const FTreeDef& Td : Trees)
		for (int32 v = 0; v < 2; v++)
		{
			FMeshBuild Wood, Leaf;
			FTreeP T; T.TrunkH = Td.H; T.TrunkR = Td.R; T.Lean = 0; T.BranchAng = Td.Ang; T.Droop = Td.Droop; T.Gravity = Td.Grav; T.LeafRad = Td.LeafRad; T.LeafL = Td.LeafL; T.LeafW = Td.LeafW; T.Depth = Td.Depth; T.Kids = Td.Kids; T.LeafN = Td.LeafN; T.Twist = Td.Twist;
			auto Tip = [&](const V3& P, const V3& D, float Rad)
			{
				if (Rad > Td.R * 0.5f && Td.Depth > 2) return;
				if (Td.Droop > 1.f)
				{
					// салбыраған шыбықтар (тал): ұзын тізбек, бойында жіңішке жапырақтар
					for (int32 k = 0; k < 4; k++)
					{
						V3 Q = P + V3(R.FRandRange(-30.f, 30.f), R.FRandRange(-30.f, 30.f), R.FRandRange(-10.f, 10.f));
						const float L = R.FRandRange(120.f, 240.f);
						const V3 Bd = V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0).GetSafeNormal();
						for (int32 s = 0; s < 6; s++)
						{
							const V3 Q2 = Q + (V3(0, 0, -1) + Bd * 0.2f).GetSafeNormal() * (L / 6.f);
							Wood.Tube(Q, Q2, 0.7f, 0.5f, 3);
							for (int32 l = 0; l < 3; l++) { const V3 O = (Bd + V3(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), -0.6f)).GetSafeNormal(); Blade(Leaf, Q + (Q2 - Q) * R.FRand(), O, Perp(O), Td.LeafL, Td.LeafW, V3(0, 0, -1), 3.f, 1, 0.6f); }
							Q = Q2;
						}
					}
				}
				else
				{
					// тұтас крона бұлты + шетінде жеке жапырақтар (сұлбасы жұмсақ болсын)
					const float Br = Td.LeafRad * R.FRandRange(0.9f, 1.4f);
					Blob(Leaf, R, P + V3(0, 0, Br * 0.25f), V3(Br * 1.15f, Br * 1.15f, Br * 0.85f));
					Leaves(Leaf, R, P + V3(0, 0, Br * 0.2f), Br * 1.25f, Td.LeafN, Td.LeafL * 2.2f, Td.LeafW * 2.4f, Td.Droop);
				}
			};
			for (int32 t = 0; t < Td.Trunks; t++)
			{
				const float A = R.FRandRange(0.f, UE_TWO_PI);
				const V3 D = (V3(FMath::Cos(A), FMath::Sin(A), 0) * (Td.Trunks > 1 ? R.FRandRange(0.3f, 0.6f) : R.FRandRange(0.02f, 0.1f)) + V3(0, 0, 1)).GetSafeNormal();
				Branch(Wood, Leaf, R, T, V3(FMath::Cos(A), FMath::Sin(A), 0) * (Td.Trunks > 1 ? 8.f : 0.f) + V3(0, 0, -20.f), D, Td.H * R.FRandRange(0.85f, 1.15f) * (Td.Depth > 2 ? 0.55f : 1.f), Td.R * R.FRandRange(0.9f, 1.1f), Td.Depth, Tip);
			}
			Wood.RecomputeNormals(); Wood.FixWinding(); Wood.RecomputeNormals();
			float MaxZ = 0; for (const V3& P : Leaf.P) MaxZ = FMath::Max(MaxZ, P.Z);
			SwayWeights(Leaf, MaxZ);
			SaveMesh(FString::Printf(TEXT("SM_KAP_%sTrunk_%d"), Td.Name, v), Wood, false, false);
			SaveWithMat(FString::Printf(TEXT("SM_KAP_%sLeaf_%d"), Td.Name, v), Leaf, Td.Mi);
			UE_LOG(LogTemp, Display, TEXT("KAPNATURE tree %s_%d: wood %d tris, leaves %d tris, h=%.0f"), Td.Name, v, Wood.Idx.Num() / 3, Leaf.Idx.Num() / 3, MaxZ);
		}
	// ---- ландшафт шөп түрлері (M_KAP_Landscape8 салмақтарымен GPU-да өседі) ----
	MakeGrass(TEXT("LGT_KAP_Steppe"), { { SmGrass, 2.4f, 2800, 4800, 0.7f, 1.35f, false }, { SmDry, 0.9f, 3200, 5200, 0.8f, 1.3f, false } });
	MakeGrass(TEXT("LGT_KAP_Wheat"), { { SmWheat, 1.7f, 4000, 6500, 0.9f, 1.12f, true } });
	MakeGrass(TEXT("LGT_KAP_Green"), { { SmGreen, 2.5f, 4000, 7000, 0.8f, 1.2f, false } });
	MakeGrass(TEXT("LGT_KAP_Reeds"), { { SmReed, 0.9f, 7000, 12000, 0.75f, 1.25f, true } });
	MakeGrass(TEXT("LGT_KAP_Sand"), { { SmSandGrass, 0.35f, 3000, 5000, 0.7f, 1.3f, false } });
	return true;
}

/** M_KAP_Animal: жануарлар терісі — вершина түсі маскалары (R екінші түс, G тұяқ/мүйіз/танау, B көз) + жүн шуы; жаңбырда жүн қараяды */
bool UKAPEditorLibrary::CreateAnimalMaterial()
{
	UMaterialParameterCollection* Env = KAPEnsureEnvMPC();
	UMaterial* M = NewMaterial(TEXT("M_KAP_Animal"));
	bool bD = false; M->SetMaterialUsage(bD, MATUSAGE_SkeletalMesh);
	auto* VC = Expr<UMaterialExpressionVertexColor>(M, -2000, 0);
	UMaterialExpression* mB = MaskC(M, VC, true, false, false, -1850, -60);
	UMaterialExpression* mH = MaskC(M, VC, false, true, false, -1850, 0);
	UMaterialExpression* mE = MaskC(M, VC, false, false, true, -1850, 60);
	UMaterialExpression* CoatA = VParam(M, TEXT("CoatA"), FLinearColor(0.23f, 0.13f, 0.07f), -1850, -300);
	UMaterialExpression* CoatB = VParam(M, TEXT("CoatB"), FLinearColor(0.05f, 0.04f, 0.035f), -1850, -200);
	UMaterialExpression* Hoof = VParam(M, TEXT("Hoof"), FLinearColor(0.035f, 0.03f, 0.028f), -1850, -100);
	auto* UV = Expr<UMaterialExpressionTextureCoordinate>(M, -2000, 300);
	auto* P3 = Expr<UMaterialExpressionAppendVector>(M, -1850, 300);
	UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, UV, K(M, 60.f, -2000, 380), -1900, 340), TEXT(""), P3, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(K(M, 0.f, -2000, 440), TEXT(""), P3, TEXT("B"));
	auto* Mottle = Expr<UMaterialExpressionNoise>(M, -1700, 300); Mottle->Scale = 0.08f; Mottle->Levels = 3; Mottle->OutputMin = 0.f; Mottle->OutputMax = 1.f; Mottle->Quality = 1;
	Mottle->Position.Connect(0, P3);
	auto* Fine = Expr<UMaterialExpressionNoise>(M, -1700, 450); Fine->Scale = 1.2f; Fine->Levels = 2; Fine->OutputMin = 0.f; Fine->OutputMax = 1.f; Fine->Quality = 1;
	Fine->Position.Connect(0, P3);
	UMaterialExpression* Coat = Lerp(M, CoatA, CoatB, mB, -1500, -250);
	Coat = Mul(M, Coat, Op2<UMaterialExpressionAdd>(M, Mul(M, Mottle, K(M, 0.35f, -1550, 360), -1450, 330), K(M, 0.8f, -1450, 390), -1350, 330), -1300, -200);
	Coat = Mul(M, Coat, Op2<UMaterialExpressionAdd>(M, Mul(M, Fine, K(M, 0.18f, -1550, 500), -1450, 480), K(M, 0.91f, -1450, 540), -1350, 480), -1150, -150);
	UMaterialExpression* C = Lerp(M, Coat, Hoof, mH, -1000, -100);
	C = Lerp(M, C, Col(M, FLinearColor(0.01f, 0.008f, 0.007f), -1150, 100), mE, -850, 0);
	UMaterialExpression* Wet = KAPEnvParam(M, Env, TEXT("Wetness"), -1150, 600);
	C = Mul(M, C, Op1<UMaterialExpressionOneMinus>(M, Mul(M, Wet, K(M, 0.28f, -1000, 650), -900, 600), -800, 600), -700, 0);
	UMaterialEditingLibrary::ConnectMaterialProperty(C, TEXT(""), MP_BaseColor);
	UMaterialExpression* Rg = Lerp(M, Lerp(M, K(M, 0.88f, -1150, 750), K(M, 0.45f, -1150, 810), mH, -1000, 760), K(M, 0.12f, -1000, 830), mE, -850, 780);
	Rg = Lerp(M, Rg, Mul(M, Rg, K(M, 0.55f, -850, 880), -700, 880), Wet, -550, 800);
	UMaterialEditingLibrary::ConnectMaterialProperty(Rg, TEXT(""), MP_Roughness);
	UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, K(M, 0.3f, -850, 950), K(M, 0.6f, -850, 1010), mE, -700, 960), TEXT(""), MP_Specular);
	UMaterialEditingLibrary::RecompileMaterial(M);
	KAPLogMatErrors(M);
	return SaveAsset(M);
}
