// AKAPNPC — адамдар (AWorkerNPC жұмысшылары, тұрғындар, жасақ, күзет, бандит, сәулеленген адам) және монстрлар
// Ұрыс шешімдері: MDP күйі → алдын ала үйретілген Q-кесте (Q-learning)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "KAP/KAPTypes.h"
#include "KAP/AI/KAPAI.h"
#include "KAP/Core/KAPInteractable.h"
#include "KAPNPC.generated.h"

UENUM()
enum class ENPCKind : uint8 { Worker, Villager, Elder, Militia, Guard, Bandit, Survivor, Irradiated, Wolf, Worm, Mother, Police, Soldier, Trader };
/** NPC тапсырмасы (KAPNPCTasks.cpp) */
enum class ENPCTask : uint8 { None, Repair, Carry, Patrol, Campfire, Flee };

UCLASS()
class KAPSIMULATOR_API AKAPNPC : public ACharacter, public IKAPInteractable
{
	GENERATED_BODY()
public:
	AKAPNPC();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type R) override;
	virtual void Tick(float Dt) override;
	virtual float TakeDamage(float Damage, struct FDamageEvent const& Event, AController* Inst, AActor* Causer) override;

	/** Спавннан кейін бір рет шақырылады */
	void Setup(ENPCKind InKind, const FString& InName, float Scale = 1.f, bool bBoss = false);

	virtual FString GetInteractText(class AKAPCharacter* By) const override;
	virtual void Interact(class AKAPCharacter* By) override;
	virtual FVector GetInteractLocation() const override { return GetActorLocation(); }
	virtual float GetInteractRadius() const override { return 280.f; }

	bool IsHuman() const { return Kind != ENPCKind::Wolf && Kind != ENPCKind::Worm; }
	bool IsHostileToPlayer() const;
	/** «Аңдушы»: қараңғыда алыстан қарап тұрады, оған қарасаң — жоғалады, қарамасаң — жақындайды */
	bool bStalker = false;
	// ---- тапсырмалар ----
	ENPCTask Task = ENPCTask::None;
	bool TaskTick(float Dt);
	void PickTask();
	void SetCarry(bool bOn);
	void SetTool(bool bOn);
	void FaceTo(const FVector2D& Pm);
	void SayTask(const TCHAR* Kk);
	float TaskT = 0.f, TaskDur = 0.f, TaskSayCd = 10.f, WorkFxT = 0.f, FleeScan = 0.f, LookYaw = 0.f;
	int32 TaskStep = 0, PatrolI = 0;
	FVector2D TaskPos = FVector2D::ZeroVector, TaskPos2 = FVector2D::ZeroVector, TaskPos3 = FVector2D::ZeroVector;
	TArray<FVector2D> PatrolPts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Carried;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tool;
	bool HasLineOfSightTo(const AActor* Other) const;
	void Kill(bool bByPlayer);
	void Alert(FVector2D AtM, float Seconds);

	ENPCKind Kind = ENPCKind::Villager;
	EKAPFaction Faction = EKAPFaction::Civil;
	FString DisplayName;
	FName TalkId;               // диалог кілті
	int32 WorkerIndex = -1;     // FKAPSession::Workers ішіндегі индекс
	int32 SurvivorId = -1;
	FName BossId;
	bool bBoss = false, bDead = false, bPersistent = false, bCampMember = false;
	bool bFollowPlayer = false, bSick = false;
	TOptional<FVector2D> GoTo;  // метр
	bool bVanishOnArrive = false;
	FVector2D Home = FVector2D::ZeroVector;
	float WanderR = 10.f, PatrolR = 25.f;
	float Hp = 100, MaxHp = 100, Ammo = 30, Dmg = 10, RangeM = 55, WalkSpeed = 320, RunSpeed = 600;
	bool bMelee = false;
	KAPAI::EQProfile Policy = KAPAI::EQProfile::Monster;
	int32 LastAction = -1, LastState = 0;
	float Aggro = 0, Alerted = 0;
	float DeadT = 0;
	/** HMM — дауылда бұл монстрдың орнын бағалау */
	TSharedPtr<KAPAI::FSoundHMM> Tracker;
	KAPAI::FSoundObs LastObs;
	float Rise = 1.f;           // құрт: 0 — құм астында, 1 — сыртта

	static const KAPAI::FQPolicy& GetPolicy(KAPAI::EQProfile P);
	static void TrainPolicies();
	static double TrainMs;
	bool IsFemale() const { return bFemale; }
	/** Келісімшарт нысанасы (өлгенде есепке алынады) */
	int32 ContractUid = 0;
	/** Әскери аймаққа рұқсатсыз кіргенде ескерту (0 — бірінші, 1 — соңғы, 2 — атыс) */
	void ArmyWarn(int32 Stage);
	/** Тегіс дене мен бөлшектер (KAPNPCLook.cpp); мештер жоқ болса false */
	bool SetupSmoothHuman(ENPCKind InKind, float Scale);
private:
	void CivilAI(float Dt);
	void CombatAI(float Dt);
	void SteerTo(FVector2D TargetM, float Speed);
	void Shoot(AActor* Target, float DistM);
	AActor* FindCover(const FVector2D& FromM, AActor* Threat, FVector2D& OutM);
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	TWeakObjectPtr<AActor> Target;
	TOptional<FVector2D> LastHeard, Wander, CoverPt;
	TWeakObjectPtr<AKAPNPC> Rescue, Carrying;
	float ScanT = 0, DecT = 0, Cd = 0, WanderT = 0, CarryT = 0, HelpCd = 0, StuckT = 0, AnimT = 0;
	bool bCarry = false;
	bool bFemale = false, bShouted = false;
	float BarkCd = 0, StepAcc = 0, GroanCd = 3.f, TwitchT = 0.f, HeadJit = 0.f;
	float WatchT = 0.f, UnseenT = 0.f, StalkLife = 0.f;
	void StalkerTick(float Dt, class AKAPCharacter* PC, float DistP);
	void Bark(float DistP);
	void SoundTick(float Dt, float DistP);
	FVector LastPos;
};
