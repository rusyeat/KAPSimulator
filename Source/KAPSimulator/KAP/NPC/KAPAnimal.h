// Дала жануарлары: жылқы, сиыр, ешкі, қой, түлкі (Blender-де жасалған қаңқалы мештер, /Game/KAP/Animals/V1).
// AKAPAnimal — бір жануар: табынымен жайылады, суға барады, жаңбырда топтасады, желге арқасын береді,
// ойыншыдан/көліктен/қасқырдан/атыстан/найзағайдан қашады (үрей көршілерге жұғады), жылқылар кейде өзінен-өзі шауып кетеді.
// Өлсе — рагдолл (физика ассеті импортта жасалған). AKAPFauna — табындарды орналастырады, қасқырларды тау етегіне шығарады.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAPAnimal.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;
class UAnimSequence;
class AKAPFauna;

UENUM()
enum class EAnimalKind : uint8 { Horse, Cow, Goat, Sheep, Fox };

UENUM()
enum class EAnimalState : uint8 { Idle, Graze, Walk, Drink, Flee, Play, Dead };

UCLASS()
class KAPSIMULATOR_API AKAPAnimal : public AActor
{
	GENERATED_BODY()
public:
	AKAPAnimal();
	virtual void Tick(float Dt) override;
	virtual float TakeDamage(float Damage, struct FDamageEvent const& Event, AController* Inst, AActor* Causer) override;
	void Setup(EAnimalKind InKind, int32 InHerd, FVector2D HomeM, float HomeR, uint32 Seed);
	/** Үрей: From нүктесінен (см) қашу, Strength 0..1; көршілерге жұғады */
	void Scare(const FVector& From, float Strength, bool bSpread = true);
	void Die(const FVector& Impulse);
	/** Ойнақ шабыс: нысана нүктеге (м) шабу */
	void StartPlay(FVector2D TargetM, float Dur) { if (State == EAnimalState::Dead) return; State = EAnimalState::Play; Target = TargetM; StateT = 0.f; StateDur = Dur; }
	bool IsDead() const { return State == EAnimalState::Dead; }

	EAnimalKind Kind = EAnimalKind::Sheep;
	EAnimalState State = EAnimalState::Idle;
	int32 Herd = -1;
	FVector2D Home = FVector2D::ZeroVector;
	float HomeR = 60.f;
	TWeakObjectPtr<AKAPFauna> Fauna;
	float Speed = 0.f;   // ағымдағы жылдамдық, см/с
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UCapsuleComponent> Body;
private:
	struct FGait { UAnimSequence* Seq = nullptr; float Native = 1.f; };
	FGait Anim[5]; // Idle, Walk, Trot, Gallop, Graze
	/** Анимацияларды GC-ден сақтау (FGait ішіндегі көрсеткіштер бақыланбайды) */
	UPROPERTY() TArray<TObjectPtr<UAnimSequence>> AnimKeep;
	int32 CurAnim = -1;
	void Play(int32 A, float Rate);
	void Think();
	void Move(float Dt);
	bool PickPoint(FVector2D Center, float R, FVector2D& Out, bool bWater) const;
	FVector2D Target = FVector2D::ZeroVector;
	FVector2D FleeFrom = FVector2D::ZeroVector;
	float StateT = 0.f, StateDur = 3.f, ThinkT = 0.f, Hp = 100.f, Fear = 0.f, Yaw = 0.f, YawOff = 0.f, Thirst = 0.f, VoiceCd = 10.f, DeadT = 0.f;
	float WantSpeed = 0.f, MeshZ = 0.f, Pitch = 0.f, Roll = 0.f, LodT = 0.f;
	FRandomStream Rng;
	bool bLod = false;
};

UCLASS()
class KAPSIMULATOR_API AKAPFauna : public AActor
{
	GENERATED_BODY()
public:
	AKAPFauna();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	static AKAPFauna* Get(const UWorld* W);
	void SpawnAll();
	/** Дыбыс/қауіп болды (атыс, жарылыс, найзағай): радиустағы жануарлар қашады */
	void Alarm(const FVector& At, float RadiusCm, float Strength);
	TArray<TWeakObjectPtr<AKAPAnimal>> Animals;
	int32 Killed = 0;
	/** Тест: жылқы табыны шабысқа шықсын */
	void Stampede(int32 HerdIdx);
	struct FHerdInfo { EAnimalKind Kind; FVector2D Home; float R; int32 Count; };
	TArray<FHerdInfo> Herds;
private:
	float WolfT = 5.f, PlayT = 60.f, LastNoise = 0.f;
	int32 WolvesSpawned = 0;
	int32 LastStrike = 0;
	FRandomStream Rng;
};
