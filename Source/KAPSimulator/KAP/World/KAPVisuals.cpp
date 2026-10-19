#include "KAPVisuals.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"

namespace KAPVis
{
	static UStaticMesh* LoadMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UStaticMesh* Mesh(EKAPShape S)
	{
		static TMap<uint8, TWeakObjectPtr<UStaticMesh>> Cache;
		if (TWeakObjectPtr<UStaticMesh>* C = Cache.Find((uint8)S)) if (C->IsValid()) return C->Get();
		UStaticMesh* M = nullptr;
		switch (S)
		{
		// Редактор скрипті (Geometry Script) жасаған мештер — болмаса қарапайым пішін
		case EKAPShape::Rock: M = LoadMesh(TEXT("/Game/KAP/Meshes/SM_KAP_Rock.SM_KAP_Rock")); if (!M) M = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")); break;
		case EKAPShape::Bush: M = LoadMesh(TEXT("/Game/KAP/Meshes/SM_KAP_Saxaul.SM_KAP_Saxaul")); if (!M) M = LoadMesh(TEXT("/Engine/BasicShapes/Cone.Cone")); break;
		case EKAPShape::Wolf: M = LoadMesh(TEXT("/Game/KAP/Meshes/SM_KAP_Wolf.SM_KAP_Wolf")); if (!M) M = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")); break;
		case EKAPShape::WormSeg: M = LoadMesh(TEXT("/Game/KAP/Meshes/SM_KAP_WormSeg.SM_KAP_WormSeg")); if (!M) M = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")); break;
		case EKAPShape::Cylinder: M = LoadMesh(TEXT("/Game/KAP/Meshes/SM_KAP_SmoothCyl.SM_KAP_SmoothCyl")); if (!M) M = LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); break;
		case EKAPShape::CylLow: M = LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); break;
		case EKAPShape::CubeLow: M = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")); break;
		case EKAPShape::Sphere: M = LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")); break;
		case EKAPShape::Cone: M = LoadMesh(TEXT("/Engine/BasicShapes/Cone.Cone")); break;
		case EKAPShape::Plane: M = LoadMesh(TEXT("/Engine/BasicShapes/Plane.Plane")); break;
		default: M = LoadMesh(TEXT("/Game/KAP/Meshes/SM_KAP_BevelCube.SM_KAP_BevelCube")); if (!M) M = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")); break;
		}
		Cache.Add((uint8)S, M);
		return M;
	}

	UMaterialInterface* BaseMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		if (!M.IsValid())
		{
			M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Surface5.M_KAP_Surface5"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Surface.M_KAP_Surface"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		return M.Get();
	}
	UMaterialInterface* TranslucentMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		if (!M.IsValid())
		{
			M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Translucent.M_KAP_Translucent"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!M.IsValid()) M = BaseMaterial();
		}
		return M.Get();
	}
	void Apply(UMaterialInstanceDynamic* MID, const FKAPMat& M)
	{
		if (!MID) return;
		MID->SetVectorParameterValue("Color", M.Color);
		MID->SetVectorParameterValue("BaseColor", M.Color);
		MID->SetScalarParameterValue("Roughness", M.Roughness);
		MID->SetScalarParameterValue("Metallic", M.Metallic);
		MID->SetVectorParameterValue("Emissive", M.Emissive);
		MID->SetScalarParameterValue("Grime", M.Grime);
		MID->SetScalarParameterValue("Rust", FMath::Min(M.Rust * 1.15f, 1.f));
		MID->SetScalarParameterValue("Ribs", M.Ribs);
		MID->SetScalarParameterValue("RibFreq", 1.f / FMath::Max(1.f, M.RibPeriod));
		MID->SetScalarParameterValue("Panel", M.Panel);
		MID->SetScalarParameterValue("PanelInv", 1.f / FMath::Max(1.f, M.PanelW));
		MID->SetScalarParameterValue("Stripes", M.Stripes);
		MID->SetScalarParameterValue("DustTop", M.DustTop);
		MID->SetScalarParameterValue("VarAmt", M.VarAmt);
		MID->SetScalarParameterValue("VarScale", M.VarScale);
		MID->SetScalarParameterValue("RustScale", M.RustScale);
	}
	UMaterialInstanceDynamic* MakeMID(UObject* Outer, const FKAPMat& M)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial(), Outer);
		Apply(MID, M);
		return MID;
	}

	UStaticMeshComponent* Add(AActor* Owner, USceneComponent* Parent, EKAPShape S, FVector Pos, FVector Size, const FKAPMat& M, FRotator Rot, bool bCollision)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh(S));
		C->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
		C->SetRelativeLocation(Pos);
		C->SetRelativeRotation(Rot);
		// Негізгі пішіндер 100 см — масштаб = өлшем / 100
		const bool bUnitCustom = (S == EKAPShape::Rock || S == EKAPShape::Bush || S == EKAPShape::Wolf || S == EKAPShape::WormSeg);
		C->SetRelativeScale3D(bUnitCustom ? Size / 100.0 : Size / 100.0);
		C->SetMaterial(0, MakeMID(C, M));
		// қадам дыбысы үшін беткей тегі
		const uint8 Sf = M.Surface ? M.Surface : (M.Metallic > 0.3f ? 1 : (M.Ribs > 0.2f && M.RibPeriod < 20.f && M.Metallic < 0.1f) ? 2 : M.Roughness > 0.88f && M.Panel > 0.f ? 3 : 0);
		if (Sf) C->ComponentTags.Add(Sf == 1 ? FName("SurfMetal") : Sf == 2 ? FName("SurfWood") : FName("SurfConcrete"));
		C->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		C->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
		Owner->AddInstanceComponent(C);
		// perf: ұсақ бөлшектер алыстан көрсетілмейді, ең ұсақтары көлеңке түсірмейді
		const float Rad = (M.Grime == 0.f && M.DustTop == 0.f) ? 1e6f : Size.GetAbsMax() * 0.5f; // жарқырайтындар (көз, фара) — әрқашан көрінеді
		if (Rad < 12.f) { C->SetCastShadow(false); C->SetCachedMaxDrawDistance(2500.f); }
		else if (Rad < 40.f) { C->bCastDynamicShadow = Rad > 25.f; C->SetCachedMaxDrawDistance(6000.f); }
		else if (Rad < 120.f) C->SetCachedMaxDrawDistance(16000.f);
		return C;
	}

	float GroundZ(const UWorld* World, float X, float Y, float Fallback)
	{
		if (!World) return Fallback;
		TArray<FHitResult> Hits;
		FCollisionQueryParams P(SCENE_QUERY_STAT(KAPGround), false);
		FCollisionObjectQueryParams O(ECC_WorldStatic);
		// Тек ландшафт есептеледі (ғимараттар мен тастар емес)
		World->LineTraceMultiByObjectType(Hits, FVector(X, Y, 200000.0), FVector(X, Y, -200000.0), O, P);
		for (const FHitResult& H : Hits)
			if (H.GetActor() && H.GetActor()->IsA(ALandscapeProxy::StaticClass())) return (float)H.ImpactPoint.Z;
		return Fallback;
	}
}
