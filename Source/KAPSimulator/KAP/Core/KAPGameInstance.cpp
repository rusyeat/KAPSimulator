#include "KAPGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

const FString UKAPGameInstance::SlotName = TEXT("KAP_Save");

UKAPGameInstance* UKAPGameInstance::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? Cast<UKAPGameInstance>(W->GetGameInstance()) : nullptr;
}

bool UKAPGameInstance::SaveSession()
{
	UKAPSaveGame* SG = Cast<UKAPSaveGame>(UGameplayStatics::CreateSaveGameObject(UKAPSaveGame::StaticClass()));
	SG->Session = S;
	return UGameplayStatics::SaveGameToSlot(SG, SlotName, 0);
}

bool UKAPGameInstance::LoadSession()
{
	if (!HasSave()) return false;
	UKAPSaveGame* SG = Cast<UKAPSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SG) return false;
	S = SG->Session;
	return true;
}

bool UKAPGameInstance::HasSave() const { return UGameplayStatics::DoesSaveGameExist(SlotName, 0); }
