// Деректер кестелері: Content/KAP/Data/*.csv → UDataTable (баланс кодсыз өзгереді)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KAP/KAPTypes.h"
#include "KAP/AI/KAPAI.h"
#include "KAPDataSubsystem.generated.h"

UCLASS()
class KAPSIMULATOR_API UKAPDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY() TObjectPtr<UDataTable> Config;
	UPROPERTY() TObjectPtr<UDataTable> Wells;
	UPROPERTY() TObjectPtr<UDataTable> Nodes;
	UPROPERTY() TObjectPtr<UDataTable> Roads;
	UPROPERTY() TObjectPtr<UDataTable> Locations;
	UPROPERTY() TObjectPtr<UDataTable> Vehicles;
	UPROPERTY() TObjectPtr<UDataTable> Weapons;
	UPROPERTY() TObjectPtr<UDataTable> Items;
	UPROPERTY() TObjectPtr<UDataTable> Quests;
	UPROPERTY() TObjectPtr<UDataTable> Names;
	UPROPERTY() TObjectPtr<UDataTable> Brigades;

	float Cfg(FName Key, float Default = 0.f) const;
	const FKAPLocationRow* Loc(FName Id) const;
	const FKAPWeaponRow* Weapon(FName Id) const;
	FString ItemName(FName Id) const;
	FString AmmoName(FName Id) const;
	const FKAPQuestRow* Quest(FName Id) const;
	TArray<FString> NamesOf(FName Type) const;

	/** A* навигаторының жол желісі (метрмен) */
	KAPAI::FRoadGraph Graph;

	static UKAPDataSubsystem* Get(const UObject* WorldContext);

private:
	UDataTable* LoadTable(const TCHAR* Name, UScriptStruct* RowStruct);
};
