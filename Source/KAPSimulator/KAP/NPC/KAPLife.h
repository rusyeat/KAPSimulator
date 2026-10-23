// Адамдардың «өмірі» мен денесі:
//  • UKAPHitReactComponent — тірі адамның физикалық реакциясы (Euphoria тәрізді «белсенді рагдолл»): оқ/соққы тиген
//    сүйек тізбегі физикаға өтеді, бұлшықет моторлары (PhysicalAnimation) оны анимацияға қайта тартады — дене серпіледі,
//    шайқалады, сосын қалпына келеді; ойыншы жүгіріп соғылса — сүрінеді. Өлім/талу — NPC-дің өз рагдоллы.
//  • AKAPLife — ауыл адамдарының күн тәртібі мен мақсаты: таңертең егіске/малға, түсте үйге, кешке от басына,
//    түнде үйге; жаңбыр мен найзағайда баспанаға; бір тұрғын — қой отарының шопаны (отармен бірге жүреді).
//    Шопан қасқыр көрсе — айғай салады, отар қашады.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "KAPLife.generated.h"

class UPhysicalAnimationComponent;
class USkeletalMeshComponent;
class AKAPNPC;

UCLASS()
class KAPSIMULATOR_API UKAPHitReactComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UKAPHitReactComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** Bone тиген жерден Impulse (см/с, жылдамдық өзгерісі) */
	void React(FName Bone, const FVector& Impulse, float Strength = 1.f);
	int32 Reactions = 0;
private:
	UFUNCTION() void OnDamage(AActor* Damaged, float Damage, const class UDamageType* Type, AController* Inst, AActor* Causer);
	FName ChainOf(FName Bone) const;
	void EndReact();
	UPROPERTY() TObjectPtr<UPhysicalAnimationComponent> PhysAnim;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	FName Chain = NAME_None;
	float T = -1.f, Hold = 0.15f, Fade = 0.6f, PushCd = 0.f, LastHurt = 0.f, LastHp = 0.f;
};

UCLASS()
class KAPSIMULATOR_API AKAPLife : public AActor
{
	GENERATED_BODY()
public:
	AKAPLife();
	virtual void Tick(float Dt) override;
	static AKAPLife* Get(const UWorld* W);
	/** Тест: барлық тұрғынның қазіргі мақсаты (логқа) */
	void Dump() const;
private:
	enum class EGoal : uint8 { None, Field, Herd, Home, Fire, Shelter, Play };
	struct FPerson { TWeakObjectPtr<AKAPNPC> N; FVector2D Village, HomeSpot, Field, Fire; EGoal Goal = EGoal::None; int32 Flock = -1; float ShoutCd = 0.f; FVector2D OrigHome; float OrigR = 10.f; };
	TArray<FPerson> People;
	float ScanT = 0.f;
	void Scan();
	void Plan(FPerson& P, double Hour, float Rain, float Thunder);
};
