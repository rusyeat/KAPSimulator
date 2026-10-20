// Ғимараттар мен нысандар (вагон-үй, үй, қирандылар, киіз үй, мұнара, су қоймасы, шатыр, тосқауыл, басқарма...)
// және E әрекет нүктелері (AKAPInteractSpot)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAP/Core/KAPInteractable.h"
#include "KAPStructure.generated.h"

class UPointLightComponent;

UENUM()
enum class EStructType : uint8 { Wagon, House, Ruin, Yurt, Campfire, Tower, Tank, Tent, Barrier, GuardPost, HQOffice, Fence, Gate, Wreck, Crate, Nest, Silo, Bones, Workshop };

UCLASS()
class KAPSIMULATOR_API AKAPStructure : public AActor
{
	GENERATED_BODY()
public:
	AKAPStructure();
	virtual void Tick(float Dt) override;
	/** Size — см; Color — негізгі түс */
	void Build(EStructType InType, FVector Size, FLinearColor Color, int32 Seed = 0, bool bBright = false);

	EStructType Type = EStructType::House;
	FVector BuildSize = FVector::ZeroVector;
	/** Жердегі табан (жарты өлшем, см, актердің жергілікті осьтерінде); 0 — тексерілмейді */
	FVector2D FootprintHalf() const;
	TArray<FBox2D> IndoorBoxes;   // әлем см
	float ShadeR = 0.f;           // см
	bool bFire = false;
	TArray<FVector2D> CoverPts;   // м
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Blinker;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Flames;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SwingBucket;
	/** Вагон: ішкі нүктелер (әлем) */
	FVector TablePos, BedPos, CratePos, DoorPos;

	static bool IsIndoor(UWorld* W, const FVector& P);
	static bool IsShaded(UWorld* W, const FVector& P);
	static bool IsNearFire(UWorld* W, const FVector& P);
	static const TArray<FVector2D>& Covers(UWorld* W);
	static void InvalidateCache() { CoverCacheFrame = 0; }
	/** Апат радиусы (м): осы қашықтықтағы ұңғымада ақау болса — маяк пен сирена */
	float AlarmRadiusM = 400.f;
	bool bAlarm = false;
private:
	friend class AKAPWorldBuilder;
	void BuildWorkshop(FRandomStream& R, bool bBig);
	void AddFlickLamp(FVector Pos, float Cd, bool bBad, bool bShadow, FLinearColor Col);
	void AddAlarm(FVector Pos, float Yaw);
	void TickLights(float Dt);
	struct FFlick { class ULocalLightComponent* L = nullptr; UStaticMeshComponent* Bulb = nullptr; float Base = 0, Next = 0, Burst = 0; bool bBad = false; FLinearColor Col = FLinearColor::White; };
	TArray<FFlick> Flicks;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Leds;
	UPROPERTY() TObjectPtr<class USpotLightComponent> BeaconL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BeaconM;
	UPROPERTY() TObjectPtr<class UKAPSynthWave> SirenW;
	float BeaconYaw = 0.f, AlarmCheck = 0.f;
	UStaticMeshComponent* Box(FVector Pos, FVector Size, const struct FKAPMat& M, FRotator R = FRotator::ZeroRotator, bool bCol = true);
	static TArray<FVector2D> CoverCache;
	static uint64 CoverCacheFrame;
};

UCLASS()
class KAPSIMULATOR_API AKAPInteractSpot : public AActor, public IKAPInteractable
{
	GENERATED_BODY()
public:
	AKAPInteractSpot();
	virtual void BeginPlay() override;
	FName Action;      // мыс.: tablet, bed, radiator, crate:wagon0, tower:tower1, res_pump, hq_gate, hq_desk, hq_radio, pickup
	float Radius = 230.f;
	TMap<FName, int32> Items; // pickup үшін
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
	virtual FString GetInteractText(class AKAPCharacter* By) const override;
	virtual void Interact(class AKAPCharacter* By) override;
	virtual FVector GetInteractLocation() const override { return GetActorLocation(); }
	virtual float GetInteractRadius() const override { return Radius; }
};
