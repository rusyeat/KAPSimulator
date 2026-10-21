// Нақты карта: әлемді жоғарыдан ортографиялық камерамен 4×4 бөлікке түсіріп (түс + биіктік),
// рельеф көлеңкесімен 2048² текстураға жинау. Толық экран карта (M): масштаб, сүйреу, белгі қою,
// әр орынға белгіше, атаулар, шартты белгілер, кеңес терезесі. Планшеттің «Карта» беті де осыны қолданады.
#include "KAPHUD.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Player/NavigatorComponent.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPVehicleHandle.h"
#include "KAP/Audio/KAPAudio.h"
#include "KAPSimulatorPawn.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/PlayerCameraManager.h"
#include "KAP/World/KAPVisuals.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"

namespace
{
	const FLinearColor MAmber(0.95f, 0.76f, 0.3f), MMuted(0.55f, 0.51f, 0.44f), MTxtc(0.94f, 0.9f, 0.82f);
	const FLinearColor MCGreen(0.37f, 0.83f, 0.54f), MCRed(1.f, 0.35f, 0.29f), MCBlue(0.44f, 0.71f, 1.f), MCOrange(1.f, 0.54f, 0.23f), MCYel(1.f, 0.82f, 0.29f);
	enum EIcon : int32 { IWell, IVillage, IPolice, IShop, IArmy, IWagon, ITower, IHQ, IWater, IBandit, INest, IRuins, ICar, IJob, IWreck, IGuard, INum };
	// қабаттар (шартты белгілер тізімінде қосып/өшіріледі)
	enum ELayer : int32 { LWells, LPlaces, LServices, LArmy, LShelter, LTowers, LJobs, LCars, LDanger, LNum };
	FLinearColor WellCol(EWellState S) { return S == EWellState::Ok ? MCGreen : S == EWellState::Fault ? MCYel : S == EWellState::Leak ? MCOrange : S == EWellState::Fire ? MCRed : FLinearColor(0.45f, 0.45f, 0.45f); }
	const TCHAR* WellName(EWellState S) { return S == EWellState::Ok ? TEXT("Жұмыс істеп тұр") : S == EWellState::Fault ? TEXT("Ақау бар — жөндеу керек") : S == EWellState::Leak ? TEXT("Ағып жатыр! Жұмысшылар сәулеленуде") : S == EWellState::Fire ? TEXT("ӨРТ!") : TEXT("Жоғалған"); }
	struct FPt { FVector2D P; int32 Icon; FLinearColor Col; FString Title, Info, Num; int32 Layer; bool bLabel; };
}

// ---------------------------------------------------------------------
// Әлемді түсіру
// ---------------------------------------------------------------------
UTexture2D* AKAPHUD::CaptureWorldMap()
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	const double T0 = FPlatformTime::Seconds();
	const int32 Tiles = 4, TS = 512, N = Tiles * TS;
	const float Ext = 2400.f, TileM = Ext / Tiles;
	// камера ең биік нүктеден сәл жоғары: алыс болса, ұсақ заттар (жол, ағаш, бағана) қашықтық бойынша кесіледі
	float MaxZ = -1e9f;
	for (int32 j = 0; j <= 32; j++) for (int32 i = 0; i <= 32; i++) MaxZ = FMath::Max(MaxZ, KAPVis::GroundZ(W, (-Ext * 0.5f + i * Ext / 32) * 100.f, (-Ext * 0.5f + j * Ext / 32) * 100.f, 0.f));
	const float CamZ = MaxZ + 6000.f;
	// түсіру кезінде көріну қашықтығын уақытша үлкейту (сол басымдықпен қайтарамыз — баптаулар бұзылмасын)
	IConsoleVariable* VDS = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale"));
	const float OldVDS = VDS ? VDS->GetFloat() : 1.f;
	const EConsoleVariableFlags VDSBy = VDS ? (EConsoleVariableFlags)(VDS->GetFlags() & ECVF_SetByMask) : ECVF_SetByCode;
	if (VDS) { VDS->Set(TEXT("50"), VDSBy); IConsoleManager::Get().CallAllConsoleVariableSinks(); }
	UTextureRenderTarget2D* RC = NewObject<UTextureRenderTarget2D>(this);
	RC->InitCustomFormat(TS, TS, PF_FloatRGBA, true);
	UTextureRenderTarget2D* RD = NewObject<UTextureRenderTarget2D>(this);
	RD->InitCustomFormat(TS, TS, PF_A32B32G32R32F, true);
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* Cap = W->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FVector(0, 0, CamZ), FRotator::ZeroRotator, SP);
	if (!Cap) return nullptr;
	USceneCaptureComponent2D* C = Cap->GetCaptureComponent2D();
	C->ProjectionType = ECameraProjectionMode::Orthographic;
	C->OrthoWidth = TileM * 100.f;
	C->bCaptureEveryFrame = false; C->bCaptureOnMovement = false;
	C->ShowFlags.SetFog(false); C->ShowFlags.SetAtmosphere(false); C->ShowFlags.SetVolumetricFog(false); C->ShowFlags.SetCloud(false);
	C->ShowFlags.SetParticles(false); C->ShowFlags.SetSkeletalMeshes(false); C->ShowFlags.SetTextRender(false); C->ShowFlags.SetDecals(false);
	C->ShowFlags.SetDynamicShadows(false); C->ShowFlags.SetGlobalIllumination(false); C->ShowFlags.SetLumenGlobalIllumination(false); C->ShowFlags.SetLumenReflections(false);
	C->ShowFlags.SetAmbientOcclusion(false); C->ShowFlags.SetDistanceFieldAO(false); C->ShowFlags.SetScreenSpaceReflections(false); C->ShowFlags.SetTranslucency(false);
	C->bAutoCalculateOrthoPlanes = false;
	// көліктер қозғалады — суретте қалмасын (картада белгішемен көрсетіледі)
	if (AKAPGameMode* GM = AKAPGameMode::Get(this))
		if (GM->Builder)
			for (const TWeakObjectPtr<AKAPVehicleHandle>& V : GM->Builder->Vehicles)
				if (V.IsValid()) { C->HiddenActors.Add(V.Get()); if (V->Pawn) C->HiddenActors.Add(V->Pawn); }
	if (APawn* Me = GetOwningPawn()) C->HiddenActors.Add(Me);
	// жоғарыдан төмен: экранның оңы — әлемнің +X, төмені — +Y (картамен бірдей)
	Cap->SetActorRotation(FRotationMatrix::MakeFromXY(FVector(0, 0, -1), FVector(1, 0, 0)).Rotator());
	// үш өту: түс (бәрі), биіктік (бәрі), биіктік (тек жер бедері — ландшафт). Айырмасы — ғимараттар, қоршаулар, бұталар
	for (TActorIterator<ALandscapeProxy> It(W); It; ++It) C->ShowOnlyActors.Add(*It);
	const bool bHasLand = C->ShowOnlyActors.Num() > 0;
	TArray<FLinearColor> Alb; Alb.SetNumZeroed(N * N);
	TArray<float> Ha, Hg; Ha.Init(1e9f, N * N); Hg.Init(1e9f, N * N);
	TArray<FLinearColor> Buf;
	int32 Valid = 0;
	for (int32 j = 0; j < Tiles; j++) for (int32 i = 0; i < Tiles; i++)
	{
		Cap->SetActorLocation(FVector((-Ext * 0.5f + (i + 0.5f) * TileM) * 100.f, (-Ext * 0.5f + (j + 0.5f) * TileM) * 100.f, CamZ));
		for (int32 Pass = 0; Pass < 3; Pass++)
		{
			if (Pass == 2 && !bHasLand) continue;
			C->PrimitiveRenderMode = Pass == 2 ? ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList : ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
			C->CaptureSource = Pass == 0 ? SCS_BaseColor : SCS_SceneDepth;
			C->TextureTarget = Pass == 0 ? RC : RD;
			C->CaptureScene();
			FTextureRenderTargetResource* R = (Pass == 0 ? RC : RD)->GameThread_GetRenderTargetResource();
			Buf.Reset();
			if (!R || !R->ReadLinearColorPixels(Buf) || Buf.Num() != TS * TS) continue;
			for (int32 y = 0; y < TS; y++) for (int32 x = 0; x < TS; x++)
			{
				const FLinearColor& V = Buf[y * TS + x];
				const int32 Id = (j * TS + y) * N + i * TS + x;
				if (Pass == 0) { Alb[Id] = V; if ((V.R + V.G + V.B) > 0.01f) Valid++; continue; }
				// биіктік (м): камерадан қашықтықтың теріс таңбасы — тек айырмашылықтар маңызды; аспан — 1e9
				(Pass == 1 ? Ha : Hg)[Id] = V.R > 0.f && V.R < 1e7f ? -V.R / 100.f : 1e9f;
			}
		}
	}
	Cap->Destroy();
	if (VDS) { VDS->Set(*FString::SanitizeFloat(OldVDS), VDSBy); IConsoleManager::Get().CallAllConsoleVariableSinks(); UE_LOG(LogTemp, Display, TEXT("KAPMAP view distance restored: %.2f (was %.2f)"), VDS->GetFloat(), OldVDS); }
	if (Valid < N * N / 4) { UE_LOG(LogTemp, Warning, TEXT("KAPMAP capture failed (%d valid)"), Valid); return nullptr; }
	int32 GValid = 0; for (float V : Hg) if (V < 1e8f) GValid++;
	if (GValid < N * N / 4) { UE_LOG(LogTemp, Warning, TEXT("KAPMAP ground pass empty (%d) — no building mask"), GValid); Hg = Ha; }
	for (int32 k = 0; k < N * N; k++) { if (Ha[k] > 1e8f) Ha[k] = Hg[k] < 1e8f ? Hg[k] : 0.f; if (Hg[k] > 1e8f) Hg[k] = Ha[k]; }
	float Gmin = 1e9f, Gmax = -1e9f; for (float V : Hg) { Gmin = FMath::Min(Gmin, V); Gmax = FMath::Max(Gmax, V); }
	// ---- нысандарды бөлу: жерден 0.5 м биік пиксельдер, байланысқан топтар. Кіші топ — бұта/бағана, үлкені — ғимарат ----
	TArray<uint8> Kind; Kind.SetNumZeroed(N * N); // 0 — жер, 1 — ұсақ (бұта, бағана), 2 — ғимарат/қоршау
	{
		TArray<uint8> Seen; Seen.SetNumZeroed(N * N);
		TArray<int32> Stack, Comp;
		for (int32 s = 0; s < N * N; s++)
		{
			if (Seen[s] || Ha[s] - Hg[s] < 0.5f) continue;
			Stack.Reset(); Comp.Reset(); Stack.Add(s); Seen[s] = 1;
			while (Stack.Num())
			{
				const int32 p = Stack.Pop(EAllowShrinking::No); Comp.Add(p);
				const int32 px = p % N, py = p / N;
				const int32 Nb[4] = { px > 0 ? p - 1 : -1, px < N - 1 ? p + 1 : -1, py > 0 ? p - N : -1, py < N - 1 ? p + N : -1 };
				for (int32 q : Nb) if (q >= 0 && !Seen[q] && Ha[q] - Hg[q] >= 0.5f) { Seen[q] = 1; Stack.Add(q); }
			}
			const uint8 Kd = Comp.Num() >= 14 ? 2 : 1;
			for (int32 p : Comp) Kind[p] = Kd;
		}
	}
	// ---- бояу: жер — түс + рельеф көлеңкесі (ғимаратсыз биіктіктен) + биіктік бойынша жеңіл реңк;
	//      ғимарат — ашық шатыр, қою жиек; ғимарат көлеңкесі оңтүстік-шығысқа түседі ----
	const float Px = Ext / N;
	const FVector L = FVector(-1.f, -1.f, 1.3f).GetSafeNormal();
	TArray<FColor> Out; Out.SetNumUninitialized(N * N);
	auto Hgv = [&](int32 x, int32 y) { return Hg[FMath::Clamp(y, 0, N - 1) * N + FMath::Clamp(x, 0, N - 1)]; };
	auto Hav = [&](int32 x, int32 y) { return Ha[FMath::Clamp(y, 0, N - 1) * N + FMath::Clamp(x, 0, N - 1)]; };
	auto KindAt = [&](int32 x, int32 y) { return Kind[FMath::Clamp(y, 0, N - 1) * N + FMath::Clamp(x, 0, N - 1)]; };
	const FLinearColor RoofLight(0.72f, 0.7f, 0.66f), Edge(0.09f, 0.08f, 0.07f), Bush(0.16f, 0.17f, 0.09f);
	for (int32 y = 0; y < N; y++) for (int32 x = 0; x < N; x++)
	{
		const int32 Id = y * N + x;
		const FLinearColor A = Alb[Id];
		FLinearColor O;
		if (Kind[Id] == 2)
		{
			const bool bEdge = KindAt(x - 1, y) != 2 || KindAt(x + 1, y) != 2 || KindAt(x, y - 1) != 2 || KindAt(x, y + 1) != 2;
			const float Dx = FMath::Clamp((Hav(x + 1, y) - Hav(x - 1, y)) / (2.f * Px), -2.f, 2.f), Dy = FMath::Clamp((Hav(x, y + 1) - Hav(x, y - 1)) / (2.f * Px), -2.f, 2.f);
			const float Sh = FMath::Clamp(0.8f + 0.3f * (float)FVector::DotProduct(FVector(-Dx, -Dy, 1.f).GetSafeNormal(), L), 0.65f, 1.15f);
			O = bEdge ? Edge : FMath::Lerp(A, RoofLight, 0.45f) * Sh;
		}
		else if (Kind[Id] == 1) O = FMath::Lerp(A * 0.5f, Bush, 0.6f);
		else
		{
			const float H0 = Hg[Id];
			const float Dx = FMath::Clamp((Hgv(x + 1, y) - Hgv(x - 1, y)) / (2.f * Px), -3.f, 3.f), Dy = FMath::Clamp((Hgv(x, y + 1) - Hgv(x, y - 1)) / (2.f * Px), -3.f, 3.f);
			const float Sh = FMath::Clamp(0.5f + 0.62f * (float)FVector::DotProduct(FVector(-Dx * 1.3f, -Dy * 1.3f, 1.f).GetSafeNormal(), L), 0.35f, 1.25f);
			const float Tn = Gmax > Gmin ? (H0 - Gmin) / (Gmax - Gmin) : 0.5f;
			O = FLinearColor(FMath::Pow(A.R, 0.92f), FMath::Pow(A.G, 0.95f), FMath::Pow(A.B, 1.02f)) * Sh * FMath::Lerp(0.95f, 1.07f, Tn) * 1.1f;
			// ғимарат көлеңкесі (күн 45°, солтүстік-батыстан)
			for (int32 k = 1; k <= 8; k++)
				if (KindAt(x - k, y - k) == 2 && Hav(x - k, y - k) - H0 > k * Px * 1.2f) { O *= 0.62f; break; }
		}
		O.A = 1.f;
		Out[Id] = O.ToFColor(true);
	}
	// тексеру үшін: түсірілген суретті PNG қылып сақтау
	if (FParse::Param(FCommandLine::Get(), TEXT("kapmapshots")))
	{
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(N, N, TArrayView64<const FColor>(Out.GetData(), Out.Num()), Png);
		FFileHelper::SaveArrayToFile(Png, *(FPaths::ScreenShotDir() / TEXT("KAPM_raw.png")));
	}
	// текстура + мип тізбегі (кішірейтілгенде жыпырламасын)
	UTexture2D* T = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
	if (!T) return nullptr;
	FTexturePlatformData* PD = T->GetPlatformData();
	{ void* D = PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE); FMemory::Memcpy(D, Out.GetData(), N * N * 4); PD->Mips[0].BulkData.Unlock(); }
	TArray<FColor> Prev = MoveTemp(Out);
	int32 S = N;
	while (S > 256)
	{
		const int32 S2 = S / 2;
		TArray<FColor> Next; Next.SetNumUninitialized(S2 * S2);
		for (int32 y = 0; y < S2; y++) for (int32 x = 0; x < S2; x++)
		{
			const FColor& a = Prev[(2 * y) * S + 2 * x]; const FColor& b = Prev[(2 * y) * S + 2 * x + 1]; const FColor& c = Prev[(2 * y + 1) * S + 2 * x]; const FColor& d = Prev[(2 * y + 1) * S + 2 * x + 1];
			Next[y * S2 + x] = FColor((a.R + b.R + c.R + d.R) / 4, (a.G + b.G + c.G + d.G) / 4, (a.B + b.B + c.B + d.B) / 4, 255);
		}
		FTexture2DMipMap* Mip = new FTexture2DMipMap(S2, S2);
		PD->Mips.Add(Mip);
		void* D = Mip->BulkData.Lock(LOCK_READ_WRITE); D = Mip->BulkData.Realloc(S2 * S2 * 4); FMemory::Memcpy(D, Next.GetData(), S2 * S2 * 4); Mip->BulkData.Unlock();
		Prev = MoveTemp(Next); S = S2;
	}
	T->SRGB = true; T->Filter = TF_Trilinear;
	T->UpdateResource();
	UE_LOG(LogTemp, Display, TEXT("KAPMAP captured %dx%d in %.2f s"), N, N, FPlatformTime::Seconds() - T0);
	return T;
}

// ---------------------------------------------------------------------
// Сурет көмекшілері
// ---------------------------------------------------------------------
void AKAPHUD::FillTri(FVector2D A, FVector2D B, FVector2D C, FLinearColor Col)
{
	Col.A *= UiA;
	TArray<FCanvasUVTri> T; FCanvasUVTri X;
	X.V0_Pos = A * Sc; X.V1_Pos = B * Sc; X.V2_Pos = C * Sc; X.V0_Color = X.V1_Color = X.V2_Color = Col; T.Add(X);
	FCanvasTriangleItem TI(T, GWhiteTexture); TI.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(TI);
}

void AKAPHUD::MapIcon(int32 Type, FVector2D C, float R, FLinearColor Col, const FString& Num, bool bHover)
{
	const FLinearColor Wt(1, 1, 1, 0.97f), Dk(0.05f, 0.05f, 0.05f, 0.9f);
	if (bHover) R *= 1.25f;
	// көлеңке, ақ жиек, түсті негіз
	if (Type == IArmy) { Rect(C.X - R - 1.5f, C.Y - R + 0.5f, 2 * R + 4, 2 * R + 4, FLinearColor(0, 0, 0, 0.45f)); Rect(C.X - R - 1.5f, C.Y - R - 1.5f, 2 * R + 3, 2 * R + 3, Wt); Rect(C.X - R, C.Y - R, 2 * R, 2 * R, Col); }
	else if (Type == IJob)
	{
		FillTri(C + FVector2D(0, -R * 1.45f), C + FVector2D(R * 1.45f, 0), C + FVector2D(0, R * 1.45f), Wt); FillTri(C + FVector2D(0, -R * 1.45f), C + FVector2D(0, R * 1.45f), C + FVector2D(-R * 1.45f, 0), Wt);
		FillTri(C + FVector2D(0, -R * 1.2f), C + FVector2D(R * 1.2f, 0), C + FVector2D(0, R * 1.2f), Col); FillTri(C + FVector2D(0, -R * 1.2f), C + FVector2D(0, R * 1.2f), C + FVector2D(-R * 1.2f, 0), Col);
		Txt(C.X, C.Y - R * 0.85f, TEXT("!"), R * 1.3f, Dk, 1, false);
		return;
	}
	else { Disc(C + FVector2D(1, 1.5f), R + 1.6f, FLinearColor(0, 0, 0, 0.45f)); Disc(C, R + 1.6f, Wt); Disc(C, R, Col); }
	const float r = R;
	auto Star = [&](FVector2D Cc, float Ro, float Ri, FLinearColor Sc2)
	{
		for (int32 k = 0; k < 5; k++)
		{
			const float A0 = -UE_HALF_PI + k * UE_TWO_PI / 5.f, A1 = A0 + UE_PI / 5.f, A2 = A0 + 2.f * UE_PI / 5.f;
			const FVector2D P0 = Cc + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Ro, P1 = Cc + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Ri, P2 = Cc + FVector2D(FMath::Cos(A2), FMath::Sin(A2)) * Ro;
			FillTri(Cc, P0, P1, Sc2); FillTri(Cc, P1, P2, Sc2);
		}
	};
	auto House = [&](FLinearColor Hc)
	{
		FillTri(C + FVector2D(-0.7f * r, -0.02f * r), C + FVector2D(0.7f * r, -0.02f * r), C + FVector2D(0, -0.68f * r), Hc);
		Rect(C.X - 0.47f * r, C.Y - 0.05f * r, 0.94f * r, 0.6f * r, Hc);
	};
	switch (Type)
	{
	case IWell: Txt(C.X, C.Y - r * 0.62f, Num, r * 1.05f, Dk, 1, false); break;
	case IVillage: House(Wt); Rect(C.X - 0.12f * r, C.Y + 0.18f * r, 0.24f * r, 0.37f * r, Col); break;
	case IPolice: Star(C + FVector2D(0, 0.05f * r), 0.68f * r, 0.28f * r, Wt); break;
	case IShop: Txt(C.X, C.Y - r * 0.72f, TEXT("₸"), r * 1.25f, Wt, 1, false); break;
	case IArmy: Star(C + FVector2D(0, 0.05f * r), 0.72f * r, 0.3f * r, Wt); break;
	case IWagon: House(Wt); Rect(C.X - 0.06f * r, C.Y + 0.05f * r, 0.12f * r, 0.42f * r, Col); Rect(C.X - 0.24f * r, C.Y + 0.2f * r, 0.48f * r, 0.12f * r, Col); break; // «+» баспана
	case ITower:
		Line(C + FVector2D(0, 0.62f * r), C + FVector2D(0, -0.45f * r), Wt, 2.f);
		Line(C + FVector2D(-0.42f * r, 0.62f * r), C + FVector2D(0, -0.1f * r), Wt, 2.f); Line(C + FVector2D(0.42f * r, 0.62f * r), C + FVector2D(0, -0.1f * r), Wt, 2.f);
		Disc(C + FVector2D(0, -0.52f * r), 0.16f * r, Wt);
		break;
	case IHQ: Rect(C.X - 0.5f * r, C.Y - 0.2f * r, 0.42f * r, 0.75f * r, Wt); Rect(C.X - 0.02f * r, C.Y - 0.6f * r, 0.52f * r, 1.15f * r, Wt); break;
	case IWater: Disc(C + FVector2D(0, 0.15f * r), 0.42f * r, Wt); FillTri(C + FVector2D(-0.4f * r, 0.08f * r), C + FVector2D(0.4f * r, 0.08f * r), C + FVector2D(0, -0.65f * r), Wt); break;
	case IBandit: Line(C + FVector2D(-0.45f * r, -0.45f * r), C + FVector2D(0.45f * r, 0.45f * r), Wt, 3.f); Line(C + FVector2D(0.45f * r, -0.45f * r), C + FVector2D(-0.45f * r, 0.45f * r), Wt, 3.f); break;
	case INest: for (int32 k = 0; k < 3; k++) { const float A = -UE_HALF_PI + k * UE_TWO_PI / 3.f; Disc(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 0.34f * r, 0.26f * r, Wt); } Disc(C, 0.14f * r, Col); break;
	case IRuins: FillTri(C + FVector2D(-0.7f * r, 0.0f), C + FVector2D(0.3f * r, -0.1f * r), C + FVector2D(-0.2f * r, -0.65f * r), Wt); Rect(C.X - 0.47f * r, C.Y - 0.02f * r, 0.94f * r, 0.56f * r, Wt); Line(C + FVector2D(0.1f * r, 0.0f), C + FVector2D(-0.05f * r, 0.5f * r), Col, 2.f); break;
	case ICar: case IWreck:
		Rect(C.X - 0.62f * r, C.Y - 0.12f * r, 1.24f * r, 0.42f * r, Wt); Rect(C.X - 0.36f * r, C.Y - 0.42f * r, 0.72f * r, 0.32f * r, Wt);
		Disc(C + FVector2D(-0.34f * r, 0.32f * r), 0.16f * r, Dk); Disc(C + FVector2D(0.34f * r, 0.32f * r), 0.16f * r, Dk);
		break;
	case IGuard: FillTri(C + FVector2D(-0.5f * r, -0.2f * r), C + FVector2D(0.5f * r, -0.2f * r), C + FVector2D(0, 0.62f * r), Wt); Rect(C.X - 0.5f * r, C.Y - 0.55f * r, r, 0.38f * r, Wt); break;
	default: break;
	}
}

// ---------------------------------------------------------------------
// Карта көрінісі (толық экран және планшет)
// ---------------------------------------------------------------------
void AKAPHUD::Scroll(float D)
{
	if (bMap || (bTablet && Tab == 2))
	{
		const FVector2D Vc = MapView.GetCenter();
		const FVector2D Wm = MapCenter + (Mouse - Vc) / FMath::Max(0.001f, MapScale);
		const float Old = MapZoom;
		MapZoom = FMath::Clamp(MapZoom * (D > 0 ? 1.25f : 0.8f), 1.f, 9.f);
		const float NewScale = MapScale * MapZoom / Old;
		MapCenter = Wm - (Mouse - Vc) / FMath::Max(0.001f, NewScale);
		return;
	}
	TabletScroll = FMath::Max(0.f, TabletScroll - D * 60.f);
}

void AKAPHUD::MapRelease()
{
	if (!bMapDrag) return;
	bMapDrag = false;
	if ((Mouse - MapDragStart).Size() > 5.f) return;
	// басу — белгі (маршрут нысанасы) қою
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!GM || !P || !D || !P->Navigator) return;
	const FVector2D Wp = MapCenter + (Mouse - MapView.GetCenter()) / FMath::Max(0.001f, MapScale);
	if (FMath::Abs(Wp.X) > 1200 || FMath::Abs(Wp.Y) > 1200) return;
	{ const int32 Rg = KAP::RegionOf(Wp.X, Wp.Y); if (!GM->S().Regions[Rg]) { GM->Toast(Rg == 0 ? TEXT("Бұл аймақ әлі ашылмаған — алдымен «Буханка» радиаторын жөндеңіз") : TEXT("Бұл аймақ әлі ашылмаған — сигнал мұнарасын қосыңыз"), "warn"); return; } }
	FString Name = MapHoverTitle;
	FVector2D Tp = MapHoverTitle.IsEmpty() ? Wp : MapHoverP;
	if (Name.IsEmpty())
	{
		const int32 Nn = D->Graph.Nearest(Wp);
		const bool bNode = Nn >= 0 && FVector2D::Distance(D->Graph.Nodes[Nn].Pos, Wp) < 30;
		Tp = bNode ? D->Graph.Nodes[Nn].Pos : Wp;
		Name = bNode ? D->Graph.Nodes[Nn].Name : FString::Printf(TEXT("Белгі (%d, %d)"), (int32)Wp.X, (int32)Wp.Y);
	}
	P->Navigator->SetTarget(Tp, Name);
	if (UKAPAudio* A = UKAPAudio::Get(this)) A->Beep(true);
}

void AKAPHUD::DrawWorldMap(float X, float Y, float W, float H, bool bFull)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	if (!GM || !P || !D || !WM) return;
	const FKAPSession& S = GM->S();
	UNavigatorComponent* Nav = P->Navigator;
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	if (!bMapCentered) { MapCenter = bFull ? Pm : FVector2D::ZeroVector; MapZoom = bFull ? 2.2f : 1.f; bMapCentered = true; }
	MapView = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	const float Fit = FMath::Min(W, H) / 2400.f;
	MapScale = Fit * MapZoom;
	if (bMapDrag && bMouseDown) MapCenter = MapDragCenter - (Mouse - MapDragStart) / MapScale;
	// көрініс карта шегінен шықпасын
	const float HalfW = W * 0.5f / MapScale, HalfH = H * 0.5f / MapScale;
	MapCenter.X = HalfW >= 1200 ? 0 : FMath::Clamp(MapCenter.X, -1200 + HalfW, 1200 - HalfW);
	MapCenter.Y = HalfH >= 1200 ? 0 : FMath::Clamp(MapCenter.Y, -1200 + HalfH, 1200 - HalfH);
	const FVector2D Vc(X + W * 0.5f, Y + H * 0.5f);
	auto To = [&](FVector2D Wp) { return Vc + (Wp - MapCenter) * MapScale; };
	if (bMapTestMouse) Mouse = To(MapTestMouseW);
	auto In = [&](FVector2D Sp, float M = 0.f) { return Sp.X > X - M && Sp.X < X + W + M && Sp.Y > Y - M && Sp.Y < Y + H + M; };
	// ---- фон пен карта суреті ----
	Rect(X, Y, W, H, FLinearColor(0.16f, 0.13f, 0.09f, 1.f));
	if (UTexture2D* T = MapTexture())
	{
		const FVector2D W0(FMath::Max(-1200.f, MapCenter.X - HalfW), FMath::Max(-1200.f, MapCenter.Y - HalfH));
		const FVector2D W1(FMath::Min(1200.f, MapCenter.X + HalfW), FMath::Min(1200.f, MapCenter.Y + HalfH));
		const FVector2D S0 = To(W0), S1 = To(W1);
		FCanvasTileItem It(S0 * Sc, T->GetResource(), (S1 - S0) * Sc, FVector2D((W0.X + 1200) / 2400.f, (W0.Y + 1200) / 2400.f), FVector2D((W1.X + 1200) / 2400.f, (W1.Y + 1200) / 2400.f), FLinearColor::White);
		It.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(It);
	}
	// ---- кесінді көрініс тіктөртбұрышына қиып салу ----
	auto Seg = [&](FVector2D A, FVector2D B, FLinearColor Col, float Th)
	{
		// Лианг–Барски
		float t0 = 0.f, t1 = 1.f; const FVector2D Dd = B - A;
		const float Pp[4] = { -Dd.X, Dd.X, -Dd.Y, Dd.Y }, Qq[4] = { A.X - X, X + W - A.X, A.Y - Y, Y + H - A.Y };
		for (int32 k = 0; k < 4; k++)
		{
			if (FMath::IsNearlyZero(Pp[k])) { if (Qq[k] < 0) return; continue; }
			const float r = Qq[k] / Pp[k];
			if (Pp[k] < 0) t0 = FMath::Max(t0, r); else t1 = FMath::Min(t1, r);
			if (t0 > t1) return;
		}
		Line(A + Dd * t0, A + Dd * t1, Col, Th);
	};
	// ---- жолдар: қара жиек + ашық сызық ----
	const float Rw = FMath::Clamp(1.6f * FMath::Sqrt(MapZoom), 1.6f, 5.f);
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges) Seg(To(D->Graph.Nodes[E.A].Pos), To(D->Graph.Nodes[E.B].Pos), FLinearColor(0.1f, 0.07f, 0.04f, 0.55f), Rw + 2.2f);
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges) Seg(To(D->Graph.Nodes[E.A].Pos), To(D->Graph.Nodes[E.B].Pos), FLinearColor(0.93f, 0.86f, 0.7f, 0.9f), Rw);
	// ---- қауіп аймақтары, әскери аймақ ----
	const bool bL[LNum] = { (MapLayers & (1 << LWells)) != 0, (MapLayers & (1 << LPlaces)) != 0, (MapLayers & (1 << LServices)) != 0, (MapLayers & (1 << LArmy)) != 0, (MapLayers & (1 << LShelter)) != 0, (MapLayers & (1 << LTowers)) != 0, (MapLayers & (1 << LJobs)) != 0, (MapLayers & (1 << LCars)) != 0, (MapLayers & (1 << LDanger)) != 0 };
	if (bL[LDanger] && bLayerDanger) for (const FKAPDangerZone& Z : GM->Zones) { const FVector2D Zc = To(Z.P); if (In(Zc, Z.R * MapScale)) { Disc(Zc, Z.R * MapScale, FLinearColor(1, 0.15f, 0.1f, 0.04f + Z.Level * 0.06f)); Circle(Zc, Z.R * MapScale, FLinearColor(0.85f, 0.15f, 0.1f, 0.45f), 1.5f, 48); } }
	// HMM жылу картасы (жұмысшылар қай жерде болуы мүмкін)
	if (bLayerHeat && GM->bHeatActive && GM->Heat.Num() >= 64 * 64)
	{
		float Mx = 0; for (float V : GM->Heat) Mx = FMath::Max(Mx, V);
		if (Mx > 0)
			for (int32 J = 0; J < 64; J++) for (int32 I = 0; I < 64; I++)
			{
				const float V = GM->Heat[J * 64 + I] / Mx; if (V < 0.03f) continue;
				const FVector2D A = To(FVector2D(-1200 + I * 37.5f, -1200 + J * 37.5f)), B = To(FVector2D(-1200 + (I + 1) * 37.5f, -1200 + (J + 1) * 37.5f));
				const float Ax = FMath::Max(A.X, X), Ay = FMath::Max(A.Y, Y), Bx = FMath::Min(B.X, X + W), By = FMath::Min(B.Y, Y + H);
				if (Bx > Ax && By > Ay) Rect(Ax, Ay, Bx - Ax + 0.5f, By - Ay + 0.5f, FLinearColor(1, 0.7f - V * 0.7f, 0, 0.15f + V * 0.45f));
			}
	}
	if (bL[LArmy] && GM->bArmyBuilt && S.Regions[KAP::RegionOf(GM->ArmyC.X, GM->ArmyC.Y)])
	{
		const FVector2D A = To(GM->ArmyC - GM->ArmyHalf), B = To(GM->ArmyC + GM->ArmyHalf);
		if (In((A + B) * 0.5f, (B - A).Size()))
		{
			Rect(A.X, A.Y, B.X - A.X, B.Y - A.Y, FLinearColor(0.9f, 0.1f, 0.05f, 0.13f));
			for (float t = 0; t < (B.X - A.X) + (B.Y - A.Y); t += 12.f) Seg(FVector2D(A.X + FMath::Max(0.f, t - (B.Y - A.Y)), A.Y + FMath::Min(t, B.Y - A.Y)), FVector2D(A.X + FMath::Min(t, B.X - A.X), A.Y + FMath::Max(0.f, t - (B.X - A.X))), FLinearColor(0.9f, 0.15f, 0.05f, 0.25f), 1.f);
			Frame(A.X, A.Y, B.X - A.X, B.Y - A.Y, FLinearColor(1.f, 0.3f, 0.15f, 0.9f), 2.f);
		}
	}
	// ---- маршрут ----
	if (Nav && Nav->bHasTarget)
	{
		// балама маршруттар — жіңішке, белсендісі — жарқыраған жасыл
		const FLinearColor RC[3] = { FLinearColor(0.3f, 0.88f, 0.54f), MCBlue, MCYel };
		for (int32 m = 0; m < 3; m++)
		{
			const KAPAI::FRoute& R = Nav->Routes[m];
			if (!R.bValid || (int32)Nav->Mode == m) continue;
			for (int32 i = 1; i < R.Points.Num(); i++) Seg(To(R.Points[i - 1]), To(R.Points[i]), RC[m] * FLinearColor(1, 1, 1, 0.7f), 1.5f);
		}
		if (const KAPAI::FRoute* Rt = Nav->Active())
			for (int32 i = 1; i < Rt->Points.Num(); i++)
			{
				Seg(To(Rt->Points[i - 1]), To(Rt->Points[i]), FLinearColor(0.05f, 0.3f, 0.12f, 0.6f), Rw + 5.f);
				Seg(To(Rt->Points[i - 1]), To(Rt->Points[i]), FLinearColor(0.35f, 1.f, 0.55f, 0.95f), Rw + 1.5f);
			}
	}
	// ---- нүктелер ----
	TArray<FPt> Pts;
	auto Unlocked = [&](FVector2D Wp) { return S.Regions[KAP::RegionOf(Wp.X, Wp.Y)]; };
	D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName& Id, const FKAPLocationRow& L)
	{
		const FVector2D Wp(L.X, L.Y);
		if (!Unlocked(Wp)) return;
		FPt Pt; Pt.P = Wp; Pt.Title = L.Title; Pt.bLabel = false; Pt.Layer = LPlaces; Pt.Icon = -1;
		const FName T = L.Type;
		if (T == "village") { Pt.Icon = IVillage; Pt.Col = FLinearColor(0.85f, 0.55f, 0.12f); Pt.Info = Id == "aksai" ? TEXT("Ауыл · ақсақал Төлеген, жасақ · жанында полиция мен дүкен") : TEXT("Ауыл · ақсақал Сағат, жасақ, полиция патрулі"); Pt.bLabel = true; }
		else if (T == "police") { Pt.Icon = IPolice; Pt.Col = FLinearColor(0.12f, 0.32f, 0.85f); Pt.Info = TEXT("Капитан Әділет — бүйір жұмыстар, бандит сыйақысы"); Pt.Layer = LServices; Pt.bLabel = MapZoom > 2.5f; }
		else if (T == "shop") { Pt.Icon = IShop; Pt.Col = FLinearColor(0.15f, 0.6f, 0.3f); Pt.Info = TEXT("Сатушы Ерболат — қару, оқ-дәрі, дәрі, сынық сатып алады"); Pt.Layer = LServices; Pt.bLabel = MapZoom > 2.5f; }
		else if (T == "army") { Pt.Icon = IArmy; Pt.Col = FLinearColor(0.3f, 0.36f, 0.16f); Pt.Info = S.Flags.Contains("armyPass") ? TEXT("Әскери аймақ · рұқсат бар · қойма 20% арзан") : TEXT("Әскери аймақ · КІРУГЕ ТЫЙЫМ! Майор Сейітов қақпада жұмыс береді"); Pt.Layer = LArmy; Pt.bLabel = true; }
		else if (T == "wagon") { Pt.Icon = IWagon; Pt.Col = FLinearColor(0.2f, 0.62f, 0.35f); Pt.Info = TEXT("Қауіпсіз баспана · ұйқы, сақтау, жәшік"); Pt.Layer = LShelter; }
		else if (T == "tower") { const bool bOn = S.Towers.Contains(Id); Pt.Icon = ITower; Pt.Col = bOn ? FLinearColor(0.2f, 0.62f, 0.35f) : FLinearColor(0.45f, 0.45f, 0.45f); Pt.Info = bOn ? TEXT("Сигнал мұнарасы · қосулы") : TEXT("Сигнал мұнарасы · қосыңыз — жаңа аймақ ашылады"); Pt.Layer = LTowers; }
		else if (T == "hq") { Pt.Icon = IHQ; Pt.Col = FLinearColor(0.15f, 0.4f, 0.7f); Pt.Info = TEXT("QazUran кеніш басқармасы · кіру рұқсаты керек"); Pt.bLabel = true; }
		else if (T == "reservoir") { Pt.Icon = IWater; Pt.Col = FLinearColor(0.15f, 0.5f, 0.85f); Pt.Info = TEXT("Су қоймасы · сорғыны жөндеуге болады"); Pt.bLabel = MapZoom > 1.6f; }
		else if (T == "bandit") { Pt.Icon = IBandit; Pt.Col = FLinearColor(0.75f, 0.12f, 0.08f); Pt.Info = TEXT("Бандит лагері · қауіпті!"); Pt.Layer = LDanger; Pt.bLabel = true; }
		else if (T == "nest") { Pt.Icon = INest; Pt.Col = FLinearColor(0.55f, 0.05f, 0.05f); Pt.Info = TEXT("Мутант ұясы · түнде белсенді"); Pt.Layer = LDanger; }
		else if (T == "ruins") { Pt.Icon = IRuins; Pt.Col = FLinearColor(0.5f, 0.35f, 0.3f); Pt.Info = TEXT("Қираған ауыл · тірі қалғандар болуы мүмкін"); Pt.bLabel = true; }
		else if (T == "wreck") { Pt.Icon = IWreck; Pt.Col = FLinearColor(0.45f, 0.42f, 0.38f); Pt.Info = TEXT("Ескі көлік қаңқасы · бөлшектер"); }
		else if (T == "guardpost") { Pt.Icon = IGuard; Pt.Col = FLinearColor(0.2f, 0.3f, 0.5f); Pt.Info = TEXT("Кеніш күзеті · Бекзат — қару рұқсаты"); Pt.Layer = LServices; }
		else if (T == "missing") { if (GM->QuestState("s_missing") != EQuestState::Active || S.Flags.Contains("missingFound")) return; Pt.Icon = IJob; Pt.Col = MCYel; Pt.Info = TEXT("Жоғалған жұмысшы осы маңда"); Pt.Layer = LJobs; }
		if (Pt.Icon >= 0) Pts.Add(Pt);
	});
	for (int32 i = 0; i < WM->Wells.Num() && i < S.Wells.Num(); i++)
	{
		const FVector2D Wp = WM->Wells[i].PosM();
		if (!Unlocked(Wp)) continue;
		FPt Pt; Pt.P = Wp; Pt.Icon = IWell; Pt.Col = WellCol(S.Wells[i].State); Pt.Num = FString::FromInt(i + 1); Pt.Layer = LWells; Pt.bLabel = false;
		Pt.Title = FString::Printf(TEXT("№%d ұңғыма"), i + 1); Pt.Info = WellName(S.Wells[i].State);
		Pts.Add(Pt);
	}
	if (GM->Builder)
		for (const TWeakObjectPtr<AKAPVehicleHandle>& V : GM->Builder->Vehicles)
			if (V.IsValid() && V->Pawn)
			{
				const FVector2D Wp = KAP::CM2M(V->Pawn->GetActorLocation());
				if (!Unlocked(Wp)) continue;
				FPt Pt; Pt.P = Wp; Pt.Icon = ICar; Pt.Col = V->CanDrive() ? FLinearColor(0.25f, 0.3f, 0.35f) : FLinearColor(0.55f, 0.3f, 0.2f); Pt.Layer = LCars; Pt.bLabel = false;
				Pt.Title = V->Def.Title; Pt.Info = V->CanDrive() ? FString::Printf(TEXT("Жүруге дайын · жанармай %.0f л"), V->Fuel) : TEXT("Ақаулы — жөндеу керек");
				Pts.Add(Pt);
			}
	if (S.bBrigadesUnlocked)
		for (const FKAPBrigadeState& B : S.Brigades)
		{
			FPt Pt; Pt.P = B.Pos; Pt.Icon = IGuard; Pt.Col = FLinearColor(0.55f, 0.35f, 0.85f); Pt.Layer = LPlaces; Pt.bLabel = false;
			Pt.Title = FString::Printf(TEXT("CSP бригадасы %s"), *B.Id.ToString());
			Pt.Info = B.State == "repair" ? TEXT("Жөндеп жатыр") : B.State == "travel" ? TEXT("Жолда") : B.State == "return" ? TEXT("Базаға қайтуда") : TEXT("Бос — тапсырма беруге болады");
			Pts.Add(Pt);
		}
	for (const FKAPContract& C : S.Contracts)
	{
		if (C.State != 1 || !C.Points.Num()) continue;
		TArray<FVector2D> Ps;
		if (C.Type == "scrap") Ps = C.Points; else Ps.Add(C.Points[C.Type == "patrol" ? FMath::Clamp(C.Have, 0, C.Points.Num() - 1) : 0]);
		for (const FVector2D& Wp : Ps)
		{
			FPt Pt; Pt.P = Wp; Pt.Icon = IJob; Pt.Col = S.Tracked == C.Uid ? MCYel : FLinearColor(0.95f, 0.6f, 0.2f); Pt.Layer = LJobs; Pt.bLabel = false;
			Pt.Title = C.Title; Pt.Info = GM->ContractProgress(C) + FString::Printf(TEXT(" · %d ₸"), C.Reward);
			Pts.Add(Pt);
		}
	}
	// тінтуір астындағы нүкте
	const bool bMouseIn = Mouse.X > X && Mouse.X < X + W && Mouse.Y > Y && Mouse.Y < Y + H;
	const float Ir = FMath::Clamp(7.f + MapZoom * 1.3f, 8.f, 15.f);
	int32 Hover = INDEX_NONE; float Hd = 1e9f;
	for (int32 i = 0; i < Pts.Num(); i++)
	{
		if (!bL[Pts[i].Layer]) continue;
		const FVector2D Sp = To(Pts[i].P);
		if (!In(Sp)) continue;
		const float Dm = (Sp - Mouse).Size();
		if (bMouseIn && Dm < Ir + 4.f && Dm < Hd) { Hd = Dm; Hover = i; }
	}
	MapHoverTitle = Hover != INDEX_NONE ? Pts[Hover].Title : FString();
	MapHoverP = Hover != INDEX_NONE ? Pts[Hover].P : FVector2D::ZeroVector;
	// салу реті: көліктер → ұңғымалар → орындар → жұмыстар
	static const int32 Order[] = { LCars, LWells, LShelter, LTowers, LPlaces, LDanger, LServices, LArmy, LJobs };
	for (int32 Ly : Order)
	{
		if (!bL[Ly]) continue;
		for (int32 i = 0; i < Pts.Num(); i++)
		{
			const FPt& Pt = Pts[i];
			if (Pt.Layer != Ly) continue;
			const FVector2D Sp = To(Pt.P);
			if (!In(Sp, 4)) continue;
			const float R = Pt.Icon == ICar ? Ir * 0.8f : Pt.Icon == IWell ? Ir * 0.9f : Ir;
			MapIcon(Pt.Icon, Sp, R, Pt.Col, Pt.Num, i == Hover);
			if (Pt.bLabel && i != Hover) Txt(Sp.X + R + 6, Sp.Y - 9, Pt.Title, 14, FLinearColor(1, 0.97f, 0.9f), 0, true);
		}
	}
	// ---- жабық аймақтар (қараңғы) және аймақ атаулары ----
	for (int32 r = 0; r < 4; r++)
	{
		const FVector2D Q0((r == 1 || r == 2) ? 0.f : -1200.f, (r == 0 || r == 1) ? 0.f : -1200.f);
		const FVector2D A = To(Q0), B = To(Q0 + FVector2D(1200, 1200));
		const float Ax = FMath::Max(A.X, X), Ay = FMath::Max(A.Y, Y), Bx = FMath::Min(B.X, X + W), By = FMath::Min(B.Y, Y + H);
		if (Bx <= Ax || By <= Ay) continue;
		const FVector2D Cc = To(Q0 + FVector2D(600, 600));
		if (!S.Regions[r])
		{
			Rect(Ax, Ay, Bx - Ax, By - Ay, FLinearColor(0.03f, 0.03f, 0.035f, 0.78f));
			if (In(Cc)) { Txt(Cc.X, Cc.Y - 22, KAP::RegionName(r).ToString(), 22, MMuted, 1); Txt(Cc.X, Cc.Y + 8, r == 0 ? TEXT("ашылмаған · алдымен «Буханка» радиаторын жөндеңіз") : TEXT("ашылмаған · сигнал мұнарасын қосыңыз"), 14, MMuted, 1); }
		}
		else if (In(Cc) && MapZoom < 2.6f)
		{
			// «1-аймақ · Ақсай» → кіші «1-АЙМАҚ», астында үлкен «Ақсай» (GTA картасындағыдай аудан атауы)
			FString Num, Nm;
			if (!KAP::RegionName(r).ToString().Split(TEXT(" · "), &Num, &Nm)) Nm = KAP::RegionName(r).ToString();
			Txt(Cc.X, Cc.Y - 30, Num.ToUpper(), 14, FLinearColor(0.15f, 0.1f, 0.05f, 0.45f), 1, false);
			Txt(Cc.X, Cc.Y - 12, Nm.ToUpper(), 26, FLinearColor(0.15f, 0.1f, 0.05f, 0.4f), 1, false);
		}
	}
	// ---- белгі (нысана) және ойыншы ----
	if (Nav && Nav->bHasTarget)
	{
		const FVector2D Tp = To(Nav->Target);
		if (In(Tp))
		{
			FillTri(Tp + FVector2D(-9, -20), Tp + FVector2D(9, -20), Tp, FLinearColor(0.1f, 0.1f, 0.1f, 0.6f));
			Disc(Tp + FVector2D(0, -22), 11, FLinearColor::White); Disc(Tp + FVector2D(0, -22), 8.5f, FLinearColor(0.62f, 0.2f, 0.95f));
			FillTri(Tp + FVector2D(-6.5f, -17), Tp + FVector2D(6.5f, -17), Tp + FVector2D(0, -1), FLinearColor(0.62f, 0.2f, 0.95f));
			Disc(Tp + FVector2D(0, -22), 3, FLinearColor::White);
		}
	}
	{
		const FVector2D Sp = To(Pm);
		const APlayerController* PC = GetOwningPlayerController();
		const float Yaw = PC && PC->PlayerCameraManager ? FMath::DegreesToRadians(PC->PlayerCameraManager->GetCameraRotation().Yaw) : 0.f;
		const FVector2D F(FMath::Cos(Yaw), FMath::Sin(Yaw)), Rt(-F.Y, F.X);
		const float Pr = 11.f;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 4.f);
		Disc(Sp, Pr + 6 + Pulse * 4, FLinearColor(1, 1, 1, 0.12f));
		FillTri(Sp + F * (Pr + 3), Sp - F * Pr * 0.7f + Rt * (Pr + 2), Sp - F * Pr * 0.7f - Rt * (Pr + 2), FLinearColor(0.05f, 0.05f, 0.05f, 0.85f));
		FillTri(Sp + F * Pr, Sp - F * Pr * 0.55f + Rt * Pr * 0.85f, Sp - F * Pr * 0.25f, FLinearColor::White);
		FillTri(Sp + F * Pr, Sp - F * Pr * 0.25f, Sp - F * Pr * 0.55f - Rt * Pr * 0.85f, FLinearColor(0.85f, 0.85f, 0.85f));
	}
	// ---- кеңес терезесі ----
	if (Hover != INDEX_NONE)
	{
		const FPt& Pt = Pts[Hover];
		const FVector2D Sp = To(Pt.P);
		const FString Dist = FString::Printf(TEXT("≈ %d м · басу — бағыт салу"), FMath::RoundToInt32(FVector2D::Distance(Pm, Pt.P)));
		const float Tw = FMath::Max(FMath::Max(Measure(Pt.Title, 17).X, Measure(Pt.Info, 13).X), Measure(Dist, 12).X) + 28;
		float Tx = Sp.X + Ir + 10, Ty = Sp.Y - 36;
		if (Tx + Tw > X + W) Tx = Sp.X - Ir - 10 - Tw;
		Ty = FMath::Clamp(Ty, Y + 4, Y + H - 80);
		Rect(Tx, Ty, Tw, 72, FLinearColor(0.04f, 0.04f, 0.045f, 0.92f)); Rect(Tx, Ty, 4, 72, Pt.Col);
		Txt(Tx + 14, Ty + 7, Pt.Title, 17, MTxtc);
		Txt(Tx + 14, Ty + 32, Pt.Info, 13, FLinearColor(0.8f, 0.77f, 0.7f));
		Txt(Tx + 14, Ty + 51, Dist, 12, MMuted);
	}
	// ---- масштаб сызғышы, солтүстік ----
	{
		static const int32 Nice[] = { 25, 50, 100, 250, 500, 1000 };
		int32 L = 100; for (int32 n : Nice) { L = n; if (n * MapScale > 90.f) break; }
		const float Lp = L * MapScale, Bx = X + 18, By = Y + H - 24;
		Rect(Bx - 6, By - 22, Lp + 70, 34, FLinearColor(0, 0, 0, 0.45f));
		Rect(Bx, By, Lp, 4, FLinearColor::White); Rect(Bx, By - 6, 2, 10, FLinearColor::White); Rect(Bx + Lp - 2, By - 6, 2, 10, FLinearColor::White);
		Txt(Bx + Lp + 8, By - 9, L >= 1000 ? TEXT("1 км") : FString::Printf(TEXT("%d м"), L), 13, FLinearColor::White);
		const FVector2D Nc(X + W - 34, Y + 34);
		Disc(Nc, 20, FLinearColor(0, 0, 0, 0.45f));
		FillTri(Nc + FVector2D(0, -16), Nc + FVector2D(6, 2), Nc + FVector2D(-6, 2), MCRed);
		FillTri(Nc + FVector2D(0, 16), Nc + FVector2D(-6, -2), Nc + FVector2D(6, -2), FLinearColor(0.9f, 0.9f, 0.9f));
		Txt(Nc.X, Nc.Y - 40, TEXT("С"), 13, FLinearColor::White, 1);
	}
	Frame(X, Y, W, H, FLinearColor(1, 1, 1, 0.18f), 1.5f);
	if (bMouseIn && bFull)
	{
		const FVector2D Wm = MapCenter + (Mouse - Vc) / MapScale;
		Txt(X + W - 12, Y + H - 26, FString::Printf(TEXT("X %d · Y %d м"), FMath::RoundToInt32(Wm.X), FMath::RoundToInt32(Wm.Y)), 12, FLinearColor(1, 1, 1, 0.75f), 2);
	}
}

// ---------------------------------------------------------------------
// Толық экран карта (M)
// ---------------------------------------------------------------------
void AKAPHUD::DrawFullMap()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	if (!GM || !P) { bMap = false; return; }
	const float VW = Canvas->ClipX / Sc;
	Rect(0, 0, VW, 1080, FLinearColor(0.025f, 0.025f, 0.03f, 0.97f));
	const float PanelW = 360, X = 30, Y = 84, W = VW - PanelW - 70, H = 1080 - Y - 40;
	Txt(X, 22, TEXT("КАРТА"), 30, MAmber);
	Txt(X + 140, 32, TEXT("QazUran кен аймағы · 2,4 × 2,4 км"), 16, MMuted);
	Txt(VW - 30, 30, TEXT("Дөңгелек — масштаб · сүйреу — жылжыту · басу — белгі · ОТБ — белгіні алу · M — жабу"), 13, MMuted, 2);
	DrawWorldMap(X, Y, W, H, true);
	// ---- оң жақ: шартты белгілер (басып қосу/өшіру) ----
	const float Px = VW - PanelW - 30;
	float Py = Y;
	Rect(Px, Y, PanelW, H, FLinearColor(0.06f, 0.06f, 0.065f, 0.95f));
	Txt(Px + 20, Py + 14, TEXT("Шартты белгілер"), 19, MTxtc); Py += 52;
	struct FLeg { int32 Layer; int32 Icon; FLinearColor Col; const TCHAR* Name; };
	static const FLeg Legs[] = {
		{ LWells, IWell, MCGreen, TEXT("Ұңғымалар (түсі — күйі)") }, { LPlaces, IVillage, FLinearColor(0.85f, 0.55f, 0.12f), TEXT("Елді мекендер, басқарма") },
		{ LServices, IPolice, FLinearColor(0.12f, 0.32f, 0.85f), TEXT("Полиция, дүкен, күзет") }, { LArmy, IArmy, FLinearColor(0.3f, 0.36f, 0.16f), TEXT("Әскери аймақ") },
		{ LShelter, IWagon, FLinearColor(0.2f, 0.62f, 0.35f), TEXT("Баспаналар (вагон-үй)") }, { LTowers, ITower, FLinearColor(0.45f, 0.45f, 0.45f), TEXT("Сигнал мұнаралары") },
		{ LJobs, IJob, MCYel, TEXT("Белсенді жұмыстар") }, { LCars, ICar, FLinearColor(0.25f, 0.3f, 0.35f), TEXT("Көліктер") },
		{ LDanger, INest, FLinearColor(0.55f, 0.05f, 0.05f), TEXT("Қауіп: ұя, бандит, радиация") },
	};
	for (const FLeg& L : Legs)
	{
		const bool bOn = (MapLayers & (1 << L.Layer)) != 0;
		const bool bHov = FBox2D(FVector2D(Px + 10, Py - 6), FVector2D(Px + PanelW - 10, Py + 30)).IsInside(Mouse);
		if (bHov) Rect(Px + 10, Py - 6, PanelW - 20, 36, FLinearColor(1, 1, 1, 0.05f));
		UiA = bOn ? 1.f : 0.35f;
		MapIcon(L.Icon, FVector2D(Px + 34, Py + 12), 10, L.Col, TEXT("1"), false);
		Txt(Px + 58, Py + 2, L.Name, 15, MTxtc);
		UiA = 1.f;
		Txt(Px + PanelW - 22, Py + 2, bOn ? TEXT("✓") : TEXT("—"), 15, bOn ? MCGreen : MMuted, 2);
		const int32 Ly = L.Layer;
		Btns.Add({ FBox2D(FVector2D(Px + 10, Py - 6), FVector2D(Px + PanelW - 10, Py + 30)), [this, Ly]() { MapLayers ^= (1u << Ly); } });
		Py += 40;
	}
	Py += 10;
	Rect(Px + 20, Py, PanelW - 40, 1, FLinearColor(1, 1, 1, 0.12f)); Py += 16;
	// ұңғыма күйлерінің түстері
	Txt(Px + 20, Py, TEXT("Ұңғыма күйі"), 15, MTxtc); Py += 26;
	const TPair<FLinearColor, const TCHAR*> St[] = { { MCGreen, TEXT("жұмыс істейді") }, { MCYel, TEXT("ақау") }, { MCOrange, TEXT("ағу") }, { MCRed, TEXT("өрт") } };
	float Sx = Px + 20;
	for (const auto& Sv : St) { Disc(FVector2D(Sx + 7, Py + 9), 7, Sv.Key); Txt(Sx + 20, Py, Sv.Value, 13, MMuted); Sx += 20 + Measure(Sv.Value, 13).X + 16; }
	Py += 40;
	// белгі (нысана)
	UNavigatorComponent* Nav = P->Navigator;
	Txt(Px + 20, Py, TEXT("Белгі"), 15, MTxtc); Py += 26;
	if (Nav && Nav->bHasTarget)
	{
		Para(Px + 20, Py, PanelW - 40, FString::Printf(TEXT("%s · ≈ %d м"), *Nav->TargetName, FMath::RoundToInt32(FVector2D::Distance(KAP::CM2M(P->GetActorLocation()), Nav->Target))), 14, FLinearColor(0.75f, 0.55f, 1.f));
		Py += 30;
		Button(Px + 20, Py, 150, 32, TEXT("Белгіні алу"), [Nav]() { Nav->Clear(); }, true, false, 14);
	}
	else { Para(Px + 20, Py, PanelW - 40, TEXT("Картадағы кез келген нүктені немесе белгішені басыңыз — навигатор жол салады."), 13, MMuted); Py += 20; }
	Py += 56;
	Button(Px + 20, Py, 100, 34, TEXT("+"), [this]() { MapZoom = FMath::Min(9.f, MapZoom * 1.4f); }, true, false, 18);
	Button(Px + 128, Py, 100, 34, TEXT("−"), [this]() { MapZoom = FMath::Max(1.f, MapZoom / 1.4f); }, true, false, 18);
	Button(Px + 236, Py, PanelW - 256, 34, TEXT("Мен"), [this, P]() { MapCenter = KAP::CM2M(P->GetActorLocation()); MapZoom = FMath::Max(MapZoom, 2.2f); }, true, false, 15);
	Txt(Px + 20, Y + H - 34, FString::Printf(TEXT("Әмиян: %s"), *Money(GM->S().Money)), 15, MCGreen);
}
