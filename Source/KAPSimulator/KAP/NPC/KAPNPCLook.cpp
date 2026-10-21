// Кейіпкердің сыртқы түрі (тегіс нұсқа): редактор генераторы жасаған «киінген» дене (SKM_KAP_BodyM/F),
// M_KAP_Human2 аймақтары (күрте, шалбар, етік, қолғап, кеудеше, капюшон/орамал/балаклава, көз ойығы)
// және тегіс қатты бөлшектер (каска, дулыға, респиратор, рюкзак, қалпақ, кепка). Мештер жоқ болса — false (ескі нұсқа).
#include "KAPNPC.h"
#include "KAP/World/KAPVisuals.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UStaticMesh* LoadM(const FString& N) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/KAP/Meshes/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet); }
	FLinearColor Jit(const FLinearColor& C, float K = 0.08f) { const float J = 1.f + FMath::FRandRange(-K, K); return FLinearColor(C.R * J, C.G * J, C.B * J, 1.f); }
}

bool AKAPNPC::SetupSmoothHuman(ENPCKind InKind, float Scale)
{
	USkeletalMeshComponent* SK = GetMesh();
	USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, bFemale ? TEXT("/Game/KAP/Characters/SKM_KAP_BodyF.SKM_KAP_BodyF") : TEXT("/Game/KAP/Characters/SKM_KAP_BodyM.SKM_KAP_BodyM"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* HumM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Human2.M_KAP_Human2"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!SK || !Body || !HumM) return false;
	SK->SetSkeletalMesh(Body);
	SK->SetRelativeLocationAndRotation(FVector(0, 0, -90), FRotator(0, -90, 0));
	if (UClass* ABP = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"))) SK->SetAnimInstanceClass(ABP);

	// ---- түстер ----
	static const FLinearColor Skins[5] = { FLinearColor(0.62f, 0.42f, 0.3f), FLinearColor(0.55f, 0.36f, 0.24f), FLinearColor(0.47f, 0.3f, 0.19f), FLinearColor(0.68f, 0.5f, 0.38f), FLinearColor(0.4f, 0.25f, 0.16f) };
	FLinearColor Skin = Skins[FMath::RandRange(0, 4)];
	const bool bOld = InKind == ENPCKind::Elder || (InKind == ENPCKind::Villager && FMath::FRand() < 0.3f);
	FLinearColor Hair = bOld ? FLinearColor(0.45f, 0.43f, 0.4f) : FMath::FRand() < 0.8f ? FLinearColor(0.015f, 0.012f, 0.01f) : FLinearColor(0.08f, 0.045f, 0.02f);
	FLinearColor Jacket(0.3f, 0.28f, 0.24f), Pants(0.1f, 0.1f, 0.11f), Boots(0.035f, 0.03f, 0.025f), Glove(0.06f, 0.055f, 0.045f), Vest(0.2f, 0.2f, 0.2f);
	float VestAmt = 0.f, HairAmt = 1.f, HairLine = 0.68f, HairFront = 0.22f, Cover = 0.f, Gloves = 0.f, Stripes = 0.f, Grime = 0.55f, Eyes = 0.65f, Beard = 0.f, Camo = 0.f;
	FLinearColor CamoA(0.16f, 0.18f, 0.1f), CamoB(0.42f, 0.37f, 0.24f);
	if (!bFemale && FMath::FRand() < 0.35f) Beard = FMath::FRandRange(0.25f, 0.6f); // қылтан/мұрт
	switch (InKind)
	{
	case ENPCKind::Worker: Jacket = FLinearColor(0.72f, 0.3f, 0.05f); Pants = Jacket * 0.86f; Vest = FLinearColor(0.72f, 0.68f, 0.07f); VestAmt = 1.f; Stripes = 1.f; Gloves = 1.f; Hair = Jacket * 0.9f; HairLine = 0.36f; break; // капюшон
	case ENPCKind::Militia: Jacket = FLinearColor(0.18f, 0.21f, 0.12f); Pants = FLinearColor(0.16f, 0.19f, 0.11f); Vest = FLinearColor(0.12f, 0.14f, 0.08f); VestAmt = 1.f; Gloves = 1.f; break;
	case ENPCKind::Guard: Jacket = FLinearColor(0.07f, 0.09f, 0.15f); Pants = FLinearColor(0.05f, 0.06f, 0.1f); Vest = FLinearColor(0.04f, 0.05f, 0.08f); VestAmt = 1.f; Gloves = 1.f; break;
	case ENPCKind::Bandit: Jacket = FLinearColor(0.14f, 0.12f, 0.1f); Pants = FLinearColor(0.1f, 0.1f, 0.12f); Vest = FLinearColor(0.12f, 0.1f, 0.08f); VestAmt = 1.f; Hair = FLinearColor(0.03f, 0.03f, 0.03f); Cover = 1.f; Gloves = 1.f; Grime = 0.9f; break; // балаклава
	case ENPCKind::Survivor: Jacket = FLinearColor(0.3f, 0.27f, 0.22f); Pants = FLinearColor(0.2f, 0.18f, 0.15f); Grime = 0.95f; break;
	case ENPCKind::Irradiated: Skin = FLinearColor(0.32f, 0.36f, 0.26f); Jacket = FLinearColor(0.07f, 0.08f, 0.05f); Pants = FLinearColor(0.05f, 0.05f, 0.04f); HairAmt = 0.f; Eyes = 1.f; Grime = 0.95f; break;
	case ENPCKind::Mother: Jacket = FLinearColor(0.3f, 0.1f, 0.2f); Pants = Jacket * 0.9f; Hair = FLinearColor(0.88f, 0.87f, 0.84f); HairLine = 0.02f; HairFront = 0.8f; break; // ақ орамал (бүйірі мен желкесі жабық)
	case ENPCKind::Elder: Jacket = FLinearColor(0.22f, 0.15f, 0.09f); Pants = FLinearColor(0.12f, 0.12f, 0.14f); Hair = FLinearColor(0.78f, 0.76f, 0.72f); Beard = 1.f; break; // ақ сақал
	case ENPCKind::Police: Jacket = FLinearColor(0.06f, 0.09f, 0.2f); Pants = FLinearColor(0.04f, 0.06f, 0.14f); Vest = FLinearColor(0.72f, 0.82f, 0.08f); VestAmt = 1.f; Stripes = 1.f; Beard = 0.f; break;
	case ENPCKind::Soldier: Jacket = FLinearColor(0.3f, 0.31f, 0.19f); Pants = Jacket * 0.9f; Vest = FLinearColor(0.2f, 0.22f, 0.13f); VestAmt = 1.f; Gloves = 1.f; Camo = 1.f; Beard = 0.f; break;
	case ENPCKind::Trader: Jacket = FLinearColor(0.42f, 0.33f, 0.2f); Pants = FLinearColor(0.15f, 0.13f, 0.11f); Vest = FLinearColor(0.14f, 0.09f, 0.05f); VestAmt = 1.f; Beard = 0.55f; break; // былғары алжапқыш, мұрт
	case ENPCKind::Villager:
	{
		static const FLinearColor V[5] = { FLinearColor(0.35f, 0.2f, 0.4f), FLinearColor(0.15f, 0.3f, 0.42f), FLinearColor(0.45f, 0.15f, 0.12f), FLinearColor(0.3f, 0.36f, 0.18f), FLinearColor(0.4f, 0.36f, 0.3f) };
		Jacket = V[FMath::RandRange(0, 4)]; Pants = FMath::FRand() < 0.5f ? FLinearColor(0.12f, 0.14f, 0.22f) : FLinearColor(0.2f, 0.16f, 0.12f);
		if (bFemale) { Hair = FMath::FRand() < 0.5f ? FLinearColor(0.85f, 0.84f, 0.8f) : FLinearColor(0.5f, 0.12f, 0.1f); HairLine = 0.02f; HairFront = 0.8f; } // орамал
		break;
	}
	default: break;
	}
	for (int32 Mi = 0; Mi < SK->GetNumMaterials(); Mi++)
	{
		UMaterialInstanceDynamic* H = UMaterialInstanceDynamic::Create(HumM, SK);
		H->SetVectorParameterValue("Jacket", Jit(Jacket)); H->SetVectorParameterValue("Pants", Jit(Pants)); H->SetVectorParameterValue("Boots", Boots);
		H->SetVectorParameterValue("Skin", Skin); H->SetVectorParameterValue("Hair", Hair); H->SetVectorParameterValue("Glove", Glove); H->SetVectorParameterValue("Vest", Vest);
		H->SetScalarParameterValue("VestAmt", VestAmt); H->SetScalarParameterValue("HairAmt", HairAmt); H->SetScalarParameterValue("HairLine", HairLine); H->SetScalarParameterValue("HairFront", HairFront);
		H->SetScalarParameterValue("Cover", Cover); H->SetScalarParameterValue("Gloves", Gloves); H->SetScalarParameterValue("StripeAmt", Stripes);
		H->SetScalarParameterValue("StripeGlow", 0.5f); H->SetScalarParameterValue("Grime", Grime); H->SetScalarParameterValue("Eyes", Eyes);
		H->SetScalarParameterValue("InvScale", 1.f / FMath::Max(0.1f, Scale));
		H->SetScalarParameterValue("Beard", Beard); H->SetScalarParameterValue("Camo", Camo); H->SetVectorParameterValue("CamoA", CamoA); H->SetVectorParameterValue("CamoB", CamoB);
		SK->SetMaterial(Mi, H);
	}

	// ---- қатты бөлшектер: табиғи жүйе (X алға, Z жоғары) → сүйек жүйесі (X жоғары, Y алға, Z бүйір) ----
	const FRotator NatToBone = FRotationMatrix::MakeFromXY(FVector(0, 1, 0), FVector(0, 0, 1)).Rotator();
	auto Attach = [&](EKAPShape S, FName Socket, FVector P, FVector Sz, const FKAPMat& Mt, FRotator R = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = KAPVis::Add(this, SK, S, P, Sz, Mt, R, false);
		C->AttachToComponent(SK, FAttachmentTransformRules::KeepRelativeTransform, Socket);
		C->SetCastShadow(Sz.GetMax() > 12.f);
		Parts.Add(C);
	};
	auto AttachMesh = [&](UStaticMesh* SMesh, FName Socket, FVector P, float K, const FKAPMat& Mt)
	{
		if (!SMesh) return;
		UStaticMeshComponent* C = KAPVis::AddMesh(this, SK, SMesh, P, FVector(K), Mt, NatToBone, false);
		C->AttachToComponent(SK, FAttachmentTransformRules::KeepRelativeTransform, Socket);
		C->SetCastShadow(true);
		Parts.Add(C);
	};
	// көп аймақты меш (0 — негізгі, 1 — екінші, 3 — қара, 4 — металл)
	auto AttachRegions = [&](const TCHAR* Base, FName Socket, FVector P, float K, const FKAPMat& M0, const FKAPMat& M1, const FKAPMat& M3, const FKAPMat& M4)
	{
		const FKAPMat* Ms[5] = { &M0, &M1, &M0, &M3, &M4 };
		for (int32 r = 0; r < 5; r++) AttachMesh(LoadM(FString::Printf(TEXT("%s_%d"), Base, r)), Socket, P, K, *Ms[r]);
	};
	FKAPMat Rubber = FKAPMat::Paint(FLinearColor(0.035f, 0.035f, 0.032f), 0.7f); Rubber.DustTop = 0.4f; Rubber.Grime = 0.5f;
	FKAPMat Canvas = FKAPMat::Paint(FLinearColor(0.24f, 0.22f, 0.15f), 0.95f); Canvas.Grime = 0.9f; Canvas.DustTop = 0.9f; Canvas.VarAmt = 0.12f;
	FKAPMat Strap = FKAPMat::Paint(FLinearColor(0.08f, 0.07f, 0.05f), 0.9f);
	const FKAPMat Metal = FKAPMat::RustySteel(FLinearColor(0.35f, 0.35f, 0.33f), 0.25f);
	FKAPMat HardHat = FKAPMat::Paint(FLinearColor(0.9f, 0.68f, 0.02f), 0.32f); HardHat.Grime = 0.55f; HardHat.DustTop = 0.8f;
	FKAPMat Olive = FKAPMat::Paint(FLinearColor(0.17f, 0.2f, 0.11f), 0.6f); Olive.Grime = 0.7f; Olive.DustTop = 0.7f;
	FKAPMat FilterY = FKAPMat::Paint(FLinearColor(0.75f, 0.6f, 0.08f), 0.5f);
	auto Respirator = [&](const FKAPMat& Filter) { AttachRegions(TEXT("SM_KAP_Respirator"), "head", FVector(0, 8.f, 0), 1.f, Rubber, Filter, Rubber, Metal); };
	auto Backpack = [&](const FKAPMat& M, float K)
	{
		AttachRegions(TEXT("SM_KAP_Backpack"), "spine_03", FVector(4.f, -13.f, 0), K, M, M, Strap, Metal);
		for (float S : { -1.f, 1.f }) Attach(EKAPShape::Cube, "spine_04", FVector(6, -1, S * 9.f), FVector(20, 26, 1.4f), Strap); // иық белдіктері
	};
	auto Pouches = [&](const FKAPMat& M) { for (int32 i = 0; i < 3; i++) Attach(EKAPShape::Cube, "spine_03", FVector(-6, 14.5f, -9 + i * 9), FVector(9, 3.5f, 7), M); };
	auto Belt = [&](bool bTools)
	{
		for (float S : { -1.f, 1.f }) Attach(EKAPShape::Cube, "pelvis", FVector(4, 7, S * 16), FVector(9, 7, 5), Canvas);
		if (bTools) Attach(EKAPShape::Cube, "pelvis", FVector(-2, -3, 19), FVector(14, 3, 2.5f), Metal);
	};
	switch (InKind)
	{
	case ENPCKind::Worker:
		// жөндеуші-инженер: каска + фонарь, респиратор, шағылыс жолақты кеудеше (материалда), рюкзак, дозиметр
		AttachMesh(LoadM(TEXT("SM_KAP_HardHat")), "head", FVector(7.f, 0.5f, 0), 1.f, HardHat);
		Attach(EKAPShape::Cylinder, "head", FVector(13.5f, 14.f, 0), FVector(5.5f, 5.5f, 4), Metal, FRotator(0, 0, 90));
		Attach(EKAPShape::Cylinder, "head", FVector(13.5f, 16.2f, 0), FVector(4.5f, 4.5f, 0.8f), FKAPMat::Glow(FLinearColor(1.f, 0.9f, 0.7f), 8), FRotator(0, 0, 90));
		if (FMath::FRand() < 0.75f) Respirator(FilterY);
		if (FMath::FRand() < 0.6f) Backpack(Canvas, FMath::FRand() < 0.4f ? 1.12f : 0.9f);
		Belt(true);
		Attach(EKAPShape::Cube, "spine_04", FVector(4, 15.5f, -8), FVector(8, 2.5f, 5.5f), FKAPMat::Paint(FLinearColor(0.85f, 0.83f, 0.75f), 0.4f)); // дозиметр
		Attach(EKAPShape::Sphere, "spine_04", FVector(6, 16.9f, -6), FVector(1.2f, 1.2f, 1.2f), FKAPMat::Glow(FLinearColor(0.2f, 1.f, 0.2f), 30));
		break;
	case ENPCKind::Militia:
		AttachMesh(LoadM(TEXT("SM_KAP_Helmet")), "head", FVector(6.f, 0.5f, 0), 1.f, Olive);
		Respirator(FKAPMat::Paint(FLinearColor(0.12f, 0.14f, 0.08f), 0.6f));
		Backpack(Olive, 0.9f); Pouches(Olive); Belt(false);
		break;
	case ENPCKind::Guard:
		AttachMesh(LoadM(TEXT("SM_KAP_Helmet")), "head", FVector(6.f, 0.5f, 0), 1.f, FKAPMat::Paint(FLinearColor(0.06f, 0.08f, 0.14f), 0.55f));
		Pouches(FKAPMat::Paint(FLinearColor(0.04f, 0.05f, 0.08f), 0.85f)); Belt(false);
		break;
	case ENPCKind::Bandit:
		Attach(EKAPShape::Cylinder, "neck_01", FVector(0, 3, 0), FVector(19, 20, 7), FKAPMat::Paint(FLinearColor(0.35f, 0.03f, 0.03f), 0.9f), FRotator(0, 0, 90)); // мойын орамалы
		if (FMath::FRand() < 0.6f) Backpack(Canvas, 0.95f);
		Pouches(FKAPMat::Paint(FLinearColor(0.12f, 0.1f, 0.08f), 0.9f));
		break;
	case ENPCKind::Survivor:
		Respirator(FKAPMat::Paint(FLinearColor(0.3f, 0.3f, 0.28f), 0.6f));
		Backpack(Canvas, 1.15f);
		break;
	case ENPCKind::Elder:
		AttachRegions(TEXT("SM_KAP_Kalpak"), "head", FVector(7.f, 0.5f, 0), 1.f, FKAPMat::Paint(FLinearColor(0.82f, 0.8f, 0.74f), 0.95f), Rubber, FKAPMat::Paint(FLinearColor(0.03f, 0.03f, 0.03f), 0.9f), Metal);
		break;
	case ENPCKind::Police:
		AttachRegions(TEXT("SM_KAP_PoliceCap"), "head", FVector(10.f, 0.5f, 0), 1.f, FKAPMat::Paint(FLinearColor(0.05f, 0.08f, 0.2f), 0.6f), FKAPMat::Paint(FLinearColor(0.08f, 0.18f, 0.45f), 0.5f), FKAPMat::Paint(FLinearColor(0.02f, 0.02f, 0.02f), 0.25f), FKAPMat::Metal(FLinearColor(0.95f, 0.72f, 0.25f), 0.3f));
		Attach(EKAPShape::Cube, "spine_04", FVector(4, 15.f, 8), FVector(9, 3, 5), FKAPMat::Paint(FLinearColor(0.03f, 0.03f, 0.03f), 0.5f)); // рация
		Belt(false);
		break;
	case ENPCKind::Soldier:
		AttachMesh(LoadM(TEXT("SM_KAP_Helmet")), "head", FVector(6.f, 0.5f, 0), 1.f, Olive);
		if (FMath::FRand() < 0.3f) Respirator(FKAPMat::Paint(FLinearColor(0.12f, 0.14f, 0.08f), 0.6f));
		if (FMath::FRand() < 0.5f) Backpack(Olive, 0.95f);
		Pouches(Olive); Belt(false);
		break;
	case ENPCKind::Trader:
		AttachRegions(TEXT("SM_KAP_Cap"), "head", FVector(8.f, 0.5f, 0), 1.f, FKAPMat::Paint(FLinearColor(0.1f, 0.09f, 0.08f), 0.9f), Rubber, Rubber, Metal);
		Belt(true);
		break;
	case ENPCKind::Villager:
		if (!bFemale && FMath::FRand() < 0.4f) AttachRegions(TEXT("SM_KAP_Cap"), "head", FVector(8.f, 0.5f, 0), 1.f, FKAPMat::Paint(Jit(FLinearColor(0.12f, 0.1f, 0.08f), 0.3f), 0.9f), Rubber, Rubber, Metal);
		break;
	case ENPCKind::Irradiated:
	{
		Attach(EKAPShape::Sphere, "head", FVector(12, 10, 4), FVector(4.5f, 4.5f, 4.5f), FKAPMat::Glow(FLinearColor(0.5f, 1.f, 0.1f), 40));
		Attach(EKAPShape::Sphere, "head", FVector(12, 10, -4), FVector(4.5f, 4.5f, 4.5f), FKAPMat::Glow(FLinearColor(0.5f, 1.f, 0.1f), 40));
		FKAPMat Tumor = FKAPMat::Paint(FLinearColor(0.18f, 0.22f, 0.07f), 0.25f); Tumor.Emissive = FLinearColor(0.15f, 0.4f, 0.03f) * 0.6f; Tumor.Grime = 0.9f; Tumor.DustTop = 0.f;
		Attach(EKAPShape::Rock, "spine_03", FVector(0, -16, 0), FVector(28, 22, 28), FKAPMat::Paint(FLinearColor(0.25f, 0.35f, 0.1f), 0.3f));
		Attach(EKAPShape::Rock, "clavicle_l", FVector(0, 8, 6), FVector(22, 18, 20), Tumor);
		Attach(EKAPShape::Rock, "upperarm_r", FVector(10, 0, 4), FVector(16, 14, 14), Tumor);
		Attach(EKAPShape::Rock, "head", FVector(6, -8, 9), FVector(14, 12, 11), Tumor);
		Attach(EKAPShape::Rock, "thigh_l", FVector(12, 6, 0), FVector(15, 13, 13), Tumor);
		Attach(EKAPShape::Sphere, "spine_02", FVector(4, 14, 8), FVector(4, 4, 4), FKAPMat::Glow(FLinearColor(0.4f, 1.f, 0.1f), 15));
		Attach(EKAPShape::Sphere, "upperarm_l", FVector(12, 4, 5), FVector(3, 3, 3), FKAPMat::Glow(FLinearColor(0.4f, 1.f, 0.1f), 15));
		break;
	}
	default: break;
	}
	if (InKind == ENPCKind::Militia || InKind == ENPCKind::Guard || InKind == ENPCKind::Bandit || InKind == ENPCKind::Soldier || InKind == ENPCKind::Police)
	{
		// автомат (hand_r кеңістігінде X — алға)
		FKAPMat Gun = FKAPMat::Metal(FLinearColor(0.045f, 0.045f, 0.045f), 0.45f); Gun.Rust = 0.15f;
		FKAPMat Wood2 = FKAPMat::Paint(FLinearColor(0.28f, 0.14f, 0.06f), 0.55f);
		Attach(EKAPShape::Cube, "hand_r", FVector(14, 0, 3), FVector(30, 5, 8), Gun);
		Attach(EKAPShape::Cylinder, "hand_r", FVector(42, 0, 5), FVector(2.4f, 2.4f, 30), Gun, FRotator(90, 0, 0));
		Attach(EKAPShape::Cube, "hand_r", FVector(30, 0, 2), FVector(16, 5.5f, 6), Wood2);
		Attach(EKAPShape::Cube, "hand_r", FVector(12, 0, -6), FVector(6, 3.5f, 14), Gun, FRotator(-15, 0, 0));
		Attach(EKAPShape::Cube, "hand_r", FVector(-8, 0, 0), FVector(22, 4.5f, 9), Wood2, FRotator(-8, 0, 0));
	}
	return true;
}
