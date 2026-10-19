#include "KAPEditorLibrary.h"
#include "KAP/KAPTypes.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/World/WeatherManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Engine/Texture2D.h"
#include "TextureCompiler.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LevelEditorSubsystem.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "FileHelpers.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------
// Көмекшілер
// ---------------------------------------------------------------------
static UPackage* MakePkg(const FString& Path) { UPackage* P = CreatePackage(*Path); P->FullyLoad(); return P; }
static bool SaveAsset(UObject* Asset)
{
	UPackage* P = Asset->GetOutermost();
	FAssetRegistryModule::AssetCreated(Asset);
	P->MarkPackageDirty();
	const FString File = FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(P, Asset, *File, Args);
}
template <class T> static T* Expr(UMaterial* M, int32 X, int32 Y) { return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass(), X, Y)); }
static UMaterial* NewMaterial(const FString& Name)
{
	const FString Path = TEXT("/Game/KAP/Materials/") + Name;
	UPackage* P = MakePkg(Path);
	UMaterialFactoryNew* F = NewObject<UMaterialFactoryNew>();
	return Cast<UMaterial>(F->FactoryCreateNew(UMaterial::StaticClass(), P, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
}
static UMaterialExpressionVectorParameter* VParam(UMaterial* M, const TCHAR* N, FLinearColor D, int32 X, int32 Y) { auto* E = Expr<UMaterialExpressionVectorParameter>(M, X, Y); E->ParameterName = N; E->DefaultValue = D; return E; }
static UMaterialExpressionScalarParameter* SParam(UMaterial* M, const TCHAR* N, float D, int32 X, int32 Y) { auto* E = Expr<UMaterialExpressionScalarParameter>(M, X, Y); E->ParameterName = N; E->DefaultValue = D; return E; }
static UMaterialExpressionNoise* Noise(UMaterial* M, float Scale, int32 Levels, int32 X, int32 Y)
{
	auto* N = Expr<UMaterialExpressionNoise>(M, X, Y);
	N->Scale = Scale; N->Levels = Levels; N->OutputMin = 0.f; N->OutputMax = 1.f; N->Quality = 1;
	return N;
}
static UMaterialExpressionConstant3Vector* Col(UMaterial* M, FLinearColor C, int32 X, int32 Y) { auto* E = Expr<UMaterialExpressionConstant3Vector>(M, X, Y); E->Constant = C; return E; }
static UMaterialExpressionConstant* K(UMaterial* M, float V, int32 X, int32 Y) { auto* E = Expr<UMaterialExpressionConstant>(M, X, Y); E->R = V; return E; }
static UMaterialExpression* Lerp(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B, UMaterialExpression* Alpha, int32 X, int32 Y)
{
	auto* L = Expr<UMaterialExpressionLinearInterpolate>(M, X, Y);
	UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), L, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(B, TEXT(""), L, TEXT("B"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Alpha, TEXT(""), L, TEXT("Alpha"));
	return L;
}
static UMaterialExpression* Mul(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B, int32 X, int32 Y)
{
	auto* E = Expr<UMaterialExpressionMultiply>(M, X, Y);
	UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), E, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(B, TEXT(""), E, TEXT("B"));
	return E;
}
static UMaterialExpression* Sat(UMaterial* M, UMaterialExpression* A, int32 X, int32 Y)
{
	auto* E = Expr<UMaterialExpressionSaturate>(M, X, Y);
	UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), E, TEXT(""));
	return E;
}
/** saturate((A - T) * S) */
static UMaterialExpression* Ramp(UMaterial* M, UMaterialExpression* A, float T, float S, int32 X, int32 Y)
{
	auto* Sub = Expr<UMaterialExpressionSubtract>(M, X, Y);
	UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), Sub, TEXT("A"));
	Sub->ConstB = T;
	return Sat(M, Mul(M, Sub, K(M, S, X, Y + 60), X + 150, Y), X + 300, Y);
}
static void Usages(UMaterial* M)
{
	bool bDummy = false;
	for (EMaterialUsage U : { MATUSAGE_InstancedStaticMeshes, MATUSAGE_SkeletalMesh, MATUSAGE_StaticLighting, MATUSAGE_Nanite })
		M->SetMaterialUsage(bDummy, U);
}

bool UKAPEditorLibrary::CreateMaterials()
{
	// ---- M_KAP_Surface: түс × (шаң/тат шуы), кедір-бұдырлық, металл, жарқырау ----
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Surface"));
		auto* C = VParam(M, TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f), -900, -200);
		auto* G = SParam(M, TEXT("Grime"), 0.4f, -900, 0);
		auto* N1 = Noise(M, 0.006f, 4, -900, 150);
		auto* N2 = Noise(M, 0.05f, 2, -900, 350);
		UMaterialExpression* Dirt = Lerp(M, K(M, 0.72f, -700, 150), K(M, 1.06f, -700, 210), Mul(M, N1, N2, -700, 300), -500, 150);
		UMaterialExpression* Var = Lerp(M, K(M, 1.f, -500, 0), Dirt, G, -300, 0);
		UMaterialExpression* Base = Mul(M, C, Var, -100, -150);
		UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
		auto* R = SParam(M, TEXT("Roughness"), 0.7f, -300, 250);
		UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, R, K(M, 0.95f, -300, 330), Mul(M, G, N2, -300, 400), -100, 300), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(SParam(M, TEXT("Metallic"), 0.f, -100, 450), TEXT(""), MP_Metallic);
		UMaterialEditingLibrary::ConnectMaterialProperty(VParam(M, TEXT("Emissive"), FLinearColor::Black, -100, 550), TEXT(""), MP_EmissiveColor);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	// ---- M_KAP_Translucent: жұмсақ шеттері бар түтін/бу шарлары ----
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Translucent"));
		M->BlendMode = BLEND_Translucent;
		M->SetShadingModel(MSM_Unlit);
		M->TwoSided = false;
		auto* C = VParam(M, TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f), -700, -100);
		auto* E = VParam(M, TEXT("Emissive"), FLinearColor::Black, -700, 50);
		auto* Add = Expr<UMaterialExpressionAdd>(M, -400, 0);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, C, K(M, 0.7f, -550, -20), -550, -100), TEXT(""), Add, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(E, TEXT(""), Add, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(Add, TEXT(""), MP_EmissiveColor);
		auto* Fr = Expr<UMaterialExpressionFresnel>(M, -700, 250);
		Fr->Exponent = 1.6f;
		auto* Om = Expr<UMaterialExpressionOneMinus>(M, -550, 250);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Fr, TEXT(""), Om, TEXT(""));
		auto* Op = SParam(M, TEXT("Opacity"), 0.5f, -550, 350);
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, Op, Om, -300, 300), TEXT(""), MP_Opacity);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	// ---- M_KAP_RadGlow: сәулеленген адамдардың жасыл жарқылы (overlay) ----
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_RadGlow"));
		M->BlendMode = BLEND_Additive;
		M->SetShadingModel(MSM_Unlit);
		auto* Fr = Expr<UMaterialExpressionFresnel>(M, -600, 0);
		Fr->Exponent = 2.5f;
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, Fr, Col(M, FLinearColor(0.6f, 3.f, 0.2f), -600, 150), -300, 50), TEXT(""), MP_EmissiveColor);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	// ---- M_KAP_Landscape: шөлейт (құм, сортаң, тасты беткейлер) ----
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Landscape"));
		auto* Macro = Noise(M, 0.0003f, 5, -1400, -300);
		auto* Mid = Noise(M, 0.002f, 4, -1400, -100);
		auto* Det = Noise(M, 0.03f, 3, -1400, 100);
		auto* SaltN = Noise(M, 0.0006f, 4, -1400, 300);
		UMaterialExpression* Sand = Lerp(M, Col(M, FLinearColor(0.55f, 0.41f, 0.26f), -1150, -400), Col(M, FLinearColor(0.72f, 0.57f, 0.38f), -1150, -330), Macro, -950, -360);
		Sand = Lerp(M, Sand, Col(M, FLinearColor(0.63f, 0.47f, 0.3f), -950, -250), Mid, -800, -300);
		Sand = Mul(M, Sand, Lerp(M, K(M, 0.86f, -950, 80), K(M, 1.05f, -950, 140), Det, -800, 100), -650, -250);
		auto* Nrm = Expr<UMaterialExpressionVertexNormalWS>(M, -1400, 500);
		auto* Mask = Expr<UMaterialExpressionComponentMask>(M, -1250, 500);
		Mask->R = false; Mask->G = false; Mask->B = true; Mask->A = false;
		UMaterialEditingLibrary::ConnectMaterialExpressions(Nrm, TEXT(""), Mask, TEXT(""));
		auto* Slope = Expr<UMaterialExpressionOneMinus>(M, -1100, 500);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mask, TEXT(""), Slope, TEXT(""));
		UMaterialExpression* RockMask = Ramp(M, Slope, 0.05f, 9.f, -950, 500);
		UMaterialExpression* Base = Lerp(M, Sand, Col(M, FLinearColor(0.36f, 0.28f, 0.2f), -650, 420), RockMask, -450, -100);
		UMaterialExpression* SaltMask = Ramp(M, SaltN, 0.78f, 6.f, -950, 300);
		Base = Lerp(M, Base, Col(M, FLinearColor(0.84f, 0.81f, 0.74f), -450, 300), Mul(M, SaltMask, K(M, 0.45f, -450, 380), -300, 300), -200, 0);
		UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Lerp(M, K(M, 0.95f, -300, 500), K(M, 0.7f, -300, 560), SaltMask, -100, 520), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	return true;
}

template <class T> static UMaterialExpression* Op2(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B, int32 X, int32 Y)
{
	auto* E = Expr<T>(M, X, Y);
	UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), E, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(B, TEXT(""), E, TEXT("B"));
	return E;
}
template <class T> static UMaterialExpression* Op1(UMaterial* M, UMaterialExpression* A, int32 X, int32 Y)
{
	auto* E = Expr<T>(M, X, Y);
	UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), E, TEXT(""));
	return E;
}
static UMaterialExpression* MaskC(UMaterial* M, UMaterialExpression* In, bool R, bool G, bool B, int32 X, int32 Y)
{
	auto* E = Expr<UMaterialExpressionComponentMask>(M, X, Y); E->R = R; E->G = G; E->B = B; E->A = false;
	UMaterialEditingLibrary::ConnectMaterialExpressions(In, TEXT(""), E, TEXT(""));
	return E;
}
static UMaterialExpressionNoise* NoiseAt(UMaterial* M, UMaterialExpression* Pos, float Scale, int32 Levels, int32 X, int32 Y)
{
	UMaterialExpressionNoise* N = Noise(M, Scale, Levels, X, Y);
	if (Pos) N->Position.Connect(0, Pos); // атау бойынша іздеу бұл кірісті таппайды — тікелей қосамыз
	return N;
}
static UMaterialExpression* SubK(UMaterial* M, UMaterialExpression* A, float B, int32 X, int32 Y)
{
	auto* E = Expr<UMaterialExpressionSubtract>(M, X, Y); E->ConstB = B;
	UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), E, TEXT("A"));
	return E;
}

// ---- M_KAP_Surface2: индустриялық PBR (текстурасыз, әлем координатасы бойынша):
//      гофр (нормаль), панель жіктері, тат дақтары, тік кір/тат ағыны, төменгі лас, жоғарғы бетте шаң, қауіп жолақтары ----
bool UKAPEditorLibrary::CreateIndustrialMaterial()
{
	if (LoadObject<UMaterial>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Surface5.M_KAP_Surface5"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return true;
	UMaterial* M = NewMaterial(TEXT("M_KAP_Surface5"));
	M->bTangentSpaceNormal = false;
	auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -3000, 0);
	auto* N = Expr<UMaterialExpressionVertexNormalWS>(M, -3000, 300);
	UMaterialExpression* Nz = MaskC(M, N, false, false, true, -2850, 300);
	UMaterialExpression* Side = Op1<UMaterialExpressionOneMinus>(M, Op1<UMaterialExpressionAbs>(M, Nz, -2700, 300), -2550, 300);
	// көлденең жанама (қабырғада — горизонталь бағыт)
	UMaterialExpression* Tn = Op1<UMaterialExpressionNormalize>(M, Op2<UMaterialExpressionAdd>(M, Op2<UMaterialExpressionCrossProduct>(M, Col(M, FLinearColor(0, 0, 1), -2850, 450), N, -2700, 450), Col(M, FLinearColor(0.0003f, 0.0007f, 0), -2700, 550), -2550, 450), -2400, 450);
	UMaterialExpression* U = Op2<UMaterialExpressionDotProduct>(M, WP, Tn, -2250, 300);
	// гофр: cos(u · жиілік) → нормаль ауытқуы
	auto* Cs = Expr<UMaterialExpressionCosine>(M, -1950, 300); Cs->Period = 1.f;
	UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, U, SParam(M, TEXT("RibFreq"), 0.08f, -2250, 400), -2100, 300), TEXT(""), Cs, TEXT(""));
	UMaterialExpression* RibAmt = Mul(M, Mul(M, SParam(M, TEXT("Ribs"), 0.f, -2100, 500), Side, -1950, 500), K(M, 0.45f, -1950, 560), -1800, 500);
	UMaterialExpression* Nn = Op1<UMaterialExpressionNormalize>(M, Op2<UMaterialExpressionAdd>(M, N, Mul(M, Tn, Mul(M, Cs, RibAmt, -1800, 300), -1650, 350), -1500, 350), -1350, 350);
	UMaterialEditingLibrary::ConnectMaterialProperty(Nn, TEXT(""), MP_Normal);
	// панель жіктері
	UMaterialExpression* Sfr = Op1<UMaterialExpressionFrac>(M, Mul(M, U, SParam(M, TEXT("PanelInv"), 0.0083f, -2250, 650), -2100, 650), -1950, 650);
	UMaterialExpression* Dd = Mul(M, Op1<UMaterialExpressionAbs>(M, SubK(M, Sfr, 0.5f, -1800, 650), -1650, 650), K(M, 2.f, -1650, 710), -1500, 650);
	UMaterialExpression* Seam = Mul(M, Mul(M, Ramp(M, Dd, 0.975f, 40.f, -1350, 650), SParam(M, TEXT("Panel"), 0.f, -1350, 760), -1050, 650), Side, -900, 650);
	// түс вариациясы
	auto* Nf = NoiseAt(M, Mul(M, WP, SParam(M, TEXT("VarScale"), 0.02f, -2550, -400), -2450, -400), 1.f, 2, -2400, -400);
	auto* C = VParam(M, TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f), -2400, -600);
	auto* G = SParam(M, TEXT("Grime"), 0.4f, -2400, -200);
	// түс вариациясы: 1 ± VarAmt (кір мөлшеріне қарай)
	auto* VarAmt = SParam(M, TEXT("VarAmt"), 0.1f, -2400, -300);
	UMaterialExpression* VarN = Op2<UMaterialExpressionAdd>(M, K(M, 1.f, -2250, -300), Mul(M, SubK(M, Nf, 0.5f, -2250, -360), Mul(M, VarAmt, K(M, 2.f, -2250, -240), -2150, -260), -2100, -330), -2000, -330);
	UMaterialExpression* Var = Lerp(M, K(M, 1.f, -2100, -300), VarN, G, -1950, -350);
	UMaterialExpression* Cc = Mul(M, C, Var, -1800, -500);
	// қауіп жолақтары (диагональ сары/қара)
	UMaterialExpression* Diag = Op1<UMaterialExpressionFrac>(M, Mul(M, Op2<UMaterialExpressionDotProduct>(M, WP, Col(M, FLinearColor(1, 1, 1), -2400, -800), -2250, -800), K(M, 1.f / 45.f, -2250, -740), -2100, -800), -1950, -800);
	UMaterialExpression* StripeC = Lerp(M, Col(M, FLinearColor(0.02f, 0.02f, 0.02f), -1800, -900), Col(M, FLinearColor(0.85f, 0.6f, 0.02f), -1800, -850), Ramp(M, Diag, 0.5f, 200.f, -1800, -800), -1500, -850);
	Cc = Lerp(M, Cc, StripeC, SParam(M, TEXT("Stripes"), 0.f, -1500, -750), -1350, -600);
	Cc = Mul(M, Cc, Op1<UMaterialExpressionOneMinus>(M, Mul(M, Seam, K(M, 0.6f, -900, 720), -750, 650), -600, 650), -1200, -550);
	// тік ағын іздері (жаңбыр/тат): шу тігінен созылған
	auto* Ns = NoiseAt(M, Mul(M, Mul(M, WP, Col(M, FLinearColor(1.f, 1.f, 0.07f), -2400, 900), -2250, 900), SParam(M, TEXT("StreakScale"), 0.03f, -2400, 960), -2250, 960), 1.f, 2, -2100, 900);
	UMaterialExpression* Streak = Mul(M, Mul(M, Ramp(M, Ns, 0.55f, 3.f, -1950, 900), G, -1650, 900), Side, -1500, 900);
	Cc = Mul(M, Cc, Op1<UMaterialExpressionOneMinus>(M, Mul(M, Streak, K(M, 0.4f, -1350, 960), -1200, 900), -1050, 900), -1050, -500);
	// тат: ірі шу + ағын іздері, Rust параметрі шекті төмендетеді
	auto* Nr = NoiseAt(M, Mul(M, WP, SParam(M, TEXT("RustScale"), 0.01f, -2550, 1100), -2450, 1100), 1.f, 3, -2400, 1100);
	auto* Rust = SParam(M, TEXT("Rust"), 0.f, -2400, 1250);
	UMaterialExpression* RustIn = Op2<UMaterialExpressionAdd>(M, Op2<UMaterialExpressionAdd>(M, Mul(M, Nr, K(M, 0.6f, -2250, 1160), -2100, 1100), Mul(M, Nf, K(M, 0.1f, -2250, 1220), -2100, 1200), -1950, 1100), Mul(M, Ns, K(M, 0.35f, -1950, 1320), -1800, 1300), -1800, 1100);
	UMaterialExpression* RTh = Op2<UMaterialExpressionSubtract>(M, RustIn, Op1<UMaterialExpressionOneMinus>(M, Mul(M, Rust, K(M, 0.65f, -1800, 1400), -1650, 1400), -1500, 1400), -1650, 1100);
	UMaterialExpression* RustM = Mul(M, Sat(M, Mul(M, RTh, K(M, 6.f, -1500, 1160), -1350, 1100), -1200, 1100), Op1<UMaterialExpressionCeil>(M, Rust, -1350, 1250), -1050, 1100);
	UMaterialExpression* RustC = Lerp(M, VParam(M, TEXT("RustDark"), FLinearColor(0.11f, 0.045f, 0.018f), -1200, 1300), VParam(M, TEXT("RustLight"), FLinearColor(0.26f, 0.11f, 0.04f), -1200, 1350), Nf, -1050, 1300);
	Cc = Lerp(M, Cc, RustC, RustM, -900, -450);
	// төменгі лас (нысан табанынан 70 см)
	auto* OP = Expr<UMaterialExpressionObjectPositionWS>(M, -2400, 1500);
	auto* OB = Expr<UMaterialExpressionObjectBounds>(M, -2400, 1600);
	UMaterialExpression* BotZ = Op2<UMaterialExpressionSubtract>(M, MaskC(M, OP, false, false, true, -2250, 1500), Mul(M, MaskC(M, OB, false, false, true, -2250, 1600), K(M, 0.5f, -2250, 1660), -2100, 1600), -1950, 1500);
	UMaterialExpression* Hh = Op2<UMaterialExpressionSubtract>(M, MaskC(M, WP, false, false, true, -2100, 1450), BotZ, -1800, 1500);
	// тек биік нысандарда (≥1.5 м): шағын бөлшектер (әйнек, дөңгелек) толық «кірленбесін»
	UMaterialExpression* Tall = Ramp(M, MaskC(M, OB, false, false, true, -1800, 1650), 150.f, 0.01f, -1650, 1650);
	UMaterialExpression* Bot = Mul(M, Mul(M, Ramp(M, Hh, 70.f, -1.f / 70.f, -1650, 1500), G, -1500, 1500), Tall, -1350, 1500);
	Cc = Lerp(M, Cc, Col(M, FLinearColor(0.24f, 0.19f, 0.13f), -900, 1550), Mul(M, Bot, K(M, 0.75f, -1200, 1560), -1050, 1500), -750, -400);
	// жоғарғы бетте шаң қабаты
	auto* Nd = NoiseAt(M, nullptr, 0.02f, 2, -2400, 1800);
	UMaterialExpression* Dust = Mul(M, Mul(M, Ramp(M, Nz, 0.55f, 4.f, -2100, 1800), SParam(M, TEXT("DustTop"), 0.7f, -2100, 1900), -1800, 1800), Ramp(M, Nd, 0.25f, 2.f, -1800, 1900), -1500, 1800);
	Cc = Lerp(M, Cc, Col(M, FLinearColor(0.68f, 0.56f, 0.42f), -900, 1850), Dust, -600, -350);
	UMaterialEditingLibrary::ConnectMaterialProperty(Cc, TEXT(""), MP_BaseColor);
	UMaterialExpression* Rg = Lerp(M, Lerp(M, Lerp(M, SParam(M, TEXT("Roughness"), 0.7f, -900, 2000), K(M, 0.95f, -900, 2060), Mul(M, G, Nf, -900, 2120), -750, 2000), K(M, 0.9f, -750, 2100), RustM, -600, 2000), K(M, 0.97f, -600, 2100), Dust, -450, 2000);
	UMaterialEditingLibrary::ConnectMaterialProperty(Rg, TEXT(""), MP_Roughness);
	UMaterialExpression* Mt = Mul(M, Mul(M, SParam(M, TEXT("Metallic"), 0.f, -900, 2250), Op1<UMaterialExpressionOneMinus>(M, RustM, -750, 2300), -600, 2250), Op1<UMaterialExpressionOneMinus>(M, Dust, -600, 2350), -450, 2250);
	UMaterialEditingLibrary::ConnectMaterialProperty(Mt, TEXT(""), MP_Metallic);
	UMaterialEditingLibrary::ConnectMaterialProperty(VParam(M, TEXT("Emissive"), FLinearColor::Black, -450, 2450), TEXT(""), MP_EmissiveColor);
	Usages(M);
	UMaterialEditingLibrary::RecompileMaterial(M);
	return SaveAsset(M);
}

// ---- M_KAP_Invisible: көрінбейтін (шаблон көлігінің рамасын жасыру үшін) ----
bool UKAPEditorLibrary::CreateInvisibleMaterial()
{
	if (LoadObject<UMaterial>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Invisible.M_KAP_Invisible"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return true;
	UMaterial* M = NewMaterial(TEXT("M_KAP_Invisible"));
	M->BlendMode = BLEND_Masked;
	UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.f, -300, 0), TEXT(""), MP_OpacityMask);
	Usages(M);
	UMaterialEditingLibrary::RecompileMaterial(M);
	return SaveAsset(M);
}

// ---- M_KAP_Landscape2: шөлейт v2 — желден пайда болған құм толқындары (әлемдік нормаль), сортаң, тасты беткей,
//      камераға жақын жерде ғана ұсақ толқын (алыста жыпылықтамас үшін), шу деңгейлері азайтылған (perf) ----
bool UKAPEditorLibrary::CreateLandscapeMaterial2()
{
	if (LoadObject<UMaterial>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Landscape3.M_KAP_Landscape3"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return true;
	UMaterial* M = NewMaterial(TEXT("M_KAP_Landscape3"));
	M->bTangentSpaceNormal = false;
	auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -2600, 0);
	auto* Macro = Noise(M, 0.0003f, 3, -2000, -500);
	auto* Mid = Noise(M, 0.002f, 3, -2000, -300);
	auto* Det = Noise(M, 0.03f, 2, -2000, -100);
	auto* SaltN = Noise(M, 0.0006f, 3, -2000, 100);
	UMaterialExpression* Sand = Lerp(M, Col(M, FLinearColor(0.52f, 0.39f, 0.25f), -1700, -600), Col(M, FLinearColor(0.7f, 0.55f, 0.37f), -1700, -540), Macro, -1500, -560);
	Sand = Lerp(M, Sand, Col(M, FLinearColor(0.6f, 0.45f, 0.29f), -1500, -460), Mid, -1350, -500);
	Sand = Mul(M, Sand, Lerp(M, K(M, 0.86f, -1500, -120), K(M, 1.05f, -1500, -60), Det, -1350, -100), -1200, -450);
	auto* N = Expr<UMaterialExpressionVertexNormalWS>(M, -2600, 400);
	UMaterialExpression* Nz = MaskC(M, N, false, false, true, -2450, 400);
	UMaterialExpression* Slope = Op1<UMaterialExpressionOneMinus>(M, Nz, -2300, 400);
	UMaterialExpression* RockMask = Ramp(M, Slope, 0.05f, 9.f, -2150, 400);
	// құм толқындары: желге перпендикуляр, қадамы ~11 см, шумен «бұралған»
	auto* WDir = Expr<UMaterialExpressionConstant2Vector>(M, -2450, 750); WDir->R = 0.94f; WDir->G = 0.34f;
	UMaterialExpression* Along = Op2<UMaterialExpressionDotProduct>(M, MaskC(M, WP, true, true, false, -2450, 650), WDir, -2300, 650);
	UMaterialExpression* Phase = Op2<UMaterialExpressionAdd>(M, Mul(M, Along, K(M, 1.f / 11.f, -2150, 700), -2000, 650), Mul(M, Mid, K(M, 6.f, -2150, 800), -2000, 750), -1850, 650);
	auto* Cs = Expr<UMaterialExpressionCosine>(M, -1700, 650); Cs->Period = 1.f;
	UMaterialEditingLibrary::ConnectMaterialExpressions(Phase, TEXT(""), Cs, TEXT(""));
	auto* CamP = Expr<UMaterialExpressionCameraPositionWS>(M, -2600, 900);
	auto* Dist = Expr<UMaterialExpressionLength>(M, -2300, 900);
	UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionSubtract>(M, WP, CamP, -2450, 900), TEXT(""), Dist, TEXT(""));
	UMaterialExpression* Near = Ramp(M, Dist, 3500.f, -1.f / 2500.f, -2150, 900);
	UMaterialExpression* RipAmt = Mul(M, Mul(M, Op1<UMaterialExpressionOneMinus>(M, RockMask, -1700, 850), Near, -1550, 850), Lerp(M, K(M, 0.15f, -1700, 950), K(M, 0.32f, -1700, 1010), Det, -1550, 950), -1400, 850);
	UMaterialExpression* Ripple = Mul(M, Cs, RipAmt, -1250, 700);
	UMaterialExpression* Dir3 = Col(M, FLinearColor(0.94f, 0.34f, 0.f), -1250, 800);
	UMaterialExpression* Nn = Op1<UMaterialExpressionNormalize>(M, Op2<UMaterialExpressionAdd>(M, N, Mul(M, Dir3, Ripple, -1100, 750), -950, 700), -800, 700);
	UMaterialEditingLibrary::ConnectMaterialProperty(Nn, TEXT(""), MP_Normal);
	// толқын қыры сәл ашық, ойығы — күңгірт (ұсақ қиыршық жиналады)
	Sand = Mul(M, Sand, Op2<UMaterialExpressionAdd>(M, K(M, 1.f, -1100, -300), Mul(M, Ripple, K(M, 0.18f, -1100, -240), -950, -260), -950, -300), -800, -400);
	UMaterialExpression* Base = Lerp(M, Sand, Lerp(M, Col(M, FLinearColor(0.33f, 0.26f, 0.19f), -1100, 300), Col(M, FLinearColor(0.42f, 0.34f, 0.25f), -1100, 360), Det, -950, 330), RockMask, -650, -200);
	UMaterialExpression* SaltMask = Ramp(M, SaltN, 0.78f, 6.f, -1700, 100);
	// сортаң тек ойпаңдарда (макро шу төмен) және әлсіз — төбелерде «камуфляж» дақтары болмасын
	UMaterialExpression* SaltW = Mul(M, Mul(M, SaltMask, Ramp(M, Macro, 0.42f, -5.f, -800, 160), -650, 160), Op1<UMaterialExpressionOneMinus>(M, RockMask, -650, 220), -500, 160);
	Base = Lerp(M, Base, Col(M, FLinearColor(0.8f, 0.77f, 0.7f), -650, 100), Mul(M, SaltW, K(M, 0.3f, -500, 230), -450, 100), -400, -150);
	// ойпаңдардағы күңгірт, ылғалдылау құм (макро шу төменгі мәндері)
	UMaterialExpression* Damp = Mul(M, Ramp(M, Macro, 0.25f, -5.f, -1700, 1150), K(M, 0.35f, -1500, 1200), -1350, 1150);
	Base = Lerp(M, Base, Mul(M, Base, K(M, 0.62f, -650, 1250), -500, 1200), Damp, -300, -100);
	UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
	UMaterialExpression* Rg = Lerp(M, Lerp(M, K(M, 0.95f, -500, 1400), K(M, 0.7f, -500, 1460), SaltW, -350, 1400), K(M, 0.82f, -350, 1500), Damp, -200, 1400);
	UMaterialEditingLibrary::ConnectMaterialProperty(Rg, TEXT(""), MP_Roughness);
	UMaterialEditingLibrary::RecompileMaterial(M);
	return SaveAsset(M);
}

// ---- M_KAP_Landscape4: шынайы құм — аралас асимметриялы толқындар (тек кей жерде), жақыннан түйіршік нормалі,
//      күнде жылтыраған кварц бөлшектері, жұмсақ контраст, алыстағы толқындар біртіндеп жоғалады ----
bool UKAPEditorLibrary::CreateLandscapeMaterial4()
{
	if (LoadObject<UMaterial>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Landscape4.M_KAP_Landscape4"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return true;
	UMaterial* M = NewMaterial(TEXT("M_KAP_Landscape4"));
	M->bTangentSpaceNormal = false;
	auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -3000, 0);
	auto* Macro = Noise(M, 0.0003f, 3, -2400, -700);
	auto* Mid = Noise(M, 0.002f, 3, -2400, -500);
	auto* Det = Noise(M, 0.03f, 2, -2400, -300);
	auto* SaltN = Noise(M, 0.0006f, 2, -2400, -100);
	auto* Patch = Noise(M, 0.0011f, 2, -2400, 100);
	auto* Fine = Noise(M, 0.45f, 1, -2400, 300);
	auto* Fine2 = NoiseAt(M, Op2<UMaterialExpressionAdd>(M, WP, Col(M, FLinearColor(37.f, 11.f, 5.f), -2700, 520), -2550, 500), 0.45f, 1, -2400, 500);
	auto* Glint = NoiseAt(M, Op2<UMaterialExpressionAdd>(M, WP, Col(M, FLinearColor(-91.f, 53.f, 17.f), -2700, 720), -2550, 700), 1.3f, 1, -2400, 700);
	// базалық түс: ірі және орташа вариация, ұсақ дақтар
	UMaterialExpression* Sand = Lerp(M, Col(M, FLinearColor(0.55f, 0.42f, 0.28f), -2000, -800), Col(M, FLinearColor(0.71f, 0.57f, 0.39f), -2000, -740), Macro, -1800, -760);
	Sand = Lerp(M, Sand, Col(M, FLinearColor(0.62f, 0.47f, 0.31f), -1800, -660), Mid, -1650, -700);
	Sand = Mul(M, Sand, Lerp(M, K(M, 0.9f, -1800, -320), K(M, 1.04f, -1800, -260), Det, -1650, -300), -1500, -650);
	Sand = Mul(M, Sand, Lerp(M, K(M, 0.94f, -1800, 260), K(M, 1.05f, -1800, 320), Fine, -1650, 300), -1350, -600);
	// беткей
	auto* N = Expr<UMaterialExpressionVertexNormalWS>(M, -3000, 900);
	UMaterialExpression* Nz = MaskC(M, N, false, false, true, -2850, 900);
	UMaterialExpression* RockMask = Ramp(M, Op1<UMaterialExpressionOneMinus>(M, Nz, -2700, 900), 0.05f, 9.f, -2550, 900);
	// камераға дейінгі қашықтық
	auto* CamP = Expr<UMaterialExpressionCameraPositionWS>(M, -3000, 1100);
	auto* Dist = Expr<UMaterialExpressionLength>(M, -2700, 1100);
	UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionSubtract>(M, WP, CamP, -2850, 1100), TEXT(""), Dist, TEXT(""));
	UMaterialExpression* Near = Ramp(M, Dist, 3500.f, -1.f / 2000.f, -2550, 1100);     // толқындар ≤35 м
	UMaterialExpression* NearG = Ramp(M, Dist, 900.f, -1.f / 600.f, -2550, 1200);      // түйіршік ≤9 м
	UMaterialExpression* Sandy = Op1<UMaterialExpressionOneMinus>(M, RockMask, -2400, 1000);
	// толқын жүйесі: Phase = dot(xy, dir)/қадам + бұралу; профиль cos(p) + 0.3·cos(2p) — асимметриялы
	auto Ripples = [&](float Dx, float Dy, float Period, UMaterialExpression* Warp, float WarpK, int32 Y) -> UMaterialExpression*
	{
		auto* D = Expr<UMaterialExpressionConstant2Vector>(M, -2400, Y + 60); D->R = Dx; D->G = Dy;
		UMaterialExpression* Along = Op2<UMaterialExpressionDotProduct>(M, MaskC(M, WP, true, true, false, -2400, Y), D, -2250, Y);
		UMaterialExpression* Ph = Op2<UMaterialExpressionAdd>(M, Mul(M, Along, K(M, 1.f / Period, -2100, Y + 60), -2000, Y), Mul(M, Warp, K(M, WarpK, -2100, Y + 120), -2000, Y + 90), -1850, Y);
		auto* C1 = Expr<UMaterialExpressionCosine>(M, -1700, Y); C1->Period = 1.f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(Ph, TEXT(""), C1, TEXT(""));
		auto* C2 = Expr<UMaterialExpressionCosine>(M, -1700, Y + 80); C2->Period = 1.f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mul(M, Ph, K(M, 2.f, -1850, Y + 120), -1750, Y + 100), TEXT(""), C2, TEXT(""));
		return Op2<UMaterialExpressionAdd>(M, C1, Mul(M, C2, K(M, 0.3f, -1600, Y + 120), -1550, Y + 90), -1450, Y);
	};
	UMaterialExpression* RA = Ripples(0.94f, 0.34f, 9.f, Mid, 7.f, 1400);
	UMaterialExpression* RB = Ripples(0.55f, 0.83f, 14.f, Macro, 11.f, 1700);
	UMaterialExpression* PatchA = Ramp(M, Patch, 0.42f, 4.f, -1700, 2000);           // толқынды аймақтар
	UMaterialExpression* PatchB = Mul(M, Ramp(M, Patch, 0.3f, -4.f, -1700, 2100), K(M, 0.6f, -1550, 2150), -1450, 2100);
	UMaterialExpression* AmpA = Mul(M, Mul(M, Mul(M, PatchA, Near, -1450, 2000), Sandy, -1300, 2000), Lerp(M, K(M, 0.07f, -1450, 2250), K(M, 0.13f, -1450, 2300), Det, -1300, 2250), -1150, 2000);
	UMaterialExpression* AmpB = Mul(M, Mul(M, Mul(M, PatchB, Near, -1300, 2100), Sandy, -1150, 2100), K(M, 0.08f, -1150, 2200), -1000, 2100);
	UMaterialExpression* RipA = Mul(M, RA, AmpA, -1000, 1400);
	UMaterialExpression* RipB = Mul(M, RB, AmpB, -1000, 1700);
	// нормаль: толқындар + түйіршік
	UMaterialExpression* NA = Mul(M, Col(M, FLinearColor(0.94f, 0.34f, 0.f), -900, 1450), RipA, -800, 1400);
	UMaterialExpression* NB = Mul(M, Col(M, FLinearColor(0.55f, 0.83f, 0.f), -900, 1750), RipB, -800, 1700);
	auto* GrainV = Expr<UMaterialExpressionAppendVector>(M, -1100, 600);
	UMaterialEditingLibrary::ConnectMaterialExpressions(SubK(M, Fine, 0.5f, -1300, 300), TEXT(""), GrainV, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(SubK(M, Fine2, 0.5f, -1300, 500), TEXT(""), GrainV, TEXT("B"));
	auto* Grain3 = Expr<UMaterialExpressionAppendVector>(M, -950, 600);
	UMaterialEditingLibrary::ConnectMaterialExpressions(GrainV, TEXT(""), Grain3, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(K(M, 0.f, -1100, 700), TEXT(""), Grain3, TEXT("B"));
	UMaterialExpression* NG = Mul(M, Grain3, Mul(M, NearG, K(M, 0.3f, -950, 760), -850, 700), -750, 600);
	UMaterialExpression* Nn = Op1<UMaterialExpressionNormalize>(M, Op2<UMaterialExpressionAdd>(M, Op2<UMaterialExpressionAdd>(M, Op2<UMaterialExpressionAdd>(M, N, NA, -650, 1400), NB, -550, 1500), NG, -450, 1000), -350, 1000);
	UMaterialEditingLibrary::ConnectMaterialProperty(Nn, TEXT(""), MP_Normal);
	// толқын қыры сәл ашық (жұмсақ), жылтыраған кварц түйіршіктері
	Sand = Mul(M, Sand, Op2<UMaterialExpressionAdd>(M, K(M, 1.f, -1100, -500), Mul(M, Op2<UMaterialExpressionAdd>(M, RipA, RipB, -1100, -440), K(M, 0.55f, -1100, -380), -950, -440), -950, -500), -800, -550);
	UMaterialExpression* GlintM = Mul(M, Ramp(M, Glint, 0.9f, 30.f, -1700, 700), NearG, -1500, 700);
	Sand = Op2<UMaterialExpressionAdd>(M, Sand, Mul(M, GlintM, K(M, 0.25f, -1350, 760), -1250, 700), -700, -500);
	// тасты беткей, сортаң, ойпаңдағы ылғалдылау құм
	UMaterialExpression* Base = Lerp(M, Sand, Lerp(M, Col(M, FLinearColor(0.33f, 0.26f, 0.19f), -1100, 100), Col(M, FLinearColor(0.42f, 0.34f, 0.25f), -1100, 160), Det, -950, 130), RockMask, -600, -400);
	UMaterialExpression* SaltW = Mul(M, Mul(M, Ramp(M, SaltN, 0.78f, 6.f, -1700, -100), Ramp(M, Macro, 0.42f, -5.f, -1550, -50), -1400, -100), Sandy, -1250, -100);
	Base = Lerp(M, Base, Col(M, FLinearColor(0.8f, 0.77f, 0.7f), -600, -200), Mul(M, SaltW, K(M, 0.25f, -1100, -150), -950, -150), -450, -350);
	UMaterialExpression* Damp = Mul(M, Ramp(M, Macro, 0.25f, -5.f, -1700, 2400), K(M, 0.3f, -1550, 2450), -1400, 2400);
	Base = Lerp(M, Base, Mul(M, Base, K(M, 0.66f, -600, 2450), -450, 2400), Damp, -300, -300);
	UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
	UMaterialExpression* Rg = Lerp(M, Lerp(M, Lerp(M, K(M, 0.95f, -600, 2600), K(M, 0.72f, -600, 2660), SaltW, -450, 2600), K(M, 0.82f, -450, 2700), Damp, -300, 2600), K(M, 0.22f, -300, 2700), GlintM, -150, 2600);
	UMaterialEditingLibrary::ConnectMaterialProperty(Rg, TEXT(""), MP_Roughness);
	UMaterialEditingLibrary::RecompileMaterial(M);
	return SaveAsset(M);
}

// =====================================================================
// Құм v5: процедуралық текстуралар (биіктік / нормаль / түс) + parallax — толқындар «боялған» емес, көлемді
// =====================================================================
static UTexture2D* SaveTex(const FString& Name, int32 S, const TArray<FColor>& Px, bool bNormal, bool bSRGB)
{
	const FString Path = TEXT("/Game/KAP/Textures/") + Name;
	UPackage* Pkg = MakePkg(Path);
	UTexture2D* T = NewObject<UTexture2D>(Pkg, FName(*Name), RF_Public | RF_Standalone);
	T->Source.Init(S, S, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Px.GetData()));
	T->SRGB = bSRGB;
	T->CompressionSettings = bNormal ? TC_Normalmap : TC_Default;
	T->LODGroup = TEXTUREGROUP_World;
	T->AddressX = TA_Wrap; T->AddressY = TA_Wrap;
	T->PostEditChange();
	T->UpdateResource();
	FTextureCompilingManager::Get().FinishAllCompilation(); // DDC-де дайын болсын — ойында шахмат текстурасы шықпасын
	SaveAsset(T);
	return T;
}

bool UKAPEditorLibrary::GenerateSandTextures()
{
	const int32 S = 1024;
	TArray<float> H; H.SetNumZeroed(S * S);
	TArray<float> G; G.SetNumZeroed(S * S);
	// периодты (жіксіз) шу: бүтін жиіліктері бар синустар қосындысы
	FRandomStream R(20261019);
	struct FW { int32 A, B; double Ph, Amp; };
	TArray<FW> Warp1, Warp2, AmpW;
	for (int32 i = 0; i < 6; i++) Warp1.Add({ R.RandRange(-3, 3), R.RandRange(1, 4), R.FRandRange(0.f, UE_TWO_PI), R.FRandRange(0.02f, 0.06f) / (1 + i * 0.4f) });
	for (int32 i = 0; i < 6; i++) Warp2.Add({ R.RandRange(1, 5), R.RandRange(-4, 4), R.FRandRange(0.f, UE_TWO_PI), R.FRandRange(0.02f, 0.05f) / (1 + i * 0.4f) });
	for (int32 i = 0; i < 5; i++) AmpW.Add({ R.RandRange(-3, 3), R.RandRange(-3, 3), R.FRandRange(0.f, UE_TWO_PI), 1.f / (1 + i) });
	auto Per = [](const TArray<FW>& Ws, float U, float V) { float Sum = 0; for (const FW& W : Ws) Sum += (float)(W.Amp * FMath::Sin(UE_TWO_PI * (W.A * U + W.B * V) + W.Ph)); return Sum; };
	for (int32 y = 0; y < S; y++) for (int32 x = 0; x < S; x++)
	{
		const float U = x / (float)S, V = y / (float)S;
		// 1-жүйе: желге перпендикуляр ≈21 толқын/тайл, бұралады; 2-жүйе әлсіз, басқа бағытта
		const float S1 = 21.f * U + 3.f * V + Per(Warp1, U, V) * 21.f;
		const float S2 = 9.f * U - 29.f * V + Per(Warp2, U, V) * 25.f;
		auto Prof = [](float Sx)
		{
			// асимметриялы профиль: жел жағы жайпақ (70%), ық жағы тік (30%), қыры сәл жұмырланған
			const float F = Sx - FMath::FloorToFloat(Sx);
			float P = F < 0.72f ? F / 0.72f : (1.f - F) / 0.28f;
			P = P * P * (3.f - 2.f * P);
			return P;
		};
		float Amp = 0.62f + 0.38f * FMath::Clamp(Per(AmpW, U, V) * 1.3f, -1.f, 1.f);
		const float Hh = Prof(S1) * Amp + Prof(S2) * 0.22f * (1.f - Amp * 0.5f);
		H[y * S + x] = Hh;
	}
	// түйіршіктер: жіксіз хэш-шу (тайл бойынша қайталанатын), 2 рет жұмсартылған
	for (int32 y = 0; y < S; y++) for (int32 x = 0; x < S; x++) { uint32 h = (uint32)(x * 73856093u) ^ (uint32)(y * 19349663u); h ^= h >> 13; h *= 1274126177u; G[y * S + x] = ((h >> 8) & 0xFFFF) / 65535.f; }
	TArray<float> G2 = G;
	auto At = [&](const TArray<float>& A, int32 x, int32 y) { return A[((y + S) % S) * S + ((x + S) % S)]; };
	for (int32 y = 0; y < S; y++) for (int32 x = 0; x < S; x++) G2[y * S + x] = (At(G, x, y) * 4 + At(G, x + 1, y) + At(G, x - 1, y) + At(G, x, y + 1) + At(G, x, y - 1)) / 8.f;
	TArray<float> Ht; Ht.SetNumZeroed(S * S);
	for (int32 i = 0; i < S * S; i++) Ht[i] = H[i] * 0.92f + (G2[i] - 0.5f) * 0.06f;
	// текстуралар: биіктік (R), нормаль, түс вариациясы (+ жылтыр маскасы альфада)
	TArray<FColor> PH, PN, PA; PH.SetNumUninitialized(S * S); PN.SetNumUninitialized(S * S); PA.SetNumUninitialized(S * S);
	const float Str = 6.5f; // нормаль күші (тайл 300 см → толқын биіктігі ≈1.2 см)
	for (int32 y = 0; y < S; y++) for (int32 x = 0; x < S; x++)
	{
		const float h = At(Ht, x, y);
		const float Dx = (At(Ht, x + 1, y) - At(Ht, x - 1, y)) * Str, Dy = (At(Ht, x, y + 1) - At(Ht, x, y - 1)) * Str;
		const FVector N = FVector(-Dx, -Dy, 1.f).GetSafeNormal();
		const uint8 Hb = (uint8)FMath::Clamp(h * 255.f, 0.f, 255.f);
		PH[y * S + x] = FColor(Hb, Hb, Hb, 255);
		PN[y * S + x] = FColor((uint8)FMath::Clamp((N.X * 0.5f + 0.5f) * 255.f, 0.f, 255.f), (uint8)FMath::Clamp((N.Y * 0.5f + 0.5f) * 255.f, 0.f, 255.f), (uint8)FMath::Clamp((N.Z * 0.5f + 0.5f) * 255.f, 0.f, 255.f), 255);
		// түс: қыры ашықтау (құрғақ, жіңішке құм), ойығы қоңырлау (ауыр қиыршық жиналады), түйіршік дақтары
		const float g = At(G, x, y);
		float A = 0.9f + h * 0.14f + (g - 0.5f) * 0.12f;
		if (g > 0.985f) A -= 0.25f;             // қара минерал түйіршіктері
		const uint8 Ab = (uint8)FMath::Clamp(A * 200.f, 0.f, 255.f);
		const uint8 Gl = g < 0.012f ? 255 : 0;  // кварц жылтыры
		PA[y * S + x] = FColor(Ab, Ab, Ab, Gl);
	}
	SaveTex(TEXT("T_KAP_SandHeight"), S, PH, false, false);
	SaveTex(TEXT("T_KAP_SandNormal"), S, PN, true, false);
	SaveTex(TEXT("T_KAP_SandAlbedo"), S, PA, false, false);
	return true;
}

bool UKAPEditorLibrary::CreateLandscapeMaterial5()
{
	if (LoadObject<UMaterial>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Landscape7.M_KAP_Landscape7"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return true;
	UTexture2D* TH = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_SandHeight.T_KAP_SandHeight"));
	UTexture2D* TN = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_SandNormal.T_KAP_SandNormal"));
	UTexture2D* TA = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_SandAlbedo.T_KAP_SandAlbedo"));
	if (!TH || !TN || !TA) return false;
	UMaterial* M = NewMaterial(TEXT("M_KAP_Landscape7"));
	auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -3200, 0);
	UMaterialExpression* XY = MaskC(M, WP, true, true, false, -3050, 0);
	UMaterialExpression* UV1 = Mul(M, XY, K(M, 1.f / 300.f, -2900, 60), -2800, 0);
	// екінші масштаб: 37° бұрылған, 4.7 м тайл — қайталану байқалмасын
	auto* RotA = Expr<UMaterialExpressionConstant2Vector>(M, -3050, 200); RotA->R = 0.7986f; RotA->G = -0.6018f;
	auto* RotB = Expr<UMaterialExpressionConstant2Vector>(M, -3050, 260); RotB->R = 0.6018f; RotB->G = 0.7986f;
	auto* UV2r = Expr<UMaterialExpressionAppendVector>(M, -2800, 200);
	UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionDotProduct>(M, XY, RotA, -2950, 200), TEXT(""), UV2r, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionDotProduct>(M, XY, RotB, -2950, 260), TEXT(""), UV2r, TEXT("B"));
	UMaterialExpression* UV2 = Mul(M, UV2r, K(M, 1.f / 470.f, -2700, 260), -2650, 200);
	// қашықтық маскалары
	auto* CamP = Expr<UMaterialExpressionCameraPositionWS>(M, -3200, 500);
	auto* Dist = Expr<UMaterialExpressionLength>(M, -2900, 500);
	UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionSubtract>(M, WP, CamP, -3050, 500), TEXT(""), Dist, TEXT(""));
	UMaterialExpression* Near = Ramp(M, Dist, 6000.f, -1.f / 4000.f, -2750, 500);
	// parallax occlusion mapping (тек жақын жерде көп қадам)
	UMaterialExpression* PUV = UV1;
	if (UMaterialFunction* POMF = LoadObject<UMaterialFunction>(nullptr, TEXT("/Engine/Functions/Engine_MaterialFunctions01/Texturing/ParallaxOcclusionMapping.ParallaxOcclusionMapping")))
	{
		auto* POM = Expr<UMaterialExpressionMaterialFunctionCall>(M, -2400, 0);
		POM->SetMaterialFunction(POMF);
		auto* HObj = Expr<UMaterialExpressionTextureObject>(M, -2600, -200); HObj->Texture = TH;
		auto* Ch = Expr<UMaterialExpressionConstant4Vector>(M, -2600, -100); Ch->Constant = FLinearColor(1, 0, 0, 0);
		for (int32 i = 0; i < POM->FunctionInputs.Num(); i++)
		{
			const FString Nm = POM->FunctionInputs[i].Input.InputName.ToString();
			FExpressionInput* In = POM->GetInput(i);
			if (!In) continue;
			if (Nm.Contains(TEXT("Heightmap Texture"))) In->Connect(0, HObj);
			else if (Nm.Contains(TEXT("Height Ratio"))) In->Connect(0, K(M, 0.012f, -2600, 0));
			else if (Nm.Contains(TEXT("Min Steps"))) In->Connect(0, K(M, 6.f, -2600, 60));
			else if (Nm.Contains(TEXT("Max Steps"))) In->Connect(0, K(M, 22.f, -2600, 120));
			else if (Nm.Equals(TEXT("UVs")) || Nm.StartsWith(TEXT("UVs"))) In->Connect(0, UV1);
			else if (Nm.Contains(TEXT("Heightmap Channel"))) In->Connect(0, Ch);
			UE_LOG(LogTemp, Display, TEXT("KAPPOM input %d: %s"), i, *Nm);
		}
		int32 OutUV = INDEX_NONE;
		for (int32 j = 0; j < POM->FunctionOutputs.Num(); j++) { const FString On = POM->FunctionOutputs[j].Output.OutputName.ToString(); UE_LOG(LogTemp, Display, TEXT("KAPPOM output %d: %s"), j, *On); if (On.Contains(TEXT("Parallax UV"))) OutUV = j; }
		if (OutUV != INDEX_NONE)
		{
			// POM-ның UV шығысы → MaskC арқылы түйін ретінде қолданамыз
			auto* Pass = Expr<UMaterialExpressionComponentMask>(M, -2200, 0); Pass->R = true; Pass->G = true; Pass->B = false; Pass->A = false;
			Pass->Input.Connect(OutUV, POM);
			PUV = Lerp(M, UV1, Pass, Near, -2050, 0);
		}
	}
	auto Samp = [&](UTexture2D* T, UMaterialExpression* UV, EMaterialSamplerType ST, int32 Y)
	{
		auto* S = Expr<UMaterialExpressionTextureSample>(M, -1900, Y); S->Texture = T; S->SamplerType = ST;
		S->Coordinates.Connect(0, UV);
		return S;
	};
	auto* N1 = Samp(TN, PUV, SAMPLERTYPE_Normal, 300);
	auto* N2 = Samp(TN, UV2, SAMPLERTYPE_Normal, 500);
	auto* A1 = Samp(TA, PUV, SAMPLERTYPE_LinearColor, 700);
	auto* A2 = Samp(TA, UV2, SAMPLERTYPE_LinearColor, 900);
	// толқынды/тегіс аймақтар (үлкен шу) және тасты беткей
	auto* Patch = Noise(M, 0.0009f, 2, -2400, 1100);
	auto* Macro = Noise(M, 0.0003f, 3, -2400, 1300);
	auto* Mid = Noise(M, 0.002f, 2, -2400, 1500);
	auto* VN = Expr<UMaterialExpressionVertexNormalWS>(M, -2400, 1700);
	UMaterialExpression* RockMask = Ramp(M, Op1<UMaterialExpressionOneMinus>(M, MaskC(M, VN, false, false, true, -2250, 1700), -2100, 1700), 0.05f, 9.f, -1950, 1700);
	UMaterialExpression* Ripply = Mul(M, Mul(M, Ramp(M, Patch, 0.22f, 1.6f, -2100, 1100), Op1<UMaterialExpressionOneMinus>(M, RockMask, -1950, 1750), -1800, 1100), Near, -1650, 1100);
	// нормаль: 1-масштаб толық, 2-масштаб (бұрылған) әлсіз — xy бұрышын кері бұрамыз
	UMaterialExpression* N2xy = MaskC(M, N2, true, true, false, -1700, 500);
	auto* RbA = Expr<UMaterialExpressionConstant2Vector>(M, -1700, 560); RbA->R = 0.7986f; RbA->G = 0.6018f;
	auto* RbB = Expr<UMaterialExpressionConstant2Vector>(M, -1700, 620); RbB->R = -0.6018f; RbB->G = 0.7986f;
	auto* N2w = Expr<UMaterialExpressionAppendVector>(M, -1550, 500);
	UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionDotProduct>(M, N2xy, RbA, -1600, 520), TEXT(""), N2w, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Op2<UMaterialExpressionDotProduct>(M, N2xy, RbB, -1600, 600), TEXT(""), N2w, TEXT("B"));
	UMaterialExpression* Nxy = Op2<UMaterialExpressionAdd>(M, MaskC(M, N1, true, true, false, -1550, 300), Mul(M, N2w, K(M, 0.22f, -1450, 560), -1400, 500), -1300, 350);
	UMaterialExpression* NxyK = Mul(M, Nxy, Mul(M, Ripply, K(M, 0.85f, -1450, 1150), -1350, 1100), -1150, 350);
	auto* Nfull = Expr<UMaterialExpressionAppendVector>(M, -1000, 350);
	UMaterialEditingLibrary::ConnectMaterialExpressions(NxyK, TEXT(""), Nfull, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(K(M, 1.f, -1150, 450), TEXT(""), Nfull, TEXT("B"));
	UMaterialEditingLibrary::ConnectMaterialProperty(Op1<UMaterialExpressionNormalize>(M, Nfull, -850, 350), TEXT(""), MP_Normal);
	// түс: ірі вариация × текстура (жақында), ық жақтағы жұмсақ көлеңке; тасты беткей, ылғалды ойпаң
	UMaterialExpression* Sand = Lerp(M, Col(M, FLinearColor(0.58f, 0.44f, 0.29f), -1700, -400), Col(M, FLinearColor(0.72f, 0.58f, 0.4f), -1700, -340), Macro, -1550, -380);
	Sand = Lerp(M, Sand, Col(M, FLinearColor(0.64f, 0.49f, 0.32f), -1550, -280), Mid, -1400, -330);
	UMaterialExpression* Atex = Mul(M, Op2<UMaterialExpressionAdd>(M, MaskC(M, A1, true, false, false, -1700, 700), MaskC(M, A2, true, false, false, -1700, 900), -1550, 800), K(M, 0.5f / 0.784f, -1550, 860), -1400, 800);
	UMaterialExpression* Var = Lerp(M, K(M, 1.f, -1300, 760), Atex, Mul(M, Near, K(M, 0.9f, -1300, 900), -1200, 880), -1150, 800);
	Sand = Mul(M, Sand, Var, -1000, -300);
	UMaterialExpression* Base = Lerp(M, Sand, Col(M, FLinearColor(0.36f, 0.28f, 0.2f), -850, 1700), RockMask, -700, -250);
	UMaterialExpression* Damp = Mul(M, Ramp(M, Macro, 0.25f, -5.f, -1700, 1900), K(M, 0.3f, -1550, 1950), -1400, 1900);
	Base = Lerp(M, Base, Mul(M, Base, K(M, 0.68f, -700, 1950), -550, 1900), Damp, -450, -200);
	UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
	auto* GlA = Expr<UMaterialExpressionComponentMask>(M, -1700, 1000); GlA->R = true; GlA->G = false; GlA->B = false; GlA->A = false;
	GlA->Input.Connect(4, A1); // текстураның альфа шығысы (4) — кварц жылтыры
	UMaterialExpression* Glint = GlA;
	UMaterialExpression* Rg = Lerp(M, Lerp(M, K(M, 0.94f, -700, 2100), K(M, 0.82f, -700, 2160), Damp, -550, 2100), K(M, 0.25f, -550, 2200), Mul(M, Glint, Near, -700, 2250), -400, 2100);
	UMaterialEditingLibrary::ConnectMaterialProperty(Rg, TEXT(""), MP_Roughness);
	// ойықтардағы жұмсақ AO (биіктіктен)
	UMaterialExpression* AO = Lerp(M, K(M, 1.f, -700, 2400), Lerp(M, K(M, 0.82f, -850, 2450), K(M, 1.f, -850, 2500), MaskC(M, Samp(TH, PUV, SAMPLERTYPE_LinearColor, 1200), true, false, false, -1050, 2450), -700, 2450), Ripply, -550, 2400);
	UMaterialEditingLibrary::ConnectMaterialProperty(AO, TEXT(""), MP_AmbientOcclusion);
	UMaterialEditingLibrary::RecompileMaterial(M);
	return SaveAsset(M);
}

/** Деңгейдегі ландшафтқа жаңа материалды тағайындап, картаны сақтау */
bool UKAPEditorLibrary::ApplyLandscapeMaterial(const FString& MapPath)
{
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Landscape7.M_KAP_Landscape7"));
	if (!Mat) return false;
	UWorld* W = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!W) return false;
	bool bDone = false;
	for (TActorIterator<ALandscapeProxy> It(W); It; ++It)
	{
		It->Modify();
		It->LandscapeMaterial = Mat;
		FProperty* P = ALandscapeProxy::StaticClass()->FindPropertyByName(TEXT("LandscapeMaterial"));
		FPropertyChangedEvent Ev(P);
		It->PostEditChangeProperty(Ev);
		bDone = true;
	}
	if (bDone) UEditorLoadingAndSavingUtils::SaveMap(W, MapPath);
	return bDone;
}

bool UKAPEditorLibrary::CreateAtmosphereMaterials()
{
	auto Exists = [](const TCHAR* N) { return LoadObject<UMaterial>(nullptr, *FString::Printf(TEXT("/Game/KAP/Materials/%s.%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr; };
	// ---- M_KAP_Dust: жарықпен жанатын мөлдір бөлшек (прожектор сәулесінде жарқырайды) ----
	if (!Exists(TEXT("M_KAP_Dust")))
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Dust"));
		M->BlendMode = BLEND_Translucent;
		M->SetShadingModel(MSM_DefaultLit);
		M->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
		UMaterialEditingLibrary::ConnectMaterialProperty(VParam(M, TEXT("Color"), FLinearColor(0.85f, 0.8f, 0.72f), -500, -100), TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 1.f, -500, 50), TEXT(""), MP_Roughness);
		auto* Fr = Expr<UMaterialExpressionFresnel>(M, -700, 200); Fr->Exponent = 1.2f;
		auto* Om = Expr<UMaterialExpressionOneMinus>(M, -550, 200);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Fr, TEXT(""), Om, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, SParam(M, TEXT("Opacity"), 0.7f, -550, 300), Om, -300, 250), TEXT(""), MP_Opacity);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	// ---- M_KAP_Human: манекенге «киім» — биіктік пен радиус бойынша аймақтар (актер орнына қатысты, см):
	//      етік / шалбар / белбеу / күрте / қолғап / бет терісі / шаш / шағылыс жолақтары ----
	if (!Exists(TEXT("M_KAP_Human")))
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Human"));
		auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -2400, 0);
		auto* AP = Expr<UMaterialExpressionActorPositionWS>(M, -2400, 120);
		auto* Rel = Expr<UMaterialExpressionSubtract>(M, -2250, 60);
		UMaterialEditingLibrary::ConnectMaterialExpressions(WP, TEXT(""), Rel, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(AP, TEXT(""), Rel, TEXT("B"));
		UMaterialExpression* RelS = Mul(M, Rel, SParam(M, TEXT("InvScale"), 1.f, -2250, 180), -2100, 60);
		auto Mask = [&](UMaterialExpression* In, bool R, bool G, bool B, int32 Y)
		{
			auto* E = Expr<UMaterialExpressionComponentMask>(M, -1950, Y); E->R = R; E->G = G; E->B = B; E->A = false;
			UMaterialEditingLibrary::ConnectMaterialExpressions(In, TEXT(""), E, TEXT(""));
			return E;
		};
		UMaterialExpression* Z = Mask(RelS, false, false, true, 0);
		auto* Rad = Expr<UMaterialExpressionLength>(M, -1800, 150);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mask(RelS, true, true, false, 150), TEXT(""), Rad, TEXT(""));
		auto Band = [&](float C, float W, int32 Y) { return Mul(M, Ramp(M, Z, C - W, 1.f, -1650, Y), Ramp(M, Z, C + W, -1.f, -1650, Y + 90), -1300, Y); };
		auto* Jacket = VParam(M, TEXT("Jacket"), FLinearColor(0.75f, 0.28f, 0.04f), -1100, -600);
		auto* Pants = VParam(M, TEXT("Pants"), FLinearColor(0.12f, 0.12f, 0.13f), -1100, -500);
		auto* Boots = VParam(M, TEXT("Boots"), FLinearColor(0.03f, 0.025f, 0.02f), -1100, -400);
		auto* Skin = VParam(M, TEXT("Skin"), FLinearColor(0.55f, 0.36f, 0.24f), -1100, -300);
		auto* Hair = VParam(M, TEXT("Hair"), FLinearColor(0.02f, 0.015f, 0.01f), -1100, -200);
		auto* Glove = VParam(M, TEXT("Glove"), FLinearColor(0.08f, 0.07f, 0.05f), -1100, -100);
		auto* Stripe = VParam(M, TEXT("Stripe"), FLinearColor(0.8f, 0.8f, 0.7f), -1100, 0);
		UMaterialExpression* C = Lerp(M, Jacket, Pants, Ramp(M, Z, 2.f, -2.f, -1650, -700), -900, -600);
		C = Lerp(M, C, Boots, Ramp(M, Z, -77.f, -1.f, -1650, -800), -800, -550);
		C = Lerp(M, C, Boots, Band(0.f, 3.f, -900), -700, -500); // белбеу
		UMaterialExpression* Hands = Mul(M, Mul(M, Ramp(M, Rad, 23.f, 0.5f, -1650, 300), Ramp(M, Z, -28.f, 0.3f, -1650, 400), -1450, 350), Ramp(M, Z, 18.f, -0.3f, -1650, 500), -1300, 400);
		C = Lerp(M, C, Lerp(M, Skin, Glove, SParam(M, TEXT("Gloves"), 1.f, -1100, 450), -900, 450), Hands, -600, -450);
		UMaterialExpression* Head = Ramp(M, Z, 61.f, 0.4f, -1650, 600);
		C = Lerp(M, C, Skin, Head, -500, -400);
		C = Lerp(M, C, Hair, Mul(M, Ramp(M, Z, 80.f, 0.5f, -1650, 700), SParam(M, TEXT("HairAmt"), 1.f, -1100, 700), -900, 700), -400, -350);
		UMaterialExpression* Str = Mul(M, Expr<UMaterialExpressionAdd>(M, -1100, 850), SParam(M, TEXT("StripeAmt"), 0.f, -1100, 950), -900, 850);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Band(31.f, 2.5f, 800), TEXT(""), Cast<UMaterialExpressionMultiply>(Str)->A.Expression, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Band(-52.f, 2.5f, 1000), TEXT(""), Cast<UMaterialExpressionMultiply>(Str)->A.Expression, TEXT("B"));
		C = Lerp(M, C, Stripe, Str, -300, -300);
		// шаң/кір: жоғарғы бет пен төменгі бөлік көбірек ластанған
		auto* N1 = Noise(M, 0.02f, 3, -900, 1150);
		UMaterialExpression* Dirt = Lerp(M, K(M, 0.78f, -700, 1150), K(M, 1.05f, -700, 1200), N1, -500, 1150);
		UMaterialExpression* Base = Mul(M, C, Lerp(M, K(M, 1.f, -400, 1100), Dirt, SParam(M, TEXT("Grime"), 0.6f, -400, 1250), -250, 1150), -100, -200);
		UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
		UMaterialExpression* Rough = Lerp(M, Lerp(M, K(M, 0.88f, -500, 1350), K(M, 0.5f, -500, 1400), Head, -350, 1350), K(M, 0.35f, -350, 1450), Str, -200, 1350);
		UMaterialEditingLibrary::ConnectMaterialProperty(Rough, TEXT(""), MP_Roughness);
		// шағылыс жолағы фара/фонарь сәулесінде «жанады» (ретрорефлектор имитациясы)
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, Mul(M, Stripe, Str, -300, 1550), SParam(M, TEXT("StripeGlow"), 0.f, -300, 1650), -100, 1550), TEXT(""), MP_EmissiveColor);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	// ---- M_KAP_Track: із декалі (күңгірт, кедір-бұдыр ойық + протектор жолақтары) ----
	if (!Exists(TEXT("M_KAP_Track")))
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Track"));
		M->MaterialDomain = MD_DeferredDecal;
		M->BlendMode = BLEND_Translucent;
		auto* UV = Expr<UMaterialExpressionTextureCoordinate>(M, -1300, 0);
		// эллипс: 1 - saturate(|uv - 0.5| * 2)^2
		auto* Sub = Expr<UMaterialExpressionSubtract>(M, -1150, 0); Sub->ConstB = 0.5f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), Sub, TEXT("A"));
		auto* Len = Expr<UMaterialExpressionLength>(M, -1000, 0);
		UMaterialEditingLibrary::ConnectMaterialExpressions(Sub, TEXT(""), Len, TEXT(""));
		UMaterialExpression* Edge = Ramp(M, Len, 0.32f, -6.f, -850, 0); // орталықта 1, шетке қарай 0
		// протектор: sin(V * 40) жолақтары
		auto* Mask = Expr<UMaterialExpressionComponentMask>(M, -1150, 200); Mask->R = false; Mask->G = true; Mask->B = false; Mask->A = false;
		UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), Mask, TEXT(""));
		auto* Sn = Expr<UMaterialExpressionSine>(M, -850, 200); Sn->Period = 0.08f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(Mask, TEXT(""), Sn, TEXT(""));
		UMaterialExpression* Stripes = Lerp(M, K(M, 1.f, -700, 260), Ramp(M, Sn, -0.2f, 3.f, -700, 200), SParam(M, TEXT("Tread"), 0.f, -700, 330), -500, 220);
		auto* Grain = Noise(M, 0.05f, 2, -850, 400);
		UMaterialExpression* Op = Mul(M, Mul(M, Edge, Stripes, -350, 50), Lerp(M, K(M, 0.6f, -500, 420), K(M, 1.f, -500, 480), Grain, -350, 420), -200, 100);
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, Op, SParam(M, TEXT("Opacity"), 0.85f, -200, 300), -50, 200), TEXT(""), MP_Opacity);
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, VParam(M, TEXT("Color"), FLinearColor(0.2f, 0.14f, 0.09f), -350, -200), Lerp(M, K(M, 0.8f, -500, -100), K(M, 1.1f, -500, -40), Grain, -350, -100), -150, -150), TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 1.f, -150, 0), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	return true;
}

// ---------------------------------------------------------------------
// Процедуралық мештер
// ---------------------------------------------------------------------
struct FMeshBuild
{
	TArray<FVector3f> P; TArray<FVector3f> N; TArray<FVector2f> UV; TArray<int32> Idx;
	void Ellipsoid(FVector3f C, FVector3f R, int32 Seg, TFunction<float(FVector3f)> Disp = nullptr, FQuat4f Rot = FQuat4f::Identity)
	{
		const int32 Base = P.Num();
		for (int32 j = 0; j <= Seg; j++) for (int32 i = 0; i <= Seg * 2; i++)
		{
			const float Th = j * PI / Seg, Ph = i * PI / Seg;
			FVector3f D(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
			float S = 1.f; if (Disp) S = Disp(D);
			P.Add(C + Rot.RotateVector(D * R * S)); N.Add(Rot.RotateVector(D)); UV.Add(FVector2f(i / (Seg * 2.f), j / (float)Seg));
		}
		const int32 W = Seg * 2 + 1;
		for (int32 j = 0; j < Seg; j++) for (int32 i = 0; i < Seg * 2; i++)
		{
			const int32 A = Base + j * W + i, B = A + 1, C2 = A + W, D2 = C2 + 1;
			Idx.Append({ A, C2, B, B, C2, D2 });
		}
	}
	void Tube(FVector3f A, FVector3f B, float R0, float R1, int32 Seg = 7)
	{
		const FVector3f Ax = (B - A).GetSafeNormal();
		FVector3f U = FVector3f::CrossProduct(Ax, FMath::Abs(Ax.Z) < 0.9f ? FVector3f(0, 0, 1) : FVector3f(1, 0, 0)).GetSafeNormal();
		const FVector3f V = FVector3f::CrossProduct(Ax, U);
		const int32 Base = P.Num();
		for (int32 r = 0; r < 2; r++) for (int32 i = 0; i <= Seg; i++)
		{
			const float An = i * 2 * PI / Seg;
			const FVector3f Dn = U * FMath::Cos(An) + V * FMath::Sin(An);
			P.Add((r ? B : A) + Dn * (r ? R1 : R0)); N.Add(Dn); UV.Add(FVector2f(i / (float)Seg, (float)r));
		}
		for (int32 i = 0; i < Seg; i++) { const int32 a = Base + i, b = a + 1, c = a + Seg + 1, d = c + 1; Idx.Append({ a, b, c, b, d, c }); }
	}
	/** Үшбұрыш бағытын төбе нормаліне сәйкестендіру (Rock конвенциясы: Cross(P2-P0, P1-P0) сыртқа) */
	void FixWinding()
	{
		for (int32 t = 0; t + 2 < Idx.Num(); t += 3)
		{
			const FVector3f Fn = FVector3f::CrossProduct(P[Idx[t + 2]] - P[Idx[t]], P[Idx[t + 1]] - P[Idx[t]]);
			if (FVector3f::DotProduct(Fn, N[Idx[t]] + N[Idx[t + 1]] + N[Idx[t + 2]]) < 0) Swap(Idx[t + 1], Idx[t + 2]);
		}
	}
	void RecomputeNormals()
	{
		TArray<FVector3f> Acc; Acc.Init(FVector3f::ZeroVector, P.Num());
		for (int32 t = 0; t + 2 < Idx.Num(); t += 3) { const FVector3f Fn = FVector3f::CrossProduct(P[Idx[t + 2]] - P[Idx[t]], P[Idx[t + 1]] - P[Idx[t]]); for (int32 k = 0; k < 3; k++) Acc[Idx[t + k]] += Fn; }
		for (int32 i = 0; i < P.Num(); i++) N[i] = Acc[i].GetSafeNormal();
	}
};

static bool SaveMesh(const FString& Name, FMeshBuild& B, bool bNanite, bool bComplexCollision)
{
	const FString Path = TEXT("/Game/KAP/Meshes/") + Name;
	UPackage* Pkg = MakePkg(Path);
	UStaticMesh* SM = NewObject<UStaticMesh>(Pkg, FName(*Name), RF_Public | RF_Standalone);
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector4f> Cols = Attr.GetVertexInstanceColors();
	const FPolygonGroupID PG = MD.CreatePolygonGroup();
	Attr.GetPolygonGroupMaterialSlotNames()[PG] = FName("Main");
	TArray<FVertexInstanceID> VI;
	for (int32 i = 0; i < B.P.Num(); i++)
	{
		const FVertexID V = MD.CreateVertex();
		Pos[V] = B.P[i];
		const FVertexInstanceID I = MD.CreateVertexInstance(V);
		Nrm[I] = B.N[i]; UVs[I] = B.UV[i]; Cols[I] = FVector4f(1, 1, 1, 1);
		VI.Add(I);
	}
	for (int32 t = 0; t + 2 < B.Idx.Num(); t += 3)
	{
		if (B.Idx[t] == B.Idx[t + 1] || B.Idx[t + 1] == B.Idx[t + 2] || B.Idx[t] == B.Idx[t + 2]) continue;
		TArray<FVertexInstanceID> Tri = { VI[B.Idx[t]], VI[B.Idx[t + 1]], VI[B.Idx[t + 2]] };
		MD.CreatePolygon(PG, Tri);
	}
	SM->GetStaticMaterials().Add(FStaticMaterial(nullptr, FName("Main"), FName("Main")));
	SM->SetNumSourceModels(1);
	FStaticMeshSourceModel& SRC = SM->GetSourceModel(0);
	SRC.BuildSettings.bRecomputeNormals = false;
	SRC.BuildSettings.bRecomputeTangents = true;
	SRC.BuildSettings.bGenerateLightmapUVs = false;
	SM->CreateMeshDescription(0, MoveTemp(MD));
	SM->CommitMeshDescription(0);
	if (bNanite) { FMeshNaniteSettings NS = SM->GetNaniteSettings(); NS.bEnabled = true; SM->SetNaniteSettings(NS); }
	SM->CreateBodySetup();
	if (UBodySetup* BS = SM->GetBodySetup()) BS->CollisionTraceFlag = bComplexCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
	SM->Build(true);
	SM->PostEditChange();
	return SaveAsset(SM);
}

// ---- Қырланған пішіндер: «қорап» көрінісін жұмсарту үшін (жиектері дөңгеленген, жарық шеттен шағылады) ----
bool UKAPEditorLibrary::CreateBevelMeshes()
{
	// SM_KAP_BevelCube: 100 см текше, жиек радиусы 3 см (дөңгеленген қорап)
	{
		FMeshBuild B;
		const float H = 50.f, Rb = 3.f, In = H - Rb;
		TArray<float> L = { -50.f, -49.6f, -48.5f, -47.f, -25.f, 0.f, 25.f, 47.f, 48.5f, 49.6f, 50.f };
		// 6 бет: әр бетте тор, нүкте ішкі қорапқа «қысылып», Rb радиуспен сыртқа итеріледі
		const FVector3f Axes[6][3] = {
			{ FVector3f(1, 0, 0), FVector3f(0, 1, 0), FVector3f(0, 0, 1) }, { FVector3f(-1, 0, 0), FVector3f(0, 0, 1), FVector3f(0, 1, 0) },
			{ FVector3f(0, 1, 0), FVector3f(0, 0, 1), FVector3f(1, 0, 0) }, { FVector3f(0, -1, 0), FVector3f(1, 0, 0), FVector3f(0, 0, 1) },
			{ FVector3f(0, 0, 1), FVector3f(1, 0, 0), FVector3f(0, 1, 0) }, { FVector3f(0, 0, -1), FVector3f(0, 1, 0), FVector3f(1, 0, 0) } };
		for (int32 f = 0; f < 6; f++)
		{
			const FVector3f Nf = Axes[f][0], U = Axes[f][1], V = Axes[f][2];
			const int32 Base = B.P.Num(), W = L.Num();
			for (int32 j = 0; j < W; j++) for (int32 i = 0; i < W; i++)
			{
				const FVector3f Q = Nf * H + U * L[i] + V * L[j];
				const FVector3f C(FMath::Clamp(Q.X, -In, In), FMath::Clamp(Q.Y, -In, In), FMath::Clamp(Q.Z, -In, In));
				const FVector3f D = (Q - C).GetSafeNormal();
				B.P.Add(C + D * Rb); B.N.Add(D); B.UV.Add(FVector2f(L[i] / 100.f + 0.5f, L[j] / 100.f + 0.5f));
			}
			for (int32 j = 0; j + 1 < W; j++) for (int32 i = 0; i + 1 < W; i++)
			{
				const int32 A = Base + j * W + i, Bb = A + 1, C2 = A + W, D2 = C2 + 1;
				// бет нормалі сыртқа қарауы үшін орауыштық тәртібі (U×V = Nf)
				B.Idx.Append({ A, Bb, C2, Bb, D2, C2 });
			}
		}
		B.FixWinding();
		SaveMesh(TEXT("SM_KAP_BevelCube"), B, false, true);
	}
	// SM_KAP_SmoothCyl: 100×100 см цилиндр, 32 сегмент, жиектері 3 см дөңгеленген
	{
		FMeshBuild B;
		struct FPr { float R, Z, Nr, Nz; };
		TArray<FPr> Pr;
		Pr.Add({ 0.f, 50.f, 0.f, 1.f });
		for (int32 k = 0; k <= 3; k++) { const float A = k / 3.f * HALF_PI; Pr.Add({ 47.f + 3.f * FMath::Sin(A), 47.f + 3.f * FMath::Cos(A), FMath::Sin(A), FMath::Cos(A) }); }
		Pr.Add({ 50.f, 0.f, 1.f, 0.f });
		for (int32 k = 0; k <= 3; k++) { const float A = HALF_PI + k / 3.f * HALF_PI; Pr.Add({ 47.f + 3.f * FMath::Sin(A), -47.f + 3.f * FMath::Cos(A), FMath::Sin(A), FMath::Cos(A) }); }
		Pr.Add({ 0.f, -50.f, 0.f, -1.f });
		const int32 Seg = 32, Rows = Pr.Num();
		for (int32 r = 0; r < Rows; r++) for (int32 s = 0; s <= Seg; s++)
		{
			const float An = s * 2.f * PI / Seg, Cx = FMath::Cos(An), Sy = FMath::Sin(An);
			B.P.Add(FVector3f(Pr[r].R * Cx, Pr[r].R * Sy, Pr[r].Z));
			B.N.Add(FVector3f(Pr[r].Nr * Cx, Pr[r].Nr * Sy, Pr[r].Nz).GetSafeNormal());
			B.UV.Add(FVector2f(s / (float)Seg, r / (float)(Rows - 1)));
		}
		for (int32 r = 0; r + 1 < Rows; r++) for (int32 s = 0; s < Seg; s++)
		{
			const int32 A = r * (Seg + 1) + s, Bb = A + 1, C2 = A + Seg + 1, D2 = C2 + 1;
			B.Idx.Append({ A, Bb, C2, Bb, D2, C2 });
		}
		B.FixWinding();
		SaveMesh(TEXT("SM_KAP_SmoothCyl"), B, false, true);
	}
	return true;
}

bool UKAPEditorLibrary::CreateMeshes()
{
	// Тас: кездейсоқ шумен деформацияланған сфера (Nanite)
	{
		FMeshBuild B;
		FRandomStream R(9);
		TArray<FVector3f> Bumps; for (int32 i = 0; i < 14; i++) Bumps.Add(FVector3f(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)).GetSafeNormal());
		B.Ellipsoid(FVector3f(0, 0, 0), FVector3f(55, 48, 40), 28, [&](FVector3f D)
		{
			float S = 1.f;
			for (const FVector3f& Bp : Bumps) S += 0.14f * FMath::Pow(FMath::Max(0.f, FVector3f::DotProduct(D, Bp)), 6.f) * (Bp.Z > 0 ? 1.f : -0.6f);
			S += 0.05f * FMath::Sin(D.X * 13 + D.Y * 7) * FMath::Cos(D.Z * 11);
			S *= D.Z < -0.3f ? 0.85f : 1.f; // жалпақ табан
			return FMath::Max(0.4f, S);
		});
		B.RecomputeNormals();
		SaveMesh(TEXT("SM_KAP_Rock"), B, true, true);
	}
	// Сексеуіл: бұралған бұтақтар + ұсақ жапырақ шоқтары
	{
		FMeshBuild B;
		FRandomStream R(5);
		TFunction<void(FVector3f, FVector3f, float, int32)> Branch = [&](FVector3f From, FVector3f Dir, float Len, int32 Depth)
		{
			const FVector3f To = From + Dir * Len;
			B.Tube(From, To, 4.f * (Depth + 1) * 0.6f, 3.f * Depth * 0.6f + 1.f, 6);
			if (Depth == 0) { B.Ellipsoid(To, FVector3f(18, 18, 12), 5, [&](FVector3f D) { return 0.8f + 0.3f * FMath::Abs(FMath::Sin(D.X * 9 + D.Y * 5)); }); return; }
			const int32 Kids = 2 + R.RandRange(0, 1);
			for (int32 k = 0; k < Kids; k++)
			{
				FVector3f Nd = (Dir + FVector3f(R.FRandRange(-0.8f, 0.8f), R.FRandRange(-0.8f, 0.8f), R.FRandRange(0.f, 0.6f))).GetSafeNormal();
				Branch(To, Nd, Len * R.FRandRange(0.55f, 0.75f), Depth - 1);
			}
		};
		for (int32 t = 0; t < 3; t++) Branch(FVector3f(R.FRandRange(-8.f, 8.f), R.FRandRange(-8.f, 8.f), 0), FVector3f(R.FRandRange(-0.4f, 0.4f), R.FRandRange(-0.4f, 0.4f), 1).GetSafeNormal(), 45, 3);
		SaveMesh(TEXT("SM_KAP_Saxaul"), B, false, false);
	}
	// Мутант қасқыр: дене, кеуде, бас, тұмсық, құлақ, аяқтар, құйрық, арқадағы өсінділер (X — алға)
	{
		FMeshBuild B;
		B.Ellipsoid(FVector3f(0, 0, 75), FVector3f(60, 22, 24), 14);
		B.Ellipsoid(FVector3f(30, 0, 80), FVector3f(30, 26, 28), 12);
		B.Ellipsoid(FVector3f(68, 0, 95), FVector3f(20, 15, 16), 12);
		B.Ellipsoid(FVector3f(88, 0, 89), FVector3f(16, 8, 8), 8);
		B.Tube(FVector3f(64, -9, 106), FVector3f(60, -11, 124), 5, 1, 5); B.Tube(FVector3f(64, 9, 106), FVector3f(60, 11, 124), 5, 1, 5);
		for (const FVector3f& L : { FVector3f(35, -12, 70), FVector3f(35, 12, 70), FVector3f(-35, -12, 70), FVector3f(-35, 12, 70) }) B.Tube(L, L + FVector3f(L.X > 0 ? 6.f : -4.f, 0, -68), 7, 4, 6);
		B.Tube(FVector3f(-58, 0, 82), FVector3f(-95, 0, 66), 7, 2, 6);
		for (int32 i = 0; i < 5; i++) B.Tube(FVector3f(28 - i * 15, 0, 96), FVector3f(20 - i * 15, 0, 118 - i), 4, 1, 5);
		B.RecomputeNormals();
		SaveMesh(TEXT("SM_KAP_Wolf"), B, false, false);
	}
	// Құрт сегменті: қатпарлы сфера
	{
		FMeshBuild B;
		B.Ellipsoid(FVector3f(0, 0, 0), FVector3f(50, 50, 50), 16, [](FVector3f D) { return 1.f + 0.06f * FMath::Sin(D.X * 18.f) + 0.04f * FMath::Cos(D.Z * 22.f); });
		B.RecomputeNormals();
		SaveMesh(TEXT("SM_KAP_WormSeg"), B, false, false);
	}
	return true;
}

// ---------------------------------------------------------------------
// CSV → Data Table
// ---------------------------------------------------------------------
bool UKAPEditorLibrary::ImportDataTables()
{
	struct FT { const TCHAR* Name; UScriptStruct* S; };
	const FT Tables[] = {
		{ TEXT("Config"), FKAPConfigRow::StaticStruct() }, { TEXT("Wells"), FKAPWellRow::StaticStruct() }, { TEXT("Nodes"), FKAPNodeRow::StaticStruct() },
		{ TEXT("Roads"), FKAPRoadRow::StaticStruct() }, { TEXT("Locations"), FKAPLocationRow::StaticStruct() }, { TEXT("Vehicles"), FKAPVehicleRow::StaticStruct() },
		{ TEXT("Weapons"), FKAPWeaponRow::StaticStruct() }, { TEXT("Items"), FKAPItemRow::StaticStruct() }, { TEXT("Quests"), FKAPQuestRow::StaticStruct() },
		{ TEXT("Names"), FKAPNameRow::StaticStruct() }, { TEXT("Brigades"), FKAPBrigadeRow::StaticStruct() } };
	for (const FT& T : Tables)
	{
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectContentDir() / TEXT("KAP/Data") / (FString(T.Name) + TEXT(".csv"))))) continue;
		const FString AName = FString(TEXT("DT_")) + T.Name;
		UPackage* P = MakePkg(TEXT("/Game/KAP/Data/") + AName);
		UDataTable* DT = NewObject<UDataTable>(P, FName(*AName), RF_Public | RF_Standalone);
		DT->RowStruct = T.S;
		for (const FString& Prob : DT->CreateTableFromCSVString(Csv)) UE_LOG(LogTemp, Warning, TEXT("KAP %s: %s"), T.Name, *Prob);
		SaveAsset(DT);
	}
	return true;
}

// ---------------------------------------------------------------------
// Шөлейт ландшафты
// ---------------------------------------------------------------------
namespace KAPGen
{
	static float Hash(int32 I, int32 J)
	{
		uint32 H = (uint32)I * 374761393u + (uint32)J * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H = H ^ (H >> 16);
		return H / 4294967295.f;
	}
	static float Noise(float X, float Z)
	{
		const int32 I = FMath::FloorToInt32(X), J = FMath::FloorToInt32(Z);
		const float Fx = X - I, Fz = Z - J, U = Fx * Fx * (3 - 2 * Fx), V = Fz * Fz * (3 - 2 * Fz);
		const float A = Hash(I, J), B = Hash(I + 1, J), C = Hash(I, J + 1), D = Hash(I + 1, J + 1);
		return A + (B - A) * U + (C - A) * V + (A - B - C + D) * U * V;
	}
	static float Fbm(float X, float Z, int32 O) { float S = 0, A = 0.5f, F = 1; for (int32 k = 0; k < O; k++) { S += A * Noise(X * F, Z * F); F *= 2; A *= 0.5f; } return S; }
	static float Smooth(float E0, float E1, float X) { const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f); return T * T * (3 - 2 * T); }
	/** метр → метр; веб-прототиппен бірдей бедер */
	static float HeightRaw(float X, float Y)
	{
		float H = 22.f * Fbm(X / 650.f + 11.3f, Y / 650.f - 4.1f, 5);
		const float Warp = 3.f * Fbm(X / 300.f, Y / 300.f, 2);
		const float R = 1.f - FMath::Abs(FMath::Sin((X * 0.72f + Y * 0.31f) / 46.f + Warp));
		H += R * R * 4.5f;
		H += 0.6f * Fbm(X / 12.f, Y / 12.f, 2); // ұсақ толқындар
		const float RiverY = 120.f * FMath::Sin(X / 260.f) + 60.f;
		H -= 6.f * (1.f - Smooth(0, 40, FMath::Abs(Y - RiverY)));
		H += Smooth(1020, 1200, FMath::Max(FMath::Abs(X), FMath::Abs(Y))) * 70.f;
		return H;
	}
}

bool UKAPEditorLibrary::CreateDesertLandscape()
{
	UWorld* W = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!W) return false;
	// тегістелетін алаңдар (ауылдар, ұңғымалар, көліктер)
	struct FSite { float X, Y, R, H; };
	TArray<FSite> Sites;
	auto LoadRows = [&](const TCHAR* Name, UScriptStruct* St, TFunction<void(uint8*)> Fn)
	{
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectContentDir() / TEXT("KAP/Data") / (FString(Name) + TEXT(".csv"))))) return;
		UDataTable* DT = NewObject<UDataTable>(); DT->RowStruct = St; DT->CreateTableFromCSVString(Csv);
		for (const TPair<FName, uint8*>& R : DT->GetRowMap()) Fn(R.Value);
	};
	LoadRows(TEXT("Locations"), FKAPLocationRow::StaticStruct(), [&](uint8* P) { const FKAPLocationRow* R = (FKAPLocationRow*)P; Sites.Add({ R->X, R->Y, R->R, KAPGen::HeightRaw(R->X, R->Y) }); });
	LoadRows(TEXT("Wells"), FKAPWellRow::StaticStruct(), [&](uint8* P) { const FKAPWellRow* R = (FKAPWellRow*)P; Sites.Add({ R->X, R->Y, 9.f, KAPGen::HeightRaw(R->X, R->Y) }); });
	LoadRows(TEXT("Vehicles"), FKAPVehicleRow::StaticStruct(), [&](uint8* P) { const FKAPVehicleRow* R = (FKAPVehicleRow*)P; Sites.Add({ R->X, R->Y, 6.f, KAPGen::HeightRaw(R->X, R->Y) }); });
	auto Height = [&](float X, float Y)
	{
		float H = KAPGen::HeightRaw(X, Y);
		for (const FSite& S : Sites)
		{
			const float D = FMath::Sqrt(FMath::Square(X - S.X) + FMath::Square(Y - S.Y));
			const float Wt = 1.f - KAPGen::Smooth(S.R, S.R + 30.f, D);
			if (Wt > 0) H = FMath::Lerp(H, S.H, Wt);
		}
		return H;
	};
	const int32 Comps = 16, Sections = 2, Quads = 63;
	const int32 QPC = Sections * Quads, Size = Comps * QPC + 1; // 2017
	const float ScaleXY = 120.f; // см/квад → 2.42 км
	TArray<uint16> Data; Data.SetNumUninitialized(Size * Size);
	for (int32 j = 0; j < Size; j++) for (int32 i = 0; i < Size; i++)
	{
		const float Xm = (i - (Size - 1) * 0.5f) * ScaleXY / 100.f, Ym = (j - (Size - 1) * 0.5f) * ScaleXY / 100.f;
		Data[j * Size + i] = (uint16)FMath::Clamp(32768.f + Height(Xm, Ym) * 128.f, 0.f, 65535.f);
	}
	TMap<FGuid, TArray<uint16>> HeightLayers; HeightLayers.Add(FGuid(), Data);
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MatLayers; MatLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
	const FVector Loc(-(Size - 1) * ScaleXY * 0.5f, -(Size - 1) * ScaleXY * 0.5f, 0);
	ALandscape* L = W->SpawnActor<ALandscape>(Loc, FRotator::ZeroRotator);
	L->SetActorRelativeScale3D(FVector(ScaleXY, ScaleXY, 100.f));
	L->LandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Landscape.M_KAP_Landscape"));
	L->StaticLightingLOD = 1;
	L->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, Sections, Quads, HeightLayers, TEXT(""), MatLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	if (ULandscapeInfo* Info = L->GetLandscapeInfo()) Info->UpdateLayerInfoMap(L);
	L->SetActorLabel(TEXT("KAP_Steppe_Landscape"));
	return true;
}

bool UKAPEditorLibrary::SpawnLevelActors()
{
	UWorld* W = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!W) return false;
	AWeatherManager* WX = W->SpawnActor<AWeatherManager>(FVector(0, 0, 500), FRotator::ZeroRotator);
	if (WX) WX->SetActorLabel(TEXT("KAP_WeatherManager"));
	if (AWorldSettings* WS = W->GetWorldSettings()) { WS->DefaultGameMode = AKAPGameMode::StaticClass(); WS->MarkPackageDirty(); }
	return true;
}

bool UKAPEditorLibrary::BuildEverything(const FString& MapPath)
{
	CreateMaterials();
	CreateAtmosphereMaterials();
	CreateMeshes();
	ImportDataTables();
	ULevelEditorSubsystem* LES = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
	if (!LES || !LES->NewLevel(MapPath, false)) { UE_LOG(LogTemp, Error, TEXT("KAP: NewLevel сәтсіз")); return false; }
	CreateDesertLandscape();
	SpawnLevelActors();
	const bool bOk = LES->SaveCurrentLevel();
	UE_LOG(LogTemp, Display, TEXT("KAP: деңгей құрылды: %s (%s)"), *MapPath, bOk ? TEXT("сақталды") : TEXT("САҚТАЛМАДЫ"));
	return bOk;
}
