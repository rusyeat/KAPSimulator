// Жануарлардың рагдолл физикасы (SKM_KAP_<Түр>_PhysicsAsset): әр сүйекке бір капсула, буындарға шек.
// FBX импорты жасаған автоматты ассетте 2–5 үлкен дене ғана болды: түлкі ауада ілініп, аяқтары анимациямен
// қозғала берді. Мұнда әр сүйектің капсуласы өз төбелеріне (ең көп салмақ) қарай өлшенеді, аяқ буындары
// тек алға-артқа аз бүгіледі, бір-біріне кіріп тұрған денелер өзара соқтығыспайды.
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SphylElem.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"

namespace KAPAnimalPhys
{
	struct FCaps { int32 Bone = INDEX_NONE; FVector C = FVector::ZeroVector, D = FVector::UpVector; float R = 5.f, Len = 0.f; int32 Body = INDEX_NONE; };

	// Буын шектері (°): бүйірге (Swing1), алға-артқа (Swing2), бұралу (Twist)
	static void Limits(const FString& N, float& Side, float& Bend, float& Twist)
	{
		Side = 10.f; Bend = 20.f; Twist = 6.f;
		if (N == TEXT("spine")) { Side = 8.f; Bend = 14.f; Twist = 6.f; }
		else if (N == TEXT("chest")) { Side = 8.f; Bend = 12.f; Twist = 6.f; }
		else if (N == TEXT("neck1")) { Side = 25.f; Bend = 35.f; Twist = 10.f; }
		else if (N == TEXT("neck2")) { Side = 20.f; Bend = 30.f; Twist = 10.f; }
		else if (N == TEXT("head")) { Side = 15.f; Bend = 35.f; Twist = 10.f; }
		else if (N.StartsWith(TEXT("tail"))) { Side = 30.f; Bend = 35.f; Twist = 10.f; }
		else if (N.StartsWith(TEXT("fore")) || N.StartsWith(TEXT("hind")))
		{
			const int32 K = N.Len() > 4 ? N[4] - TEXT('0') : 0;
			if (K == 0) { Side = 12.f; Bend = 40.f; Twist = 6.f; }
			else if (K == 1) { Side = 4.f; Bend = 35.f; Twist = 3.f; }
			else if (K == 2) { Side = 4.f; Bend = 30.f; Twist = 3.f; }
			else { Side = 3.f; Bend = 25.f; Twist = 3.f; }
		}
	}

	// Тізбектегі келесі сүйек (капсула бағыты): pelvis→spine→chest→neck1→neck2→head, tail1→tail2, fore0→fore1→…
	static FString NextBone(const FString& N)
	{
		static const TMap<FString, FString> M = { {TEXT("pelvis"), TEXT("spine")}, {TEXT("spine"), TEXT("chest")}, {TEXT("chest"), TEXT("neck1")},
			{TEXT("neck1"), TEXT("neck2")}, {TEXT("neck2"), TEXT("head")}, {TEXT("tail1"), TEXT("tail2")} };
		if (const FString* S = M.Find(N)) return *S;
		if ((N.StartsWith(TEXT("fore")) || N.StartsWith(TEXT("hind"))) && N.Len() > 4 && N[4] >= TEXT('0') && N[4] < TEXT('3'))
		{
			FString S = N; S[4] = N[4] + 1; return S;
		}
		return FString();
	}

	static float Pct(TArray<float>& V, float P)
	{
		if (V.Num() == 0) return 0.f;
		V.Sort();
		return V[FMath::Clamp(FMath::RoundToInt(P * (V.Num() - 1)), 0, V.Num() - 1)];
	}

	static bool Build(USkeletalMesh* SK)
	{
		UPhysicsAsset* PA = SK ? SK->GetPhysicsAsset() : nullptr;
		FSkeletalMeshModel* Model = SK ? SK->GetImportedModel() : nullptr;
		if (!PA || !Model || Model->LODModels.Num() == 0) { UE_LOG(LogTemp, Error, TEXT("KAPANIMPHYS %s: no physics asset / model"), *GetNameSafe(SK)); return false; }
		const FReferenceSkeleton& RS = SK->GetRefSkeleton();
		const int32 NB = RS.GetNum();
		// сүйектердің компонент кеңістігіндегі тірек қалпы
		TArray<FTransform> CS; CS.SetNum(NB);
		for (int32 b = 0; b < NB; b++) { const int32 P = RS.GetParentIndex(b); CS[b] = P == INDEX_NONE ? RS.GetRefBonePose()[b] : RS.GetRefBonePose()[b] * CS[P]; }
		// әр төбе ең көп салмақ берген сүйегіне
		TArray<TArray<FVector>> Verts; Verts.SetNum(NB);
		for (const FSkelMeshSection& Sec : Model->LODModels[0].Sections)
			for (const FSoftSkinVertex& V : Sec.SoftVertices)
			{
				int32 Best = 0; uint16 Bw = 0;
				for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; i++) if (V.InfluenceWeights[i] > Bw) { Bw = V.InfluenceWeights[i]; Best = i; }
				if (Bw == 0 || !Sec.BoneMap.IsValidIndex(V.InfluenceBones[Best])) continue;
				Verts[Sec.BoneMap[V.InfluenceBones[Best]]].Add(FVector(V.Position));
			}
		const int32 Hi = RS.FindBoneIndex("head"), Pi = RS.FindBoneIndex("pelvis");
		if (Hi == INDEX_NONE || Pi == INDEX_NONE) return false;
		const FVector Fw = (CS[Hi].GetLocation() - CS[Pi].GetLocation()).GetSafeNormal2D();
		const FVector Lat = FVector::CrossProduct(FVector::UpVector, Fw).GetSafeNormal();

		// ескі денелер мен буындарды алып тастау
		for (USkeletalBodySetup* B : PA->SkeletalBodySetups) if (B) B->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		for (UPhysicsConstraintTemplate* C : PA->ConstraintSetup) if (C) C->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		PA->SkeletalBodySetups.Empty(); PA->ConstraintSetup.Empty(); PA->CollisionDisableTable.Empty();
		PA->UpdateBodySetupIndexMap(); PA->UpdateBoundsBodiesArray();

		FPhysAssetCreateParams Params;
		TArray<FCaps> Caps;
		TMap<int32, int32> BoneToCap;
		for (int32 b = 0; b < NB; b++)
		{
			const FString N = RS.GetBoneName(b).ToString();
			if (N == TEXT("root") || Verts[b].Num() < 6) continue;
			const FVector H = CS[b].GetLocation();
			FVector D;
			const int32 Ch = RS.FindBoneIndex(FName(*NextBone(N)));
			if (Ch != INDEX_NONE) D = (CS[Ch].GetLocation() - H).GetSafeNormal();
			else { FVector M = FVector::ZeroVector; for (const FVector& V : Verts[b]) M += V; D = (M / Verts[b].Num() - H).GetSafeNormal(); }
			if (D.IsNearlyZero()) D = -FVector::UpVector;
			TArray<float> T, Rr;
			for (const FVector& V : Verts[b]) { const float t = FVector::DotProduct(V - H, D); T.Add(t); Rr.Add(((V - H) - D * t).Size()); }
			const float T0 = Pct(T, 0.03f), T1 = Pct(T, 0.97f);
			float R = Pct(Rr, 0.5f) * 0.95f;
			R = FMath::Clamp(R, 1.5f, FMath::Max(1.5f, (T1 - T0) * 0.5f));
			FCaps C; C.Bone = b; C.D = D; C.R = R; C.Len = FMath::Max(0.f, (T1 - T0) - 2.f * R); C.C = H + D * (0.5f * (T0 + T1));
			// кеуде: төбелері иық пен мойынға кетеді, сондықтан кеуде шары кішкентай шығады — дененің алдыңғы жағы
			// жерге батпас үшін арқаның (spine) жуандығына жақындатамыз
			if (N == TEXT("chest"))
				if (const int32* Si = BoneToCap.Find(RS.FindBoneIndex("spine")))
				{
					const int32 Nx = RS.FindBoneIndex("neck1");
					const FVector End = Nx != INDEX_NONE ? CS[Nx].GetLocation() : H + D * C.R * 2.f;
					C.R = FMath::Max(C.R, Caps[*Si].R * 0.85f); C.Len = 0.f;
					C.C = (H + End) * 0.5f - FVector::UpVector * C.R * 0.25f;
				}
			const int32 Bi = FPhysicsAssetUtils::CreateNewBody(PA, RS.GetBoneName(b), Params);
			USkeletalBodySetup* BS = PA->SkeletalBodySetups[Bi];
			BS->AggGeom.EmptyElements();
			// FBX сүйектерінде масштаб бар (Blender → ×100): дене пішіні жұмыс кезінде сүйек масштабына
			// көбейтіледі, сондықтан өлшемді сүйектің жергілікті бірлігіне бөлеміз
			const float S = FMath::Max(KINDA_SMALL_NUMBER, (float)CS[b].GetScale3D().GetAbsMax());
			FKSphylElem E(C.R / S, C.Len / S);
			const FVector DL = CS[b].InverseTransformVectorNoScale(D);
			E.Center = CS[b].InverseTransformPosition(C.C);
			E.Rotation = FRotationMatrix::MakeFromZ(DL).Rotator();
			BS->AggGeom.SphylElems.Add(E);
			BS->PhysicsType = PhysType_Default;
			BS->CollisionReponse = EBodyCollisionResponse::BodyCollision_Enabled;
			BS->DefaultInstance.LinearDamping = 0.15f;
			BS->DefaultInstance.AngularDamping = 0.8f;
			C.Body = Bi;
			BoneToCap.Add(b, Caps.Add(C));
			UE_LOG(LogTemp, Display, TEXT("KAPANIMPHYS %s body %-8s verts=%4d r=%.1f len=%.1f dir=(%.2f %.2f %.2f) boneScale=%.3f headZ=%.1f"), *SK->GetName(), *N, Verts[b].Num(), C.R, C.Len, D.X, D.Y, D.Z, S, H.Z);
		}
		// буындар: бала сүйектің басында, X — сүйек бойымен, Y — жануардың бүйір осі
		int32 NC = 0;
		for (const FCaps& C : Caps)
		{
			int32 P = RS.GetParentIndex(C.Bone);
			while (P != INDEX_NONE && !BoneToCap.Contains(P)) P = RS.GetParentIndex(P);
			if (P == INDEX_NONE) continue;
			const FCaps& PC = Caps[BoneToCap[P]];
			const FName CN = RS.GetBoneName(C.Bone), PN = RS.GetBoneName(P);
			const int32 Ci = FPhysicsAssetUtils::CreateNewConstraint(PA, CN);
			UPhysicsConstraintTemplate* CT = PA->ConstraintSetup[Ci];
			FConstraintInstance& CI = CT->DefaultInstance;
			CI.ConstraintBone1 = CN; CI.ConstraintBone2 = PN;
			const FVector X = C.D;
			FVector Y = Lat - X * FVector::DotProduct(Lat, X);
			if (Y.SizeSquared() < 0.05f) Y = FVector::UpVector - X * FVector::DotProduct(FVector::UpVector, X);
			Y.Normalize();
			const FVector Z = FVector::CrossProduct(X, Y);
			const FTransform FrameCS(FMatrix(X, Y, Z, CS[C.Bone].GetLocation()));
			CI.SetRefFrame(EConstraintFrame::Frame1, FrameCS * CS[C.Bone].Inverse());
			CI.SetRefFrame(EConstraintFrame::Frame2, FrameCS * CS[P].Inverse());
			float Side, Bend, Twist; Limits(CN.ToString(), Side, Bend, Twist);
			CI.SetAngularSwing1Limit(ACM_Limited, Side);
			CI.SetAngularSwing2Limit(ACM_Limited, Bend);
			CI.SetAngularTwistLimit(ACM_Limited, Twist);
			CI.SetLinearXLimit(LCM_Locked, 0.f); CI.SetLinearYLimit(LCM_Locked, 0.f); CI.SetLinearZLimit(LCM_Locked, 0.f);
			CI.SetDisableCollision(true);
			CT->SetDefaultProfile(CI);
			PA->DisableCollision(C.Body, PC.Body);
			NC++;
		}
		// тірек қалпында бір-біріне кіріп тұрған (немесе өте жақын) денелер өзара соқтығыспасын
		int32 ND = 0;
		for (int32 i = 0; i < Caps.Num(); i++)
			for (int32 j = i + 1; j < Caps.Num(); j++)
			{
				const FCaps& A = Caps[i]; const FCaps& B = Caps[j];
				FVector Pa, Pb;
				FMath::SegmentDistToSegmentSafe(A.C - A.D * (A.Len * 0.5f), A.C + A.D * (A.Len * 0.5f), B.C - B.D * (B.Len * 0.5f), B.C + B.D * (B.Len * 0.5f), Pa, Pb);
				if (FVector::Dist(Pa, Pb) < A.R + B.R + 3.f) { PA->DisableCollision(A.Body, B.Body); ND++; }
			}
		PA->UpdateBodySetupIndexMap(); PA->UpdateBoundsBodiesArray();
		PA->PreviewSkeletalMesh = SK;
		PA->MarkPackageDirty();
		UE_LOG(LogTemp, Display, TEXT("KAPANIMPHYS %s: bodies=%d constraints=%d disabled pairs=%d"), *SK->GetName(), Caps.Num(), NC, ND);
		return Caps.Num() > 0 && SaveAsset(PA);
	}
}

bool UKAPEditorLibrary::BuildAnimalPhysics()
{
	bool bOk = true;
	for (const TCHAR* K : { TEXT("Horse"), TEXT("Cow"), TEXT("Goat"), TEXT("Sheep"), TEXT("Fox") })
	{
		USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/KAP/Animals/V1/SKM_KAP_%s.SKM_KAP_%s"), K, K));
		bOk &= KAPAnimalPhys::Build(SK);
	}
	return bOk;
}
