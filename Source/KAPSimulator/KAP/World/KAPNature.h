// AKAPNature — жаңа табиғат (KAPTerrain.h-пен бірдей): өзен/атырап/көл/оазис суы, шекарадағы алып таулар мен
// көрінбейтін қабырға, өзен үстіндегі көпірлер, ағаштар (терек, тал, қарағаш, жыңғыл), тау етегіндегі тастар,
// сынатын сексеуіл (балтамен шабу, көлікпен соғу → құлайды, бөлшектері ұшады, отын қалады).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAP/Core/KAPInteractable.h"
#include "KAPNature.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UBoxComponent;
class AKAPWorldBuilder;
class UKAPDataSubsystem;

/** Құлаған сексеуілдің отыны: E — жинау */
UCLASS()
class KAPSIMULATOR_API AKAPWoodPile : public AActor, public IKAPInteractable
{
	GENERATED_BODY()
public:
	AKAPWoodPile();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type R) override;
	virtual FString GetInteractText(class AKAPCharacter* By) const override;
	virtual void Interact(class AKAPCharacter* By) override;
	virtual FVector GetInteractLocation() const override { return GetActorLocation(); }
	virtual float GetInteractRadius() const override { return 260.f; }
	int32 Amount = 3;
};

UCLASS()
class KAPSIMULATOR_API AKAPNature : public AActor
{
	GENERATED_BODY()
public:
	AKAPNature();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	/** -kapnatureshots: өзен, көпір, атырап, оазис, таулар, сексеуіл, егін, ауа райы суреттері (KAPNatureTest.cpp) */
	void RunShots();
	/** -kapfixshots: кіріспе (есік, шығу, бу), NPC жүрісі, ұңғыма жанындағы костюм, жәшіктер, таулар (KAPFixTest.cpp) */
	void RunFixShots();
	void DriveTest(class AKAPGameMode* GM, int32 Mode);
	void AnimalShot(int32 Herd, TFunctionRef<void(FVector2D, FVector2D, float, float)> Go, float Dist, float Up);
	TWeakObjectPtr<class AKAPNPC> TestNPC;
	static AKAPNature* Get(const UWorld* W);

	/** Жолдар салынбай тұрып: көпір аралықтары (жол су арнасын кесіп өтетін жер) */
	void PlanBridges(UKAPDataSubsystem* D, TFunctionRef<float(FVector2D)> GroundZcm);
	/** Жол бетінің биіктігі (см): көпір үстінде — көпір төсемі, әйтпесе жер */
	float DeckZ(FVector2D Pm, float GroundZcm) const;
	/** Жолдардан кейін: су, таулар, шекара, көпірлер, ағаштар, тастар, сексеуіл */
	void Build(AKAPWorldBuilder* WB);

	/** Су: бар болса true; OutZ — су беті (см), OutDepth — тереңдік (м, нақты жерге қарай) */
	static bool WaterAt(const UWorld* W, const FVector& P, float& OutZcm, float& OutDepthM);
	/** Жердің ылғалы (0..1): жаңбыр (Env.Wet) + тұрақты батпақ (өзен жағасы, атырап) — батпақ, лай шашырауы */
	static float MudAt(const UWorld* W, FVector2D Pm);
	/** Балта/қару соққысы: сексеуілге тисе — шабылады; тисе true (дыбысы осында) */
	bool MeleeHit(const FVector& From, const FVector& Dir, float RangeCm, float Damage, bool bAxe);
	/** Көлік: денесі тиген сексеуілді сындырады; сынғандар саны (көлік сәл баяулайды) */
	int32 VehicleSweep(const FVector& Pos, const FVector& Vel, float RadiusCm);
	int32 AliveSaxaul() const { int32 N = 0; for (const FSax& S : Sax) N += !S.bDead; return N; }
	/** Тест: камераға ең жақын тірі сексеуіл (см) */
	bool NearestSaxaul(const FVector& From, FVector& Out) const;

	struct FBridge { FVector2D A, B; float ZA, ZB, W; bool bAsph; };
	TArray<FBridge> Bridges;
	/** Жол/нысан маңы: ағаш, бұта қоюға болмайды */
	bool Clear(FVector2D Pm, float RoadMargin, float SiteMargin) const;
	float RoadDist(FVector2D Pm) const;
private:
	void BuildWater();
	void BuildMountains();
	void BuildMountainForest();
	void BuildBridges();
	void BuildTrees();
	void BuildRocks();
	void BuildSaxaul();
	void BreakSaxaul(int32 I, const FVector& Push);
	UHierarchicalInstancedStaticMeshComponent* MakeH(UStaticMesh* M, UMaterialInterface* Mat, bool bCol, float Cull, bool bShadow);

	struct FSax { FVector P; float Yaw, S; uint8 V; float Hp; bool bDead; float DeadT; int32 Inst; };
	TArray<FSax> Sax;
	TMap<FIntPoint, TArray<int32>> SaxGrid;
	static FIntPoint Cell(const FVector& P) { return FIntPoint(FMath::FloorToInt32(P.X / 2000.f), FMath::FloorToInt32(P.Y / 2000.f)); }
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> SaxWood;
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> SaxTwig;
	UPROPERTY() TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SaxCore;
	UPROPERTY() TObjectPtr<UMaterialInterface> BarkMat;
	struct FFelled { UStaticMeshComponent* Wood = nullptr; UStaticMeshComponent* Twig = nullptr; FQuat Base; FVector Axis; float T = 0.f; TWeakObjectPtr<AKAPWoodPile> Pile; bool bHadPile = false; };
	TArray<FFelled> Felled;
	struct FDebris { UStaticMeshComponent* C = nullptr; float T = 0.f; };
	TArray<FDebris> Debris;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Water;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mountains;
	UPROPERTY() TArray<TObjectPtr<class UInstancedStaticMeshComponent>> Forest;
	UPROPERTY() TArray<TObjectPtr<UBoxComponent>> Walls;
	TWeakObjectPtr<AKAPWorldBuilder> Builder;
	TWeakObjectPtr<UKAPDataSubsystem> Data;
	float RegrowAcc = 0.f;
};
