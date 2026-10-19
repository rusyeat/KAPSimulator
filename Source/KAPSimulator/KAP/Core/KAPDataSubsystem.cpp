#include "KAPDataSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UKAPDataSubsystem* UKAPDataSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UKAPDataSubsystem>() : nullptr;
}

UDataTable* UKAPDataSubsystem::LoadTable(const TCHAR* Name, UScriptStruct* RowStruct)
{
	// 1) Редактор скрипті импорттаған актив: /Game/KAP/Data/DT_<Name>
	const FString AssetPath = FString::Printf(TEXT("/Game/KAP/Data/DT_%s.DT_%s"), Name, Name);
	if (UDataTable* Asset = LoadObject<UDataTable>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		if (Asset->GetRowStruct() == RowStruct && Asset->GetRowMap().Num() > 0) return Asset;
	}
	// 2) CSV файлынан тікелей (Content/KAP/Data/<Name>.csv)
	UDataTable* T = NewObject<UDataTable>(this, FName(FString(TEXT("DT_")) + Name));
	T->RowStruct = RowStruct;
	FString Csv;
	const FString File = FPaths::ProjectContentDir() / TEXT("KAP/Data") / (FString(Name) + TEXT(".csv"));
	if (FFileHelper::LoadFileToString(Csv, *File))
	{
		const TArray<FString> Problems = T->CreateTableFromCSVString(Csv);
		for (const FString& P : Problems) UE_LOG(LogTemp, Warning, TEXT("KAP CSV %s: %s"), Name, *P);
	}
	else UE_LOG(LogTemp, Error, TEXT("KAP: CSV табылмады: %s"), *File);
	return T;
}

void UKAPDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = LoadTable(TEXT("Config"), FKAPConfigRow::StaticStruct());
	Wells = LoadTable(TEXT("Wells"), FKAPWellRow::StaticStruct());
	Nodes = LoadTable(TEXT("Nodes"), FKAPNodeRow::StaticStruct());
	Roads = LoadTable(TEXT("Roads"), FKAPRoadRow::StaticStruct());
	Locations = LoadTable(TEXT("Locations"), FKAPLocationRow::StaticStruct());
	Vehicles = LoadTable(TEXT("Vehicles"), FKAPVehicleRow::StaticStruct());
	Weapons = LoadTable(TEXT("Weapons"), FKAPWeaponRow::StaticStruct());
	Items = LoadTable(TEXT("Items"), FKAPItemRow::StaticStruct());
	Quests = LoadTable(TEXT("Quests"), FKAPQuestRow::StaticStruct());
	Names = LoadTable(TEXT("Names"), FKAPNameRow::StaticStruct());
	Brigades = LoadTable(TEXT("Brigades"), FKAPBrigadeRow::StaticStruct());

	Graph = KAPAI::FRoadGraph();
	Nodes->ForeachRow<FKAPNodeRow>(TEXT("KAP"), [this](const FName& Key, const FKAPNodeRow& R)
		{ Graph.AddNode(Key, FVector2D(R.X, R.Y), R.Title); });
	Roads->ForeachRow<FKAPRoadRow>(TEXT("KAP"), [this](const FName&, const FKAPRoadRow& R)
		{ Graph.AddEdge(R.A, R.B, R.Danger); });
}

float UKAPDataSubsystem::Cfg(FName Key, float Default) const
{
	const FKAPConfigRow* R = Config ? Config->FindRow<FKAPConfigRow>(Key, TEXT("KAP"), false) : nullptr;
	return R ? R->Value : Default;
}
const FKAPLocationRow* UKAPDataSubsystem::Loc(FName Id) const { return Locations ? Locations->FindRow<FKAPLocationRow>(Id, TEXT("KAP"), false) : nullptr; }
const FKAPWeaponRow* UKAPDataSubsystem::Weapon(FName Id) const { return Weapons ? Weapons->FindRow<FKAPWeaponRow>(Id, TEXT("KAP"), false) : nullptr; }
const FKAPQuestRow* UKAPDataSubsystem::Quest(FName Id) const { return Quests ? Quests->FindRow<FKAPQuestRow>(Id, TEXT("KAP"), false) : nullptr; }
FString UKAPDataSubsystem::ItemName(FName Id) const
{
	if (const FKAPItemRow* R = Items ? Items->FindRow<FKAPItemRow>(Id, TEXT("KAP"), false) : nullptr) return R->Title;
	if (const FKAPWeaponRow* W = Weapon(Id)) return W->Title;
	return AmmoName(Id);
}
FString UKAPDataSubsystem::AmmoName(FName Id) const
{
	if (Id == "9mm") return TEXT("9×18 мм");
	if (Id == "12g") return TEXT("12 калибр");
	if (Id == "545") return TEXT("5.45 мм");
	if (Id == "762") return TEXT("7.62 мм");
	if (Id == "flare_ammo") return TEXT("Сигнал оқы");
	if (Id == "flare") return TEXT("Сигнал шамы");
	if (Id == "molotov") return TEXT("Молотов");
	return Id.ToString();
}
TArray<FString> UKAPDataSubsystem::NamesOf(FName Type) const
{
	TArray<FString> Out;
	if (Names) Names->ForeachRow<FKAPNameRow>(TEXT("KAP"), [&](const FName&, const FKAPNameRow& R) { if (R.Type == Type) Out.Add(R.Value); });
	return Out;
}
