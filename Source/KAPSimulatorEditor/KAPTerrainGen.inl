// ---------------------------------------------------------------------
// Жаңа жер бедері (KAP/World/KAPTerrain.h): ландшафт, биом текстурасы, ауа райы MPC-сі,
// ландшафт/су/тау материалдары, шекарадағы алып таулар (Nanite).
// Іске қосу: Scripts/build_terrain.py (UnrealEditor-Cmd ... -run=pythonscript)
// ---------------------------------------------------------------------
#include "KAP/World/KAPTerrain.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Landscape.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMax.h"
#include "LandscapeGrassType.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"

namespace KAPTerrainGen
{
	struct FSiteC { float X, Y, R; };
	struct FData { TArray<FSiteC> Sites; TArray<FVector4f> Roads; };

	static FData LoadData()
	{
		FData D;
		auto LoadRows = [&](const TCHAR* Name, UScriptStruct* St, TFunction<void(FName, uint8*)> Fn)
		{
			FString Csv;
			if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectContentDir() / TEXT("KAP/Data") / (FString(Name) + TEXT(".csv"))))) return;
			UDataTable* DT = NewObject<UDataTable>(); DT->RowStruct = St; DT->CreateTableFromCSVString(Csv);
			for (const TPair<FName, uint8*>& R : DT->GetRowMap()) Fn(R.Key, R.Value);
		};
		LoadRows(TEXT("Locations"), FKAPLocationRow::StaticStruct(), [&](FName, uint8* P) { const FKAPLocationRow* R = (FKAPLocationRow*)P; D.Sites.Add({ R->X, R->Y, R->R }); });
		LoadRows(TEXT("Wells"), FKAPWellRow::StaticStruct(), [&](FName, uint8* P) { const FKAPWellRow* R = (FKAPWellRow*)P; D.Sites.Add({ R->X, R->Y, 9.f }); });
		LoadRows(TEXT("Vehicles"), FKAPVehicleRow::StaticStruct(), [&](FName, uint8* P) { const FKAPVehicleRow* R = (FKAPVehicleRow*)P; D.Sites.Add({ R->X, R->Y, 6.f }); });
		TMap<FName, FVector2f> Nodes;
		LoadRows(TEXT("Nodes"), FKAPNodeRow::StaticStruct(), [&](FName K, uint8* P) { const FKAPNodeRow* R = (FKAPNodeRow*)P; Nodes.Add(K, FVector2f(R->X, R->Y)); });
		LoadRows(TEXT("Roads"), FKAPRoadRow::StaticStruct(), [&](FName, uint8* P)
		{
			const FKAPRoadRow* R = (FKAPRoadRow*)P;
			if (const FVector2f* A = Nodes.Find(R->A)) if (const FVector2f* B = Nodes.Find(R->B)) D.Roads.Add(FVector4f(A->X, A->Y, B->X, B->Y));
		});
		return D;
	}
	static float RoadDist(const FData& D, float X, float Y)
	{
		float Best = 1e9f;
		for (const FVector4f& R : D.Roads)
		{
			const FVector2f A(R.X, R.Y), V(R.Z - R.X, R.W - R.Y), P(X, Y);
			const float T = FMath::Clamp(FVector2f::DotProduct(P - A, V) / FMath::Max(1e-6f, V.SizeSquared()), 0.f, 1.f);
			Best = FMath::Min(Best, FVector2f::Distance(P, A + V * T));
		}
		return Best;
	}
	static void InitFields(const FData& D)
	{
		KAPTerrain::BuildFields([&](float X, float Y) { return RoadDist(D, X, Y); },
			[&](float X, float Y, float Margin) { for (const FSiteC& S : D.Sites) if (FMath::Square(X - S.X) + FMath::Square(Y - S.Y) < FMath::Square(S.R + Margin)) return false; return true; });
	}
	/** Ландшафттың соңғы биіктігі: нысандар (ауыл, ұңғыма, көлік) өз орталығының биіктігіне тегістеледі */
	static float FinalHeight(const FData& D, const TArray<float>& SiteH, float X, float Y)
	{
		float H = KAPTerrain::Height(X, Y);
		for (int32 i = 0; i < D.Sites.Num(); i++)
		{
			const FSiteC& S = D.Sites[i];
			const float Dd = FMath::Sqrt(FMath::Square(X - S.X) + FMath::Square(Y - S.Y));
			const float Wt = 1.f - KAPTerrain::Smooth(S.R, S.R + 30.f, Dd);
			if (Wt > 0) H = FMath::Lerp(H, SiteH[i], Wt);
		}
		return H;
	}
	// ландшафт өлшемі (CreateDesertLandscape-пен бірдей): 2017² нүкте, 1.2 м
	constexpr int32 Comps = 16, Sections = 2, Quads = 63, Size = Comps * Sections * Quads + 1;
	constexpr float ScaleXY = 120.f;
	constexpr float HalfM = (Size - 1) * ScaleXY / 200.f; // 1209.6 м
}

bool UKAPEditorLibrary::CreateTerrainTextures()
{
	using namespace KAPTerrainGen;
	const FData D = LoadData();
	InitFields(D);
	UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN fields=%d sites=%d roads=%d"), KAPTerrain::FieldStore().Num(), D.Sites.Num(), D.Roads.Num());
	// ---- T_KAP_Biome (2048², сызықтық): R — шөп, G — егін (түрі: 1/3, 2/3, 1), B — тұрақты ылғал, A — құм ----
	{
		const int32 S = 2048;
		TArray<FColor> Px; Px.SetNumUninitialized(S * S);
		ParallelFor(S, [&](int32 y)
		{
			for (int32 x = 0; x < S; x++)
			{
				const float X = -HalfM + (x + 0.5f) / S * 2.f * HalfM, Y = -HalfM + (y + 0.5f) / S * 2.f * HalfM;
				int32 Crop = 0;
				const float Fm = KAPTerrain::FieldMask(X, Y, &Crop);
				const float Dm = KAPTerrain::DuneMask(X, Y), Wm = KAPTerrain::WetMask(X, Y), Hm = KAPTerrain::FoothillMask(X, Y);
				float Rd = RoadDist(D, X, Y);
				float Site = 1.f; for (const FSiteC& Sc : D.Sites) Site = FMath::Min(Site, KAPTerrain::Smooth(Sc.R * 0.6f, Sc.R + 12.f, FMath::Sqrt(FMath::Square(X - Sc.X) + FMath::Square(Y - Sc.Y))));
				float G = (1.f - Dm) * (1.f - Fm) * (0.35f + 0.65f * KAPTerrain::Fbm(X / 90.f + 1.7f, Y / 90.f - 3.3f, 3)) * (1.f - 0.65f * Hm);
				G = FMath::Max(G, Wm * 0.55f) * KAPTerrain::Smooth(3.f, 11.f, Rd) * Site; // жол мен нысан маңында шөп жоқ (ландшафт шөбі де осыны оқиды)
				float Wz; const bool bWater = KAPTerrain::WaterAt(X, Y, Wz);
				// G — егістік маскасы; A — құм, ал егістікте егін түрі × маска (материал A/G қатынасын алады: шетінде бүлінбейді)
					const float Code = Crop == 0 ? 0.3f : Crop == 1 ? 0.6f : 0.9f;
					Px[y * S + x] = FColor((uint8)FMath::Clamp(G * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(Fm * 255.f, 0.f, 255.f),
						(uint8)FMath::Clamp((bWater ? 1.f : Wm * 0.9f) * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(FMath::Lerp(Dm, Code, Fm) * 255.f, 0.f, 255.f));
			}
		});
		const FString Name = TEXT("T_KAP_Biome");
		UPackage* Pkg = MakePkg(TEXT("/Game/KAP/Textures/") + Name);
		UTexture2D* T = NewObject<UTexture2D>(Pkg, FName(*Name), RF_Public | RF_Standalone);
		T->Source.Init(S, S, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Px.GetData()));
		T->SRGB = false; T->CompressionSettings = TC_VectorDisplacementmap; T->LODGroup = TEXTUREGROUP_World;
		T->AddressX = TA_Clamp; T->AddressY = TA_Clamp; T->MipGenSettings = TMGS_FromTextureGroup;
		T->PostEditChange(); T->UpdateResource();
		FTextureCompilingManager::Get().FinishAllCompilation();
		SaveAsset(T);
	}
	// ---- T_KAP_WaterNormal (512², жіксіз): бүтін жиілікті толқындар қосындысы ----
	{
		const int32 S = 512;
		TArray<float> H; H.SetNumZeroed(S * S);
		FRandomStream R(20261023);
		struct FW { int32 A, B; float Ph, Amp; };
		TArray<FW> Ws;
		for (int32 i = 0; i < 24; i++) { const int32 Fq = 1 + i / 3; Ws.Add({ R.RandRange(-Fq * 2, Fq * 2), R.RandRange(-Fq * 2, Fq * 2), (float)R.FRandRange(0.f, UE_TWO_PI), 1.f / (1.f + i * 0.5f) }); }
		for (int32 y = 0; y < S; y++) for (int32 x = 0; x < S; x++)
		{
			float Sum = 0; for (const FW& W : Ws) Sum += W.Amp * FMath::Sin(UE_TWO_PI * (W.A * x + W.B * y) / S + W.Ph);
			H[y * S + x] = Sum;
		}
		TArray<FColor> Px; Px.SetNumUninitialized(S * S);
		for (int32 y = 0; y < S; y++) for (int32 x = 0; x < S; x++)
		{
			const float Dx = H[y * S + (x + 1) % S] - H[y * S + (x + S - 1) % S], Dy = H[((y + 1) % S) * S + x] - H[((y + S - 1) % S) * S + x];
			const FVector3f N = FVector3f(-Dx * 0.9f, -Dy * 0.9f, 1.f).GetSafeNormal();
			Px[y * S + x] = FColor((uint8)((N.X * 0.5f + 0.5f) * 255), (uint8)((N.Y * 0.5f + 0.5f) * 255), (uint8)((N.Z * 0.5f + 0.5f) * 255), 255);
		}
		SaveTex(TEXT("T_KAP_WaterNormal"), S, Px, true, false);
	}
	return true;
}

/** Материал компиляциясының қателерін логқа шығару (рендерлі командлетте) */
static void KAPLogMatErrors(UMaterial* M)
{
	if (!M) return;
	if (FMaterialResource* R = M->GetMaterialResource(GMaxRHIShaderPlatform))
	{
		R->FinishCompilation();
		const TArray<FString>& Er = R->GetCompileErrors();
		UE_LOG(LogTemp, Display, TEXT("KAPMAT %s: %d errors"), *M->GetName(), Er.Num());
		for (const FString& E : Er) UE_LOG(LogTemp, Error, TEXT("KAPMAT %s: %s"), *M->GetName(), *E);
	}
}
/** Ауа райы параметрлері: материалдар оқиды, AWeatherManager жазады */
static UMaterialParameterCollection* KAPEnsureEnvMPC()
{
	if (UMaterialParameterCollection* E = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/KAP/Materials/MPC_KAPEnv.MPC_KAPEnv"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return E;
	UPackage* P = MakePkg(TEXT("/Game/KAP/Materials/MPC_KAPEnv"));
	UMaterialParameterCollection* C = NewObject<UMaterialParameterCollection>(P, FName("MPC_KAPEnv"), RF_Public | RF_Standalone);
	for (const TCHAR* N : { TEXT("Wetness"), TEXT("Rain"), TEXT("WindX"), TEXT("WindY"), TEXT("WindSpeed"), TEXT("Puddles") })
	{
		FCollectionScalarParameter Sp; Sp.ParameterName = N; Sp.DefaultValue = FCString::Strcmp(N, TEXT("WindX")) == 0 ? 1.f : 0.f;
		C->ScalarParameters.Add(Sp);
	}
	C->PostEditChange();
	SaveAsset(C);
	return C;
}
static UMaterialExpression* KAPEnvParam(UMaterial* M, UMaterialParameterCollection* C, const TCHAR* N, int32 X, int32 Y)
{
	auto* E = Expr<UMaterialExpressionCollectionParameter>(M, X, Y);
	E->Collection = C; E->ParameterName = N; E->ParameterId = C->GetParameterId(N);
	return E;
}

bool UKAPEditorLibrary::CreateTerrainMaterials()
{
	UMaterialParameterCollection* Env = KAPEnsureEnvMPC();
	UTexture2D* TB = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_Biome.T_KAP_Biome"));
	UTexture2D* TN = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_SandNormal.T_KAP_SandNormal"));
	UTexture2D* TA = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_SandAlbedo.T_KAP_SandAlbedo"));
	UTexture2D* TW = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_WaterNormal.T_KAP_WaterNormal"));
	if (!Env || !TB || !TN || !TA || !TW) { UE_LOG(LogTemp, Error, TEXT("KAPTERRAIN: textures/MPC missing")); return false; }
	auto Samp = [](UMaterial* M, UTexture2D* T, UMaterialExpression* UV, EMaterialSamplerType ST, int32 X, int32 Y)
	{
		auto* S = Expr<UMaterialExpressionTextureSample>(M, X, Y); S->Texture = T; S->SamplerType = ST;
		S->Coordinates.Connect(0, UV);
		return S;
	};
	// ================= M_KAP_Landscape8: биомдар + жаңбыр ылғалы =================
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Landscape8"));
		auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -3600, 0);
		UMaterialExpression* XY = MaskC(M, WP, true, true, false, -3450, 0);
		// биом UV: (XY/100 + 1209.6) / 2419.2
		UMaterialExpression* BUV = Op2<UMaterialExpressionAdd>(M, Mul(M, XY, K(M, 1.f / 241920.f, -3300, 60), -3200, 0), K(M, 0.5f, -3200, 60), -3050, 0);
		auto* Bio = Samp(M, TB, BUV, SAMPLERTYPE_LinearColor, -2900, 0);
		UMaterialExpression* wGrass = MaskC(M, Bio, true, false, false, -2700, -100);
		UMaterialExpression* wField = MaskC(M, Bio, false, true, false, -2700, 0);
		UMaterialExpression* wWet = MaskC(M, Bio, false, false, true, -2700, 100);
		auto* SandA = Expr<UMaterialExpressionComponentMask>(M, -2700, 200); SandA->R = false; SandA->G = false; SandA->B = false; SandA->A = true; SandA->Input.Connect(5, Bio); // 5 — RGBA шығысы (0 — RGB, альфасыз)
		// A — құм (егістіктен тыс), егістікте егін түрі × маска: түрі = A / G (шетінде араласпайды)
		UMaterialExpression* CropCode = Op2<UMaterialExpressionDivide>(M, SandA, Op2<UMaterialExpressionMax>(M, wField, K(M, 0.05f, -2850, 260), -2700, 260), -2550, 230);
		UMaterialExpression* wSand = Mul(M, SandA, Op1<UMaterialExpressionOneMinus>(M, wField, -2700, 320), -2550, 300);
		// қашықтық, шу
		auto* CamP = Expr<UMaterialExpressionCameraPositionWS>(M, -3600, 500);
		auto* Dist = Expr<UMaterialExpressionLength>(M, -3300, 500);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionSubtract>(M, WP, CamP, -3450, 500), TEXT(""), Dist, TEXT(""));
		UMaterialExpression* Near = Ramp(M, Dist, 6000.f, -1.f / 5000.f, -3150, 500);
		auto* Macro = Noise(M, 0.00008f, 3, -3000, 700);
		auto* Mid = Noise(M, 0.0012f, 2, -3000, 850);
		auto* Fine = Noise(M, 0.02f, 2, -3000, 1000);
		auto* VN = Expr<UMaterialExpressionVertexNormalWS>(M, -3000, 1150);
		UMaterialExpression* Rock = Ramp(M, Op1<UMaterialExpressionOneMinus>(M, MaskC(M, VN, false, false, true, -2850, 1150), -2700, 1150), 0.13f, 7.f, -2550, 1150);
		// құм (бұрынғы құм текстурасы, бір масштаб)
		UMaterialExpression* UV1 = Mul(M, XY, K(M, 1.f / 300.f, -3300, 300), -3150, 300);
		auto* SA = Samp(M, TA, UV1, SAMPLERTYPE_LinearColor, -2500, 300);
		auto* SN = Samp(M, TN, UV1, SAMPLERTYPE_Normal, -2500, 450);
		UMaterialExpression* SandC = Lerp(M, Col(M, FLinearColor(0.58f, 0.44f, 0.29f), -2300, 200), Col(M, FLinearColor(0.72f, 0.58f, 0.4f), -2300, 260), Macro, -2150, 220);
		SandC = Mul(M, SandC, Lerp(M, K(M, 1.f, -2150, 330), Mul(M, MaskC(M, SA, true, false, false, -2300, 330), K(M, 1.25f, -2300, 380), -2150, 380), Near, -2000, 330), -1850, 250);
		// дала: құрғақ топырақ + шөп (сары → жасыл), жақында ұсақ дақтар
		UMaterialExpression* Soil = Lerp(M, Col(M, FLinearColor(0.33f, 0.28f, 0.21f), -2300, 600), Col(M, FLinearColor(0.42f, 0.36f, 0.27f), -2300, 660), Macro, -2150, 620);
		UMaterialExpression* GrassC = Lerp(M, Col(M, FLinearColor(0.44f, 0.41f, 0.22f), -2300, 760), Col(M, FLinearColor(0.24f, 0.31f, 0.12f), -2300, 820), Mid, -2150, 780);
		UMaterialExpression* GrassAmt = Sat(M, Mul(M, wGrass, Lerp(M, K(M, 1.25f, -2300, 900), Mul(M, Fine, K(M, 1.9f, -2300, 960), -2150, 960), Near, -2050, 900), -1950, 900), -1850, 900);
		UMaterialExpression* SteppeC = Lerp(M, Soil, GrassC, GrassAmt, -1800, 700);
		// егін: түрін G мәнінен ажыратамыз (1/3 бидай, 2/3 жасыл, 1 жыртылған), жолақтар 75 см
		UMaterialExpression* FieldOn = Ramp(M, wField, 0.3f, 8.f, -2300, 1300);
		UMaterialExpression* CropC = Lerp(M, Col(M, FLinearColor(0.66f, 0.53f, 0.26f), -2300, 1400), Col(M, FLinearColor(0.24f, 0.36f, 0.12f), -2300, 1460), Ramp(M, CropCode, 0.45f, 20.f, -2300, 1520), -2000, 1420);
		CropC = Lerp(M, CropC, Col(M, FLinearColor(0.27f, 0.2f, 0.14f), -2000, 1520), Ramp(M, CropCode, 0.75f, 20.f, -2300, 1600), -1850, 1450);
		UMaterialExpression* RowS = Op1<UMaterialExpressionFrac>(M, Mul(M, MaskC(M, WP, false, true, false, -2450, 1700), K(M, 1.f / 75.f, -2450, 1760), -2300, 1700), -2150, 1700);
		UMaterialExpression* RowSin = Op1<UMaterialExpressionSine>(M, Mul(M, RowS, K(M, UE_TWO_PI, -2150, 1760), -2000, 1700), -1850, 1700);
		CropC = Mul(M, CropC, Op2<UMaterialExpressionAdd>(M, Mul(M, Mul(M, RowSin, K(M, 0.12f, -1850, 1760), -1700, 1700), Near, -1600, 1760), K(M, 0.96f, -1700, 1760), -1550, 1700), -1450, 1450); // жолақтар алыста сөнеді (муар болмасын)
		// ылғалды жер (жаға, батпақ): қара балшық + қою шөп
		UMaterialExpression* WetC = Lerp(M, Col(M, FLinearColor(0.16f, 0.13f, 0.1f), -2300, 1900), Col(M, FLinearColor(0.18f, 0.25f, 0.1f), -2300, 1960), Mid, -2150, 1920);
		UMaterialExpression* RockC = Lerp(M, Col(M, FLinearColor(0.32f, 0.3f, 0.28f), -2300, 2100), Col(M, FLinearColor(0.45f, 0.41f, 0.36f), -2300, 2160), Mid, -2150, 2120);
		UMaterialExpression* C = Lerp(M, SteppeC, SandC, wSand, -1500, 300);
		C = Lerp(M, C, CropC, FieldOn, -1350, 400);
		C = Lerp(M, C, WetC, wWet, -1200, 500);
		C = Lerp(M, C, RockC, Rock, -1050, 600);
		// жаңбыр: топырақ қараяды, ойпаңда шалшық
		UMaterialExpression* Wetness = KAPEnvParam(M, Env, TEXT("Wetness"), -1500, 2400);
		UMaterialExpression* Porous = Op1<UMaterialExpressionOneMinus>(M, Mul(M, wSand, K(M, 0.5f, -1500, 2500), -1350, 2500), -1200, 2500);
		UMaterialExpression* Dark = Mul(M, Mul(M, Wetness, Porous, -1050, 2450), K(M, 0.3f, -1050, 2510), -900, 2450);
		C = Mul(M, C, Op1<UMaterialExpressionOneMinus>(M, Dark, -750, 2450), -900, 650);
		// шалшық: ұсақ (≈8 м) ойпаңдар, ірі шу аз әсер етеді — бүкіл дала айнаға айналмасын
		UMaterialExpression* Low = Op2<UMaterialExpressionAdd>(M, Op2<UMaterialExpressionAdd>(M, Mul(M, Mid, K(M, 0.6f, -1650, 2650), -1500, 2650), Mul(M, Macro, K(M, 0.4f, -1650, 2700), -1500, 2700), -1350, 2650), Mul(M, wWet, K(M, 0.35f, -1500, 2760), -1350, 2760), -1200, 2650);
		UMaterialExpression* Puddle = Mul(M, Mul(M, Ramp(M, Wetness, 0.5f, 3.f, -1050, 2600), Ramp(M, Low,  0.7f, 9.f, -1050, 2700), -900, 2650), Mul(M, Op1<UMaterialExpressionOneMinus>(M, Rock, -1050, 2800), Porous, -900, 2800), -750, 2700);
		C = Lerp(M, C, Mul(M, C, K(M, 0.55f, -750, 750), -600, 750), Puddle, -450, 650);
		UMaterialEditingLibrary::ConnectMaterialProperty(C, TEXT(""), MP_BaseColor);
		// кедір-бұдырлық
		UMaterialExpression* Rg = Lerp(M, K(M, 0.95f, -1500, 3000), K(M, 0.92f, -1500, 3060), wSand, -1350, 3000);
		Rg = Lerp(M, Rg, K(M, 0.55f, -1350, 3100), wWet, -1200, 3000);
		Rg = Lerp(M, Rg, K(M, 0.86f, -1200, 3100), Rock, -1050, 3000);
		Rg = Lerp(M, Rg, K(M, 0.32f, -1050, 3100), Mul(M, Wetness, Porous, -1050, 3160), -900, 3000);
		Rg = Lerp(M, Rg, K(M, 0.05f, -900, 3100), Puddle, -750, 3000);
		UMaterialEditingLibrary::ConnectMaterialProperty(Rg, TEXT(""), MP_Roughness);
		// нормаль: құм толқындары (құмда, жақында) + егін жолақтары; шалшық тегістейді
		UMaterialExpression* Nsand = Mul(M, MaskC(M, SN, true, true, false, -2300, 3300), Mul(M, Mul(M, wSand, Near, -2150, 3360), Op1<UMaterialExpressionOneMinus>(M, Rock, -2150, 3420), -2000, 3360), -1850, 3300);
		auto* RowN = Expr<UMaterialExpressionAppendVector>(M, -1850, 3500);
		UMaterialEditingLibrary::ConnectMaterialExpressions(K(M, 0.f, -2000, 3500), TEXT(""), RowN, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, Mul(M, Op1<UMaterialExpressionCosine>(M, Mul(M, RowS, K(M, UE_TWO_PI, -2300, 3600), -2150, 3600), -2000, 3600), K(M, 0.22f, -2000, 3660), -1850, 3600), FieldOn, -1700, 3600), TEXT(""), RowN, TEXT("B"));
		UMaterialExpression* Nxy = Mul(M, Op2<UMaterialExpressionAdd>(M, Nsand, RowN, -1600, 3400), Op1<UMaterialExpressionOneMinus>(M, Puddle, -1600, 3500), -1450, 3400);
		auto* Nfull = Expr<UMaterialExpressionAppendVector>(M, -1300, 3400);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Nxy, TEXT(""), Nfull, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(K(M, 1.f, -1450, 3500), TEXT(""), Nfull, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(Op1<UMaterialExpressionNormalize>(M, Nfull, -1150, 3400), TEXT(""), MP_Normal);
		// ---- ландшафт шөбі (GPU): дала шөбі, бидай, жас егін, қамыс, құм шөбі — биом текстурасынан (MPC емес: шөп картасы бір рет есептеледі) ----
		{
			auto Load = [](const TCHAR* N) { return LoadObject<ULandscapeGrassType>(nullptr, *FString::Printf(TEXT("/Game/KAP/Foliage/%s.%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet); };
			auto* GO = Expr<UMaterialExpressionLandscapeGrassOutput>(M, -300, 3900);
			GO->GrassTypes.Reset();
			auto AddG = [&](const TCHAR* N, const TCHAR* Asset, UMaterialExpression* W) { if (ULandscapeGrassType* T = Load(Asset)) { FGrassInput In{ FName(N) }; In.GrassType = T; In.Input.Connect(0, W); GO->GrassTypes.Add(In); } };
			UMaterialExpression* NoRock = Op1<UMaterialExpressionOneMinus>(M, Ramp(M, Op1<UMaterialExpressionOneMinus>(M, MaskC(M, VN, false, false, true, -1500, 4100), -1350, 4100), 0.1f, 6.f, -1200, 4100), -900, 4100);
			UMaterialExpression* NoField = Op1<UMaterialExpressionOneMinus>(M, FieldOn, -900, 4200);
			UMaterialExpression* Steppe = Mul(M, Mul(M, Sat(M, Mul(M, wGrass, K(M, 1.4f, -1050, 4300), -900, 4300), -750, 4300), NoField, -600, 4250), NoRock, -450, 4200);
			UMaterialExpression* NotWheat = Ramp(M, CropCode, 0.45f, 20.f, -1050, 4450);
			UMaterialExpression* Wheat = Mul(M, FieldOn, Op1<UMaterialExpressionOneMinus>(M, NotWheat, -750, 4450), -600, 4400);
			UMaterialExpression* Green = Mul(M, Mul(M, FieldOn, NotWheat, -750, 4550), Op1<UMaterialExpressionOneMinus>(M, Ramp(M, CropCode, 0.75f, 20.f, -1050, 4600), -750, 4600), -600, 4550);
			UMaterialExpression* Reed = Mul(M, Mul(M, Ramp(M, wWet, 0.42f, 5.f, -1050, 4700), Op1<UMaterialExpressionOneMinus>(M, Ramp(M, wWet, 0.93f, 40.f, -1050, 4760), -750, 4760), -600, 4700),
				Mul(M, Ramp(M, wGrass, 0.04f, 25.f, -1050, 4850), Ramp(M, Mid, 0.3f, 3.f, -1050, 4910), -750, 4850), -450, 4750);
			UMaterialExpression* SandG = Mul(M, Mul(M, wSand, NoRock, -750, 5000), Ramp(M, Mid, 0.45f, 4.f, -1050, 5000), -600, 5000);
			AddG(TEXT("Steppe"), TEXT("LGT_KAP_Steppe"), Steppe);
			AddG(TEXT("Wheat"), TEXT("LGT_KAP_Wheat"), Wheat);
			AddG(TEXT("Green"), TEXT("LGT_KAP_Green"), Green);
			AddG(TEXT("Reeds"), TEXT("LGT_KAP_Reeds"), Reed);
			AddG(TEXT("Sand"), TEXT("LGT_KAP_Sand"), SandG);
			UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN landscape grass types: %d"), GO->GrassTypes.Num());
		}
		UMaterialEditingLibrary::RecompileMaterial(M);
		KAPLogMatErrors(M);
		SaveAsset(M);
	}
	// ================= M_KAP_Water: бір қабатты су (SingleLayerWater), жел бағытымен толқындар =================
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Water"));
		M->SetShadingModel(MSM_SingleLayerWater);
		auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -2400, 0);
		UMaterialExpression* XY = MaskC(M, WP, true, true, false, -2250, 0);
		auto* T = Expr<UMaterialExpressionTime>(M, -2400, 200);
		UMaterialExpression* WindX = KAPEnvParam(M, Env, TEXT("WindX"), -2400, 300), *WindY = KAPEnvParam(M, Env, TEXT("WindY"), -2400, 360);
		UMaterialExpression* WSpd = KAPEnvParam(M, Env, TEXT("WindSpeed"), -2400, 420);
		auto* WDir = Expr<UMaterialExpressionAppendVector>(M, -2250, 330);
		UMaterialEditingLibrary::ConnectMaterialExpressions(WindX, TEXT(""), WDir, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WindY, TEXT(""), WDir, TEXT("B"));
		// толқын жел бағытымен жылжиды; жел күшейгенде тезірек әрі биігірек
		UMaterialExpression* Speed = Op2<UMaterialExpressionAdd>(M, K(M, 0.012f, -2250, 450), Mul(M, WSpd, K(M, 0.004f, -2250, 510), -2100, 480), -1950, 450);
		UMaterialExpression* Off = Mul(M, Mul(M, WDir, T, -2100, 300), Speed, -1950, 300);
		UMaterialExpression* UVa = Op2<UMaterialExpressionAdd>(M, Mul(M, XY, K(M, 1.f / 420.f, -2100, 0), -1950, 0), Off, -1800, 0);
		UMaterialExpression* UVb = Op2<UMaterialExpressionSubtract>(M, Mul(M, XY, K(M, 1.f / 1350.f, -2100, 100), -1950, 100), Mul(M, Off, K(M, 0.37f, -1950, 160), -1800, 160), -1650, 100);
		auto* Na = Samp(M, TW, UVa, SAMPLERTYPE_Normal, -1500, 0);
		auto* Nb = Samp(M, TW, UVb, SAMPLERTYPE_Normal, -1500, 200);
		UMaterialExpression* Rain = KAPEnvParam(M, Env, TEXT("Rain"), -1500, 400);
		UMaterialExpression* Amp = Op2<UMaterialExpressionAdd>(M, K(M, 0.35f, -1350, 450), Op2<UMaterialExpressionAdd>(M, Mul(M, WSpd, K(M, 0.05f, -1350, 510), -1200, 480), Mul(M, Rain, K(M, 0.5f, -1350, 570), -1200, 540), -1050, 500), -900, 450);
		UMaterialExpression* Nxy = Mul(M, Op2<UMaterialExpressionAdd>(M, MaskC(M, Na, true, true, false, -1300, 0), MaskC(M, Nb, true, true, false, -1300, 200), -1150, 100), Amp, -1000, 100);
		auto* Nf = Expr<UMaterialExpressionAppendVector>(M, -850, 100);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Nxy, TEXT(""), Nf, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(K(M, 1.f, -1000, 200), TEXT(""), Nf, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(Op1<UMaterialExpressionNormalize>(M, Nf, -700, 100), TEXT(""), MP_Normal);
		UMaterialEditingLibrary::ConnectMaterialProperty(Col(M, FLinearColor(0.03f, 0.06f, 0.05f), -700, -100), TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Op2<UMaterialExpressionAdd>(M, K(M, 0.03f, -850, 300), Mul(M, Rain, K(M, 0.08f, -850, 360), -700, 330), -550, 300), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.f, -700, 400), TEXT(""), MP_Metallic);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.5f, -700, 450), TEXT(""), MP_Specular);
		// лайлы дала өзені: жасыл-қоңыр, тереңдігі 1–2 м-де түбі көмескі
		auto* Out = Expr<UMaterialExpressionSingleLayerWaterMaterialOutput>(M, -400, 600);
		Out->ScatteringCoefficients.Connect(0, Col(M, FLinearColor(0.05f, 0.11f, 0.09f), -700, 600));
		Out->AbsorptionCoefficients.Connect(0, Col(M, FLinearColor(0.55f, 0.22f, 0.16f), -700, 700));
		Out->PhaseG.Connect(0, K(M, 0.15f, -700, 800));
		UMaterialEditingLibrary::RecompileMaterial(M);
		KAPLogMatErrors(M);
		SaveAsset(M);
	}
	// ================= M_KAP_Mountain: жартас, биікте қар, етекте шөп (бәрі әлем кеңістігінде) =================
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Mountain"));
		bool bD = false; M->SetMaterialUsage(bD, MATUSAGE_Nanite);
		auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -2400, 0);
		UMaterialExpression* Z = MaskC(M, WP, false, false, true, -2250, 0);
		auto* VN = Expr<UMaterialExpressionVertexNormalWS>(M, -2400, 300);
		UMaterialExpression* Nz = MaskC(M, VN, false, false, true, -2250, 300);
		auto* Big = Noise(M, 0.00001f, 3, -2400, 500);
		auto* Mid = Noise(M, 0.0006f, 3, -2400, 650);
		// қабаттар: биіктікке қарай жолақтар (шу араластырып)
		UMaterialExpression* Strata = Op1<UMaterialExpressionFrac>(M, Op2<UMaterialExpressionAdd>(M, Mul(M, Z, K(M, 1.f / 5200.f, -2250, 800), -2100, 800), Mul(M, Mid, K(M, 0.8f, -2250, 860), -2100, 860), -1950, 800), -1800, 800);
		UMaterialExpression* RockC = Lerp(M, Col(M, FLinearColor(0.27f, 0.25f, 0.24f), -1950, 100), Col(M, FLinearColor(0.44f, 0.4f, 0.35f), -1950, 160), Big, -1800, 120);
		RockC = Mul(M, RockC, Op2<UMaterialExpressionAdd>(M, Mul(M, Strata, K(M, 0.22f, -1800, 900), -1650, 850), K(M, 0.85f, -1650, 910), -1500, 850), -1650, 150);
		// етек: жайпақ төменгі беткейде шөп
		UMaterialExpression* Low = Mul(M, Ramp(M, Nz, 0.78f, 8.f, -1950, 1100), Ramp(M, Z, 80000.f, -1.f / 30000.f, -1950, 1200), -1800, 1150);
		UMaterialExpression* C = Lerp(M, RockC, Col(M, FLinearColor(0.32f, 0.31f, 0.17f), -1650, 1050), Low, -1500, 300);
		// қар: 2.0 км-ден жоғары, тік емес беткейлерде
		UMaterialExpression* Snow = Mul(M, Ramp(M, Op2<UMaterialExpressionAdd>(M, Z, Mul(M, Big, K(M, 40000.f, -1950, 1450), -1800, 1450), -1650, 1400), 205000.f, 1.f / 25000.f, -1500, 1400), Ramp(M, Nz, 0.5f, 5.f, -1500, 1550), -1350, 1450);
		C = Lerp(M, C, Col(M, FLinearColor(0.86f, 0.88f, 0.9f), -1350, 1350), Snow, -1200, 400);
		UMaterialEditingLibrary::ConnectMaterialProperty(C, TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, K(M, 0.88f, -1350, 1700), K(M, 0.45f, -1350, 1760), Snow, -1200, 1700), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::RecompileMaterial(M);
		KAPLogMatErrors(M);
		SaveAsset(M);
	}
	return true;
}

/** Шекарадағы алып таулар: ландшафт шетінен (≈150 м) басталып, 3 км шыңдарға жетеді; шығыста өзен шатқалы */
bool UKAPEditorLibrary::CreateMountainMesh()
{
	using namespace KAPTerrain;
	const int32 NA = 1600, NS = 260;
	const float Rb0 = KAPTerrainGen::HalfM;
	FMeshBuild B;
	B.P.Reserve(NA * NS); B.N.Reserve(NA * NS); B.UV.Reserve(NA * NS);
	auto SuperR = [&](float A) { const float C = FMath::Abs(FMath::Cos(A)), S = FMath::Abs(FMath::Sin(A)); return Rb0 / FMath::Pow(FMath::Pow(C, 10.f) + FMath::Pow(S, 10.f), 0.1f); };
	for (int32 i = 0; i < NA; i++)
	{
		const float A = i * UE_TWO_PI / NA;
		const float Ca = FMath::Cos(A), Sa = FMath::Sin(A);
		const float Re = SuperR(A);
		// ландшафт шетіндегі биіктік (тігіс көрінбесін)
		const float Ex = Ca * (Re - 3.f), Ey = Sa * (Re - 3.f);
		const float He = Height(Ex, Ey);
		for (int32 j = 0; j < NS; j++)
		{
			const float T = j / (float)(NS - 1);
			const float S = -40.f + 15000.f * FMath::Pow(T, 1.7f);
			const float X = Ca * (Re + S), Y = Sa * (Re + S);
			float H;
			if (S < 0.f) H = Height(FMath::Clamp(X, -Rb0 + 1, Rb0 - 1), FMath::Clamp(Y, -Rb0 + 1, Rb0 - 1)) - 4.f;
			else
			{
				// бөктер (0–3 км): 150–450 м төбелер; негізгі жота (2–10 км): 2.4–3.6 км шыңдар; беткейлер ~20–35°
				const float Hills = Smooth(0.f, 1400.f, S) * (90.f + 240.f * Ridged(X / 700.f + 4.1f, Y / 700.f - 2.7f, 5)) * (1.f - 0.4f * Smooth(4000.f, 10000.f, S));
				const float Rise = FMath::Pow(Smooth(1600.f, 10500.f, S), 1.3f);
				const float Peaks = 0.5f + 0.5f * Fbm(X / 3600.f + 3.3f, Y / 3600.f - 8.1f, 3);          // қай жотаның биік екені
				const float Ridges = Ridged(X / 1400.f + 1.1f, Y / 1400.f + 7.7f, 6);                        // жоталар мен сайлар
				const float Detail = Ridged(X / 260.f - 4.2f, Y / 260.f + 2.9f, 4) * Smooth(0.f, 900.f, S);
				H = He + Hills + Rise * (1500.f + 1900.f * Peaks) * (0.55f + 0.7f * Ridges) + Detail * (18.f + 70.f * Rise);
				// шығыстағы өзен аңғары: жоталарды кесіп өтеді, тауға қарай кеңейеді
				if (X > Rb0 - 5.f)
				{
					const float Dy = FMath::Abs(Y - GorgeY(X));
					const float Floor = GorgeFloor(X);
					const float Cut = 1.f - Smooth(40.f, 260.f + S * 0.12f, Dy);
					const float Bed = Dy < 14.f ? Floor + 0.6f * FMath::Square(Dy / 14.f) : Floor + 0.6f + (Dy - 14.f) * 0.08f + (Dy - 14.f) * (Dy - 14.f) * 0.0015f;
					H = FMath::Lerp(H, FMath::Min(H, Bed), Cut * (1.f - Smooth(7000.f, 11000.f, S)));
				}
			}
			B.P.Add(FVector3f(X * 100.f, Y * 100.f, H * 100.f));
			B.N.Add(FVector3f(0, 0, 1));
			B.UV.Add(FVector2f(i / (float)NA, T));
		}
	}
	for (int32 i = 0; i < NA; i++)
	{
		const int32 i2 = (i + 1) % NA;
		for (int32 j = 0; j + 1 < NS; j++)
		{
			const int32 a = i * NS + j, b = i2 * NS + j, c = i * NS + j + 1, d = i2 * NS + j + 1;
			B.Idx.Append({ a, c, b, b, c, d });
		}
	}
	B.RecomputeNormals();
	for (FVector3f& N : B.N) if (N.Z < 0) N = -N;
	B.FixWinding();
	B.RecomputeNormals();
	const bool bOk = SaveMesh(TEXT("SM_KAP_Mountains"), B, true, false);
	if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/KAP/Meshes/SM_KAP_Mountains.SM_KAP_Mountains")))
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Mountain.M_KAP_Mountain")))
		{
			SM->SetMaterial(0, Mat);
			SM->PostEditChange();
			SaveAsset(SM);
		}
	UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN mountains verts=%d tris=%d ok=%d"), B.P.Num(), B.Idx.Num() / 3, bOk ? 1 : 0);
	return bOk;
}

/** Картадағы ландшафтты жаңа бедермен ауыстыру (ескі ландшафт өшіріледі), M_KAP_Landscape8 тағайындалады, карта сақталады */
bool UKAPEditorLibrary::RebuildTerrain(const FString& MapPath)
{
	using namespace KAPTerrainGen;
	UWorld* W = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!W) return false;
	const FData D = LoadData();
	TArray<float> SiteH; for (const FSiteC& S : D.Sites) SiteH.Add(KAPTerrain::Height(S.X, S.Y));
	for (const FSiteC& S : D.Sites)
	{
		float Hw; const float Dc = KAPTerrain::ChannelDist(S.X, S.Y, Hw);
		if (Dc < Hw * 1.5f + S.R + 30.f) UE_LOG(LogTemp, Warning, TEXT("KAPTERRAIN site near water: (%.0f, %.0f) R=%.0f dist=%.0f"), S.X, S.Y, S.R, Dc);
		if (KAPTerrain::FoothillMask(S.X, S.Y) > 0.01f || KAPTerrain::LakeDist(S.X, S.Y) < KAPTerrain::LakeR + S.R + 20.f || KAPTerrain::OasisDist(S.X, S.Y) < KAPTerrain::OasisR + S.R + 10.f)
			UE_LOG(LogTemp, Warning, TEXT("KAPTERRAIN site in foothill/lake/oasis: (%.0f, %.0f) R=%.0f"), S.X, S.Y, S.R);
	}
	int32 Removed = 0;
	for (TActorIterator<ALandscapeProxy> It(W); It; ++It) { W->EditorDestroyActor(*It, true); Removed++; }
	TArray<uint16> Data; Data.SetNumUninitialized(Size * Size);
	ParallelFor(Size, [&](int32 j)
	{
		for (int32 i = 0; i < Size; i++)
		{
			const float Xm = (i - (Size - 1) * 0.5f) * ScaleXY / 100.f, Ym = (j - (Size - 1) * 0.5f) * ScaleXY / 100.f;
			Data[j * Size + i] = (uint16)FMath::Clamp(32768.f + FinalHeight(D, SiteH, Xm, Ym) * 128.f, 0.f, 65535.f);
		}
	});
	TMap<FGuid, TArray<uint16>> HeightLayers; HeightLayers.Add(FGuid(), Data);
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MatLayers; MatLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
	const FVector Loc(-(Size - 1) * ScaleXY * 0.5f, -(Size - 1) * ScaleXY * 0.5f, 0);
	ALandscape* L = W->SpawnActor<ALandscape>(Loc, FRotator::ZeroRotator);
	L->SetActorRelativeScale3D(FVector(ScaleXY, ScaleXY, 100.f));
	L->LandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Landscape8.M_KAP_Landscape8"));
	L->StaticLightingLOD = 1;
	L->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, Sections, Quads, HeightLayers, TEXT(""), MatLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	if (ULandscapeInfo* Info = L->GetLandscapeInfo()) Info->UpdateLayerInfoMap(L);
	L->SetActorLabel(TEXT("KAP_Steppe_Landscape"));
	// 5.8: ландшафт әрқашан edit-layer — соңғы биіктік картасы GPU-да біріктіріледі (командлет -AllowCommandletRendering-мен іске қосылуы керек)
	L->ForceUpdateLayersContent();
	FlushRenderingCommands();
	{
		const TArray<FVector2D> Probe = { FVector2D(0, 0), FVector2D(300, KAPTerrain::RiverY(300.f)), FVector2D(60, -440), FVector2D(1150, 0), FVector2D(-960, 15) };
		for (const FVector2D& P : Probe)
		{
			TOptional<float> Hz = L->GetHeightAtLocation(FVector(P.X * 100, P.Y * 100, 0));
			UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN probe (%.0f,%.0f): landscape=%.2f m expected=%.2f m"), P.X, P.Y, Hz.IsSet() ? Hz.GetValue() / 100.f : -9999.f, FinalHeight(D, SiteH, P.X, P.Y));
		}
	}
	const bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(W, MapPath);
	UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN landscape rebuilt (removed %d) saved=%d"), Removed, bSaved ? 1 : 0);
	return bSaved;
}

bool UKAPEditorLibrary::BuildTerrainAll(const FString& MapPath)
{
	const bool a = CreateTerrainTextures();
	const bool n = CreateNatureAssets();
	const bool b = CreateTerrainMaterials();
	const bool c = CreateMountainMesh();
	const bool d = RebuildTerrain(MapPath);
	UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN all: tex=%d nature=%d mat=%d mtn=%d land=%d"), a, n, b, c, d);
	return a && n && b && c && d;
}

/** Ландшафттан басқасының бәрі (NullRHI командлетте жылдам): текстуралар, өсімдіктер, материалдар, таулар */
bool UKAPEditorLibrary::BuildTerrainAssets(const FString& MapPath)
{
	const bool a = CreateTerrainTextures();
	const bool n = CreateNatureAssets();
	const bool b = CreateTerrainMaterials();
	const bool c = CreateMountainMesh();
	UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN assets: tex=%d nature=%d mat=%d mtn=%d"), a, n, b, c);
	return a && n && b && c;
}

/** Ойыншының бастапқы нүктесі: картада PlayerStart жоқ болса ойыншы (0,0,0)-де туады — жаңа бедер оны жерге көміп тастайды.
 *  Жердің үстіне PlayerStart қояды (бар болса — биіктігін түзетеді) */
bool UKAPEditorLibrary::PlacePlayerStart(const FString& MapPath)
{
	UWorld* W = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!W) return false;
	const float Hm = KAPTerrain::Height(0.f, 0.f);
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(W); It; ++It) Starts.Add(*It);
	if (!Starts.Num())
	{
		APlayerStart* PS = W->SpawnActor<APlayerStart>(FVector(0, 0, Hm * 100.f + 140.f), FRotator::ZeroRotator);
		if (PS) { PS->SetActorLabel(TEXT("KAP_PlayerStart")); Starts.Add(PS); }
	}
	for (APlayerStart* PS : Starts)
	{
		const FVector L = PS->GetActorLocation();
		PS->SetActorLocation(FVector(L.X, L.Y, KAPTerrain::Height(L.X / 100.f, L.Y / 100.f) * 100.f + 140.f));
	}
	const bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(W, MapPath);
	UE_LOG(LogTemp, Display, TEXT("KAPTERRAIN player starts=%d (ground %.2f m) saved=%d"), Starts.Num(), Hm, bSaved ? 1 : 0);
	return bSaved;
}
