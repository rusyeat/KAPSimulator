// AKAPHUD — барлық интерфейс Canvas арқылы: HUD (көрсеткіштер, компас, миникарта), хабарламалар, субтитр,
// планшет (UTabletWidget функциялары: монитор, жұмысшылар, карта/навигатор, тапсырмалар, ауа райы, рұқсаттар,
// инвентарь, CSP бригадалары, AI мониторы, жүйе), диалогтар, жөндеу мини-ойындары, мәзірлер, титр
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAPHUD.generated.h"

class UTexture2D;

UCLASS()
class KAPSIMULATOR_API AKAPHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	static AKAPHUD* Get(const UObject* Ctx);

	void AddToast(const FString& Msg, FName Kind);
	void SetSubtitle(const FString& Kk, const FString& Ru, const FString& Who, float Dur);
	void ShowDialog(const FString& Name, const FString& Text, TArray<FKAPDialogOption> Opts);
	void StartMinigame(const FString& Title, const TArray<FString>& Steps, TFunction<void(bool)> OnDone);
	void ToggleTablet();
	void CloseAll();
	bool IsModal() const;
	void Click(FVector2D Pos, bool bDown);
	bool HandleKey(FName K, bool bDown);
	void Scroll(float D) { TabletScroll = FMath::Max(0.f, TabletScroll - D * 60.f); }
	float Fade = 0.f;

private:
	// ---- сурет салу көмекшілері ----
	void Rect(float X, float Y, float W, float H, FLinearColor C);
	void Frame(float X, float Y, float W, float H, FLinearColor C, float T = 2.f);
	void Line(FVector2D A, FVector2D B, FLinearColor C, float T = 2.f);
	void Circle(FVector2D C, float R, FLinearColor Col, float T = 2.f, int32 Seg = 24);
	void Disc(FVector2D C, float R, FLinearColor Col);
	FVector2D Measure(const FString& S, float Size) const;
	float Txt(float X, float Y, const FString& S, float Size, FLinearColor C = FLinearColor::White, int32 Align = 0, bool bShadow = true);
	float Para(float X, float Y, float W, const FString& S, float Size, FLinearColor C = FLinearColor::White);
	bool Button(float X, float Y, float W, float H, const FString& Label, TFunction<void()> Fn, bool bEnabled = true, bool bPrimary = false, float Size = 15.f);
	struct FBtn { FBox2D R; TFunction<void()> Fn; };
	TArray<FBtn> Btns;
	FVector2D Mouse;
	float Sc = 1.f; // экран масштабы

	// ---- бөлімдер ----
	void DrawGameplay(float Dt);
	void DrawMinimap(float X, float Y, float R);
	void DrawToasts(float Dt);
	void DrawSubtitle(float Dt);
	void DrawMenu();
	void DrawPause();
	void DrawDeath();
	void DrawCredits();
	void DrawDialog();
	void DrawTablet();
	void TabWells(float X, float Y, float W, float H);
	void TabWorkers(float X, float Y, float W, float H);
	void TabMap(float X, float Y, float W, float H);
	void TabQuests(float X, float Y, float W, float H);
	void TabWeather(float X, float Y, float W, float H);
	void TabPermits(float X, float Y, float W, float H);
	void TabInv(float X, float Y, float W, float H);
	void TabBrig(float X, float Y, float W, float H);
	void TabAI(float X, float Y, float W, float H);
	void TabSys(float X, float Y, float W, float H);
	void DrawMinigame(float Dt);
	UTexture2D* MapTexture();

	// ---- күй ----
	struct FToast { FString Msg; FName Kind; float T; };
	TArray<FToast> Toasts;
	FString SubKk, SubRu, SubWho; float SubT = 0;
	bool bDialog = false; FString DName, DText; TArray<FKAPDialogOption> DOpts;
	bool bTablet = false; int32 Tab = 0; int32 SelWell = 0; float TabletScroll = 0; bool bLayerDanger = true, bLayerHeat = true;
	bool bHelp = false;
	UPROPERTY() TObjectPtr<UTexture2D> MapTex;
	// мини-ойын
	struct FMG
	{
		bool bActive = false; FString Title; TArray<FString> Steps; int32 I = 0; TFunction<void(bool)> OnDone; FString Item;
		FString Kind; float A = 0, B = 0, T = 0, Last = -999; int32 Phase = 0, K = 0; TArray<int32> Order, Right; TArray<bool> Done, Bolts; int32 Drag = -1; float Spark = 0; FString Msg; float MsgT = 0;
		TArray<FVector4f> Flames;
	} MG;
	bool bMouseDown = false, bSpaceDown = false;
	void MGStartStep();
	void MGStepDone();
	void MGFinish(bool bOk);
	FBox2D MGArea;
	double LastTime = 0;
};
