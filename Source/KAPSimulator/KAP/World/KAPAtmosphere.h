// AKAPAtmosphere — кинематографиялық атмосфера:
//  • камера айналасында ұшатын шаң/құм бөлшектері (прожектор сәулесінде жарқырайды)
//  • аяқ пен дөңгелек іздері (декалдар, уақыт өте өшеді)
//  • түнде/дауылда жанатын прожектор бағандары, барлық жарыққа көлемді шашырау
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAPAtmosphere.generated.h"

class UInstancedStaticMeshComponent;
class UDecalComponent;
class USpotLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class KAPSIMULATOR_API AKAPAtmosphere : public AActor
{
	GENERATED_BODY()
public:
	AKAPAtmosphere();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** Прожектор бағаны (9 м): төмен қараған көлемді сәуле, қызыл жарқылдақ */
	void AddLampPost(FVector BaseCm, float Yaw, bool bBeacon);
	/** Әлемдегі барлық жергілікті жарыққа көлемді тұман шашырауын қосу */
	void TuneAllLights();
	static AKAPAtmosphere* Get(UWorld* W);
	/** Тест: камера алдында шаң құйынын бірден көрсету */
	void ForceDevil();
private:
	void UpdateParticles(float Dt, const FVector& Cam);
	void UpdateTracks(float Dt);
	void StampTrack(FVector Pos, float Yaw, FVector Size, bool bTread);

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Dust;
	TArray<FVector> DustPos;
	TArray<FVector> DustVel;
	TArray<FTransform> DustXf;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Puffs;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PuffMID;
	TArray<FVector> PuffPos;
	TArray<FTransform> PuffXf;
	// шаң құйыны (күндіз): спираль бойымен айналатын шаң шарлары, баяу жылжиды
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Devil;
	TArray<FTransform> DevilXf;
	FVector DevilPos = FVector::ZeroVector, DevilVel = FVector::ZeroVector;
	float DevilT = -1.f, DevilCd = 20.f, DevilLife = 0.f;
	void UpdateDevil(float Dt, const FVector& Cam);
	// тіршілік: аспанда айналатын құзғындар, жел айдаған қаңбақтар
	struct FBird { UStaticMeshComponent* Body = nullptr; UStaticMeshComponent* WL = nullptr; UStaticMeshComponent* WR = nullptr; float Ph = 0, Rad = 0, Alt = 0, Spd = 0; };
	struct FWeed { UStaticMeshComponent* C = nullptr; FVector P = FVector::ZeroVector; float Roll = 0, S = 1, Bph = 0; };
	TArray<FBird> Birds; TArray<FWeed> Weeds;
	FVector BirdC = FVector::ZeroVector;
	void UpdateLife(float Dt, const FVector& Cam);

	struct FTrack { UDecalComponent* D = nullptr; UMaterialInstanceDynamic* M = nullptr; float Age = 0; };
	TArray<FTrack> Tracks;
	int32 NextTrack = 0;
	FVector LastFoot = FVector::ZeroVector;
	bool bLeftFoot = false;
	TMap<TWeakObjectPtr<AActor>, FVector> LastWheel;

	struct FLamp { USpotLightComponent* L = nullptr; UStaticMeshComponent* Bulb = nullptr; UStaticMeshComponent* Beacon = nullptr; };
	TArray<FLamp> Lamps;
	float LampLevel = -1.f;
	float TuneT = 0.f, ShadowT = 0.f;
};
