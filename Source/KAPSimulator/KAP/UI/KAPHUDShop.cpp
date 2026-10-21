// Экономика интерфейсі: қару-жарақ дүкені, жұмыс тақтасы, планшеттегі «Жұмыстар», ақша көрсеткіші;
// жаңа басты мәзір (кинематографиялық фон, радиация белгісі, радио жаңалықтары) және параметрлер.
#include "KAPHUD.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Player/NavigatorComponent.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/Audio/KAPAudio.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "TextureResource.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Scalability.h"
#include "AudioDevice.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	const FLinearColor Amber(0.95f, 0.76f, 0.3f), Panel(0.07f, 0.06f, 0.045f, 0.8f), Muted(0.55f, 0.51f, 0.44f), Txtc(0.94f, 0.9f, 0.82f);
	const FLinearColor CGreen(0.37f, 0.83f, 0.54f), CRed(1.f, 0.35f, 0.29f), CBlue(0.44f, 0.71f, 1.f), COrange(1.f, 0.54f, 0.23f);
	const FLinearColor Card(0.115f, 0.115f, 0.105f, 0.98f), Win(0.055f, 0.058f, 0.062f, 0.97f);
	const FLinearColor CatCol[4] = { CRed, Amber, CGreen, CBlue };
}

FString AKAPHUD::Money(int32 V)
{
	FString S = FString::FromInt(FMath::Abs(V)), Out;
	for (int32 i = 0; i < S.Len(); i++) { if (i > 0 && (S.Len() - i) % 3 == 0) Out += TEXT(" "); Out += S[i]; }
	return (V < 0 ? TEXT("−") : TEXT("")) + Out + TEXT(" ₸");
}

// ---------------- ақша көрсеткіші (өзгергенде 4 с, Q ұстағанда) ----------------
void AKAPHUD::DrawMoney(float Dt)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GM) return;
	const int32 M = GM->S().Money;
	if (LastMoney >= 0 && M != LastMoney) { MoneyDelta = M - LastMoney; MoneyT = 4.f; }
	LastMoney = M;
	MoneyT -= Dt;
	const float A = FMath::Clamp(bStatusHeld ? 1.f : MoneyT, 0.f, 1.f);
	if (A <= 0.01f) return;
	UiA = A;
	// сол жақта, тапсырма панелінің астында (оң жақтағы хабарламалармен қабаттаспайды)
	Rect(20, 92, 280, 40, Panel); Rect(20, 92, 4, 40, CGreen);
	Txt(290, 99, Money(M), 21, Txtc, 2);
	Txt(34, 102, TEXT("Әмиян"), 14, Muted);
	if (MoneyT > 0.f && MoneyDelta != 0) Txt(290, 140 - (4.f - MoneyT) * 4.f, (MoneyDelta > 0 ? TEXT("+") : TEXT("")) + Money(MoneyDelta), 16, (MoneyDelta > 0 ? CGreen : CRed) * FLinearColor(1, 1, 1, FMath::Clamp(MoneyT, 0.f, 1.f)), 2);
	UiA = 1.f;
}

// ---------------- дүкен ----------------
void AKAPHUD::DrawShop()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!GM || !D) { bShop = false; return; }
	FKAPSession& S = GM->S();
	const float VW = Canvas->ClipX / Sc;
	const bool bArmy = ShopKind == "army";
	Rect(0, 0, VW, 1080, FLinearColor(0, 0, 0, 0.6f));
	const float W = 1260, H = 820, X = (VW - W) / 2, Y = 120;
	Rect(X, Y, W, H, Win); Frame(X, Y, W, H, FLinearColor(Amber.R, Amber.G, Amber.B, 0.35f), 1.5f);
	Rect(X, Y, W, 74, FLinearColor(0.1f, 0.095f, 0.08f, 1.f)); Rect(X, Y + 74, W, 2, Amber * FLinearColor(1, 1, 1, 0.6f));
	Txt(X + 28, Y + 14, bArmy ? TEXT("ӘСКЕРИ ҚОЙМА") : TEXT("ҚАРУ-ЖАРАҚ ДҮКЕНІ"), 28, Amber);
	Txt(X + 30, Y + 50, bArmy ? TEXT("Майор Сейітов · қару мен оқ-дәрі 20% арзан") : TEXT("Сатушы Ерболат · Ақсай ауылы"), 14, Muted);
	Txt(X + W - 28, Y + 14, Money(S.Money), 28, CGreen, 2);
	Txt(X + W - 28, Y + 50, S.bPermitWeapon ? TEXT("Қару рұқсаты: бар") : TEXT("Қару рұқсаты: жоқ (атыс қаруын сатуға болмайды)"), 14, S.bPermitWeapon ? CGreen : COrange, 2);
	// санаттар
	static const TCHAR* Cats[5] = { TEXT("Қару"), TEXT("Оқ-дәрі"), TEXT("Дәрі мен азық"), TEXT("Жабдық"), TEXT("Сату") };
	float Tx = X + 24;
	for (int32 c = 0; c < 5; c++)
	{
		if (bArmy && (c == 2 || c == 4)) continue;
		const FString L = Cats[c];
		const float Tw = Measure(L, 16).X + 40;
		Button(Tx, Y + 92, Tw, 38, L, [this, c]() { ShopCat = c; }, true, ShopCat == c, 16);
		Tx += Tw + 8;
	}
	const float GX = X + 24, GY = Y + 148, CW = (W - 48 - 2 * 16) / 3.f, CH = 132;
	if (ShopCat == 4)
	{
		const int32 N = S.Inv.FindRef("scrap");
		Rect(GX, GY, W - 48, 140, Card); Rect(GX, GY, 5, 140, Amber);
		Txt(GX + 22, GY + 16, TEXT("Металл сынығы"), 22, Txtc);
		Para(GX + 22, GY + 52, 700, FString::Printf(TEXT("Сізде: %d дана · бір данасы 70 ₸. Сынықты «Металл сынығын жинау» жұмысында, қираған көліктер мен ауылдардан табасыз."), N), 15, Muted);
		Button(GX + W - 48 - 260, GY + 46, 236, 48, FString::Printf(TEXT("Бәрін сату · %s"), *Money(N * 70)), [GM]() { GM->SellScrap(); }, N > 0, true, 16);
	}
	else
	{
		int32 K = 0;
		for (const AKAPGameMode::FShopItem& It : AKAPGameMode::ShopItems())
		{
			if (It.Cat != ShopCat) continue;
			if (bArmy && It.Cat == 3 && It.Id != "armor" && It.Id != "fuel") continue;
			const float Cx = GX + (K % 3) * (CW + 16), Cy = GY + (K / 3) * (CH + 14);
			K++;
			const FKAPWeaponRow* Wr = D->Weapon(It.Id);
			const bool bWeapon = Wr && It.Cat == 0;
			FString Title, Sub;
			if (bWeapon)
			{
				Title = Wr->Title;
				Sub = Wr->Kind == "melee" ? FString::Printf(TEXT("Суық қару · зақым %d"), FMath::RoundToInt32(Wr->Damage))
					: Wr->Kind == "throw" ? FString::Printf(TEXT("Лақтыру · %d дана"), It.Qty)
					: FString::Printf(TEXT("Зақым %d · оқ %d · %d м"), FMath::RoundToInt32(Wr->Damage * Wr->Pellets), Wr->Mag, FMath::RoundToInt32(Wr->Range));
			}
			else if (It.Cat == 1) { Title = D->AmmoName(It.Id); Sub = FString::Printf(TEXT("Бір қорап · %d дана"), It.Qty); }
			else { Title = D->ItemName(It.Id); if (const FKAPItemRow* Ir = D->Items->FindRow<FKAPItemRow>(It.Id, TEXT("KAP"), false)) Sub = Ir->Desc; }
			const int32 Have = bWeapon ? (S.Weapons.Contains(It.Id) ? 1 : 0) : It.Cat == 1 ? S.Ammo.FindRef(It.Id) : S.Inv.FindRef(It.Id);
			FString Why;
			const bool bCan = GM->CanBuy(It, ShopKind, Why);
			const bool bHover = FBox2D(FVector2D(Cx, Cy), FVector2D(Cx + CW, Cy + CH)).IsInside(Mouse);
			Rect(Cx, Cy, CW, CH, bHover ? FLinearColor(0.15f, 0.145f, 0.13f, 1.f) : Card);
			Rect(Cx, Cy, 4, CH, CatCol[FMath::Clamp(It.Cat, 0, 3)]);
			Txt(Cx + 18, Cy + 12, Title, 19, Txtc);
			Para(Cx + 18, Cy + 42, CW - 36, Sub, 13, Muted);
			if (Have > 0) Txt(Cx + 18, Cy + CH - 30, bWeapon ? TEXT("Сізде бар") : FString::Printf(TEXT("Сізде: %d"), Have), 13, CBlue);
			Txt(Cx + CW - 16, Cy + 12, Money(GM->ShopPrice(It, ShopKind)), 19, Amber, 2);
			const AKAPGameMode::FShopItem Copy = It; const FName Kd = ShopKind;
			Button(Cx + CW - 156, Cy + CH - 44, 140, 32, bCan ? TEXT("Сатып алу") : Why, [GM, Copy, Kd]() { GM->Buy(Copy, Kd); }, bCan, bCan, 14);
		}
	}
	Txt(X + 28, Y + H - 34, TEXT("Esc — жабу · атыс қаруы мен оқ-дәрі үшін кеніш күзетінің рұқсаты керек"), 13, Muted);
	Button(X + W - 160, Y + H - 50, 136, 36, TEXT("Жабу"), [this]() { bShop = false; }, true, false, 15);
}

// ---------------- жұмыс тақтасы ----------------
void AKAPHUD::DrawJobs()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	if (!GM || !P) { bJobs = false; return; }
	GM->RefreshContracts(false);
	const float VW = Canvas->ClipX / Sc;
	Rect(0, 0, VW, 1080, FLinearColor(0, 0, 0, 0.6f));
	const float W = 1180, H = 830, X = (VW - W) / 2, Y = 115;
	Rect(X, Y, W, H, Win); Frame(X, Y, W, H, FLinearColor(Amber.R, Amber.G, Amber.B, 0.35f), 1.5f);
	Rect(X, Y, W, 74, FLinearColor(0.1f, 0.095f, 0.08f, 1.f)); Rect(X, Y + 74, W, 2, Amber * FLinearColor(1, 1, 1, 0.6f));
	const bool bPol = JobsBoard == "police", bArmy = JobsBoard == "army";
	Txt(X + 28, Y + 14, bPol ? TEXT("ПОЛИЦИЯ · ЖҰМЫСТАР") : bArmy ? TEXT("ӘСКЕРИ ТАПСЫРМАЛАР") : TEXT("ЕРБОЛАТТЫҢ ТАПСЫРМАЛАРЫ"), 28, bPol ? CBlue : bArmy ? COrange : Amber);
	Txt(X + 30, Y + 50, TEXT("Жаңа ұсыныстар әр таң сайын жаңарады · бір мезгілде ең көбі 3 жұмыс"), 14, Muted);
	Txt(X + W - 28, Y + 20, Money(GM->S().Money), 26, CGreen, 2);
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	const int32 NActive = GM->ContractsOf(NAME_None, 1).Num();
	float Cy = Y + 96;
	Txt(X + 28, Cy, TEXT("Ұсыныстар"), 18, Txtc); Cy += 32;
	TArray<FKAPContract*> Offers = GM->ContractsOf(JobsBoard, 0);
	if (!Offers.Num()) { Txt(X + 28, Cy, TEXT("Бүгінге жұмыс қалмады. Ертең келіңіз."), 15, Muted); Cy += 40; }
	for (FKAPContract* C : Offers)
	{
		const float CH = 118;
		Rect(X + 24, Cy, W - 48, CH, Card); Rect(X + 24, Cy, 4, CH, bPol ? CBlue : bArmy ? COrange : Amber);
		Txt(X + 44, Cy + 12, C->Title, 20, Txtc);
		Para(X + 44, Cy + 42, W - 420, C->Desc, 14, Muted);
		const int32 Dm = C->Points.Num() ? FMath::RoundToInt32(FVector2D::Distance(Pm, C->Points[0])) : 0;
		Txt(X + W - 44, Cy + 12, Money(C->Reward), 22, Amber, 2);
		Txt(X + W - 44, Cy + 44, FString::Printf(TEXT("≈ %d м"), Dm), 14, Muted, 2);
		const int32 Uid = C->Uid;
		Button(X + W - 214, Cy + CH - 46, 170, 34, NActive >= 3 ? TEXT("Орын жоқ (3/3)") : TEXT("Қабылдау"), [GM, Uid]() { GM->AcceptContract(Uid); }, NActive < 3, true, 15);
		Cy += CH + 12;
	}
	Cy += 8;
	Txt(X + 28, Cy, FString::Printf(TEXT("Белсенді жұмыстар (%d/3)"), NActive), 18, Txtc); Cy += 30;
	for (FKAPContract* C : GM->ContractsOf(NAME_None, 1))
	{
		Rect(X + 24, Cy, W - 48, 52, Card);
		Txt(X + 44, Cy + 8, C->Title, 16, GM->S().Tracked == C->Uid ? Amber : Txtc);
		Txt(X + 44, Cy + 30, GM->ContractProgress(*C), 13, Muted);
		Txt(X + W - 430, Cy + 16, Money(C->Reward), 16, Amber, 2);
		const int32 Uid = C->Uid;
		Button(X + W - 404, Cy + 10, 170, 32, TEXT("Бағыттау"), [GM, Uid]() { GM->TrackContract(Uid); }, true, GM->S().Tracked == Uid, 14);
		Button(X + W - 224, Cy + 10, 180, 32, TEXT("Бас тарту"), [GM, Uid]() { GM->AbandonContract(Uid); }, true, false, 14);
		Cy += 60;
	}
	Txt(X + 28, Y + H - 34, TEXT("Esc — жабу · белсенді жұмыстар планшеттің «Жұмыстар ₸» бетінде де бар"), 13, Muted);
	Button(X + W - 160, Y + H - 50, 136, 36, TEXT("Жабу"), [this]() { bJobs = false; }, true, false, 15);
}

// ---------------- планшет: Жұмыстар ----------------
void AKAPHUD::TabJobs(float X, float Y, float W, float H)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	if (!GM || !P) return;
	const FKAPSession& S = GM->S();
	Txt(X, Y, TEXT("Әмиян"), 16, Muted); Txt(X + 80, Y - 4, Money(S.Money), 24, CGreen);
	Txt(X + 320, Y + 2, TEXT("Ақша табу: бүйір жұмыстар, ұңғыма жөндеу (250 ₸), бандит/мутант сыйақысы, сынық сату"), 13, Muted);
	float Cy = Y + 46;
	Txt(X, Cy, TEXT("Белсенді жұмыстар"), 18, Txtc); Cy += 30;
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	TArray<FKAPContract*> Act = GM->ContractsOf(NAME_None, 1);
	if (!Act.Num()) { Para(X, Cy, W, TEXT("Белсенді жұмыс жоқ. Жұмысты Ақсай полициясының капитанынан, Ерболаттың дүкенінен немесе әскери аймақ қақпасындағы майордан алыңыз."), 14, Muted); Cy += 60; }
	for (FKAPContract* C : Act)
	{
		Rect(X, Cy, W, 86, Card); Rect(X, Cy, 4, 86, C->Board == "police" ? CBlue : C->Board == "army" ? COrange : Amber);
		Txt(X + 18, Cy + 10, C->Title, 18, S.Tracked == C->Uid ? Amber : Txtc);
		Txt(X + 18, Cy + 38, GM->ContractProgress(*C), 14, Muted);
		const int32 Dm = C->Points.Num() ? FMath::RoundToInt32(FVector2D::Distance(Pm, C->Points[C->Type == "patrol" ? FMath::Clamp(C->Have, 0, C->Points.Num() - 1) : 0])) : 0;
		Txt(X + 18, Cy + 60, FString::Printf(TEXT("%s · ≈ %d м"), *C->TargetName, Dm), 13, Muted);
		Txt(X + W - 420, Cy + 12, Money(C->Reward), 20, Amber, 2);
		const int32 Uid = C->Uid;
		Button(X + W - 390, Cy + 26, 180, 34, TEXT("Бағыттау"), [GM, Uid]() { GM->TrackContract(Uid); }, true, S.Tracked == Uid, 14);
		Button(X + W - 200, Cy + 26, 186, 34, TEXT("Бас тарту"), [GM, Uid]() { GM->AbandonContract(Uid); }, true, false, 14);
		Cy += 96;
	}
	Cy += 10;
	Txt(X, Cy, TEXT("Бүгін орындалған"), 16, Txtc); Cy += 26;
	for (FKAPContract* C : GM->ContractsOf(NAME_None, 2)) { Txt(X + 6, Cy, FString::Printf(TEXT("✓ %s · %s"), *C->Title, *Money(C->Reward)), 14, CGreen); Cy += 22; }
}

// ---------------- басты мәзір ----------------
bool AKAPHUD::MenuItem(float X, float Y, const FString& Label, TFunction<void()> Fn, bool bEnabled)
{
	const FBox2D Bx(FVector2D(X - 24, Y - 6), FVector2D(X + 440, Y + 44));
	const bool bHover = bEnabled && Bx.IsInside(Mouse);
	if (bHover) { Rect(X - 24, Y - 4, 4, 42, Amber); Rect(X - 20, Y - 4, 440, 42, FLinearColor(Amber.R, Amber.G, Amber.B, 0.07f)); }
	Txt(X + (bHover ? 10.f : 0.f), Y, Label, 26, !bEnabled ? FLinearColor(0.4f, 0.38f, 0.34f, 0.7f) : bHover ? Amber : Txtc, 0, true);
	if (bEnabled && Fn) Btns.Add({ Bx, Fn });
	return bHover;
}

void AKAPHUD::Trefoil(FVector2D C, float R, float Ang, FLinearColor Col)
{
	// радиация белгісі: үш 60° сектор + орталық дөңгелек
	Col.A *= UiA;
	TArray<FCanvasUVTri> Tris;
	auto Pt = [&](float A, float Rr) { return (C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr) * Sc; };
	for (int32 k = 0; k < 3; k++)
	{
		const float A0 = Ang + k * UE_TWO_PI / 3.f - UE_PI / 6.f;
		const int32 Seg = 10;
		for (int32 i = 0; i < Seg; i++)
		{
			const float a = A0 + (UE_PI / 3.f) * i / Seg, b = A0 + (UE_PI / 3.f) * (i + 1) / Seg;
			FCanvasUVTri T1; T1.V0_Pos = Pt(a, R * 0.24f); T1.V1_Pos = Pt(a, R); T1.V2_Pos = Pt(b, R); T1.V0_Color = T1.V1_Color = T1.V2_Color = Col; Tris.Add(T1);
			FCanvasUVTri T2; T2.V0_Pos = Pt(a, R * 0.24f); T2.V1_Pos = Pt(b, R); T2.V2_Pos = Pt(b, R * 0.24f); T2.V0_Color = T2.V1_Color = T2.V2_Color = Col; Tris.Add(T2);
		}
	}
	FCanvasTriangleItem TI(Tris, GWhiteTexture); TI.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(TI);
	Disc(C, R * 0.15f, Col);
}

void AKAPHUD::DrawMenu()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	const float VW = Canvas->ClipX / Sc;
	const float T = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	// сол жақтан қараңғылау градиенті (оқылуы үшін), жоғарғы/төменгі жолақтар, шеттері қараңғы
	{
		// тегіс градиент: төбе түстері бар үшбұрыштар (жолақсыз)
		TArray<FCanvasUVTri> G;
		auto Quad = [&](float X0, float X1, float A0, float A1)
		{
			const FLinearColor C0(0.015f, 0.012f, 0.01f, A0), C1(0.015f, 0.012f, 0.01f, A1);
			const FVector2D P00(X0 * Sc, 0), P10(X1 * Sc, 0), P01(X0 * Sc, 1080 * Sc), P11(X1 * Sc, 1080 * Sc);
			FCanvasUVTri A; A.V0_Pos = P00; A.V1_Pos = P10; A.V2_Pos = P01; A.V0_Color = C0; A.V1_Color = C1; A.V2_Color = C0; G.Add(A);
			FCanvasUVTri B; B.V0_Pos = P10; B.V1_Pos = P11; B.V2_Pos = P01; B.V0_Color = C1; B.V1_Color = C1; B.V2_Color = C0; G.Add(B);
		};
		Quad(0, VW * 0.26f, 0.82f, 0.55f); Quad(VW * 0.26f, VW * 0.44f, 0.55f, 0.15f); Quad(VW * 0.44f, VW * 0.62f, 0.15f, 0.f);
		FCanvasTriangleItem TI(G, GWhiteTexture); TI.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(TI);
	}
	Rect(0, 0, VW, 54, FLinearColor(0, 0, 0, 0.35f)); Rect(0, 1026, VW, 54, FLinearColor(0, 0, 0, 0.5f));
	// жыпылықтайтын шу жолағы (ескі камера/рация сезімі)
	const float Scan = FMath::Fmod(T * 90.f, 1200.f) - 60.f;
	Rect(0, Scan, VW, 3, FLinearColor(1, 0.9f, 0.7f, 0.025f));
	// радиация белгісі мен атау
	const float Flick = 0.9f + 0.1f * FMath::Sin(T * 23.f) * FMath::Sin(T * 3.1f);
	Trefoil(FVector2D(196, 238), 64, T * 0.12f, FLinearColor(Amber.R, Amber.G, Amber.B, 0.9f * Flick));
	Circle(FVector2D(196, 238), 76, FLinearColor(Amber.R, Amber.G, Amber.B, 0.35f), 2, 48);
	Txt(292, 150, TEXT("KAP"), 128, FLinearColor(0.97f, 0.9f, 0.74f, Flick), 0, true);
	Txt(298, 296, TEXT("S  I  M  U  L  A  T  O  R"), 30, Amber, 0, true);
	Txt(298, 342, TEXT("Уран кеніші · 21 ұңғыма · бір инженер"), 19, Muted);
	Rect(298, 380, 460, 2, FLinearColor(Amber.R, Amber.G, Amber.B, 0.7f));
	// мәзір
	float Y = 430;
	const bool bSave = GI && GI->HasSave();
	MenuItem(298, Y, TEXT("ЖАҢА ОЙЫН"), [GM]() { GM->MenuNewGame(); }); Y += 60;
	MenuItem(298, Y, bSave ? TEXT("ЖАЛҒАСТЫРУ") : TEXT("ЖАЛҒАСТЫРУ — сақталған ойын жоқ"), [GM]() { GM->MenuContinue(); }, bSave); Y += 60;
	MenuItem(298, Y, TEXT("ПАРАМЕТРЛЕР"), [this]() { bSettings = !bSettings; bHelp = false; }); Y += 60;
	MenuItem(298, Y, TEXT("БАСҚАРУ"), [this]() { bHelp = !bHelp; bSettings = false; }); Y += 60;
	MenuItem(298, Y, TEXT("ШЫҒУ"), [this]() { UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false); });
	// оң жақ: рация жаңалықтары (әр 7 с ауысады)
	static const TCHAR* News[] = {
		TEXT("QazUran: №7 ұңғымада қысым түсті, жөндеуші-инженер шақырылды."),
		TEXT("Ақсай полициясы: жолдағы бандиттер үшін сыйақы — 80 ₸ бастап."),
		TEXT("Ауа райы: оңтүстік-батыстан құм дауылы жақындап келеді."),
		TEXT("Әскери аймаққа рұқсатсыз кіруге тыйым салынады. Күзет оқ атады."),
		TEXT("Ерболаттың дүкені: оқ-дәрі мен бронежилет келді. Сынық металл сатып аламыз."),
		TEXT("Жосалы: түнде құмды төбелер жақтан біртүрлі дыбыс естілді..."),
	};
	const int32 NN = UE_ARRAY_COUNT(News);
	const float Ph = T / 7.f; const int32 Ni = FMath::FloorToInt32(Ph) % NN; const float Fr = FMath::Frac(Ph);
	const float NA = FMath::Clamp(FMath::Min(Fr * 4.f, (1.f - Fr) * 4.f), 0.f, 1.f);
	if (!bSettings && !bHelp)
	{
		const float Rx = 298, Ry = 772; // сол жақта, мәзірдің астында (оң жақта — сахнадағы инженер)
		Rect(Rx, Ry, 520, 120, FLinearColor(0.04f, 0.035f, 0.03f, 0.7f)); Rect(Rx, Ry, 4, 120, CRed);
		Disc(FVector2D(Rx + 28, Ry + 26), 5, FLinearColor(1, 0.2f, 0.15f, 0.5f + 0.5f * FMath::Sin(T * 5.f)));
		Txt(Rx + 42, Ry + 15, TEXT("РАДИО · 103.2 FM"), 14, CRed);
		UiA = NA; Para(Rx + 24, Ry + 46, 470, News[Ni], 17, Txtc); UiA = 1.f;
	}
	if (bHelp)
	{
		const float Px = VW * 0.55f, Py = 380;
		Rect(Px, Py, 620, 420, FLinearColor(0.04f, 0.035f, 0.03f, 0.85f)); Rect(Px, Py, 4, 420, Amber);
		Txt(Px + 26, Py + 18, TEXT("БАСҚАРУ"), 22, Amber);
		Para(Px + 26, Py + 60, 570, TEXT("WASD — жүру · Shift — жүгіру · Space — секіру\nТышқан — қарау · СТБ — ату/соғу · ОТБ — нысана\nE — әрекет (есік, пульт, көлік, сөйлесу) · R — оқтау\nF — фонарь · H — аптечка · J — су · K — тамақ\n1–9 — қару · Tab — планшет · M — карта · Q — көрсеткіштер мен әмиян\nКөлікте: W/S — газ/тежеу · A/D — руль · Space — қол тежегіш · V — камера\nEsc — үзіліс · F9 — уақыт жылдамдығы"), 16, Txtc);
	}
	if (bSettings) DrawSettings(VW * 0.55f, 380);
	// төменгі ақпарат
	Para(298, 960, 820, TEXT("Оқиғалар мен кейіпкерлер ойдан шығарылған. «QazUran» — ойдан шығарылған компания. Мутациялар мен монстрлар — көркем ойдан шығару, нақты ISR өндірісінде мұндай нәрсе жоқ."), 12, FLinearColor(0.6f, 0.56f, 0.5f, 0.8f));
	Txt(VW - 40, 1042, FString::Printf(TEXT("v0.9 · Unreal Engine 5 · Q-кесте %d мс"), FMath::RoundToInt32(AKAPNPC::TrainMs)), 12, FLinearColor(0.6f, 0.56f, 0.5f, 0.8f), 2);
}

void AKAPHUD::DrawSettings(float X, float Y)
{
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	if (!GI) return;
	Rect(X, Y, 620, 440, FLinearColor(0.04f, 0.035f, 0.03f, 0.85f)); Rect(X, Y, 4, 440, Amber);
	Txt(X + 26, Y + 18, TEXT("ПАРАМЕТРЛЕР"), 22, Amber);
	float Ry = Y + 70;
	// графика сапасы
	Txt(X + 26, Ry + 6, TEXT("Графика"), 17, Txtc);
	Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
	const int32 Cur = Q.GetSingleQualityLevel();
	static const TCHAR* QN[4] = { TEXT("Төмен"), TEXT("Орташа"), TEXT("Жоғары"), TEXT("Эпик") };
	for (int32 i = 0; i < 4; i++)
		Button(X + 200 + i * 100, Ry, 94, 34, QN[i], [i]() { Scalability::FQualityLevels L = Scalability::GetQualityLevels(); L.SetFromSingleQualityLevel(i); Scalability::SetQualityLevels(L); Scalability::SaveState(GGameUserSettingsIni); }, true, Cur == i, 14);
	Ry += 62;
	// тышқан сезімталдығы
	Txt(X + 26, Ry + 6, TEXT("Тышқан сезімталдығы"), 17, Txtc);
	Button(X + 300, Ry, 44, 34, TEXT("−"), [GI]() { GI->S.MouseSens = FMath::Clamp(GI->S.MouseSens - 0.1f, 0.3f, 3.f); }, true, false, 18);
	Txt(X + 392, Ry + 6, FString::Printf(TEXT("%.1f"), GI->S.MouseSens), 18, Amber, 1);
	Button(X + 440, Ry, 44, 34, TEXT("+"), [GI]() { GI->S.MouseSens = FMath::Clamp(GI->S.MouseSens + 0.1f, 0.3f, 3.f); }, true, false, 18);
	Ry += 62;
	// дыбыс
	const double Vol = GI->S.Flags.Contains("vol") ? GI->S.Flags["vol"] : 1.0;
	auto SetVol = [this, GI](double V)
	{
		GI->S.Flags.FindOrAdd("vol") = FMath::Clamp(V, 0.0, 1.0);
		if (UWorld* Wd = GetWorld()) if (FAudioDevice* AD = Wd->GetAudioDeviceRaw()) AD->SetTransientPrimaryVolume((float)GI->S.Flags["vol"]);
	};
	Txt(X + 26, Ry + 6, TEXT("Дыбыс қаттылығы"), 17, Txtc);
	Button(X + 300, Ry, 44, 34, TEXT("−"), [SetVol, Vol]() { SetVol(Vol - 0.1); }, true, false, 18);
	Txt(X + 392, Ry + 6, FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Vol * 100)), 18, Amber, 1);
	Button(X + 440, Ry, 44, 34, TEXT("+"), [SetVol, Vol]() { SetVol(Vol + 0.1); }, true, false, 18);
	Ry += 62;
	// субтитр, толық экран
	Txt(X + 26, Ry + 6, TEXT("Орысша субтитр"), 17, Txtc);
	Button(X + 300, Ry, 184, 34, GI->S.bRuSubtitles ? TEXT("ҚОСУЛЫ") : TEXT("өшірулі"), [GI]() { GI->S.bRuSubtitles = !GI->S.bRuSubtitles; }, true, GI->S.bRuSubtitles, 15);
	Ry += 62;
	UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const bool bFull = U && U->GetFullscreenMode() != EWindowMode::Windowed;
	Txt(X + 26, Ry + 6, TEXT("Толық экран"), 17, Txtc);
	Button(X + 300, Ry, 184, 34, bFull ? TEXT("ҚОСУЛЫ") : TEXT("өшірулі"), [U, bFull]() { if (U) { U->SetFullscreenMode(bFull ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen); U->ApplySettings(false); } }, U != nullptr, bFull, 15);
}
