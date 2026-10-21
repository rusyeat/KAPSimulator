#include "KAPHUD.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Player/NavigatorComponent.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/KAPVehicleHandle.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Audio/KAPAudio.h"
#include "KAP/Core/KAPRegistry.h"
#include "Engine/Canvas.h"
#include "KAP/Core/KAPRegistry.h"
#include "CanvasItem.h"
#include "KAP/Core/KAPRegistry.h"
#include "Engine/Texture2D.h"
#include "KAP/Core/KAPRegistry.h"
#include "Engine/Font.h"
#include "KAP/Core/KAPRegistry.h"
#include "TextureResource.h"
#include "KAP/Core/KAPRegistry.h"
#include "Styling/CoreStyle.h"
#include "KAP/Core/KAPRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "KAP/Core/KAPRegistry.h"
#include "Fonts/FontMeasure.h"
#include "KAP/Core/KAPRegistry.h"
#include "EngineUtils.h"
#include "KAP/Core/KAPRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "KAP/Core/KAPRegistry.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Player/KAPSurvivalAttributeSet.h"
#include "KAP/Core/KAPRegistry.h"
#include "Camera/PlayerCameraManager.h"
#include "KAP/Core/KAPRegistry.h"

static const FLinearColor Amber(0.95f, 0.76f, 0.3f), Panel(0.07f, 0.06f, 0.045f, 0.8f), Muted(0.55f, 0.51f, 0.44f), Txtc(0.94f, 0.9f, 0.82f);
static const FLinearColor CGreen(0.37f, 0.83f, 0.54f), CRed(1.f, 0.35f, 0.29f), CYel(1.f, 0.82f, 0.29f), COrange(1.f, 0.54f, 0.23f), CBlue(0.44f, 0.71f, 1.f);
static FLinearColor StateCol(EWellState S) { return S == EWellState::Ok ? CGreen : S == EWellState::Fault ? CYel : S == EWellState::Leak ? COrange : S == EWellState::Fire ? CRed : FLinearColor(0.4f, 0.4f, 0.4f); }

static FSlateFontInfo KFont(float PixelSize)
{
	static TWeakObjectPtr<UFont> F;
	if (!F.IsValid()) F = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	return FSlateFontInfo(F.Get(), FMath::Max(6, FMath::RoundToInt32(PixelSize)), FName("Regular"));
}
AKAPHUD* AKAPHUD::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return PC ? Cast<AKAPHUD>(PC->GetHUD()) : nullptr;
}

// ---------------- көмекшілер (виртуал 1920×1080) ----------------
void AKAPHUD::Rect(float X, float Y, float W, float H, FLinearColor C)
{
	C.A *= UiA;
	if (C.A <= 0.003f) return;
	FCanvasTileItem T(FVector2D(X * Sc, Y * Sc), FVector2D(W * Sc, H * Sc), C);
	T.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(T);
}
void AKAPHUD::Frame(float X, float Y, float W, float H, FLinearColor C, float T)
{
	Rect(X, Y, W, T, C); Rect(X, Y + H - T, W, T, C); Rect(X, Y, T, H, C); Rect(X + W - T, Y, T, H, C);
}
void AKAPHUD::Line(FVector2D A, FVector2D B, FLinearColor C, float T)
{
	C.A *= UiA;
	if (C.A <= 0.003f) return;
	FCanvasLineItem L(A * Sc, B * Sc);
	L.SetColor(C); L.LineThickness = T * Sc;
	Canvas->DrawItem(L);
}
void AKAPHUD::Circle(FVector2D C, float R, FLinearColor Col, float T, int32 Seg)
{
	for (int32 i = 0; i < Seg; i++)
	{
		const float A0 = i * UE_TWO_PI / Seg, A1 = (i + 1) * UE_TWO_PI / Seg;
		Line(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, Col, T);
	}
}
void AKAPHUD::Disc(FVector2D C, float R, FLinearColor Col)
{
	Col.A *= UiA;
	if (Col.A <= 0.003f) return;
	TArray<FCanvasUVTri> Tris;
	const int32 Seg = 16;
	for (int32 i = 0; i < Seg; i++)
	{
		FCanvasUVTri T;
		T.V0_Pos = C * Sc; T.V1_Pos = (C + FVector2D(FMath::Cos(i * UE_TWO_PI / Seg), FMath::Sin(i * UE_TWO_PI / Seg)) * R) * Sc; T.V2_Pos = (C + FVector2D(FMath::Cos((i + 1) * UE_TWO_PI / Seg), FMath::Sin((i + 1) * UE_TWO_PI / Seg)) * R) * Sc;
		T.V0_Color = T.V1_Color = T.V2_Color = Col;
		Tris.Add(T);
	}
	FCanvasTriangleItem TI(Tris, GWhiteTexture);
	TI.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(TI);
}
FVector2D AKAPHUD::Measure(const FString& S, float Size) const
{
	const FSlateFontInfo F = KFont(Size * Sc);
	if (!FSlateApplication::IsInitialized()) return FVector2D(S.Len() * Size * 0.55f, Size * 1.3f);
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F) / Sc;
}
float AKAPHUD::Txt(float X, float Y, const FString& S, float Size, FLinearColor C, int32 Align, bool bShadow)
{
	const FSlateFontInfo F = KFont(Size * Sc);
	const FVector2D M = Measure(S, Size);
	C.A *= UiA;
	if (C.A <= 0.003f) return M.X;
	const float Ox = Align == 1 ? -M.X / 2 : Align == 2 ? -M.X : 0;
	FCanvasTextItem T(FVector2D((X + Ox) * Sc, Y * Sc), FText::FromString(S), F, C);
	if (bShadow) { T.EnableShadow(FLinearColor(0, 0, 0, 0.8f * UiA), FVector2D(1, 1)); }
	Canvas->DrawItem(T);
	return M.X;
}
float AKAPHUD::Para(float X, float Y, float W, const FString& S, float Size, FLinearColor C)
{
	TArray<FString> Paras; S.ParseIntoArray(Paras, TEXT("\n"), false);
	float Yy = Y;
	for (const FString& P : Paras)
	{
		TArray<FString> Words; P.ParseIntoArray(Words, TEXT(" "));
		FString Cur;
		for (const FString& Wd : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
			if (Measure(Try, Size).X > W && !Cur.IsEmpty()) { Txt(X, Yy, Cur, Size, C); Yy += Size * 1.45f; Cur = Wd; }
			else Cur = Try;
		}
		Txt(X, Yy, Cur, Size, C); Yy += Size * 1.45f;
	}
	return Yy - Y;
}
bool AKAPHUD::Button(float X, float Y, float W, float H, const FString& Label, TFunction<void()> Fn, bool bEnabled, bool bPrimary, float Size)
{
	const bool bHover = bEnabled && FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)).IsInside(Mouse);
	Rect(X, Y, W, H, bPrimary ? FLinearColor(0.42f, 0.32f, 0.12f, 0.95f) : bHover ? FLinearColor(0.24f, 0.3f, 0.33f, 0.95f) : FLinearColor(0.16f, 0.19f, 0.21f, 0.95f));
	Frame(X, Y, W, H, bHover ? Amber : FLinearColor(0.3f, 0.34f, 0.36f), 1.f);
	Txt(X + W / 2, Y + (H - Size * 1.3f) / 2, Label, Size, bEnabled ? Txtc : Muted, 1, false);
	if (bEnabled && Fn) Btns.Add({ FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)), Fn });
	return bHover;
}

// ---------------- API ----------------
void AKAPHUD::AddToast(const FString& Msg, FName Kind) { Toasts.Insert({ Msg, Kind, 7.f }, 0); if (Toasts.Num() > 6) Toasts.SetNum(6); }
void AKAPHUD::SetSubtitle(const FString& Kk, const FString& Ru, const FString& Who, float Dur) { SubKk = Kk; SubRu = Ru; SubWho = Who; SubT = Dur; }
void AKAPHUD::ShowDialog(const FString& Name, const FString& Text, TArray<FKAPDialogOption> Opts) { bDialog = true; DName = Name; DText = Text; DOpts = MoveTemp(Opts); }
void AKAPHUD::ToggleTablet() { bTablet = !bTablet; TabletScroll = 0; }
void AKAPHUD::CloseAll() { bTablet = false; bDialog = false; bShop = false; bJobs = false; if (MG.bActive) MGFinish(false); }
bool AKAPHUD::IsModal() const
{
	const AKAPGameMode* GM = AKAPGameMode::Get(this);
	return bTablet || bDialog || bShop || bJobs || MG.bActive || (GM && (GM->bInMenu || GM->bPaused || GM->bPlayerDead || GM->bCredits));
}

void AKAPHUD::Click(FVector2D Pos, bool bDown)
{
	bMouseDown = bDown;
	if (!bDown) { if (MG.bActive && MG.Kind == TEXT("wires") && MG.Drag >= 0) MG.T = -1; return; } // жіптерді жіберу
	const FVector2D P = Pos / Sc;
	for (int32 i = Btns.Num() - 1; i >= 0; i--) if (Btns[i].R.IsInside(P)) { TFunction<void()> F = Btns[i].Fn; UKAPAudio* A = UKAPAudio::Get(this); if (A) A->Play(EKAPSound::Beep, 0.15f); F(); return; }
}

bool AKAPHUD::HandleKey(FName K, bool bDown)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GM) return false;
	if (K == "Jump") bSpaceDown = bDown;
	if (K == "Status") { bStatusHeld = bDown; return false; }
	if (!bDown) return MG.bActive;
	if (GM->bCredits) { if (K == "Pause" || K == "Enter") UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this))); return true; }
	if (GM->bInMenu) { if (K == "Enter") GM->MenuNewGame(); return true; }
	if (MG.bActive) { if (K == "Pause") MGFinish(false); return true; }
	if (bShop || bJobs) { if (K == "Pause" || K == "Tablet") { bShop = false; bJobs = false; } return true; }
	if (bDialog)
	{
		if (K == "Pause") { bDialog = false; return true; }
		if (K.ToString().StartsWith(TEXT("S")) && K.ToString().Len() == 2)
		{
			const int32 I = FCString::Atoi(*K.ToString().RightChop(1)) - 1;
			if (DOpts.IsValidIndex(I) && !DOpts[I].bDisabled) { TFunction<void()> F = DOpts[I].Fn; bDialog = false; if (F) F(); }
		}
		return true;
	}
	if (bTablet) { if (K == "Tablet" || K == "Pause") bTablet = false; return true; }
	if (GM->bPlayerDead) { if (K == "Enter") GM->RespawnPlayer(); return true; }
	if (GM->bPaused) { if (K == "Pause") GM->bPaused = false; return true; }
	return false;
}

// ---------------- негізгі сурет ----------------
void AKAPHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	const double Now = FPlatformTime::Seconds();
	const float Dt = LastTime > 0 ? FMath::Min(0.1f, (float)(Now - LastTime)) : 0.016f;
	LastTime = Now;
	Sc = Canvas->ClipY / 1080.f;
	Btns.Reset();
	UiA = 1.f;
	float Mx, My;
	if (GetOwningPlayerController() && GetOwningPlayerController()->GetMousePosition(Mx, My)) Mouse = FVector2D(Mx, My) / Sc;
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GM) return;
	if (GM->bCredits) { DrawCredits(); return; }
	if (GM->bInMenu) { DrawMenu(); DrawToasts(Dt); return; }
	if (GM->bCinematic)
	{
		const float VW = Canvas->ClipX / Sc;
		Rect(0, 0, VW, 118, FLinearColor::Black); Rect(0, 962, VW, 118, FLinearColor::Black);
		Txt(VW - 30, 1030, TEXT("Enter — өткізу"), 13, Muted, 2);
		DrawSubtitle(Dt);
		DrawToasts(Dt);
	}
	else
	{
		DrawGameplay(Dt);
		DrawToasts(Dt);
		DrawSubtitle(Dt);
		if (bTablet) DrawTablet();
		if (bDialog) DrawDialog();
		if (bShop) DrawShop();
		if (bJobs) DrawJobs();
		if (MG.bActive) DrawMinigame(Dt);
		if (GM->bPaused) DrawPause();
		if (GM->bPlayerDead) DrawDeath();
	}
	if (Fade > 0) { Rect(0, 0, Canvas->ClipX / Sc, 1080, FLinearColor(0, 0, 0, Fade)); Fade = FMath::Max(0.f, Fade - Dt * 0.7f); }
}

void AKAPHUD::DrawToasts(float Dt)
{
	const float VW = Canvas->ClipX / Sc;
	float Y = 110;
	for (int32 i = Toasts.Num() - 1; i >= 0; i--) { Toasts[i].T -= Dt; if (Toasts[i].T <= 0) Toasts.RemoveAt(i); }
	for (const FToast& T : Toasts)
	{
		const FLinearColor C = T.Kind == "danger" ? CRed : T.Kind == "warn" ? COrange : T.Kind == "ok" ? CGreen : T.Kind == "info" ? CBlue : Amber;
		const float W = 420;
		TArray<FString> Dummy;
		const float H = 14 + FMath::CeilToFloat(Measure(T.Msg, 14).X / (W - 24)) * 20;
		const float A = FMath::Clamp(T.T, 0.f, 1.f);
		Rect(VW - W - 20, Y, W, H, T.Kind == "danger" ? FLinearColor(0.24f, 0.04f, 0.03f, 0.85f * A) : FLinearColor(Panel.R, Panel.G, Panel.B, 0.85f * A));
		Rect(VW - W - 20, Y, 4, H, C);
		Para(VW - W - 8, Y + 6, W - 24, T.Msg, 14, Txtc * FLinearColor(1, 1, 1, A));
		Y += H + 6;
	}
}

void AKAPHUD::DrawSubtitle(float Dt)
{
	if (SubT <= 0) return;
	SubT -= Dt;
	const float VW = Canvas->ClipX / Sc;
	const FString Line = SubWho.IsEmpty() ? SubKk : SubWho + TEXT(": ") + SubKk;
	const float W = FMath::Min(VW - 200, Measure(Line, 24).X + 40);
	TArray<FString> L;
	float Y = 900;
	Rect(VW / 2 - W / 2 - 10, Y - 8, W + 20, SubRu.IsEmpty() ? 50 : 82, FLinearColor(0, 0, 0, 0.45f));
	Txt(VW / 2, Y, Line, 24, Txtc, 1);
	if (!SubRu.IsEmpty()) Txt(VW / 2, Y + 36, SubRu, 18, FLinearColor(0.85f, 0.81f, 0.73f), 1);
}

// ---------------- HUD ----------------
void AKAPHUD::DrawGameplay(float Dt)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	const FKAPSession& S = GM->S();
	const AWeatherManager* WX = GM->Weather;
	if (!P || !WX) return;
	const float VW = Canvas->ClipX / Sc;
	const FKAPEnv& E = WX->Env;
	// ---- қай панель қашан көрінеді: оқиғадан кейін бірнеше секунд, қауіпті мәнде тұрақты, Q ұстағанда бәрі ----
	GameT += Dt;
	const bool bAll = bStatusHeld;
	{
		const FString Obj = GM->ObjectiveText();
		if (Obj != LastObj) { LastObj = Obj; ObjT = 8.f; }
		if ((int32)E.Cur != LastWeather || E.bNight != bLastNight) { if (LastWeather >= 0) ClockT = 6.f; LastWeather = (int32)E.Cur; bLastNight = E.bNight; }
		if (GM->TimeScale > 1) ClockT = FMath::Max(ClockT, 0.5f);
		const bool bStorm = E.Storm > 0.5f;
		if (bStorm != bLastStorm || P->bIndoor != bLastIndoor) { BarsT = FMath::Max(BarsT, 4.f); bLastStorm = bStorm; bLastIndoor = P->bIndoor; }
		if (LastHp >= 0.f && P->GetHp() < LastHp - 0.5f) BarsT = 5.f;
		LastHp = P->GetHp();
		if (S.Equipped != LastEquip || S.Weapons.Num() != LastWeapons) { if (LastWeapons >= 0) { HotT = 2.5f; AmmoT = 3.f; } LastEquip = S.Equipped; LastWeapons = S.Weapons.Num(); }
		const int32 Mg = S.Mag.FindRef(S.Equipped);
		if (Mg != LastMag) { if (LastMag >= 0) AmmoT = 3.f; LastMag = Mg; }
		if (GM->CurrentVehicle.Get() != LastVeh.Get()) { LastVeh = GM->CurrentVehicle.Get(); if (LastVeh.IsValid()) VehT = 7.f; }
		ObjT -= Dt; ClockT -= Dt; BarsT -= Dt; HotT -= Dt; AmmoT -= Dt; VehT -= Dt;
	}
	const bool bDoseOn = S.Inv.FindRef("dosimeter") > 0 && P->DoseRate > 0.05f;
	const bool bBarsNeed = P->GetHp() < 50.f || P->GetWater() < 35.f || P->GetStamina() < 97.f || P->GetTemp() > 38.5f || P->GetTemp() < 35.2f || bDoseOn;
	FadeTo(AObj, bAll || ObjT > 0.f, Dt);
	FadeTo(AClock, bAll || ClockT > 0.f, Dt);
	FadeTo(ACompass, bAll || (P->Navigator && P->Navigator->bHasTarget), Dt);
	FadeTo(ABars, bAll || BarsT > 0.f || bBarsNeed, Dt);
	FadeTo(AHot, bAll || HotT > 0.f, Dt);
	FadeTo(AAmmo, bAll || AmmoT > 0.f || P->ReloadT > 0.f, Dt);
	FadeTo(AMap, bAll, Dt);
	// экран эффектілері
	if (P->HurtFlash > 0 || P->GetHp() < 25)
	{
		const float A = FMath::Max(P->HurtFlash, P->GetHp() < 25 ? 0.3f + 0.12f * FMath::Sin(GetWorld()->GetTimeSeconds() * 3) : 0.f);
		Rect(0, 0, VW, 60, FLinearColor(0.6f, 0, 0, A * 0.5f)); Rect(0, 1020, VW, 60, FLinearColor(0.6f, 0, 0, A * 0.5f));
		Rect(0, 0, 80, 1080, FLinearColor(0.6f, 0, 0, A * 0.4f)); Rect(VW - 80, 0, 80, 1080, FLinearColor(0.6f, 0, 0, A * 0.4f));
	}
	if (P->GetWater() < 25) Rect(0, 0, VW, 1080, FLinearColor(0.5f, 0.45f, 0.35f, (25 - P->GetWater()) / 100.f));
	if (P->GetTemp() > 38.8f) Rect(0, 0, VW, 1080, FLinearColor(1, 0.6f, 0.3f, FMath::Min(0.25f, (P->GetTemp() - 38.8f) * 0.15f)));
	if (P->GetTemp() < 35.f) Rect(0, 0, VW, 1080, FLinearColor(0.5f, 0.6f, 1.f, 0.12f));
	// тапсырма (жаңа тапсырмада 8 с)
	UiA = AObj;
	if (UiA > 0.01f)
	{
		Rect(20, 20, 470, 62, Panel); Rect(20, 20, 4, 62, Amber);
		Txt(34, 26, TEXT("ТАПСЫРМА"), 12, Amber);
		Txt(34, 46, LastObj, 17, Txtc);
	}
	// сағат пен ауа райы (ауа райы не тәулік ауысқанда)
	UiA = AClock;
	if (UiA > 0.01f)
	{
		const FString Clock = KAP::FmtTime(S.Time) + FString::Printf(TEXT(" · %d-күн"), FMath::FloorToInt32(S.Time / 24) + 1);
		Rect(VW - 300, 20, 280, 82, Panel); Rect(VW - 24, 20, 4, 82, Amber);
		Txt(VW - 34, 26, Clock, 18, Txtc, 2);
		Txt(VW - 34, 52, KAP::WeatherName(E.Cur).ToString() + (E.bNight ? TEXT(" · түн") : TEXT("")) + (GM->TimeScale > 1 ? FString::Printf(TEXT(" · ×%d"), (int32)GM->TimeScale) : TEXT("")), 13, Muted, 2);
		Txt(VW - 34, 72, FString::Printf(TEXT("Ауа %d°C · көріну %d м"), FMath::RoundToInt32(E.Temp), FMath::RoundToInt32(E.Visibility)), 13, Muted, 2);
	}
	// компас (тек навигатор маршруты бар кезде)
	UiA = ACompass * 0.85f;
	if (UiA > 0.01f)
	{
		const float Cx = VW / 2, W = 520;
		Rect(Cx - W / 2, 18, W, 34, Panel);
		const APlayerController* PC = GetOwningPlayerController();
		const float Yaw = PC ? PC->PlayerCameraManager->GetCameraRotation().Yaw : 0.f; // 0 = +X (шығыс), солтүстік = −Y
		const float Heading = FMath::Fmod(Yaw + 90.f + 3600.f, 360.f);
		static const TCHAR* Dir[8] = { TEXT("С"), TEXT("СШ"), TEXT("Ш"), TEXT("ОШ"), TEXT("О"), TEXT("ОБ"), TEXT("Б"), TEXT("СБ") };
		for (int32 D = 0; D < 360; D += 15)
		{
			float Diff = FMath::Fmod(D - Heading + 540.f, 360.f) - 180.f;
			if (FMath::Abs(Diff) > 80) continue;
			const float X = Cx + Diff * 3.f;
			Txt(X, 25, D % 45 == 0 ? Dir[D / 45] : FString::FromInt(D), D % 45 == 0 ? 15 : 11, D % 45 == 0 ? Amber : Muted, 1, false);
		}
		Rect(Cx - 1, 18, 2, 34, Amber);
		if (P->Navigator->bHasTarget && P->Navigator->bHasNext)
		{
			const FVector2D Pm = KAP::CM2M(P->GetActorLocation()), Nx = P->Navigator->Next;
			const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(Nx.X - Pm.X, -(Nx.Y - Pm.Y)));
			const float Diff = FMath::Fmod(Bearing - Heading + 540.f, 360.f) - 180.f;
			if (FMath::Abs(Diff) < 80) { const float X = Cx + Diff * 3.f; Disc(FVector2D(X, 22), 7, FLinearColor(0.3f, 0.88f, 0.54f)); }
			Txt(Cx + W / 2 - 8, 25, FString::Printf(TEXT("%d м"), FMath::RoundToInt32(FVector2D::Distance(Pm, P->Navigator->Target))), 13, FLinearColor(0.3f, 0.88f, 0.54f), 2, false);
		}
	}
	// көрсеткіштер (жарақат, шөл, шаршау, ыстық/суық, сәуле кезінде)
	UiA = ABars;
	if (UiA > 0.01f)
	{
		const float X = 20, Y = 900, W = 300;
		Rect(X, Y, W, 160, Panel);
		auto Bar = [&](float Yy, const TCHAR* L, float V, float Max, FLinearColor C)
		{
			Txt(X + 12, Yy - 3, L, 13, Muted);
			Rect(X + 60, Yy, W - 76, 12, FLinearColor(1, 1, 1, 0.08f));
			if (Max < 100) Rect(X + 60 + (W - 76) * Max / 100.f, Yy, (W - 76) * (1 - Max / 100.f), 12, FLinearColor(0.3f, 0.07f, 0.07f, 0.9f));
			Rect(X + 60, Yy, (W - 76) * FMath::Clamp(V / 100.f, 0.f, 1.f), 12, C);
		};
		Bar(Y + 14, TEXT("HP"), P->GetHp(), P->GetMaxHp(), FLinearColor(0.84f, 0.27f, 0.27f));
		Bar(Y + 40, TEXT("Су"), P->GetWater(), 100, FLinearColor(0.24f, 0.56f, 0.84f));
		Bar(Y + 66, TEXT("Күш"), P->GetStamina(), 100, FLinearColor(0.88f, 0.7f, 0.24f));
		const FLinearColor TC = P->GetTemp() > 38.5f ? COrange : P->GetTemp() < 35.2f ? CBlue : Txtc;
		Txt(X + 12, Y + 92, TEXT("Дене"), 13, Muted); Txt(X + 60, Y + 90, FString::Printf(TEXT("%.1f°C"), P->GetTemp()), 15, TC);
		const bool bDos = S.Inv.FindRef("dosimeter") > 0;
		Txt(X + 150, Y + 92, TEXT("Доза"), 13, Muted);
		Txt(X + 196, Y + 90, bDos ? FString::Printf(TEXT("%d мЗв%s"), FMath::RoundToInt32(P->GetDose()), P->DoseRate > 0.05f ? TEXT(" ^") : TEXT("")) : TEXT("? (дозиметр)"), 15, P->DoseRate > 0.05f ? FLinearColor(0.7f, 1.f, 0.23f) : Txtc);
		Txt(X + 12, Y + 120, P->bIndoor ? TEXT("Баспанада") : WX->Env.Storm > 0.5f ? TEXT("! Құм дауылы") : TEXT(""), 13, Amber);
	}
	// қолдағы құрал/қару (ауыстырғанда 2.5 с), оқ саны (атқанда/оқтағанда)
	{
		const float N = S.Weapons.Num(), W = 76, X0 = VW / 2 - N * (W + 4) / 2, Y = 1010;
		UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
		UiA = AHot;
		for (int32 i = 0; UiA > 0.01f && i < S.Weapons.Num(); i++)
		{
			const bool bSel = S.Weapons[i] == S.Equipped;
			const FKAPWeaponRow* Wr = D->Weapon(S.Weapons[i]);
			Rect(X0 + i * (W + 4), Y, W, 56, bSel ? FLinearColor(0.24f, 0.19f, 0.08f, 0.9f) : Panel);
			Frame(X0 + i * (W + 4), Y, W, 56, bSel ? Amber : FLinearColor(1, 1, 1, 0.12f), 1);
			Txt(X0 + i * (W + 4) + W - 6, Y + 3, FString::FromInt(i + 1), 11, Amber, 2);
			Para(X0 + i * (W + 4) + 5, Y + 18, W - 10, Wr ? Wr->Title : S.Weapons[i].ToString(), 11, bSel ? FLinearColor::White : Muted);
			if (Wr && Wr->Permit && !S.bPermitWeapon) Txt(X0 + i * (W + 4) + W - 6, Y + 38, TEXT("x"), 11, CRed, 2);
		}
		const FKAPWeaponRow* W2 = D->Weapon(S.Equipped);
		UiA = AAmmo;
		if (W2 && W2->Kind == "gun") Txt(VW - 280, 1020, FString::Printf(TEXT("%d / %d"), S.Mag.FindRef(S.Equipped), S.Ammo.FindRef(W2->Ammo)), 26, FLinearColor::White, 2);
		else if (W2 && W2->Kind == "throw") Txt(VW - 280, 1020, FString::Printf(TEXT("× %d"), S.Ammo.FindRef(W2->Ammo)), 26, FLinearColor::White, 2);
		UiA = 1.f;
		if (P->ReloadT > 0) Txt(VW / 2, 600, TEXT("Оқталуда..."), 16, Amber, 1);
	}
	UiA = 1.f;
	// нысана: кішкентай нүкте (қару қолда болса — сәл анығырақ)
	if (P->bScoped) { Circle(FVector2D(VW / 2, 540), 300, FLinearColor::Black, 6, 48); Line(FVector2D(VW / 2 - 300, 540), FVector2D(VW / 2 + 300, 540), FLinearColor::Black, 1); Line(FVector2D(VW / 2, 240), FVector2D(VW / 2, 840), FLinearColor::Black, 1); }
	else if (!GM->CurrentVehicle.IsValid())
	{
		const FKAPWeaponRow* Wc = UKAPDataSubsystem::Get(this)->Weapon(S.Equipped);
		Disc(FVector2D(VW / 2, 540), 2.2f, FLinearColor(1, 1, 1, Wc && Wc->Kind == "gun" ? 0.7f : 0.4f));
	}
	// әрекет мәтіні
	if (!P->InteractText.IsEmpty() && !GM->CurrentVehicle.IsValid())
	{
		const FString L = TEXT("E — ") + P->InteractText;
		const float W = Measure(L, 17).X + 30;
		Rect(VW / 2 - W / 2, 620, W, 34, FLinearColor(0, 0, 0, 0.55f));
		Txt(VW / 2, 625, L, 17, Txtc, 1);
	}
	// көлік
	if (AKAPVehicleHandle* V = GM->CurrentVehicle.Get())
	{
		// отырғанда 7 с, кейін тек ақауда (жанармай аз, двигатель тоқтады, дөңгелек жарылды)
		FadeTo(AVeh, bAll || VehT > 0.f || V->Fuel < 5 || V->State == "broken" || V->Flats > 0, Dt);
		UiA = AVeh;
	}
	if (AKAPVehicleHandle* V = GM->CurrentVehicle.Get(); V && UiA > 0.01f)
	{
		Rect(VW - 520, 860, 260, 130, Panel);
		Txt(VW - 508, 868, V->Def.Title, 16, Amber);
		Txt(VW - 508, 892, FString::Printf(TEXT("Жылдамдық: %d км/сағ"), FMath::RoundToInt32(V->SpeedKmh())), 14, Txtc);
		Txt(VW - 508, 912, FString::Printf(TEXT("Жанармай: %.1f / %d л"), V->Fuel, (int32)V->Def.FuelCap), 14, V->Fuel < 5 ? CRed : Txtc);
		Txt(VW - 508, 932, FString::Printf(TEXT("Двигатель: %d%%"), FMath::RoundToInt32(V->Hp)), 14, Txtc);
		if (V->Flats) Txt(VW - 508, 952, FString::Printf(TEXT("Жарылған дөңгелек: %d"), V->Flats), 14, COrange);
		if (V->State == "broken") Txt(VW - 508, 968, TEXT("ДВИГАТЕЛЬ ТОҚТАДЫ"), 14, CRed);
		if (VehT > 0.f || bAll) Txt(VW / 2, 1040, TEXT("W/S — газ/тежеу · A/D — руль · Space — қол тежегіш · V — камера · E — түсу"), 13, Muted, 1);
	}
	// миникарта — тек Q ұстағанда (картаның өзі планшетте)
	UiA = AMap;
	if (UiA > 0.01f) DrawMinimap(VW - 130, 960, 110);
	UiA = 1.f;
	DrawMoney(Dt);
	UiA = 1.f;
}

void AKAPHUD::DrawMinimap(float Cx, float Cy, float R)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	const FKAPSession& S = GM->S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	Disc(FVector2D(Cx, Cy), R, FLinearColor(0.27f, 0.21f, 0.13f, 0.78f));
	Circle(FVector2D(Cx, Cy), R, FLinearColor(Amber.R, Amber.G, Amber.B, 0.4f), 2, 40);
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	const float Scale = GM->bHeatActive ? 0.45f : 0.32f; // px/м
	const APlayerController* PC = GetOwningPlayerController();
	const float Yaw = PC ? FMath::DegreesToRadians(PC->PlayerCameraManager->GetCameraRotation().Yaw) : 0;
	// карта ойыншы бағытымен айналады: көру бағыты жоғары
	auto To = [&](FVector2D W) -> TOptional<FVector2D>
	{
		const FVector2D Dv = (W - Pm) * Scale;
		const float A = -Yaw - UE_HALF_PI;
		const FVector2D Rr(Dv.X * FMath::Cos(A) - Dv.Y * FMath::Sin(A), Dv.X * FMath::Sin(A) + Dv.Y * FMath::Cos(A));
		if (Rr.Size() > R - 3) return TOptional<FVector2D>();
		return FVector2D(Cx, Cy) + Rr;
	};
	if (GM->bHeatActive)
	{
		float Mx = 0; for (float V : GM->Heat) Mx = FMath::Max(Mx, V);
		if (Mx > 0)
			for (int32 J = 0; J < 64; J++) for (int32 I = 0; I < 64; I++)
			{
				const float V = GM->Heat[J * 64 + I] / Mx;
				if (V < 0.05f) continue;
				const FVector2D C(-1200 + (I + 0.5) * 37.5, -1200 + (J + 0.5) * 37.5);
				if (TOptional<FVector2D> Sp = To(C)) Rect(Sp->X - 8, Sp->Y - 8, 16, 16, FLinearColor(1, 0.78f - V * 0.78f, 0, 0.15f + V * 0.5f));
			}
		Txt(Cx, Cy + R - 22, TEXT("HMM қауіп картасы"), 10, COrange, 1);
	}
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
	{
		TOptional<FVector2D> A = To(D->Graph.Nodes[E.A].Pos), B = To(D->Graph.Nodes[E.B].Pos);
		if (A && B) Line(*A, *B, FLinearColor(0.63f, 0.51f, 0.35f, 0.9f), 2);
	}
	if (const KAPAI::FRoute* Rt = P->Navigator->Active())
		for (int32 i = 1; i < Rt->Points.Num(); i++) { TOptional<FVector2D> A = To(Rt->Points[i - 1]), B = To(Rt->Points[i]); if (A && B) Line(*A, *B, FLinearColor(0.3f, 0.88f, 0.54f), 3); }
	for (int32 i = 0; i < WM->Wells.Num() && i < S.Wells.Num(); i++) if (TOptional<FVector2D> A = To(WM->Wells[i].PosM())) Disc(*A, 4, StateCol(S.Wells[i].State));
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
	{
		if (It->bDead) continue;
		const FVector2D Nm = KAP::CM2M(It->GetActorLocation());
		const float Dd = FVector2D::Distance(Nm, Pm);
		if (Dd > 220) continue;
		if (It->Faction == EKAPFaction::Monster && (GM->Weather->Env.Storm > 0.4f || Dd > GM->Weather->Env.Visibility)) continue;
		if (TOptional<FVector2D> A = To(Nm)) Rect(A->X - 2, A->Y - 2, 4, 4, It->IsHostileToPlayer() ? CRed : (It->Faction == EKAPFaction::Militia || It->Faction == EKAPFaction::Guard) ? CBlue : Txtc);
	}
	Line(FVector2D(Cx, Cy - 8), FVector2D(Cx - 5, Cy + 6), FLinearColor::White, 2); Line(FVector2D(Cx, Cy - 8), FVector2D(Cx + 5, Cy + 6), FLinearColor::White, 2);
	// солтүстік белгісі
	const float An = -Yaw - UE_HALF_PI;
	const FVector2D Nd(FMath::Sin(An), -FMath::Cos(An));
	Txt(Cx + Nd.X * (R - 12), Cy + Nd.Y * (R - 12) - 8, TEXT("С"), 13, Amber, 1);
}

// ---------------- мәзірлер ----------------
void AKAPHUD::DrawPause()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const float VW = Canvas->ClipX / Sc, Cx = VW / 2;
	Rect(0, 0, VW, 1080, FLinearColor(0, 0, 0, 0.6f));
	Txt(Cx, 320, TEXT("Үзіліс"), 44, Txtc, 1);
	Button(Cx - 150, 420, 300, 50, TEXT("Жалғастыру"), [GM]() { GM->bPaused = false; }, true, true, 18);
	Button(Cx - 150, 482, 300, 50, TEXT("Сақтау"), [GM]() { GM->Save(false); }, true, false, 18);
	Button(Cx - 150, 544, 300, 50, TEXT("Басты мәзір"), [this]() { UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this))); }, true, false, 18);
}
void AKAPHUD::DrawDeath()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const float VW = Canvas->ClipX / Sc, Cx = VW / 2;
	Rect(0, 0, VW, 1080, FLinearColor(0.15f, 0, 0, 0.75f));
	Txt(Cx, 380, TEXT("Сіз қаза таптыңыз"), 46, Txtc, 1);
	Txt(Cx, 460, TEXT("Соңғы чекпоинттан басталады. Уақыт пен ұңғымалардың күйі сақталады."), 18, Muted, 1);
	Button(Cx - 150, 540, 300, 52, TEXT("Қайта бастау (Enter)"), [GM]() { GM->RespawnPlayer(); }, true, true, 18);
}
void AKAPHUD::DrawCredits()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const float VW = Canvas->ClipX / Sc;
	Rect(0, 0, VW, 1080, FLinearColor::Black);
	TArray<FString> L; GM->CreditsText.ParseIntoArray(L, TEXT("\n"), false);
	float Y = 1100 - GM->CreditsT * 40.f;
	for (int32 i = 0; i < L.Num(); i++)
	{
		const bool bHead = L[i].StartsWith(TEXT("«")) || L[i].StartsWith(TEXT("ҚҰТҚ")) || L[i].StartsWith(TEXT("ҚАЗА"));
		if (Y > -40 && Y < 1100) Txt(VW / 2, Y, L[i], bHead ? 30 : 19, bHead ? Amber : FLinearColor(0.91f, 0.86f, 0.75f), 1, false);
		Y += bHead ? 52 : 30;
	}
}

// ---------------- диалог ----------------
void AKAPHUD::DrawDialog()
{
	const float VW = Canvas->ClipX / Sc;
	const float W = FMath::Min(900.f, VW - 80), X = VW / 2 - W / 2;
	const float TH = Measure(DText, 17).X / (W - 40) * 26 + 26 * (1 + DText.Len() / 400) + 40;
	const float H = 70 + TH + DOpts.Num() * 46;
	const float Y = 1080 - H - 80;
	Rect(X, Y, W, H, FLinearColor(0.06f, 0.06f, 0.05f, 0.95f)); Frame(X, Y, W, H, FLinearColor(Amber.R, Amber.G, Amber.B, 0.4f), 1);
	Txt(X + 20, Y + 14, DName, 18, Amber);
	const float Ph = Para(X + 20, Y + 46, W - 40, DText, 17, Txtc);
	float Yy = Y + 56 + Ph;
	for (int32 i = 0; i < DOpts.Num(); i++)
	{
		const FKAPDialogOption O = DOpts[i];
		Button(X + 20, Yy, W - 40, 40, FString::Printf(TEXT("%d. %s"), i + 1, *O.Label), [this, O]() { bDialog = false; if (O.Fn) O.Fn(); }, !O.bDisabled, false, 16);
		Yy += 46;
	}
}

// ---------------- планшет ----------------
void AKAPHUD::DrawTablet()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	const float VW = Canvas->ClipX / Sc;
	const float W = FMath::Min(1700.f, VW - 80), H = 960, X = VW / 2 - W / 2, Y = 60;
	Rect(0, 0, VW, 1080, FLinearColor(0, 0, 0, 0.35f));
	// корпус (берік өнеркәсіптік планшет)
	Rect(X - 22, Y - 22, W + 44, H + 44, FLinearColor(0.12f, 0.12f, 0.1f, 1.f));
	Frame(X - 22, Y - 22, W + 44, H + 44, FLinearColor(0.25f, 0.25f, 0.22f), 3);
	Rect(X, Y, W, H, FLinearColor(0.06f, 0.07f, 0.08f, 1.f));
	Rect(X, Y, W, 40, FLinearColor(0.13f, 0.15f, 0.17f, 1.f));
	Txt(X + 16, Y + 9, TEXT("QazUran · Кен планшеті"), 17, Amber);
	Txt(X + 300, Y + 11, KAP::FmtTime(S.Time) + FString::Printf(TEXT(" · %d-күн · "), FMath::FloorToInt32(S.Time / 24) + 1) + KAP::WeatherName(GM->Weather->Env.Cur).ToString(), 14, Txtc);
	Txt(X + W - 16, Y + 12, TEXT("Tab — жабу"), 13, Muted, 2);
	static const TCHAR* Tabs[11] = { TEXT("Ұңғымалар"), TEXT("Жұмысшылар"), TEXT("Карта"), TEXT("Тапсырмалар"), TEXT("Жұмыстар ₸"), TEXT("Ауа райы"), TEXT("Рұқсаттар"), TEXT("Инвентарь"), TEXT("Бригадалар"), TEXT("AI мониторы"), TEXT("Жүйе") };
	float Tx = X + 8;
	for (int32 i = 0; i < 11; i++)
	{
		const FString L = Tabs[i] + FString((i == 8 && !S.bBrigadesUnlocked) ? TEXT(" (жабық)") : TEXT(""));
		const float Tw = Measure(L, 14).X + 26;
		Button(Tx, Y + 46, Tw, 32, L, [this, i]() { Tab = i; TabletScroll = 0; }, true, Tab == i, 14);
		Tx += Tw + 4;
	}
	const float CX = X + 16, CY = Y + 92, CW = W - 32, CH = H - 104;
	switch (Tab)
	{
	case 0: TabWells(CX, CY, CW, CH); break;
	case 1: TabWorkers(CX, CY, CW, CH); break;
	case 2: TabMap(CX, CY, CW, CH); break;
	case 3: TabQuests(CX, CY, CW, CH); break;
	case 4: TabJobs(CX, CY, CW, CH); break;
	case 5: TabWeather(CX, CY, CW, CH); break;
	case 6: TabPermits(CX, CY, CW, CH); break;
	case 7: TabInv(CX, CY, CW, CH); break;
	case 8: TabBrig(CX, CY, CW, CH); break;
	case 9: TabAI(CX, CY, CW, CH); break;
	default: TabSys(CX, CY, CW, CH); break;
	}
}

void AKAPHUD::TabWells(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	const float TW = W * 0.62f;
	Txt(X, Y, TEXT("Ұңғымалар мониторы · 21"), 18, Amber);
	const float Cols[9] = { 0, 50, 110, 260, 360, 450, 600, 760, 940 };
	static const TCHAR* Hd[9] = { TEXT("№"), TEXT("Аймақ"), TEXT("Күйі"), TEXT("Қысым"), TEXT("Шығым"), TEXT("Ақау"), TEXT("Бұзылу қаупі"), TEXT("Болжамды уақыт"), TEXT("Адам") };
	for (int32 c = 0; c < 9; c++) Txt(X + Cols[c] * TW / 1000.f, Y + 32, Hd[c], 12, Muted);
	float Yy = Y + 54 - TabletScroll;
	for (int32 i = 0; i < WM->Wells.Num(); i++)
	{
		if (Yy > Y + H - 20) break;
		const FKAPWellRuntime& R = WM->Wells[i];
		const FKAPWellSave& Ws = S.Wells[i];
		if (Yy >= Y + 50)
		{
			if (SelWell == i) Rect(X, Yy - 3, TW, 30, FLinearColor(0.14f, 0.17f, 0.19f, 1));
			Btns.Add({ FBox2D(FVector2D(X, Yy - 3), FVector2D(X + TW, Yy + 27)), [this, i]() { SelWell = i; } });
			auto C = [&](int32 c, const FString& T, FLinearColor Col) { Txt(X + Cols[c] * TW / 1000.f, Yy, T, 14, Col, 0, false); };
			C(0, FString::Printf(TEXT("№%d"), i + 1), Txtc); C(1, FString::FromInt(R.Row.Region + 1), Txtc);
			if (!S.Regions[R.Row.Region]) C(2, TEXT("аймақ ашылмаған — сигнал мұнарасын қосыңыз"), Muted);
			else
			{
				C(2, KAP::WellStateName(Ws.State).ToString(), StateCol(Ws.State));
				C(3, R.bTelemetry ? FString::Printf(TEXT("%.2f МПа"), R.Pressure) : TEXT("телем. жоқ"), R.bTelemetry ? Txtc : Muted);
				C(4, R.bTelemetry ? FString::Printf(TEXT("%.1f м³/сағ"), R.Flow) : TEXT("—"), Txtc);
				C(5, KAP::FaultName(Ws.Fault).ToString(), Txtc);
				if (Ws.State == EWellState::Ok)
				{
					const float Pct = R.FailProb;
					Rect(X + Cols[6] * TW / 1000.f, Yy + 5, FMath::Max(4.f, Pct * 90), 9, FMath::Lerp(CGreen, CRed, Pct));
					C(6, FString::Printf(TEXT("              %d%%"), FMath::RoundToInt32(Pct * 100)), Txtc);
				}
				else C(6, Ws.State == EWellState::Lost ? TEXT("—") : TEXT("БҰЗЫЛҒАН"), CRed);
				const double Tf = WM->TimeToFailure(S, i);
				C(7, Ws.State == EWellState::Ok ? TEXT("~") + KAP::FmtDur(Tf) : Ws.State == EWellState::Fault ? TEXT("ағуға ") + KAP::FmtDur(Tf) : Ws.State == EWellState::Leak ? TEXT("өртке ") + KAP::FmtDur(Tf) : TEXT("—"), Ws.State == EWellState::Leak ? CRed : Txtc);
				int32 Alive = 0; for (const FKAPWorker& K : S.Workers) if (K.WellId == i + 1 && K.State < 4 && K.EvacTo.IsEmpty()) Alive++;
				C(8, FString::Printf(TEXT("%d/%d"), Alive, R.Row.Workers), Txtc);
			}
		}
		Yy += 32;
	}
	// таңдалған ұңғыма — Байес желісінің түсіндірмесі
	const float PX = X + TW + 20, PW = W - TW - 20;
	const FKAPWellRuntime& R = WM->Wells[SelWell];
	const FKAPWellSave& Ws = S.Wells[SelWell];
	Rect(PX, Y, PW, H, FLinearColor(0.09f, 0.1f, 0.11f, 1));
	if (!S.Regions[R.Row.Region]) { Txt(PX + 16, Y + 16, TEXT("Аймақ ашылмаған"), 16, Muted); return; }
	float Py = Y + 14;
	Txt(PX + 16, Py, FString::Printf(TEXT("№%d ұңғыма · "), SelWell + 1) + KAP::WellStateName(Ws.State).ToString(), 18, StateCol(Ws.State)); Py += 34;
	const KAPAI::FWellEvidence Ev = WM->Evidence(S, SelWell, GM->Weather->Env.Temp);
	const KAPAI::FWellPrediction Pr = KAPAI::WellFailureProb(Ev);
	auto KV = [&](const FString& K, const FString& V) { Txt(PX + 16, Py, K, 14, Muted); Txt(PX + 230, Py, V, 14, Txtc); Py += 24; };
	KV(TEXT("Жасы"), FString::Printf(TEXT("%d жыл"), (int32)R.Row.Age));
	KV(TEXT("Қысым / норма"), (Ev.bTelemetry ? FString::Printf(TEXT("%.2f"), R.Pressure) : FString(TEXT("?"))) + FString::Printf(TEXT(" / %.1f МПа"), R.Row.BasePressure));
	KV(TEXT("Ауа температурасы"), FString::Printf(TEXT("%d°C"), FMath::RoundToInt32(Ev.EnvTemp)));
	KV(TEXT("Көрші ұңғымалар қаупі"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Ev.NeighborBad * 100)));
	KV(TEXT("Қышқыл резервуары"), R.Row.AcidTank ? TEXT("бар") : TEXT("жоқ"));
	KV(TEXT("Жөнделген рет"), FString::FromInt(Ws.Repaired));
	Py += 8;
	Txt(PX + 16, Py, TEXT("Байес желісі: P(Бұзылу | дәлелдер)"), 15, Txtc); Py += 28;
	auto Lam = [&](const FVector2D& L, bool bHas) { return bHas ? FString::Printf(TEXT("λ = [%.2f, %.2f]"), L.X, L.Y) : FString(TEXT("дәлел жоқ (приор)")); };
	KV(TEXT("Жасы (A)"), Lam(Pr.LamA, true)); KV(TEXT("Қысым (P)"), Lam(Pr.LamP, Pr.bHasP)); KV(TEXT("Температура (T)"), Lam(Pr.LamT, true)); KV(TEXT("Көрші (N)"), Lam(Pr.LamN, true));
	KV(TEXT("Тозу (W)"), TEXT("жасырын — санау арқылы"));
	Py += 10;
	Txt(PX + 16, Py, FString::Printf(TEXT("№%d ұңғыма: %d%% бұзылу қаупі"), SelWell + 1, FMath::RoundToInt32(Pr.P * 100)), 22, Pr.P > 0.6 ? CRed : Pr.P > 0.3 ? CYel : CGreen); Py += 40;
	Txt(PX + 16, Py, FString::Printf(TEXT("Сағаттық қауіп = %.2f × P(F) = %.2f%%/сағ"), UKAPDataSubsystem::Get(this)->Cfg("hazardScale", 0.06f), UKAPDataSubsystem::Get(this)->Cfg("hazardScale", 0.06f) * Pr.P * 100), 13, Muted); Py += 30;
	const int32 Sel = SelWell;
	Button(PX + 16, Py, 200, 36, TEXT("Маршрут салу"), [this, Sel]() { AKAPCharacter* P = AKAPCharacter::Get(this); const UWellManagerSubsystem* W2 = GetWorld()->GetSubsystem<UWellManagerSubsystem>(); P->Navigator->SetTarget(W2->Wells[Sel].PosM(), FString::Printf(TEXT("№%d ұңғыма"), Sel + 1)); Tab = 2; }, true, true, 15);
	Py += 50;
	if (Ws.Fault != EFaultType::None)
	{
		static const TMap<FString, FString> SN = { { TEXT("valve"), TEXT("Клапан бұрау") }, { TEXT("wires"), TEXT("Сымды жалғау") }, { TEXT("part"), TEXT("Бөлшекті ауыстыру") }, { TEXT("pressure"), TEXT("Қысымды теңестіру") }, { TEXT("fire"), TEXT("Өртті сөндіру") } };
		FString St; for (const FString& X2 : KAP::RepairSteps(Ws.Fault)) { FString Kk, It; if (!X2.Split(TEXT(":"), &Kk, &It)) Kk = X2; St += (St.IsEmpty() ? TEXT("") : TEXT(" → ")) + SN.FindRef(Kk) + (It.IsEmpty() ? FString() : TEXT(" (") + UKAPDataSubsystem::Get(this)->ItemName(FName(*It)) + TEXT(")")); }
		Para(PX + 16, Py, PW - 32, TEXT("Жөндеу қадамдары: ") + St, 14, Txtc);
	}
}

void AKAPHUD::TabWorkers(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	AKAPCharacter* P = AKAPCharacter::Get(this);
	Txt(X, Y, TEXT("Жұмысшылар"), 18, Amber);
	for (int32 i = 0; i < 21; i++) Button(X + 130 + i * 44, Y - 2, 40, 28, FString::FromInt(i + 1), [this, i]() { SelWell = i; }, true, SelWell == i, 13);
	const int32 Dead = WM->DeadCount(S), Tot = S.Workers.Num();
	int32 Evac = 0; for (const FKAPWorker& K : S.Workers) if (!K.EvacTo.IsEmpty()) Evac++;
	Txt(X, Y + 36, FString::Printf(TEXT("Барлығы: %d · Адам шығыны: %d (%d%%) · эвакуацияланған: %d"), Tot, Dead, Tot ? Dead * 100 / Tot : 0, Evac), 15, Txtc);
	if (!S.Regions[WM->Wells[SelWell].Row.Region]) { Txt(X, Y + 70, TEXT("Аймақ ашылмаған."), 15, Muted); return; }
	const float Cols[8] = { 0, 230, 290, 380, 640, 760, 900, 1010 };
	static const TCHAR* Hd[8] = { TEXT("Аты-жөні"), TEXT("Жасы"), TEXT("Доза, мЗв"), TEXT("Денсаулық (HMM болжамы)"), TEXT("Ауру ықт. (Байес)"), TEXT("Өлімге дейін"), TEXT("Қорғаныс"), TEXT("Отбасы") };
	const float K = W / 1500.f;
	for (int32 c = 0; c < 8; c++) Txt(X + Cols[c] * K * 1.4f, Y + 70, Hd[c], 12, Muted);
	float Yy = Y + 94;
	static const TCHAR* HS[4] = { TEXT("сау"), TEXT("уланған"), TEXT("ауру"), TEXT("өлім алдында") };
	static const TCHAR* SY[4] = { TEXT("белгі жоқ"), TEXT("жүрек айну"), TEXT("құсу, әлсіздік"), TEXT("ес жоғалту") };
	const bool bNear = FVector2D::Distance(KAP::CM2M(P->GetActorLocation()), WM->Wells[SelWell].PosM()) < 25 && S.Inv.FindRef("dosimeter") > 0;
	for (const FKAPWorker& Wk : S.Workers)
	{
		if (Wk.WellId != SelWell + 1) continue;
		auto C = [&](int32 c, const FString& T, FLinearColor Col) { Txt(X + Cols[c] * K * 1.4f, Yy, T, 14, Col, 0, false); };
		C(0, Wk.Name + (Wk.bSpecial ? TEXT(" *") : TEXT("")), Txtc); C(1, FString::FromInt(Wk.Age), Txtc); C(2, FString::FromInt(FMath::RoundToInt32(Wk.Dose)), Txtc);
		if (Wk.State == 4) C(3, Wk.bMutated ? TEXT("қайтыс болды (мутация)") : TEXT("қайтыс болды"), Muted);
		else if (!Wk.EvacTo.IsEmpty()) C(3, TEXT("эвакуацияланды → ") + Wk.EvacTo, CGreen);
		else if (bNear) C(3, FString(HS[Wk.State]) + TEXT(" (дозиметр — нақты)"), Txtc);
		else { int32 Bi = 0; for (int32 j = 1; j < 4; j++) if (Wk.Belief[j] > Wk.Belief[Bi]) Bi = j; C(3, FString::Printf(TEXT("%s %d%% · %s"), HS[Bi], FMath::RoundToInt32(Wk.Belief[Bi] * 100), SY[Wk.Symptom]), Txtc); }
		C(4, Wk.State < 4 ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(KAPAI::WorkerSickProb(Wk.Dose, FMath::Fmod(S.Time, 12.0), Wk.bProtected) * 100)) : TEXT("—"), Txtc);
		const double Ttd = WM->TimeToDeath(S, Wk);
		C(5, Wk.State < 4 && Ttd < 999 ? KAP::FmtDur(Ttd) : TEXT("—"), CRed);
		C(6, FString(Wk.bProtected ? TEXT("костюм") : TEXT("—")) + (S.Time < Wk.IodineUntil ? TEXT(" · йод") : TEXT("")), Txtc);
		C(7, Wk.Family, Muted);
		Yy += 30;
	}
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	Para(X, Yy + 16, W, FString::Printf(TEXT("Ағу кезінде доза уақыт бойы жиналады. Шектер: уланған %d · ауру %d · өлім алдында %d · өлім %d мЗв. Жұмысшыға жақындап (E) йод, антидот беруге немесе эвакуациялауға болады. HMM: жасырын күй = {сау, уланған, ауру, өлім алдында}, бақылау = сағат сайынғы симптом есебі (Forward алгоритмі)."),
		(int32)D->Cfg("dosePoisoned", 100), (int32)D->Cfg("doseSick", 250), (int32)D->Cfg("doseDying", 450), (int32)D->Cfg("doseDeath", 650)), 13, Muted);
}

UTexture2D* AKAPHUD::MapTexture()
{
	if (MapTex) return MapTex;
	const int32 N = 200;
	MapTex = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
	if (!MapTex) return nullptr;
	FColor* Px = static_cast<FColor*>(MapTex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	TArray<float> Hh; Hh.SetNum(N * N);
	for (int32 J = 0; J < N; J++) for (int32 I = 0; I < N; I++) Hh[J * N + I] = KAPVis::GroundZ(GetWorld(), (-1200 + (I + 0.5f) * 2400.f / N) * 100, (-1200 + (J + 0.5f) * 2400.f / N) * 100, 0) / 100.f;
	for (int32 J = 0; J < N; J++) for (int32 I = 0; I < N; I++)
	{
		const float Hv = Hh[J * N + I], Hx = Hh[J * N + FMath::Min(N - 1, I + 1)] - Hv;
		const float Sh = FMath::Clamp(1.f + Hx * 0.08f, 0.7f, 1.3f);
		Px[J * N + I] = FColor((uint8)FMath::Clamp((150 + Hv * 1.4f) * Sh, 0.f, 255.f), (uint8)FMath::Clamp((120 + Hv * 1.1f) * Sh, 0.f, 255.f), (uint8)FMath::Clamp((80 + Hv * 0.7f) * Sh, 0.f, 255.f), 255);
	}
	MapTex->GetPlatformData()->Mips[0].BulkData.Unlock();
	MapTex->UpdateResource();
	return MapTex;
}

void AKAPHUD::TabMap(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	AKAPCharacter* P = AKAPCharacter::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	UNavigatorComponent* Nav = P->Navigator;
	const float M = FMath::Min(H, W * 0.6f), Sm = M / 2400.f;
	auto To = [&](FVector2D Wp) { return FVector2D(X + (Wp.X + 1200) * Sm, Y + (Wp.Y + 1200) * Sm); };
	if (UTexture2D* T = MapTexture()) { FCanvasTileItem It(FVector2D(X, Y) * Sc, T->GetResource(), FVector2D(M, M) * Sc, FLinearColor::White); Canvas->DrawItem(It); }
	else Rect(X, Y, M, M, FLinearColor(0.55f, 0.45f, 0.3f));
	// карта басу — нысана таңдау
	Btns.Add({ FBox2D(FVector2D(X, Y), FVector2D(X + M, Y + M)), [this, X, Y, Sm, Nav, D, GM]()
	{
		const FVector2D Wp((Mouse.X - X) / Sm - 1200, (Mouse.Y - Y) / Sm - 1200);
		if (!GM->S().Regions[KAP::RegionOf(Wp.X, Wp.Y)]) { GM->Toast(TEXT("Бұл аймақ әлі ашылмаған"), "warn"); return; }
		const int32 N = D->Graph.Nearest(Wp);
		const bool bNode = N >= 0 && FVector2D::Distance(D->Graph.Nodes[N].Pos, Wp) < 40;
		Nav->SetTarget(bNode ? D->Graph.Nodes[N].Pos : Wp, bNode ? D->Graph.Nodes[N].Name : FString::Printf(TEXT("Белгі (%d, %d)"), (int32)Wp.X, (int32)Wp.Y));
	} });
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges) Line(To(D->Graph.Nodes[E.A].Pos), To(D->Graph.Nodes[E.B].Pos), FLinearColor(0.42f, 0.33f, 0.2f), 2.5f);
	if (bLayerDanger) for (const FKAPDangerZone& Z : GM->Zones) { Disc(To(Z.P), Z.R * Sm, FLinearColor(1, 0.15f, 0.1f, 0.1f + Z.Level * 0.18f)); }
	if (bLayerHeat && GM->bHeatActive)
	{
		float Mx = 0; for (float V : GM->Heat) Mx = FMath::Max(Mx, V);
		if (Mx > 0) for (int32 J = 0; J < 64; J++) for (int32 I = 0; I < 64; I++) { const float V = GM->Heat[J * 64 + I] / Mx; if (V < 0.03f) continue; Rect(X + I * 37.5f * Sm, Y + J * 37.5f * Sm, 37.5f * Sm + 0.5f, 37.5f * Sm + 0.5f, FLinearColor(1, 0.7f - V * 0.7f, 0, 0.2f + V * 0.6f)); }
	}
	const FLinearColor RC[3] = { FLinearColor(0.3f, 0.88f, 0.54f), CBlue, CYel };
	for (int32 m = 2; m >= 0; m--)
	{
		const KAPAI::FRoute& R = Nav->Routes[m];
		if (!Nav->bHasTarget || !R.bValid) continue;
		for (int32 i = 1; i < R.Points.Num(); i++) Line(To(R.Points[i - 1]), To(R.Points[i]), RC[m], (int32)Nav->Mode == m ? 4.f : 1.5f);
	}
	D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName& Id, const FKAPLocationRow& L)
	{
		if (L.Type == "nest") return;
		const FVector2D Pp = To(FVector2D(L.X, L.Y));
		const FLinearColor C = L.Type == "village" ? Amber : L.Type == "ruins" ? CRed : L.Type == "hq" ? CBlue : L.Type == "bandit" ? CRed : L.Type == "police" ? CBlue : L.Type == "shop" ? CGreen : L.Type == "army" ? COrange : Txtc;
		Rect(Pp.X - 3, Pp.Y - 3, 6, 6, C);
		if (L.Type == "village" || L.Type == "hq" || L.Type == "ruins" || L.Type == "bandit" || L.Type == "reservoir" || L.Type == "police" || L.Type == "shop" || L.Type == "army") Txt(Pp.X + 6, Pp.Y - 8, L.Title, 11, C);
		if (L.Type == "army") Frame(Pp.X - 62 * Sm, Pp.Y - 47 * Sm, 124 * Sm, 94 * Sm, COrange, 1.5f);
		if (L.Type == "tower") Txt(Pp.X + 6, Pp.Y - 8, S.Towers.Contains(Id) ? TEXT("мұнара ✓") : TEXT("мұнара"), 10, Txtc);
	});
	for (int32 i = 0; i < WM->Wells.Num(); i++) { const FVector2D Pp = To(WM->Wells[i].PosM()); Disc(Pp, 7, StateCol(S.Wells[i].State)); Txt(Pp.X, Pp.Y - 7, FString::FromInt(i + 1), 9, FLinearColor::Black, 1, false); }
	if (S.bBrigadesUnlocked) for (const FKAPBrigadeState& B : S.Brigades) { const FVector2D Pp = To(B.Pos); Rect(Pp.X - 4, Pp.Y - 4, 8, 8, FLinearColor(0.76f, 0.55f, 1.f)); }
	const FVector2D Quad[4] = { FVector2D(0, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0), FVector2D(0, 0) };
	for (int32 r = 0; r < 4; r++) if (!S.Regions[r]) { Rect(X + Quad[r].X * M, Y + Quad[r].Y * M, M / 2, M / 2, FLinearColor(0, 0, 0, 0.82f)); Txt(X + (Quad[r].X + 0.25f) * M, Y + (Quad[r].Y + 0.24f) * M, KAP::RegionName(r).ToString() + TEXT(" — ашылмаған"), 13, Muted, 1); }
	Line(FVector2D(X + M / 2, Y), FVector2D(X + M / 2, Y + M), FLinearColor(1, 1, 1, 0.25f), 1); Line(FVector2D(X, Y + M / 2), FVector2D(X + M, Y + M / 2), FLinearColor(1, 1, 1, 0.25f), 1);
	const FVector2D Pp = To(KAP::CM2M(P->GetActorLocation()));
	Disc(Pp, 6, FLinearColor::White);
	if (Nav->bHasTarget) Circle(To(Nav->Target), 9, RC[0], 2);
	// оң жақ панель
	const float PX = X + M + 20, PW = W - M - 20;
	float Py = Y;
	Txt(PX, Py, TEXT("Навигатор (A*)"), 18, Amber); Py += 34;
	static const TCHAR* MN[3] = { TEXT("Ең қысқа"), TEXT("Ең қауіпсіз"), TEXT("Жолсыз тура") };
	for (int32 m = 0; m < 3; m++) Button(PX + m * 150, Py, 144, 34, MN[m], [Nav, m]() { Nav->Mode = (ENavMode)m; Nav->Recompute(); }, true, (int32)Nav->Mode == m, 14);
	Py += 48;
	if (Nav->bHasTarget)
	{
		Txt(PX, Py, TEXT("Нысана: ") + Nav->TargetName, 16, Txtc); Py += 28;
		for (int32 m = 0; m < 3; m++)
		{
			const KAPAI::FRoute& R = Nav->Routes[m];
			if (!R.bValid) continue;
			Rect(PX, Py + 6, 14, 6, RC[m]);
			Txt(PX + 22, Py, FString::Printf(TEXT("%s: %.2f км · қауіп %d%% · A* ашқан түйін: %d"), MN[m], R.Length / 1000.0, FMath::RoundToInt32(R.Danger * 100), R.Expanded), 14, Txtc);
			Py += 24;
		}
		Button(PX, Py + 6, 200, 32, TEXT("Маршрутты өшіру"), [Nav]() { Nav->Clear(); }, true, false, 14); Py += 50;
	}
	else { Txt(PX, Py, TEXT("Картаны басып нысана таңдаңыз немесе тізімнен."), 14, Muted); Py += 30; }
	Button(PX, Py, 220, 30, bLayerDanger ? TEXT("[x] Қауіп аймақтары") : TEXT("[ ] Қауіп аймақтары"), [this]() { bLayerDanger = !bLayerDanger; }, true, false, 13);
	Button(PX + 230, Py, 220, 30, bLayerHeat ? TEXT("[x] HMM жылу картасы") : TEXT("[ ] HMM жылу картасы"), [this]() { bLayerHeat = !bLayerHeat; }, true, false, 13);
	Py += 46;
	Txt(PX, Py, TEXT("Орындар"), 15, Txtc); Py += 26;
	float Bx = PX;
	for (FName Id : { FName("wagon0"), FName("aksai"), FName("josaly"), FName("res"), FName("hq"), FName("ruins"), FName("bandit"), FName("tower1"), FName("tower2"), FName("tower3"), FName("wagon1"), FName("wagon2"), FName("wagon3"), FName("missing") })
	{
		const int32* I = D->Graph.Index.Find(Id);
		if (!I) continue;
		const KAPAI::FRoadNode& N = D->Graph.Nodes[*I];
		if (!S.Regions[KAP::RegionOf(N.Pos.X, N.Pos.Y)]) continue;
		const float Bw = Measure(N.Name, 13).X + 20;
		if (Bx + Bw > PX + PW) { Bx = PX; Py += 34; }
		const FVector2D Np = N.Pos; const FString Nn = N.Name;
		Button(Bx, Py, Bw, 30, N.Name, [Nav, Np, Nn]() { Nav->SetTarget(Np, Nn); }, true, false, 13);
		Bx += Bw + 6;
	}
	Py += 46; Bx = PX;
	Txt(PX, Py, TEXT("Ұңғымалар"), 15, Txtc); Py += 26;
	for (int32 i = 0; i < WM->Wells.Num(); i++)
	{
		if (!S.Regions[WM->Wells[i].Row.Region]) continue;
		if (Bx + 54 > PX + PW) { Bx = PX; Py += 34; }
		const FVector2D Wp = WM->Wells[i].PosM();
		Button(Bx, Py, 50, 30, FString::FromInt(i + 1), [Nav, Wp, i]() { Nav->SetTarget(Wp, FString::Printf(TEXT("№%d ұңғыма"), i + 1)); }, true, S.Wells[i].State != EWellState::Ok, 13);
		Bx += 56;
	}
}

void AKAPHUD::TabQuests(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	Txt(X, Y, TEXT("Тапсырмалар журналы"), 18, Amber);
	float Yy = Y + 36 - TabletScroll;
	for (const TCHAR* Kind : { TEXT("main"), TEXT("side") })
	{
		Txt(X, Yy, FCString::Strcmp(Kind, TEXT("main")) == 0 ? TEXT("Негізгі") : TEXT("Қосымша"), 15, Txtc); Yy += 28;
		D->Quests->ForeachRow<FKAPQuestRow>(TEXT("KAP"), [&](const FName& Id, const FKAPQuestRow& Q)
		{
			if (Q.Kind != Kind) return;
			const EQuestState St = GM->QuestState(Id);
			if (St == EQuestState::None) return;
			const bool bDone = St == EQuestState::Done;
			const FLinearColor C = bDone ? Muted : Txtc;
			Rect(X, Yy, W, 58, FLinearColor(0.09f, 0.1f, 0.11f, 1));
			Txt(X + 12, Yy + 6, (bDone ? TEXT("+ ") : TEXT("> ")) + Q.Title + TEXT("  · ") + Q.Giver, 15, C);
			const FString Pr = GM->QuestProgress(Id);
			Txt(X + 12, Yy + 30, Q.Desc + (Pr.IsEmpty() ? TEXT("") : TEXT("   ") + Pr), 13, bDone ? Muted : FLinearColor(0.85f, 0.8f, 0.7f));
			Yy += 64;
		});
		Yy += 12;
	}
	Txt(X, Yy, TEXT("Радио журналы"), 15, Txtc); Yy += 26;
	for (int32 i = GM->RadioLog.Num() - 1; i >= 0 && Yy < Y + H; i--) { Txt(X, Yy, GM->RadioLog[i], 13, Muted); Yy += 20; }
}

void AKAPHUD::TabWeather(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	const FKAPEnv& E = GM->Weather->Env;
	Txt(X, Y, TEXT("Қазіргі ауа райы"), 18, Amber);
	float Py = Y + 36;
	auto KV = [&](const FString& K, const FString& V) { Txt(X, Py, K, 15, Muted); Txt(X + 200, Py, V, 15, Txtc); Py += 26; };
	KV(TEXT("Жағдай"), KAP::WeatherName(E.Cur).ToString());
	KV(TEXT("Температура"), FString::Printf(TEXT("%d°C"), FMath::RoundToInt32(E.Temp)));
	KV(TEXT("Көріну"), FString::Printf(TEXT("%d м"), FMath::RoundToInt32(E.Visibility)));
	KV(TEXT("Жел"), FString::Printf(TEXT("%d м/с"), FMath::RoundToInt32(E.Wind.Size() / 100.f)));
	KV(TEXT("Жол күйі"), E.Wet > 0.2f ? FString::Printf(TEXT("батпақ (%d%%)"), FMath::RoundToInt32(E.Wet * 100)) : FString(TEXT("құрғақ")));
	KV(TEXT("Тәулік"), E.bNight ? TEXT("түн") : TEXT("күн"));
	Py += 14;
	Para(X, Py, W * 0.45f, TEXT("Геймплейге әсері:\n• Құм дауылында көріну 15 м, телеметрия жоғалады, монстрлар ойыншыны дыбыс арқылы табады.\n• Жаңбырда жол батпақ, найзағай электр ақауын тудыруы мүмкін.\n• Ыстық толқынында су тез таусылады, ыстық соққысы қаупі.\n• Түнде суық, монстрлар 2.5 есе көп."), 14, Txtc);
	const float PX = X + W * 0.5f;
	Txt(PX, Y, TEXT("Болжам · 24 сағат"), 18, Amber);
	float Yy = Y + 36;
	for (const FKAPWeatherSeg& Sg : S.Weather)
	{
		if (Sg.Start + Sg.Dur < S.Time || Sg.Start > S.Time + 24) continue;
		const FString Warn = Sg.Type == EWeatherType::Storm ? TEXT("! Құм дауылы — баспана табыңыз") : Sg.Type == EWeatherType::Heat ? TEXT("! Ыстық толқыны — су алыңыз") : Sg.Type == EWeatherType::Rain ? TEXT("! Найзағай — электр ақаулары") : Sg.Type == EWeatherType::Fog ? TEXT("Тұман — көріну нашар") : TEXT("");
		Txt(PX, Yy, KAP::FmtTime(FMath::Max(S.Time, Sg.Start)), 14, Txtc);
		Txt(PX + 80, Yy, KAP::WeatherName(Sg.Type).ToString(), 14, Txtc);
		Txt(PX + 260, Yy, KAP::FmtDur(Sg.Start + Sg.Dur - FMath::Max(S.Time, Sg.Start)), 14, Muted);
		Txt(PX + 400, Yy, Warn, 14, Sg.Type == EWeatherType::Storm ? CRed : Txtc);
		Yy += 26;
	}
}

void AKAPHUD::TabPermits(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	float Py = Y;
	Txt(X, Py, TEXT("Рұқсаттар"), 18, Amber); Py += 34;
	Txt(X, Py, TEXT("Қару рұқсаты"), 15, Muted); Txt(X + 260, Py, S.bPermitWeapon ? TEXT("БЕРІЛДІ") : TEXT("жоқ — №1 ұңғыманы жөндеп, күзетпен сөйлесіңіз"), 15, S.bPermitWeapon ? CGreen : CRed); Py += 26;
	Txt(X, Py, TEXT("Басқармаға кіру рұқсаты"), 15, Muted); Txt(X + 260, Py, S.bPermitAccess ? FString(TEXT("БЕРІЛДІ")) : FString::Printf(TEXT("жоқ — %d ұңғыма жөндеңіз (%d/%d)"), (int32)D->Cfg("accessPermitWells", 5), S.WellsRepaired, (int32)D->Cfg("accessPermitWells", 5)), 15, S.bPermitAccess ? CGreen : CRed); Py += 30;
	Txt(X, Py, TEXT("Рұқсатсыз қару ұстасаңыз, күзет пен жасақтар дұшпандыққа көшеді."), 13, Muted); Py += 40;
	Txt(X, Py, TEXT("Фракциялар (UFactionSubsystem)"), 18, Amber); Py += 34;
	const float Rel = S.RepMilitia;
	Txt(X, Py, TEXT("Ауыл әскерлері (жасақтар)"), 15, Txtc); Rect(X + 300, Py + 6, (Rel + 100) / 2, 8, FMath::Lerp(CRed, CGreen, (Rel + 100) / 200)); Txt(X + 420, Py, FString::Printf(TEXT("%d · %s"), (int32)Rel, Rel >= 50 ? TEXT("Одақтас") : (Rel <= -30 || GM->Provoked > 0) ? TEXT("Жау") : TEXT("Бейтарап")), 15, Txtc); Py += 26;
	Txt(X, Py, TEXT("Кеніш жұмысшылары"), 15, Txtc); Txt(X + 420, Py, TEXT("Достас"), 15, CGreen); Py += 26;
	Txt(X, Py, TEXT("Бандиттер"), 15, Txtc); Txt(X + 420, Py, TEXT("Үнемі жау"), 15, CRed); Py += 26;
	Txt(X, Py, TEXT("Мутанттар"), 15, Txtc); Txt(X + 420, Py, TEXT("Үнемі жау"), 15, CRed); Py += 26;
	const float PX = X + W * 0.52f;
	float Qy = Y;
	const int32 Dead = WM->DeadCount(S), Tot = FMath::Max(1, S.Workers.Num());
	int32 Bosses = 0; for (bool B : S.BossAlive) if (!B) Bosses++;
	Txt(PX, Qy, TEXT("Аймақ жағдайы (UMutationSubsystem)"), 18, Amber); Qy += 34;
	auto KV = [&](const FString& K, const FString& V, FLinearColor C) { Txt(PX, Qy, K, 15, Muted); Txt(PX + 260, Qy, V, 15, C); Qy += 26; };
	KV(TEXT("Мутация таралуы"), FString::Printf(TEXT("%.1f%%"), S.Mutation), S.Mutation > 70 ? CRed : CYel);
	KV(TEXT("Адам шығыны"), FString::Printf(TEXT("%d / %d (%d%%)"), Dead, Tot, Dead * 100 / Tot), Txtc);
	KV(TEXT("Жанған ұңғымалар"), FString::FromInt(S.Burned), Txtc);
	KV(TEXT("Жұмыс істейтіндер"), FString::Printf(TEXT("%d / 21"), WM->OkCount(S)), Txtc);
	KV(TEXT("Өлтірілген босстар"), FString::Printf(TEXT("%d / 4"), Bosses), Txtc);
	Qy += 14;
	Txt(PX, Qy, TEXT("Концовка шарттары (UKAPGameInstance)"), 16, Txtc); Qy += 30;
	Para(PX, Qy, W * 0.46f, FString::Printf(TEXT("«Қашу»: жанған ұңғыма > %d (қазір %d) НЕМЕСЕ өлім > %d%% (қазір %d%%) НЕМЕСЕ мутация 100%% (қазір %d%%)\n\n«Құтқару»: апат себебі анықталып, есеп жіберілгенде ≥ %d ұңғыма жұмыс істеп, ағу/өрт болмаса және мутация < %d%%"),
		(int32)D->Cfg("failBurned", 8), S.Burned, (int32)(D->Cfg("failDeadPct", 0.6f) * 100), Dead * 100 / Tot, FMath::RoundToInt32(S.Mutation), (int32)D->Cfg("successWells", 11), (int32)D->Cfg("successMutation", 70)), 14, Txtc);
}

void AKAPHUD::TabInv(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	Txt(X, Y, TEXT("Инвентарь"), 18, Amber);
	float Yy = Y + 36;
	static const TSet<FName> Usable = { "medkit", "bandage", "food", "water", "iodine", "antidote" };
	static const TSet<FName> Wearable = { "suit", "hat", "coat", "armor" };
	D->Items->ForeachRow<FKAPItemRow>(TEXT("KAP"), [&](const FName& Id, const FKAPItemRow& R)
	{
		const int32 N = S.Inv.FindRef(Id);
		if (N <= 0) return;
		Txt(X, Yy + 6, FString::Printf(TEXT("%s × %d"), *R.Title, N), 15, Txtc);
		Txt(X + 280, Yy + 7, R.Desc, 13, Muted);
		if (Usable.Contains(Id)) Button(X + W * 0.42f, Yy, 110, 30, TEXT("Қолдану"), [this, Id]() { if (AKAPCharacter* P = AKAPCharacter::Get(this)) P->UseItem(Id); }, true, false, 13);
		if (Wearable.Contains(Id)) Button(X + W * 0.42f, Yy, 110, 30, S.Wear.Contains(Id) ? TEXT("Шешу") : TEXT("Кию"), [GM, Id]() { GM->UseWearToggle(Id); }, true, S.Wear.Contains(Id), 13);
		Yy += 36;
	});
	const float PX = X + W * 0.55f;
	float Py = Y;
	Txt(PX, Py, TEXT("Қарулар мен құралдар"), 18, Amber); Py += 36;
	for (const FName& Wn : S.Weapons)
	{
		const FKAPWeaponRow* Wr = D->Weapon(Wn);
		if (!Wr) continue;
		const FString Info = Wr->Kind == "gun" ? FString::Printf(TEXT("оқ: %d + %d"), S.Mag.FindRef(Wn), S.Ammo.FindRef(Wr->Ammo)) : Wr->Kind == "throw" ? FString::Printf(TEXT("× %d"), S.Ammo.FindRef(Wr->Ammo)) : FString(TEXT("құрал"));
		Txt(PX, Py, Wr->Title, 15, Txtc); Txt(PX + 260, Py, Info + (Wr->Permit ? TEXT(" · рұқсат керек") : TEXT("")), 13, Muted); Py += 26;
	}
	Py += 14;
	Txt(PX, Py, TEXT("Оқ-дәрі"), 15, Txtc); Py += 26;
	for (const TPair<FName, int32>& A : S.Ammo) if (A.Value > 0) { Txt(PX, Py, D->AmmoName(A.Key), 14, Muted); Txt(PX + 260, Py, FString::FromInt(A.Value), 14, Txtc); Py += 22; }
}

void AKAPHUD::TabBrig(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	Txt(X, Y, TEXT("Жөндеу бригадалары"), 18, Amber);
	if (!S.bBrigadesUnlocked) { Txt(X, Y + 40, FString::Printf(TEXT("Кейінгі кезең: %d ұңғыманы өзіңіз жөндегеннен кейін ашылады (%d/%d)."), (int32)D->Cfg("brigadeUnlockWells", 3), S.WellsRepaired, (int32)D->Cfg("brigadeUnlockWells", 3)), 15, Muted); return; }
	float Py = Y + 36;
	static const TMap<FString, FString> TN = { { TEXT("pump"), TEXT("сорғы") }, { TEXT("pipe"), TEXT("құбыр") }, { TEXT("electric"), TEXT("электр") }, { TEXT("acid"), TEXT("қышқыл") }, { TEXT("fire"), TEXT("өрт") } };
	static const TMap<FName, FString> SN = { { "idle", TEXT("базада") }, { "travel", TEXT("жолда") }, { "repair", TEXT("жөндеуде") }, { "return", TEXT("қайтуда") } };
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (const FKAPBrigadeState& B : S.Brigades)
	{
		const FKAPBrigadeRow* R = D->Brigades->FindRow<FKAPBrigadeRow>(B.Id, TEXT("KAP"), false);
		FString Tools; for (const FString& T : WM->BrigadeTools(B.Id)) Tools += (Tools.IsEmpty() ? TEXT("") : TEXT(", ")) + TN.FindRef(T);
		Txt(X, Py, R ? R->Title : B.Id.ToString(), 15, Txtc); Txt(X + 240, Py, Tools, 14, Muted); Txt(X + 480, Py, SN.FindRef(B.State), 14, Txtc); Txt(X + 600, Py, B.Target > 0 ? FString::Printf(TEXT("№%d"), B.Target) : TEXT("—"), 14, Txtc);
		Py += 28;
	}
	Py += 10;
	Para(X, Py, W * 0.5f, TEXT("CSP моделі: айнымалылар — бос бригадалар; мәндер — ақаулы ұңғымалар немесе «бос». Шектеулер: (1) бригадада ақауға сай құрал бар; (2) қашықтық ≤ 1500 м; (3) жету + жөндеу уақыты ≤ өртке дейінгі уақыт; (4) бір ұңғымаға бір бригада. Мақсат: Σ басымдық (қауіптегі адамдар × 2 + кезең салмағы)."), 13, Muted);
	Py += 110;
	Button(X, Py, 300, 38, TEXT("Backtracking арқылы шешу"), [GM]() { GM->RunCSP(true); }, true, true, 15);
	Button(X + 310, Py, 300, 38, TEXT("Hill climbing арқылы шешу"), [GM]() { GM->RunCSP(false); }, true, false, 15);
	const float PX = X + W * 0.55f;
	float Ry = Y + 36;
	if (!GM->CSPResult.IsSet()) { Txt(PX, Ry, TEXT("Кесте әлі құрылмаған."), 15, Muted); return; }
	const KAPAI::FCSPResult& R = *GM->CSPResult;
	Txt(PX, Ry, TEXT("Нәтиже"), 18, Amber); Ry += 30;
	Para(PX, Ry, W * 0.43f, FString::Printf(TEXT("%s\nМақсат мәні: %.0f · түйін/итерация: %d · уақыт: %.2f мс"), *R.Method, R.Score, R.Nodes, R.Ms), 14, Txtc); Ry += 60;
	bool bAny = false;
	for (int32 i = 0; i < GM->CSPBrigadeIds.Num(); i++)
	{
		const FKAPBrigadeRow* Br = D->Brigades->FindRow<FKAPBrigadeRow>(GM->CSPBrigadeIds[i], TEXT("KAP"), false);
		FString Dom; for (int32 V : R.Domains[i]) Dom += (Dom.IsEmpty() ? TEXT("") : TEXT(", ")) + (V < 0 ? FString(TEXT("∅")) : FString::Printf(TEXT("№%d"), V));
		Txt(PX, Ry, Br ? Br->Title : TEXT("?"), 14, Txtc); Txt(PX + 220, Ry, Dom, 13, Muted);
		const int32 A = R.Assign.IsValidIndex(i) ? R.Assign[i] : -1;
		Txt(PX + 520, Ry, A > 0 ? FString::Printf(TEXT("→ №%d"), A) : TEXT("—"), 15, A > 0 ? CGreen : Muted);
		if (A > 0) bAny = true;
		Ry += 26;
	}
	Ry += 12;
	if (bAny) Button(PX, Ry, 300, 38, TEXT("Кесте бойынша жіберу"), [GM]() { GM->DispatchCSP(); }, true, true, 15);
	else Txt(PX, Ry, TEXT("Шектеулерді қанағаттандыратын тағайындау жоқ."), 14, Muted);
}

void AKAPHUD::TabAI(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPSession& S = GM->S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	Txt(X, Y, TEXT("AI мониторы — силлабус тақырыптары"), 18, Amber);
	const float CW = W * 0.48f;
	float Py = Y + 36;
	auto Card = [&](float Cx, float& Cy, const FString& Title, const FString& Body)
	{
		const float Ht = 40 + FMath::CeilToFloat(Measure(Body, 13).X / (CW - 30) + Body.Len() / 160.f + 1) * 19;
		Rect(Cx, Cy, CW, Ht, FLinearColor(0.09f, 0.1f, 0.11f, 1));
		Txt(Cx + 12, Cy + 8, Title, 15, Amber);
		Para(Cx + 12, Cy + 32, CW - 24, Body, 13, Txtc);
		Cy += Ht + 10;
	};
	FString Astar = FString::Printf(TEXT("Граф: %d түйін, %d қабырға. Эвристика — түзу (евклид) қашықтық (рұқсатты, өйткені қабырға құны ≥ ұзындық). «Қауіпсіз» режимде салмаққа ×(1 + 6·қауіп) қосылады; «тура» режим 60×60 торда беткей мен қауіпті есептейді."), D->Graph.Nodes.Num(), D->Graph.Edges.Num());
	static const TCHAR* MN[3] = { TEXT("Ең қысқа"), TEXT("Ең қауіпсіз"), TEXT("Жолсыз тура") };
	for (int32 m = 0; m < 3; m++) if (P->Navigator->bHasTarget && P->Navigator->Routes[m].bValid) Astar += FString::Printf(TEXT("\n• %s: %d м, қауіп %d%%, ашылған түйін %d"), MN[m], (int32)P->Navigator->Routes[m].Length, FMath::RoundToInt32(P->Navigator->Routes[m].Danger * 100), P->Navigator->Routes[m].Expanded);
	Card(X, Py, TEXT("2-апта · A* іздеу"), Astar);
	TArray<int32> Ok; for (int32 i = 0; i < WM->Wells.Num(); i++) if (S.Wells[i].State == EWellState::Ok && S.Regions[WM->Wells[i].Row.Region]) Ok.Add(i);
	Ok.Sort([&](int32 A, int32 B) { return WM->Wells[A].FailProb > WM->Wells[B].FailProb; });
	FString Top; for (int32 k = 0; k < FMath::Min(3, Ok.Num()); k++) Top += FString::Printf(TEXT("%s№%d — %d%%"), k ? TEXT(", ") : TEXT(""), Ok[k] + 1, FMath::RoundToInt32(WM->Wells[Ok[k]].FailProb * 100));
	Card(X, Py, TEXT("6-апта · Байес желісі"), TEXT("Жасы, Температура → Тозу (жасырын); Тозу, Қысым, Көрші → Бұзылу. Толық санау әдісі; үздіксіз датчиктер виртуалды дәлел (λ) ретінде. Дауылда телеметрия жоғалса, қысым түйіні приорға оралады. Ауру желісі: Доза, Жұмыс уақыты, Қорғаныс → Ауру.\nЕң қауіпті: ") + Top);
	Card(X, Py, TEXT("7-апта · HMM (Forward алгоритмі)"), FString(TEXT("Жасырын күй — монстр тұрған тор ұяшығы (64×64), бақылау — естілген дыбыс (8 сектор × қатты/әлсіз) немесе тыныштық. ")) + (GM->bHeatActive ? FString::Printf(TEXT("БЕЛСЕНДІ: %d монстр бақылануда."), GM->HeatCount) : FString(TEXT("Дауыл/тұман кезінде қосылады."))) + TEXT("\nЖұмысшының жасырын күйі: {сау, уланған, ауру, өлім алдында}; бақылау — сағат сайынғы симптом есебі."));
	Card(X, Py, TEXT("3-апта · CSP"), TEXT("Бригадалар бөлімінде: backtracking (MRV + forward checking + branch & bound) және hill climbing салыстыру."));
	// Q-кестелер
	const float QX = X + W * 0.52f;
	float Qy = Y + 36;
	Txt(QX, Qy, FString::Printf(TEXT("5-апта · MDP + Q-learning (үйрету %d мс, 3×25 000 эпизод, α=0.12, γ=0.9)"), FMath::RoundToInt32(AKAPNPC::TrainMs)), 14, Amber); Qy += 24;
	Para(QX, Qy, W * 0.46f, TEXT("Күйлер: денсаулық(3) × қашықтық(3) × оқ(3) × серік(2) = 54. Әрекеттер: шабуыл, жасырыну, шегіну, көмек шақыру."), 13, Txtc); Qy += 44;
	const FLinearColor AC[4] = { CRed, CBlue, CYel, CGreen };
	static const TCHAR* PN[3] = { TEXT("Ауыл жасағы"), TEXT("Бандиттер"), TEXT("Мутанттар") };
	static const TCHAR* HpN[3] = { TEXT("төмен"), TEXT("орта"), TEXT("жоғары") };
	for (int32 p = 0; p < 3; p++)
	{
		const KAPAI::FQPolicy& Pol = AKAPNPC::GetPolicy((KAPAI::EQProfile)p);
		Txt(QX, Qy, FString(PN[p]) + TEXT(" — саясат (оқ толық, серік жоқ)"), 14, Txtc); Qy += 22;
		Txt(QX, Qy, TEXT("HP \\ қашықтық"), 12, Muted); Txt(QX + 140, Qy, TEXT("жақын"), 12, Muted); Txt(QX + 280, Qy, TEXT("орта"), 12, Muted); Txt(QX + 420, Qy, TEXT("алыс"), 12, Muted); Qy += 20;
		for (int32 Hp = 2; Hp >= 0; Hp--)
		{
			Txt(QX, Qy, HpN[Hp], 13, Txtc);
			for (int32 Dd = 0; Dd < 3; Dd++) { const int32 A = Pol.Best(KAPAI::EncodeState(Hp, Dd, 2, 0)); Txt(QX + 140 + Dd * 140, Qy, KAPAI::ActionName(A), 13, AC[A]); }
			Qy += 20;
		}
		Qy += 8;
	}
	Txt(QX, Qy, TEXT("Жақындағы агенттер"), 14, Txtc); Qy += 22;
	int32 Shown = 0;
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It && Shown < 6; ++It)
	{
		if (It->bDead || It->LastAction < 0 || FVector::Dist(It->GetActorLocation(), P->GetActorLocation()) > 12000.f) continue;
		const int32 Sx = It->LastState;
		Txt(QX, Qy, FString::Printf(TEXT("%s · HP %d · күй(hp%d,d%d,a%d,s%d) → %s"), *It->DisplayName, (int32)It->Hp, Sx / 18, (Sx / 6) % 3, (Sx / 2) % 3, Sx % 2, KAPAI::ActionName(It->LastAction)), 12, Txtc);
		Qy += 18; Shown++;
	}
}

void AKAPHUD::TabSys(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	float Py = Y;
	Txt(X, Py, TEXT("Жүйе"), 18, Amber); Py += 40;
	Button(X, Py, 200, 38, TEXT("Сақтау"), [GM]() { GM->Save(false); }, true, true, 15);
	Txt(X + 220, Py + 10, TEXT("Автосақтау: ұңғыма жөнделгенде, жаңа аймаққа кіргенде, вагон-үйде ұйықтағанда."), 13, Muted); Py += 56;
	Button(X, Py, 320, 38, GI->S.bRuSubtitles ? TEXT("Орысша субтитр: ҚОСУЛЫ") : TEXT("Орысша субтитр: өшірулі"), [GI]() { GI->S.bRuSubtitles = !GI->S.bRuSubtitles; }, true, false, 15); Py += 56;
	Txt(X, Py + 8, TEXT("Тышқан сезімталдығы:"), 15, Txtc);
	for (int32 i = 0; i < 4; i++) { const float V = 0.5f + i * 0.5f; Button(X + 220 + i * 70, Py, 64, 34, FString::SanitizeFloat(V), [GI, V]() { GI->S.MouseSens = V; }, true, FMath::IsNearlyEqual(GI->S.MouseSens, V), 14); }
	Py += 52;
	Txt(X, Py + 8, TEXT("Уақыт жылдамдығы (демо):"), 15, Txtc);
	const int32 TS[4] = { 1, 4, 12, 30 };
	for (int32 i = 0; i < 4; i++) { const int32 V = TS[i]; Button(X + 260 + i * 70, Py, 64, 34, FString::Printf(TEXT("×%d"), V), [GM, V]() { GM->TimeScale = V; }, true, (int32)GM->TimeScale == V, 14); }
	Py += 52;
	Txt(X, Py + 8, TEXT("Ауа райын сынау (демо):"), 15, Txtc);
	const TCHAR* WN[5] = { TEXT("Кесте бойынша"), TEXT("Құм дауылы"), TEXT("Жаңбыр"), TEXT("Тұман"), TEXT("Ыстық") };
	const EWeatherType WT[5] = { EWeatherType::Clear, EWeatherType::Storm, EWeatherType::Rain, EWeatherType::Fog, EWeatherType::Heat };
	float Bx = X + 260;
	for (int32 i = 0; i < 5; i++)
	{
		const float Bw = Measure(WN[i], 14).X + 24;
		const EWeatherType T = WT[i]; const bool bNone = i == 0;
		Button(Bx, Py, Bw, 34, WN[i], [GM, T, bNone]() { GM->Weather->bOverride = !bNone; GM->Weather->OverrideType = T; }, true, false, 14);
		Bx += Bw + 6;
	}
	Py += 60;
	Para(X, Py, W * 0.7f, TEXT("Басқару: WASD · Shift · Space · E — әрекет · Tab — планшет · 1–9 — құрал · R — оқтау · F — фонарь · H/J/K — аптечка/су/тамақ · көлікте V — камера, E — түсу · Esc — үзіліс"), 13, Muted);
}

// ---------------- мини-ойындар ----------------
void AKAPHUD::StartMinigame(const FString& Title, const TArray<FString>& Steps, TFunction<void(bool)> OnDone)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	TMap<FName, int32> Need;
	for (const FString& S : Steps) { FString K, It; if (S.Split(TEXT(":"), &K, &It)) Need.FindOrAdd(FName(*It))++; }
	TArray<FString> Miss;
	for (const TPair<FName, int32>& N : Need) if (GM->Count(N.Key) < N.Value) Miss.Add(D->ItemName(N.Key));
	if (Miss.Num()) { GM->Toast(TEXT("Жетіспейді: ") + FString::Join(Miss, TEXT(", ")), "warn"); UKAPAudio::Get(this)->Beep(false); return; }
	MG = FMG();
	MG.bActive = true; MG.Title = Title; MG.Steps = Steps; MG.I = 0; MG.OnDone = OnDone;
	MGStartStep();
}
void AKAPHUD::MGStartStep()
{
	FString K, It;
	if (!MG.Steps[MG.I].Split(TEXT(":"), &K, &It)) K = MG.Steps[MG.I];
	MG.Kind = K; MG.Item = It; MG.A = 0; MG.B = 0; MG.T = 0; MG.Phase = 0; MG.K = 0; MG.Drag = -1; MG.Spark = 0; MG.MsgT = 0; MG.Last = -999;
	MG.Done.Init(false, 4); MG.Bolts.Init(true, 6); MG.Flames.Reset();
	MG.Order = { 0, 3, 1, 4, 2, 5 };
	MG.Right = { 0, 1, 2, 3 };
	for (int32 i = 3; i > 0; i--) MG.Right.Swap(i, FMath::RandRange(0, i));
}
void AKAPHUD::MGStepDone()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!MG.Item.IsEmpty()) GM->TakeItem(FName(*MG.Item), 1);
	UKAPAudio::Get(this)->Beep(true);
	MG.I++;
	if (MG.I >= MG.Steps.Num()) { MGFinish(true); return; }
	MGStartStep();
}
void AKAPHUD::MGFinish(bool bOk)
{
	TFunction<void(bool)> F = MG.OnDone;
	MG.bActive = false;
	if (AKAPCharacter* P = AKAPCharacter::Get(this)) P->SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), P->GetStamina() - 15);
	if (!bOk) if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Toast(TEXT("Жөндеу тоқтатылды"), "warn");
	if (F) F(bOk);
}

void AKAPHUD::DrawMinigame(float Dt)
{
	const float VW = Canvas->ClipX / Sc;
	const float W = 840, H = 540, X = VW / 2 - W / 2, Y = 230;
	Rect(0, 0, VW, 1080, FLinearColor(0, 0, 0, 0.5f));
	Rect(X - 16, Y - 70, W + 32, H + 140, FLinearColor(0.08f, 0.09f, 0.1f, 0.98f)); Frame(X - 16, Y - 70, W + 32, H + 140, FLinearColor(0.23f, 0.23f, 0.19f), 2);
	static const TMap<FString, FString> Names = { { TEXT("valve"), TEXT("Клапан бұрау") }, { TEXT("wires"), TEXT("Сымды жалғау") }, { TEXT("part"), TEXT("Бөлшекті ауыстыру") }, { TEXT("pressure"), TEXT("Қысымды теңестіру") }, { TEXT("fire"), TEXT("Өртті сөндіру") } };
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	auto StepName = [&](const FString& S) { FString K, It; if (!S.Split(TEXT(":"), &K, &It)) K = S; return Names.FindRef(K) + (It.IsEmpty() ? FString() : TEXT(" (") + D->ItemName(FName(*It)) + TEXT(")")); };
	Txt(VW / 2, Y - 56, MG.Title + TEXT(" — ") + StepName(MG.Steps[MG.I]), 22, Amber, 1);
	Rect(X, Y, W, H, FLinearColor(0.04f, 0.05f, 0.06f, 1));
	FString Steps; for (int32 i = 0; i < MG.Steps.Num(); i++) Steps += (i < MG.I ? TEXT("+ ") : i == MG.I ? TEXT("> ") : TEXT("· ")) + StepName(MG.Steps[i]) + TEXT("    ");
	Txt(VW / 2, Y + H + 46, Steps, 13, Muted, 1);
	Txt(VW / 2, Y + H + 70, TEXT("Esc — тоқтату"), 12, Muted, 1);
	const FVector2D C(X + W / 2, Y + H / 2 - 10);
	const FVector2D M = Mouse;
	const bool bDown = bMouseDown;
	bool bDone = false;
	FString Hint;
	if (MG.Kind == TEXT("valve"))
	{
		Hint = TEXT("Тышқанды басып тұрып, клапанды сағат тілімен 3 толық айналдырыңыз");
		if (bDown)
		{
			const float Ang = FMath::Atan2(M.Y - C.Y, M.X - C.X);
			if (MG.Last > -900) { float Dd = Ang - MG.Last; if (Dd > UE_PI) Dd -= UE_TWO_PI; if (Dd < -UE_PI) Dd += UE_TWO_PI; if (FMath::Abs(Dd) < 1.f) MG.A += Dd * (Dd > 0 ? 1.f : 1.2f); }
			MG.Last = Ang;
		}
		else MG.Last = -999;
		MG.A = FMath::Max(0.f, MG.A);
		for (int32 i = 0; i < 6; i++) { const float A = MG.A + i * UE_PI / 3; Line(C, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 160, FLinearColor(0.75f, 0.22f, 0.17f), 12); }
		Circle(C, 160, FLinearColor(0.75f, 0.22f, 0.17f), 16, 40);
		Disc(C, 30, FLinearColor(0.49f, 0.52f, 0.55f));
		const float P = MG.A / (UE_TWO_PI * 3);
		Rect(X + 120, Y + H - 40, W - 240, 14, FLinearColor(0.15f, 0.15f, 0.15f)); Rect(X + 120, Y + H - 40, (W - 240) * FMath::Min(1.f, P), 14, CGreen);
		Txt(VW / 2, Y + H - 70, FString::Printf(TEXT("%.1f / 3 айналым"), P * 3), 16, Txtc, 1);
		bDone = P >= 1;
	}
	else if (MG.Kind == TEXT("wires"))
	{
		Hint = TEXT("Сымдарды сол жақтан оң жақтағы сәйкес түсті клеммаға сүйреңіз");
		const FLinearColor Cols[4] = { FLinearColor(0.9f, 0.3f, 0.24f), FLinearColor(0.2f, 0.6f, 0.86f), FLinearColor(0.95f, 0.77f, 0.06f), FLinearColor(0.18f, 0.8f, 0.44f) };
		auto L = [&](int32 i) { return FVector2D(X + 130, Y + 90 + i * 110); };
		auto R = [&](int32 j) { return FVector2D(X + W - 130, Y + 90 + j * 110); };
		if (bDown && MG.Drag < 0) for (int32 i = 0; i < 4; i++) if (!MG.Done[i] && FVector2D::Distance(M, L(i)) < 30) MG.Drag = i;
		if (MG.T < 0 && MG.Drag >= 0)
		{
			for (int32 j = 0; j < 4; j++) if (FVector2D::Distance(M, R(j)) < 36)
			{
				if (MG.Right[j] == MG.Drag) { MG.Done[MG.Drag] = true; UKAPAudio::Get(this)->Beep(true); }
				else { MG.Spark = 0.4f; UKAPAudio::Get(this)->Beep(false); if (AKAPCharacter* Pc = AKAPCharacter::Get(this)) Pc->DamagePlayer(2, 0, true); }
			}
			MG.Drag = -1; MG.T = 0;
		}
		if (!bDown && MG.Drag >= 0 && MG.T >= 0) MG.T = -1;
		for (int32 i = 0; i < 4; i++)
		{
			Rect(X + 20, L(i).Y - 8, 110, 16, Cols[i]); Disc(L(i), 16, Cols[i]);
			if (MG.Done[i]) { const int32 j = MG.Right.IndexOfByKey(i); Line(L(i), R(j), Cols[i], 10); }
		}
		for (int32 j = 0; j < 4; j++) { Rect(R(j).X - 18, R(j).Y - 22, 36, 44, FLinearColor(0.27f, 0.27f, 0.27f)); Rect(R(j).X + 22, R(j).Y - 8, 90, 16, Cols[MG.Right[j]]); }
		if (MG.Drag >= 0) Line(L(MG.Drag), M, Cols[MG.Drag], 10);
		if (MG.Spark > 0) { MG.Spark -= Dt; Rect(X, Y, W, H, FLinearColor(1, 0.95f, 0.5f, MG.Spark * 2)); Txt(VW / 2, Y + H / 2, TEXT("ҚЫСҚА ТҰЙЫҚТАЛУ!"), 30, FLinearColor::Black, 1, false); }
		bDone = MG.Done[0] && MG.Done[1] && MG.Done[2] && MG.Done[3];
	}
	else if (MG.Kind == TEXT("part"))
	{
		Hint = TEXT("1) Барлық болтты бұрап алыңыз · 2) ескі бөлшекті шығарып, жаңасын салыңыз · 3) болттарды жұлдызша ретімен бекітіңіз");
		auto Bolt = [&](int32 i) { const float A = i * UE_PI / 3 - UE_HALF_PI; return C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 170; };
		const bool bClick = bDown && MG.Last < -900;
		MG.Last = bDown ? 0.f : -999.f;
		if (bClick)
		{
			int32 Hit = -1; for (int32 i = 0; i < 6; i++) if (FVector2D::Distance(M, Bolt(i)) < 30) Hit = i;
			if (MG.Phase == 0 && Hit >= 0 && MG.Bolts[Hit]) { MG.Bolts[Hit] = false; UKAPAudio::Get(this)->Play(EKAPSound::Swing, 0.4f); bool bAll = true; for (bool B : MG.Bolts) if (B) bAll = false; if (bAll) MG.Phase = 1; }
			else if (MG.Phase == 1 && FVector2D::Distance(M, C) < 110) { MG.Phase = 2; UKAPAudio::Get(this)->Play(EKAPSound::Pickup, 0.5f); }
			else if (MG.Phase == 2 && FBox2D(FVector2D(X + W - 170, Y + H - 130), FVector2D(X + W - 30, Y + H - 30)).IsInside(M)) { MG.Phase = 3; UKAPAudio::Get(this)->Play(EKAPSound::Pickup, 0.5f); }
			else if (MG.Phase == 3 && Hit >= 0 && !MG.Bolts[Hit])
			{
				if (Hit == MG.Order[MG.K]) { MG.Bolts[Hit] = true; MG.K++; UKAPAudio::Get(this)->Play(EKAPSound::Swing, 0.4f); if (MG.K >= 6) MG.Phase = 4; }
				else { MG.Msg = TEXT("Қате рет! Жұлдызша ретімен бекітіңіз — қайта бастаңыз"); MG.MsgT = 2; for (int32 i = 0; i < 6; i++) MG.Bolts[i] = false; MG.K = 0; UKAPAudio::Get(this)->Beep(false); }
			}
		}
		if (MG.Phase != 2) { Disc(C, 120, MG.Phase >= 3 ? FLinearColor(0.29f, 0.48f, 0.35f) : FLinearColor(0.42f, 0.35f, 0.23f)); Txt(C.X, C.Y - 12, MG.Phase >= 3 ? TEXT("ЖАҢА") : TEXT("ЕСКІ"), 20, Txtc, 1); }
		if (MG.Phase == 1) Txt(C.X, C.Y + 20, TEXT("басып шығарыңыз"), 14, Amber, 1);
		for (int32 i = 0; i < 6; i++)
		{
			Disc(Bolt(i), 26, MG.Bolts[i] ? FLinearColor(0.67f, 0.67f, 0.73f) : FLinearColor(0.2f, 0.2f, 0.2f));
			Line(Bolt(i) - FVector2D(12, 0), Bolt(i) + FVector2D(12, 0), FLinearColor::Black, 4);
			if (MG.Phase == 3 && MG.Order.IsValidIndex(MG.K) && MG.Order[MG.K] == i) Circle(Bolt(i), 34, Amber, 3);
			if (MG.Phase == 3) Txt(Bolt(i).X + 40, Bolt(i).Y - 26, FString::FromInt(MG.Order.IndexOfByKey(i) + 1), 13, Muted);
		}
		if (MG.Phase == 2) { Rect(X + W - 170, Y + H - 130, 140, 100, FLinearColor(0.29f, 0.48f, 0.35f)); Txt(X + W - 100, Y + H - 160, TEXT("Жаңа бөлшек"), 14, Amber, 1); }
		static const TCHAR* PhN[5] = { TEXT("Болттарды бұрап алу"), TEXT("Ескі бөлшекті шығару"), TEXT("Жаңа бөлшекті салу"), TEXT("Жұлдызша ретімен бекіту"), TEXT("Дайын") };
		Txt(VW / 2, Y + 16, PhN[MG.Phase], 18, Amber, 1);
		if (MG.MsgT > 0) { MG.MsgT -= Dt; Txt(VW / 2, Y + H - 36, MG.Msg, 16, CRed, 1); }
		bDone = MG.Phase == 4;
	}
	else if (MG.Kind == TEXT("pressure"))
	{
		Hint = TEXT("Space немесе тышқанды басып ұстап қысымды көтеріңіз. Жасыл аймақта 5 секунд ұстаңыз");
		MG.B = FMath::Clamp(MG.B + FMath::FRandRange(-1.f, 1.f) * Dt * 2.f, -0.4f, 0.4f);
		MG.A = FMath::Clamp(MG.A + ((bDown || bSpaceDown) ? 0.55f : -0.42f) * Dt + MG.B * Dt * 0.5f, 0.f, 1.f);
		const float Z0 = 0.45f, Z1 = 0.62f;
		const bool bOk = MG.A > Z0 && MG.A < Z1;
		if (bOk) MG.T += Dt;
		if (MG.A >= 0.98f) MG.T = FMath::Max(0.f, MG.T - Dt * 3);
		const FVector2D G(C.X, C.Y + 80);
		for (int32 i = 0; i < 40; i++)
		{
			const float A0 = UE_PI + i / 40.f * UE_PI, A1 = UE_PI + (i + 1) / 40.f * UE_PI, F = i / 40.f;
			const FLinearColor Col = (F > Z0 && F < Z1) ? FLinearColor(0.18f, 0.49f, 0.29f) : F > 0.88f ? FLinearColor(0.49f, 0.18f, 0.18f) : FLinearColor(0.2f, 0.2f, 0.2f);
			Line(G + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * 220, G + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * 220, Col, 26);
		}
		const float An = UE_PI + MG.A * UE_PI;
		Line(G, G + FVector2D(FMath::Cos(An), FMath::Sin(An)) * 205, FLinearColor::White, 5);
		Txt(G.X, G.Y + 20, FString::Printf(TEXT("%.2f МПа"), MG.A * 8.f), 24, bOk ? CGreen : Txtc, 1);
		Txt(G.X, Y + H - 80, bOk ? TEXT("Тұрақты!") : MG.A > Z1 ? TEXT("Тым жоғары") : TEXT("Тым төмен"), 16, bOk ? CGreen : COrange, 1);
		Rect(X + 120, Y + H - 40, W - 240, 14, FLinearColor(0.15f, 0.15f, 0.15f)); Rect(X + 120, Y + H - 40, (W - 240) * FMath::Min(1.f, MG.T / 5.f), 14, CGreen);
		bDone = MG.T >= 5;
	}
	else if (MG.Kind == TEXT("fire"))
	{
		Hint = TEXT("Жалындарды өсіп кетпей тұрып басып сөндіріңіз (12 жалын)");
		MG.A += Dt;
		const bool bClick = bDown && MG.Last < -900;
		MG.Last = bDown ? 0.f : -999.f;
		if (MG.Flames.Num() < 5 && FMath::FRand() < Dt * 2.2f) MG.Flames.Add(FVector4f(X + 80 + FMath::FRand() * (W - 160), Y + 90 + FMath::FRand() * (H - 200), 0.2f, 0));
		int32 Big = 0;
		for (FVector4f& F : MG.Flames) { F.Z = FMath::Min(1.4f, F.Z + Dt * 0.25f); if (F.Z >= 1.4f) Big++; }
		if (Big >= 3) { MG.K = FMath::Max(0, MG.K - 1); for (FVector4f& F : MG.Flames) F.Z = 0.6f; }
		if (bClick) for (int32 i = MG.Flames.Num() - 1; i >= 0; i--) if (FVector2D::Distance(M, FVector2D(MG.Flames[i].X, MG.Flames[i].Y)) < 24 + MG.Flames[i].Z * 40) { MG.Flames.RemoveAt(i); MG.K++; UKAPAudio::Get(this)->Play(EKAPSound::Steam, 0.4f); break; }
		for (const FVector4f& F : MG.Flames)
		{
			const float R = 24 + F.Z * 40;
			Disc(FVector2D(F.X, F.Y), R, FLinearColor(1, 0.35f, 0, 0.55f));
			Disc(FVector2D(F.X, F.Y - R * 0.2f), R * 0.6f, FLinearColor(1, 0.75f, 0.2f, 0.8f));
			Disc(FVector2D(F.X, F.Y - R * 0.3f), R * 0.25f, FLinearColor(1, 0.97f, 0.7f, 0.9f));
		}
		Rect(X + 120, Y + H - 40, W - 240, 14, FLinearColor(0.15f, 0.15f, 0.15f)); Rect(X + 120, Y + H - 40, (W - 240) * FMath::Min(1.f, MG.K / 12.f), 14, CBlue);
		Txt(VW / 2, Y + H - 70, FString::Printf(TEXT("Сөндірілді: %d / 12"), MG.K), 16, Txtc, 1);
		bDone = MG.K >= 12;
	}
	Txt(VW / 2, Y + H + 18, Hint, 15, FLinearColor(0.8f, 0.74f, 0.61f), 1);
	if (bDone) MGStepDone();
}
