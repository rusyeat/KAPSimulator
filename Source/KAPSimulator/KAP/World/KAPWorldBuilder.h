// AKAPWorldBuilder — деректер кестелерінен әлемді құрастырады: ұңғымалар, ауылдар, басқарма, мұнаралар,
// су қоймасы, бандит лагері, жолдар, сексеуіл мен тастар (HISM), көліктер, жәшіктер мен әрекет нүктелері
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAPWorldBuilder.generated.h"

class AKAPStructure;
class AKAPInteractSpot;
class AKAPVehicleHandle;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class KAPSIMULATOR_API AKAPWorldBuilder : public AActor
{
	GENERATED_BODY()
public:
	/** Жол беттері (BuildRoadSurfaces) */
	UPROPERTY() TArray<TObjectPtr<class UProceduralMeshComponent>> RoadMeshes;
	/** Ақсай көшесіндегі дүкен орны (ауыл құрылғанда таңдалады; дүкен ғимараты мен сатушы осында) */
	FVector2D ShopSlot = FVector2D::ZeroVector; float ShopYaw = 0.f; bool bShopSlot = false;
	AKAPWorldBuilder();
	void BuildAll();

	TMap<FName, TWeakObjectPtr<AKAPStructure>> Places;
	TArray<TWeakObjectPtr<AKAPVehicleHandle>> Vehicles;
	TWeakObjectPtr<AKAPStructure> HQGate;
	TWeakObjectPtr<AKAPStructure> HQOffice;
	TWeakObjectPtr<AKAPStructure> Wreck;

	AKAPStructure* SpawnStruct(int32 Type, FVector2D PosM, float YawDeg, FVector Size, FLinearColor Color, int32 Seed = 0, bool bBright = false);
	AKAPInteractSpot* SpawnSpot(FName Action, FVector PosCm, float Radius = 230.f, bool bVisualBox = false);
	float Z(FVector2D PosM) const;
private:
	void BuildRoads();
	/** Асфальт/топырақ жол беті (процедуралық таспа); активтер жоқ болса — false */
	bool BuildRoadSurfaces();
	/** Жолдың үстіне түсіп қалған ғимараттарды алып тастау */
	void ClearRoadOverlaps();
	bool OnRoad(const class AKAPStructure* S, float Margin) const;
	float DistToRoadCm(FVector2D Pcm) const;
	bool bSplittingFence = false;
	void BuildVegetation();
	void BuildVehicles();
	UPROPERTY() TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Roads;
	UPROPERTY() TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Rocks;
	UPROPERTY() TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Bushes;
	UPROPERTY() TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Grass;
};
