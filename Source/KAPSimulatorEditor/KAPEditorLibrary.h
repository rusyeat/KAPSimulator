// Редактор құралдары: деңгейді автоматты құру (Python скрипті шақырады)
//  • PBR материалдары (бет, мөлдір түтін, радиация жарқылы, шөлейт ландшафты)
//  • процедуралық мештер (Nanite тастар, сексеуіл, мутант қасқыр, құрт сегменті)
//  • CSV → Data Table активтері
//  • 2.4×2.4 км шөлейт ландшафты (құмды төбелер, құрғаған өзен арнасы, шекара төбелері)
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KAPEditorLibrary.generated.h"

UCLASS()
class KAPSIMULATOREDITOR_API UKAPEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateMaterials();
	/** Жолдар: асфальт пен топырақ жол текстуралары (процедуралық), M_KAP_Road */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateRoadAssets();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateRoadMaterial();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateMeshes();
	/** Атмосфера материалдары: жарықта жарқырайтын шаң, аяқ/дөңгелек ізі декалі */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateAtmosphereMaterials();
	/** Индустриялық беттер: гофр, жіктер, тат, ағын іздері, шаң қабаты */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateIndustrialMaterial();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateInvisibleMaterial();
	/** Жиегі қырланған текше мен тегіс цилиндр */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateBevelMeshes();
	/** Көлік корпустары (SDF → тегіс Nanite мештер), шина мен диск */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateVehicleMeshes();
	/** Кейіпкерлер: киінген тегіс денелер (SKM_KAP_Body*), M_KAP_Human2, каска/дулыға/респиратор/рюкзак/қалпақ */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateHumanAssets();
	/** TextRender белгілері үшін кирилл қаріпі (F_KAP_Sign) */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateSignFont();
	/** Шөлейт v2: құм толқындары, ойпаңдар */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateLandscapeMaterial2();
	/** Шынайы құм v4: аралас асимметриялы толқындар, түйіршік, жылтыр */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateLandscapeMaterial4();
	/** Құм v5: процедуралық биіктік/нормаль/түс текстуралары + parallax */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool GenerateSandTextures();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateLandscapeMaterial5();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool ApplyLandscapeMaterial(const FString& MapPath);
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool ImportDataTables();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateDesertLandscape();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool SpawnLevelActors();
	/** Мынаны толық орындайды: материалдар, мештер, кестелер, жаңа деңгей, ландшафт, актерлер, сақтау */
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool BuildEverything(const FString& MapPath);
	// ---- жаңа жер бедері (KAPTerrainGen.inl): өзен, атырап, оазис, егін, шекара таулары ----
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateTerrainTextures();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateNatureAssets();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateAnimalMaterial();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool BuildTerrainAssets(const FString& MapPath);
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateTerrainMaterials();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool CreateMountainMesh();
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool RebuildTerrain(const FString& MapPath);
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool PlacePlayerStart(const FString& MapPath);
	UFUNCTION(BlueprintCallable, Category = "KAP") static bool BuildTerrainAll(const FString& MapPath);
};
