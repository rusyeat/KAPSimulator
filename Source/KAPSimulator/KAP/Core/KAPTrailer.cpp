// 15 секундтық тизер-трейлер. Ойынның өз кинематика қозғалтқышы (StartCine): камера кілттері кодта,
// кино жолақтары мен жазулар — Slate қабатында (MRQ-дың UI рендері оны кадрға қосады), дыбыс — ойынның өз музыкасы мен эффектілері.
//   -kaptrailer       — Movie Render Queue рендері кезінде: уақыт MRQ ойнатып тұрған Level Sequence-тен алынады (кадрға дәл)
//   -kaptrailershots  — жай ойында: өз сағаты, әр сахнадан скриншот (KAPT_*.png), соңында ойын жабылады
// Сахналар: 0 — ұңғыма үстінен (күндіз), 1 — жұмысшылар, 2 — қара экран + жазу, 3 — цех апаты (түн, сирена),
//           4 — фонарь жарығындағы мутант, 5 — вагон есігін ұру, 6 — атау
#include "KAPGameMode.h"
#include "KAPGameInstance.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/WellActor.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/KAPDoor.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Audio/KAPAudio.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Font.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateColorBrush.h"
#include "AudioDevice.h"

namespace KAPTr
{
	// сахна шекаралары (секунд)
	constexpr float Shot2 = 3.0f, Card = 5.2f, Shot3 = 6.4f, Shot4 = 8.7f, Shot5 = 11.0f, Title = 12.8f, End = 15.0f;
	static const float Starts[7] = { 0.f, Shot2, Card, Shot3, Shot4, Shot5, Title };
	static const float Ends[7] = { Shot2, Card, Shot3, Shot4, Shot5, Title, End };
	static float Ease(float A) { A = FMath::Clamp(A, 0.f, 1.f); return A * A * (3.f - 2.f * A); }
	static float Ramp(float T, float A, float B) { return FMath::Clamp((T - A) / (B - A), 0.f, 1.f); }
	/** In0→In1 жұмсақ пайда болады, Out0→Out1 жұмсақ жоғалады */
	static float Window(float T, float In0, float In1, float Out0, float Out1) { return FMath::Min(Ramp(T, In0, In1), 1.f - Ramp(T, Out0, Out1)); }
	static int32 ShotAt(float T) { int32 S = 0; while (S < 6 && T >= Starts[S + 1]) S++; return S; }
	static FSlateFontInfo Font(int32 Size, bool bBold, int32 Spacing = 0)
	{
		static TWeakObjectPtr<UFont> F;
		if (!F.IsValid()) F = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		FSlateFontInfo I(F.Get(), Size, bBold ? FName("Bold") : FName("Regular"));
		I.LetterSpacing = Spacing;
		return I;
	}
}

void AKAPGameMode::StartTrailer(bool bShots)
{
	bTrailerShots = bShots;
	bTrailerMRQ = !bShots;
	UE_LOG(LogTemp, Display, TEXT("KAPTRAILER start (%s)"), bShots ? TEXT("shots") : TEXT("mrq"));
	// таймерлер GameInstance-те тұрады: MRQ картаны қайта жүктегенде ескі GameMode-тың таймері жаңа әлемде атылмасын (WeakLambda)
	FTimerHandle H0; GetWorldTimerManager().SetTimer(H0, FTimerDelegate::CreateWeakLambda(this, [this]() { MenuNewGame(); }), 0.2f, false);
	FTimerHandle H1; GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateWeakLambda(this, [this]() { if (Cine.IsSet()) SkipCinematic(); TrailerBegin(); }), 0.6f, false);
}

void AKAPGameMode::TrailerBegin()
{
	if (UKAPAudio* Au = UKAPAudio::Get(this)) { Au->StopSpeech(); Au->SetMenuMusic(false); Au->SetMusic(0.f, 0.f); Au->SetDread(0.f, 0.f); }
	Weather->bFastExposure = true;
	// бөгде құбыжықтар мен қарақшылар кадрға кірмесін
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
		if (It->Faction == EKAPFaction::Monster || It->Faction == EKAPFaction::Bandit) It->Destroy();
	if (AKAPCharacter* P = Player())
	{
		P->SetActorHiddenInGame(true); P->SetActorEnableCollision(false);
		P->GetCharacterMovement()->DisableMovement();
		if (P->Flashlight) P->Flashlight->SetIntensity(0.f);
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (PC->MyHUD) PC->MyHUD->bShowHUD = false;

	// Slate қабаты: қараңғылық, кино жолақтары (2.39:1), жазулар
	static FSlateColorBrush White(FLinearColor::White);
	const FLinearColor Ink(0.93f, 0.91f, 0.86f);
	auto A = [Ink](float K) { return FSlateColor(FLinearColor(Ink.R, Ink.G, Ink.B, K)); };
	const float BarH = 138.f;
	TrailerUI = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SBorder).BorderImage(&White).Padding(0.f).BorderBackgroundColor_Lambda([this]() { return FSlateColor(FLinearColor(0.f, 0.f, 0.f, TrBlack)); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top)
		[
			SNew(SBox).HeightOverride(BarH)[SNew(SBorder).BorderImage(&White).Padding(0.f).BorderBackgroundColor(FLinearColor::Black)]
		]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
		[
			SNew(SBox).HeightOverride(BarH)[SNew(SBorder).BorderImage(&White).Padding(0.f).BorderBackgroundColor(FLinearColor::Black)]
		]
		// сахна үстіндегі шағын жазу (жолақтың үстінде)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, BarH + 34.f)
		[
			SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(TrSmall); }).Font(KAPTr::Font(26, false, 120))
			.ColorAndOpacity_Lambda([this, A]() { return A(TrSmallA * 0.9f); }).ShadowOffset(FVector2D(1.5f, 1.5f)).ShadowColorAndOpacity_Lambda([this]() { return FLinearColor(0, 0, 0, TrSmallA * 0.8f); })
		]
		// қара экрандағы жазу
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(TrCard); }).Font(KAPTr::Font(46, false, 60)).Justification(ETextJustify::Center)
			.ColorAndOpacity_Lambda([this, A]() { return A(TrCardA); })
		]
		// атау
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("KAP SIMULATOR"))).Font(KAPTr::Font(96, true, 240))
				.ColorAndOpacity_Lambda([this, A]() { return A(TrTitleA); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(420.f).HeightOverride(3.f)
				[
					SNew(SBorder).BorderImage(&White).Padding(0.f).BorderBackgroundColor_Lambda([this]() { return FSlateColor(FLinearColor(0.75f, 0.08f, 0.04f, TrSubA)); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 22.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("ЖАҚЫНДА"))).Font(KAPTr::Font(30, false, 600))
				.ColorAndOpacity_Lambda([this, A]() { return A(TrSubA * 0.85f); })
			]
		];
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->AddViewportWidgetContent(TrailerUI.ToSharedRef(), 50);
	TrailerPrevT = -1.f; TrailerShotI = -1; TrailerFired.Reset();
	StartCine(1e6f, [this](float T, float Dt) { TrailerTick(T, Dt); }, nullptr);
	bCineSkippable = false;
}

void AKAPGameMode::TrailerEnterShot(int32 Shot)
{
	UWorld* W = GetWorld();
	UKAPAudio* Au = UKAPAudio::Get(this);
	UWellManagerSubsystem* WM = W->GetSubsystem<UWellManagerSubsystem>();
	AWellActor* Well = WM && WM->Wells.Num() ? WM->Wells[0].Actor.Get() : nullptr;
	auto Gz = [W](const FVector& P) { return KAPVis::GroundZ(W, P.X, P.Y, P.Z); };
	auto OnGround = [&](FVector P, float Up) { return FVector(P.X, P.Y, Gz(P) + Up); };
	auto Spawn = [&](ENPCKind Kind, FVector At, float Yaw, const TCHAR* Name) -> AKAPNPC*
	{
		AKAPNPC* N = SpawnNPC((int32)Kind, KAP::CM2M(At), Name, true);
		if (!N) return nullptr;
		N->SetActorLocation(OnGround(At, 94.f)); N->SetActorRotation(FRotator(0, Yaw, 0));
		TrailerNPCs.Add(N);
		return N;
	};
	auto ClearNPCs = [&]() { for (const TWeakObjectPtr<AKAPNPC>& N : TrailerNPCs) if (N.IsValid()) N->Destroy(); TrailerNPCs.Reset(); TrailerWalker = nullptr; TrailerMonster = nullptr; };
	// ойыншы денесі (көрінбейді) — камераның астында: жақын NPC-лер толық жаңарады (LOD)
	auto PawnTo = [&](FVector At) { if (AKAPCharacter* P = Player()) P->SetActorLocation(FVector(At.X, At.Y, Gz(At) - 800.f), false, nullptr, ETeleportType::TeleportPhysics); };
	TrSmall.Reset(); TrShake = 0.f;
	if (TrailerTorch) TrailerTorch->SetIntensity(0.f);
	UE_LOG(LogTemp, Display, TEXT("KAPTRAILER shot %d"), Shot);
	switch (Shot)
	{
	case 0: // ұңғыма үстінен: таңғы күн, ұзын көлеңкелер, дрон баяу жақындайды
	{
		Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->OverrideK = 1.f; Weather->bInstant = true;
		S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 24 + 9.6; // таңертең, бірақ төбелердің көлеңкесі тым ұзын емес
		Weather->Indoor = 0.f;
		if (Au) { Au->SetWind(0.38f); Au->SetIndoor(0.f); }
		const FVector C = Well ? Well->GetActorLocation() : FVector::ZeroVector;
		const FVector D0 = FVector(-0.78f, -0.62f, 0.f).GetSafeNormal(), D1 = D0.RotateAngleAxis(16.f, FVector::UpVector);
		TrA = C + D0 * 4300.f + FVector(0, 0, 900); TrB = C + D1 * 2500.f + FVector(0, 0, 480);
		TrLook = C + FVector(0, 0, 300); TrLookB = C + FVector(0, 0, 160); TrFov = 50.f;
		TrSmall = TEXT("QAZURAN УРАН КЕНІШІ  ·  №1 ҰҢҒЫМА");
		PawnTo(C);
		break;
	}
	case 1: // жұмысшылар: камера төменде, бүйірлей жылжиды, біреуі кадрдан өтіп кетеді
	{
		if (!Well) break;
		const FTransform Xf = Well->GetActorTransform();
		const FVector P = Well->GetInteractLocation(), F = Xf.GetUnitAxis(EAxis::X), R = Xf.GetUnitAxis(EAxis::Y); // пульт
		const FVector Ff = (P - Well->GetActorLocation()).GetSafeNormal2D(); // ұңғыма ортасынан пультке қарай
		const FVector Rr(-Ff.Y, Ff.X, 0.f);
		auto FaceYaw = [](FVector From, FVector To) { return (To - From).Rotation().Yaw; };
		// екеуі пульт алдында (бір-біріне сәл бұрылған); камера пульттің оң жағынан — беттері көрінеді
		const FVector W1 = P + Ff * 120.f - Rr * 70.f, W2 = P + Ff * 110.f + Rr * 90.f;
		if (AKAPNPC* N = Spawn(ENPCKind::Worker, W1, FaceYaw(W1, P + Rr * 60.f), TEXT("Жұмысшы"))) N->SetTool(true);
		if (AKAPNPC* N = Spawn(ENPCKind::Worker, W2, FaceYaw(W2, W1), TEXT("Жұмысшы"))) N->SetTool(true);
		const float G = Gz(P);
		TrA = P + Ff * 520.f + Rr * 640.f; TrA.Z = G + 150.f;
		TrB = P + Ff * 470.f + Rr * 420.f; TrB.Z = G + 140.f;
		TrLook = P + Ff * 110.f + Rr * 20.f; TrLook.Z = G + 130.f; TrLookB = P + Ff * 100.f - Rr * 20.f; TrLookB.Z = G + 125.f;
		TrFov = 40.f;
		PawnTo(P);
		UE_LOG(LogTemp, Display, TEXT("KAPTRAILER well panel=%s fwd=%s right=%s ff=%s"), *P.ToString(), *F.ToString(), *R.ToString(), *Ff.ToString());
		break;
	}
	case 2: // қара экран: «Бір түні сорғылар тоқтады.» — осы кезде түн, дауыл, №1 ұңғымада ақау (цех маягы мен сиренасы қосылады)
	{
		ClearNPCs();
		TrCard = TEXT("Бір түні сорғылар тоқтады.");
		Weather->bOverride = true; Weather->OverrideType = EWeatherType::Storm; Weather->OverrideK = 0.38f; Weather->bInstant = true;
		S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 23.4;
		if (S().Wells.Num()) { S().Wells[0].State = EWellState::Fault; }
		if (Au) Au->SetWind(0.5f);
		// музыка: «Бос үміт» бәсеңдейді, оның орнына қауіп әуені (Danger) кіреді
		if (TrailerMusic) TrailerMusic->AdjustVolume(0.8f, 0.3f);
		if (Au) if (USoundWave* Dw = Au->Pick("Danger")) { TrailerMusic2 = UGameplayStatics::CreateSound2D(W, Dw, 1.f, 1.f, 0.f, nullptr, true, false); if (TrailerMusic2) TrailerMusic2->FadeIn(0.9f, 1.f); }
		break;
	}
	case 3: // цех апаты: қызыл маяк айналады, сирена, дауыл шаңы; бір жұмысшы қашып барады
	{
		AKAPStructure* Ws = nullptr;
		const FVector C0 = Well ? Well->GetActorLocation() : FVector::ZeroVector;
		for (KAPReg::TIter<AKAPStructure> It(W, KAPReg::Structures()); It; ++It)
			if (It->Type == EStructType::Workshop && (!Ws || FVector::DistSquared2D(It->GetActorLocation(), C0) < FVector::DistSquared2D(Ws->GetActorLocation(), C0))) Ws = *It;
		if (!Ws) break;
		Ws->SirenGainMul = 3.f;
		const FTransform Xf = Ws->GetActorTransform();
		TrA = Xf.TransformPosition(FVector(-1050, 2300, 0)); TrA.Z = Gz(TrA) + 175.f;
		TrB = Xf.TransformPosition(FVector(-700, 1750, 0)); TrB.Z = Gz(TrB) + 160.f;
		TrLook = Xf.TransformPosition(FVector(150, 600, 330)); TrLookB = Xf.TransformPosition(FVector(220, 600, 360));
		TrFov = 46.f;
		const FVector Rs = Xf.TransformPosition(FVector(150, 900, 0));
		TrailerWalker = Spawn(ENPCKind::Worker, Rs, (Xf.TransformPosition(FVector(-1600, 1500, 0)) - Rs).Rotation().Yaw, TEXT("Жұмысшы"));
		TrailerWalkTo = KAP::CM2M(Xf.TransformPosition(FVector(-1600, 1500, 0)));
		PawnTo(TrB);
		break;
	}
	case 4: // түн: фонарь жарығында мутант камераға қарай келе жатыр; камера қолда, сәл шегінеді
	{
		ClearNPCs();
		const FVector C = Well ? Well->GetActorLocation() : FVector::ZeroVector;
		const FVector Away = FVector(0.55f, -0.83f, 0.f).GetSafeNormal();
		const FVector Cam = C + Away * 3200.f, ToWell = -Away;
		TrA = Cam; TrA.Z = Gz(Cam) + 160.f;
		TrB = Cam - ToWell * 70.f; TrB.Z = Gz(TrB) + 158.f;
		TrLook = Cam + ToWell * 1000.f; TrLook.Z = Gz(TrLook) + 120.f; TrLookB = TrLook + FVector(0, 0, 10);
		TrFov = 48.f; TrShake = 1.f;
		const FVector M = Cam + ToWell * 900.f;
		TrailerMonster = Spawn(ENPCKind::Irradiated, M, (Cam - M).Rotation().Yaw, TEXT("Мутант"));
		if (!TrailerTorch && CineCam)
		{
			TrailerTorch = NewObject<USpotLightComponent>(CineCam);
			TrailerTorch->SetupAttachment(CineCam->GetRootComponent());
			TrailerTorch->SetRelativeLocation(FVector(0, 18, -14));
			TrailerTorch->SetIntensityUnits(ELightUnits::Candelas);
			TrailerTorch->SetOuterConeAngle(24.f); TrailerTorch->SetInnerConeAngle(11.f); TrailerTorch->SetAttenuationRadius(4500.f);
			TrailerTorch->SetLightColor(FLinearColor(0.85f, 0.9f, 1.f)); TrailerTorch->SetVolumetricScatteringIntensity(3.f);
			TrailerTorch->SetCastShadows(true);
			TrailerTorch->RegisterComponent();
		}
		if (TrailerTorch) TrailerTorch->SetIntensity(2600.f);
		PawnTo(Cam);
		break;
	}
	case 5: // вагон іші: шам жыпылықтайды, біреу есікті ұрып жатыр
	{
		ClearNPCs();
		AKAPStructure* Wg = nullptr;
		if (Builder) if (const TWeakObjectPtr<AKAPStructure>* P = Builder->Places.Find("wagon1")) Wg = P->Get();
		if (!Wg) break;
		Wg->ForceFlicker(true);
		Weather->Indoor = 1.f;
		if (Au) Au->SetIndoor(1.f);
		AKAPDoor* D = Wg->Doors.Num() ? Wg->Doors[0].Get() : nullptr;
		const FTransform Xf = Wg->GetActorTransform();
		// бөлменің түкпірінен (төсек жанынан) есікке қарай баяу жақындау: шам, қабырға, есік — бәрі кадрда
		// төменнен (отырған адамның көзімен) есікке қарай: жоғарыда жыпылықтаған шам, оң жақта есік
		TrA = Xf.TransformPosition(FVector(-260, -95, 60 + 118)); TrB = Xf.TransformPosition(FVector(-170, -70, 60 + 116));
		TrLook = Xf.TransformPosition(FVector(150, 110, 210)); TrLookB = Xf.TransformPosition(FVector(150, 125, 200));
		TrFov = 70.f; TrShake = 0.35f;
		PawnTo(TrA);
		break;
	}
	case 6: // атау
	{
		ClearNPCs();
		// атау алдында кенет тыныштық (музыка кесіледі)
		if (TrailerMusic) { TrailerMusic->FadeOut(0.06f, 0.f); TrailerMusic = nullptr; }
		if (TrailerMusic2) { TrailerMusic2->FadeOut(0.06f, 0.f); TrailerMusic2 = nullptr; }
		if (Builder) if (const TWeakObjectPtr<AKAPStructure>* P = Builder->Places.Find("wagon1")) if (P->IsValid()) (*P)->ForceFlicker(false);
		break;
	}
	}
}

void AKAPGameMode::TrailerTick(float CineT, float Dt)
{
	using namespace KAPTr;
	// уақыт: MRQ кезінде — Level Sequence уақыты (жылыту кадрларында 0-де тұрады), әйтпесе — кинематика сағаты
	float T = CineT;
	if (bTrailerMRQ)
	{
		T = 0.f;
		for (TActorIterator<ALevelSequenceActor> It(GetWorld()); It; ++It)
			if (ULevelSequencePlayer* Sp = It->GetSequencePlayer()) { T = (float)Sp->GetCurrentTime().AsSeconds(); break; }
	}
	T = FMath::Clamp(T, 0.f, End);
	const float dT = TrailerPrevT < 0.f ? 0.f : FMath::Max(0.f, T - TrailerPrevT);
	TrailerPrevT = T;
	const int32 Sh = ShotAt(T);
	while (TrailerShotI < Sh) TrailerEnterShot(++TrailerShotI);
	UKAPAudio* Au = UKAPAudio::Get(this);
	auto Once = [&](int32 Id, float At, TFunctionRef<void()> Fn) { if (T >= At && !TrailerFired.Contains(Id)) { TrailerFired.Add(Id); if (Id < 100) UE_LOG(LogTemp, Display, TEXT("KAPTRAILER event %d at %.2f"), Id, T); Fn(); } };

	// камера: екі кілт арасында жұмсақ қозғалыс + қолдағы камераның дірілі
	{
		const int32 CamShot = Sh == 2 ? 3 : Sh; // қара экранда камера келесі сахнаның басында күтеді (Lumen үлгереді)
		const float K = Sh == 2 ? 0.f : Ease(Ramp(T, Starts[CamShot], Ends[CamShot]));
		const FVector Sk = TrShake * FVector(FMath::Sin(T * 7.3f) * 1.3f + FMath::Sin(T * 13.1f) * 0.6f, FMath::Sin(T * 6.1f + 1.f) * 1.3f + FMath::Sin(T * 11.7f) * 0.5f, FMath::Sin(T * 8.7f + 2.f) * 1.1f);
		const FVector Lk = TrShake * FVector(FMath::Sin(T * 3.1f) * 6.f, FMath::Sin(T * 2.3f + 0.7f) * 6.f, FMath::Sin(T * 4.1f + 1.3f) * 4.f);
		if (Sh < 6) CineCamera(FMath::Lerp(TrA, TrB, K) + Sk, FMath::Lerp(TrLook, TrLookB, K) + Lk, TrFov);
		// «құлақ» камерада: MRQ кезінде ойын терезесі салынбайды, ал қозғалтқыш тыңдаушыны тек терезе салғанда жаңартады —
		// онсыз 3D дыбыстар (есік, айқай, күркіреу) алыстағы ескі нүктеде «естіліп», жазбаға түспейді
		if (CineCam) if (FAudioDevice* AD = GetWorld()->GetAudioDeviceRaw())
		{
			AD->SetListener(GetWorld(), 0, FTransform(CineCam->GetActorRotation(), CineCam->GetActorLocation()), Dt);
			if (bTrailerMRQ) AD->SetTransientPrimaryVolume(2.2f); // MRQ жазбасы тым тыныш (шың −15 дБ) — жалпы деңгейді көтереміз
		}
	}
	// NPC-лерді жүргізу (уақыт жүріп тұрғанда ғана)
	if (dT > 0.f)
	{
		if (TrailerWalker.IsValid()) TrailerWalker->ScriptSteer(TrailerWalkTo, Sh == 3 ? 520.f : 150.f);
		if (TrailerMonster.IsValid() && CineCam) TrailerMonster->ScriptSteer(KAP::CM2M(CineCam->GetActorLocation()), 175.f);
	}
	if (TrailerTorch && Sh == 4) TrailerTorch->SetIntensity(2600.f * (0.94f + 0.06f * FMath::Sin(T * 41.f)));

	// қараңғылық пен жазулар
	TrBlack = 0.f;
	if (T < 0.7f) TrBlack = 1.f - Ramp(T, 0.f, 0.7f);
	if (T >= Card - 0.18f && T < Shot3 + 0.15f) TrBlack = FMath::Min(Ramp(T, Card - 0.18f, Card), 1.f - Ramp(T, Shot3, Shot3 + 0.15f));
	if (FMath::Abs(T - Shot4) < 0.05f || FMath::Abs(T - Shot5) < 0.05f) TrBlack = 1.f; // қысқа «жыпылық» кесу
	if (T >= Title) TrBlack = 1.f;
	TrSmallA = Sh == 0 ? Window(T, 0.7f, 1.2f, 2.4f, 2.85f) : 0.f;
	TrCardA = Window(T, Card + 0.12f, Card + 0.4f, Shot3 - 0.32f, Shot3 - 0.08f);
	TrTitleA = Ramp(T, Title + 0.4f, Title + 1.1f);
	TrSubA = Ramp(T, Title + 1.2f, Title + 1.7f);

	// дыбыс оқиғалары
	if (Au)
	{
		Once(1, 0.02f, [&]()
		{
			if (USoundWave* Mw = Au->Pick("Menu")) { TrailerMusic = UGameplayStatics::CreateSound2D(GetWorld(), Mw, 1.f, 1.f, 0.f, nullptr, true, false); if (TrailerMusic) TrailerMusic->FadeIn(0.8f, 1.f); }
		});
		// трейлер миксі: негізгі соққылар 2D (құлақта) — 3D-да қашықтықтан әлсіреп, музыканың астында көміліп қалатын
		auto Sfx2D = [&](FName Bank, float Vol, float Pitch, float Start = 0.f) -> UAudioComponent*
		{
			UAudioComponent* C = Au->PlayBank(Bank, FVector::ZeroVector, Vol, Pitch, true, Start);
			if (C) TrailerSfx.Add(C);
			return C;
		};
		auto FadeLater = [this](UAudioComponent* C, float After, float Fade)
		{
			if (!C) return;
			TWeakObjectPtr<UAudioComponent> Wc(C); FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Wc, Fade]() { if (Wc.IsValid()) Wc->FadeOut(Fade, 0.f); }), After, false);
		};
		Once(2, Shot3 + 0.55f, [&]() { Sfx2D("Yell", 0.55f, 1.f); });
		// мутанттың ырылы: ұзын жазбаның ортасынан қысқа бөлік, баяу сөнеді
		Once(3, Shot4 + 0.5f, [&]() { FadeLater(Sfx2D("Moan", 0.75f, 0.88f, 2.f), 1.1f, 0.4f); });
		Once(4, Shot4 + 1.8f, [&]() { FadeLater(Sfx2D("Moan", 0.8f, 0.8f, 6.f), 0.5f, 0.3f); });
		// есікті ұру: шын есік тарсылы (DoorClose) + металл соққы, полотно дірілдейді; соңғысы — ең қатты, үстінен шыңғырық
		static const float KnockT[4] = { Shot5 + 0.35f, Shot5 + 0.85f, Shot5 + 1.3f, Shot5 + 1.7f };
		for (int32 k = 0; k < 4; k++)
			Once(10 + k, KnockT[k], [&]()
			{
				AKAPStructure* Wg = nullptr;
				if (Builder) if (const TWeakObjectPtr<AKAPStructure>* P = Builder->Places.Find("wagon1")) Wg = P->Get();
				if (Wg && Wg->Doors.Num() && Wg->Doors[0].IsValid()) Wg->Doors[0]->Knock(1.f);
				Sfx2D("DoorClose", 0.3f + k * 0.08f, k == 3 ? 0.72f : 0.86f);
				Sfx2D("ImpMetal", 0.2f + k * 0.06f, k == 3 ? 0.6f : 0.8f);
				if (k == 3) FadeLater(Sfx2D("Moan", 0.4f, 1.35f, 3.f), 0.35f, 0.15f);
			});
		// атау: терең соққы (баяулатылған тарс + металл)
		Once(20, Title + 0.4f, [&]() { Sfx2D("DoorClose", 0.5f, 0.5f); Sfx2D("ImpMetal", 0.4f, 0.4f); });
	}

	// алдын ала кадрлар: әр сахнадан бір скриншот, соңында шығу
	if (bTrailerShots)
	{
		static const float ShotT[12] = { 1.0f, 2.6f, 3.5f, 4.8f, 5.8f, 7.0f, 8.3f, 9.3f, 10.7f, 11.6f, 12.6f, 14.6f };
		for (int32 i = 0; i < 12; i++) Once(100 + i, ShotT[i], [&]() { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("KAPT_%02d.png"), i + 1), true, false); UE_LOG(LogTemp, Display, TEXT("KAPTRAILER shot image %d at %.2f"), i + 1, T); });
		Once(120, End, [&]() { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([]() { FGenericPlatformMisc::RequestExit(false); }), 0.8f, false); });
	}
}
