// Кейіпкерлер: UE манекенінен «киінген» тегіс денелер (SKM_KAP_BodyM/F) — киім қалыңдығы, балақ/жең қатпарлары,
// етік пен белбеу тікелей мешке жазылады (вершина түсі + UV1/UV2 аймақтары), сондықтан анимацияда киім денемен бірге иіледі.
// Қатты бөлшектер (каска, дулыға, респиратор, рюкзак, қалпақ) — тегіс айналу/SDF мештері.
// KAPEditorLibrary.cpp ішіне KAPVehicleGen.inl-ден кейін қосылады.
#include "Engine/SkeletalMesh.h"

#include "SkeletalMeshAttributes.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Algo/Reverse.h"
#include "StaticMeshOperations.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace KAPHGen
{
	using KAPVGen::V3;
	enum ECat : int32 { CHead, CNeck, CTorso, CPelvis, CUpperArm, CLowerArm, CHand, CThigh, CCalf, CFoot, CNum };
	static ECat CatOf(const FString& B)
	{
		if (B.Contains(TEXT("head"))) return CHead;
		if (B.Contains(TEXT("neck"))) return CNeck;
		if (B.Contains(TEXT("hand")) || B.Contains(TEXT("index")) || B.Contains(TEXT("middle")) || B.Contains(TEXT("ring")) || B.Contains(TEXT("pinky")) || B.Contains(TEXT("thumb"))) return CHand;
		if (B.Contains(TEXT("lowerarm"))) return CLowerArm;
		if (B.Contains(TEXT("upperarm"))) return CUpperArm;
		if (B.Contains(TEXT("thigh"))) return CThigh;
		if (B.Contains(TEXT("calf"))) return CCalf;
		if (B.Contains(TEXT("foot")) || B.Contains(TEXT("ball"))) return CFoot;
		if (B.Contains(TEXT("pelvis"))) return CPelvis;
		return CTorso; // spine, clavicle, root
	}
	struct FCatV { float Off, R, G, B, A, Torso; };
	//                         киім қалыңдығы (см), күрте, шалбар, етік/белбеу, қол, кеудеше аймағы
	static const FCatV Tab[CNum] = {
		{ 0.f, 0, 0, 0, 0, 0 },        // бас
		{ 0.6f, 0, 0, 0, 0, 0 },       // мойын (жаға төменде позиция бойынша)
		{ 1.7f, 1, 0, 0, 0, 1 },       // кеуде
		{ 1.6f, 0, 1, 0, 0, 0 },       // жамбас
		{ 1.35f, 1, 0, 0, 0, 0 },      // иық
		{ 1.15f, 1, 0, 0, 0, 0 },      // білек
		{ 0.25f, 0, 0, 0, 1, 0 },      // қол (қолғап)
		{ 1.55f, 0, 1, 0, 0, 0 },      // сан
		{ 1.3f, 0, 1, 0, 0, 0 },       // балтыр
		{ 1.8f, 0, 0, 1, 0, 0 },       // табан (етік)
	};

	/** Манекенді көшіріп, «киім» деформациясы мен аймақ белгілерін жазу */
	static bool MakeBody(const TCHAR* SrcPath, const FString& Name)
	{
		USkeletalMesh* Src = LoadObject<USkeletalMesh>(nullptr, SrcPath);
		if (!Src) { UE_LOG(LogTemp, Warning, TEXT("KAPHUM no source %s"), SrcPath); return false; }
		UPackage* Pkg = MakePkg(TEXT("/Game/KAP/Characters/") + Name);
		if (UObject* Old = StaticFindObject(USkeletalMesh::StaticClass(), Pkg, *Name)) Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		USkeletalMesh* SK = DuplicateObject<USkeletalMesh>(Src, Pkg, FName(*Name));
		SK->SetFlags(RF_Public | RF_Standalone);
		const FReferenceSkeleton& RS = SK->GetRefSkeleton();
		// сүйектердің компонент кеңістігіндегі орны (тірек позасы)
		TArray<FTransform> CS; CS.SetNum(RS.GetNum());
		for (int32 i = 0; i < RS.GetNum(); i++) { const int32 Pi = RS.GetParentIndex(i); CS[i] = Pi >= 0 ? RS.GetRefBonePose()[i] * CS[Pi] : RS.GetRefBonePose()[i]; }
		auto BoneP = [&](const TCHAR* N) { const int32 I = RS.FindBoneIndex(N); return I >= 0 ? V3(CS[I].GetLocation()) : V3(0); };
		TArray<uint8> Cat; Cat.SetNum(RS.GetNum());
		for (int32 i = 0; i < RS.GetNum(); i++) Cat[i] = (uint8)CatOf(RS.GetBoneName(i).ToString().ToLower());
		const V3 KneeL = BoneP(TEXT("calf_l")), KneeR = BoneP(TEXT("calf_r")), ElbL = BoneP(TEXT("lowerarm_l")), ElbR = BoneP(TEXT("lowerarm_r"));
		const V3 HandL = BoneP(TEXT("hand_l")), HandR = BoneP(TEXT("hand_r")), FootL = BoneP(TEXT("foot_l"));
		const float BootTop = FMath::Lerp(FootL.Z, KneeL.Z, 0.42f);
		const float BeltZ = (BoneP(TEXT("pelvis")).Z + BoneP(TEXT("spine_01")).Z) * 0.5f + 2.f;
		const float NeckZ = BoneP(TEXT("neck_01")).Z;
		const float HeadZ = BoneP(TEXT("head")).Z;
		int32 Done = 0;
		for (int32 L = 0; L < SK->GetLODNum(); L++)
		{
			if (!SK->HasMeshDescription(L)) continue;
			FMeshDescription* MD = SK->GetMeshDescription(L);
			if (!MD) continue;
			FSkeletalMeshAttributes A(*MD);
			TVertexAttributesRef<FVector3f> Pos = A.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> Nrm = A.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector4f> Col = A.GetVertexInstanceColors();
			TVertexInstanceAttributesRef<FVector2f> UV = A.GetVertexInstanceUVs();
			if (UV.GetNumChannels() < 3) UV.SetNumChannels(3);
			FSkinWeightsVertexAttributesRef SW = A.GetVertexSkinWeights();
			// ---- манекен бұлшықеттерін (кеуде, іш, балтыр) жұмсарту: Таубин тегістеуі, тек киім астында — киім «бояу» емес, мата болып көрінеді ----
			{
				const int32 NV = MD->Vertices().GetArraySize();
				TArray<float> Sm; Sm.Init(0.f, NV);
				static const float SmTab[CNum] = { 0.f, 0.3f, 1.f, 1.f, 0.85f, 0.7f, 0.f, 0.9f, 0.8f, 0.4f };
				for (const FVertexID V : MD->Vertices().GetElementIDs())
				{
					float S = 0.f, Ws = 0.f;
					for (const auto& W : SW.Get(V)) if (Cat.IsValidIndex(W.GetBoneIndex())) { S += SmTab[Cat[W.GetBoneIndex()]] * W.GetWeight(); Ws += W.GetWeight(); }
					Sm[V] = Ws > 0.f ? S / Ws : 0.f;
				}
				TArray<TArray<int32>> Adj; Adj.SetNum(NV);
				for (const FTriangleID Tr : MD->Triangles().GetElementIDs())
				{
					const TArrayView<const FVertexID> Tv = MD->GetTriangleVertices(Tr);
					for (int32 e = 0; e < 3; e++) { const int32 Va = Tv[e], Vb = Tv[(e + 1) % 3]; Adj[Va].AddUnique(Vb); Adj[Vb].AddUnique(Va); }
				}
				TArray<V3> Cur; Cur.SetNum(NV);
				for (const FVertexID V : MD->Vertices().GetElementIDs()) Cur[V] = Pos[V];
				TArray<V3> Nx = Cur;
				// 1) күшті Лаплас тегістеуі (~5 см радиус): бұлшықет бедері жоғалады, бірақ дене сәл кішірейеді
				for (int32 It = 0; It < 120; It++)
				{
					for (const FVertexID V : MD->Vertices().GetElementIDs())
					{
						const TArray<int32>& Nb = Adj[V];
						if (!Nb.Num() || Sm[V] <= 0.f) { Nx[V] = Cur[V]; continue; }
						V3 Avg(0); for (const int32 j : Nb) Avg += Cur[j];
						Avg /= (float)Nb.Num();
						Nx[V] = Cur[V] + (Avg - Cur[V]) * (0.5f * Sm[V]);
					}
					Swap(Cur, Nx);
				}
				// 2) h = тегіс бетке дейінгі қашықтық (нормаль бойынша); оның жергілікті орташасы — жалпы кішіреюі, айырмасы — бедер
				TArray<float> Hh; Hh.Init(0.f, NV);
				TArray<V3> Nvs; Nvs.Init(V3(0, 0, 1), NV);
				for (const FVertexID V : MD->Vertices().GetElementIDs())
				{
					V3 Nv(0); for (const FVertexInstanceID I : MD->GetVertexVertexInstanceIDs(V)) Nv += Nrm[I];
					Nvs[V] = Nv.GetSafeNormal();
					Hh[V] = V3::DotProduct(Cur[V] - Pos[V], Nvs[V]);
				}
				TArray<float> Hb = Hh, Hn = Hh;
				for (int32 It = 0; It < 120; It++)
				{
					for (const FVertexID V : MD->Vertices().GetElementIDs())
					{
						const TArray<int32>& Nb = Adj[V];
						if (!Nb.Num()) { Hn[V] = Hb[V]; continue; }
						float A2 = 0.f; for (const int32 j : Nb) A2 += Hb[j];
						Hn[V] = Hb[V] + (A2 / Nb.Num() - Hb[V]) * 0.5f;
					}
					Swap(Hb, Hn);
				}
				// 3) мата бұлшықеттің үстінен тартылады: ойықтар толады, дөңестер басылады (жалпы көлем сақталады)
				double SumH = 0; int32 NH = 0, NAdj = 0;
				for (const FVertexID V : MD->Vertices().GetElementIDs())
				{
					const float D = Hh[V] - Hb[V];
					if (Sm[V] > 0.f) { SumH += FMath::Abs(D); NH++; }
					NAdj += Adj[V].Num();
					// тегіс қабық (көлемі қалпына келтірілген): Cur − N·Hb
					Pos[V] = FMath::Lerp(Pos[V], Cur[V] - Nvs[V] * Hb[V], FMath::Min(1.f, Sm[V] * 1.15f));
				}
				UE_LOG(LogTemp, Display, TEXT("KAPHUM smooth: mean |h| %.2f cm over %d verts, avg neighbours %.1f"), NH ? SumH / NH : 0.0, NH, NV ? (float)NAdj / NV : 0.f);
			}
			// бас: орталығы, радиусы, төбесі
			V3 HC(0); int32 Hn = 0; float TopZ = -1e9f;
			for (const FVertexID V : MD->Vertices().GetElementIDs())
			{
				float Wh = 0.f; for (const auto& W : SW.Get(V)) if (Cat.IsValidIndex(W.GetBoneIndex()) && Cat[W.GetBoneIndex()] == CHead) Wh += W.GetWeight();
				if (Wh > 0.5f) { HC += Pos[V]; Hn++; TopZ = FMath::Max(TopZ, Pos[V].Z); }
			}
			if (Hn) HC /= (float)Hn;
			float HR = 1.f;
			for (const FVertexID V : MD->Vertices().GetElementIDs())
			{
				float Wh = 0.f; for (const auto& W : SW.Get(V)) if (Cat.IsValidIndex(W.GetBoneIndex()) && Cat[W.GetBoneIndex()] == CHead) Wh += W.GetWeight();
				if (Wh > 0.5f) HR = FMath::Max(HR, FVector2f(Pos[V].X - HC.X, Pos[V].Y - HC.Y).Size());
			}
			const float ChinZ = HeadZ - 4.f;
			TArray<V3> NewP; NewP.SetNum(MD->Vertices().GetArraySize());
			TArray<FVector4f> VCol; VCol.SetNum(MD->Vertices().GetArraySize());
			TArray<FVector4f> VUv; VUv.SetNum(MD->Vertices().GetArraySize());
			for (const FVertexID V : MD->Vertices().GetElementIDs())
			{
				const V3 P = Pos[V];
				V3 N(0); for (const FVertexInstanceID I : MD->GetVertexVertexInstanceIDs(V)) N += Nrm[I];
				N = N.GetSafeNormal();
				FCatV C = { 0, 0, 0, 0, 0, 0 }; float Wsum = 0.f, Wleg = 0.f, Whead = 0.f, Wneck = 0.f, Wlow = 0.f, Warm = 0.f, Wtor = 0.f;
				for (const auto& W : SW.Get(V))
				{
					if (!Cat.IsValidIndex(W.GetBoneIndex())) continue;
					const int32 c = Cat[W.GetBoneIndex()]; const float w = W.GetWeight();
					const FCatV& T = Tab[c];
					C.Off += T.Off * w; C.R += T.R * w; C.G += T.G * w; C.B += T.B * w; C.A += T.A * w; C.Torso += T.Torso * w; Wsum += w;
					if (c == CThigh || c == CCalf || c == CFoot) Wleg += w;
					if (c == CHead) Whead += w;
					if (c == CNeck) Wneck += w;
					if (c == CLowerArm) Wlow += w;
					if (c == CUpperArm || c == CLowerArm) Warm += w;
					if (c == CTorso || c == CPelvis) Wtor += w;
				}
				if (Wsum > 0.f) { C.Off /= Wsum; C.R /= Wsum; C.G /= Wsum; C.B /= Wsum; C.A /= Wsum; C.Torso /= Wsum; }
				// жаға: мойынның төменгі бөлігі — күрте
				if (Wneck + Wtor > 0.4f && P.Z < NeckZ + 3.f) { const float t = FMath::Clamp((NeckZ + 3.f - P.Z) / 3.f, 0.f, 1.f); C.R = FMath::Max(C.R, t); C.Off = FMath::Max(C.Off, 1.3f * t); }
				// етік қонышы
				if (Wleg > 0.5f && P.Z < BootTop + 2.f)
				{
					const float t = FMath::Clamp((BootTop + 2.f - P.Z) / 2.5f, 0.f, 1.f);
					C.B = FMath::Lerp(C.B, 1.f, t); C.G = FMath::Lerp(C.G, 0.f, t); C.Off = FMath::Lerp(C.Off, 1.9f, t);
					if (FMath::Abs(P.Z - BootTop) < 1.2f) C.Off += 0.35f; // қоныш жиегі
				}
				// белбеу
				if (Wtor > 0.5f && FMath::Abs(P.Z - BeltZ) < 2.4f) { C.B = FMath::Max(C.B, 1.f - FMath::Abs(P.Z - BeltZ) / 2.6f); C.Off += 0.45f; }
				// жең ұшы (манжет)
				if (Wlow > 0.3f) { const float Dh = FMath::Min((P - HandL).Size(), (P - HandR).Size()); if (Dh < 6.f) C.Off += 0.55f * (1.f - Dh / 6.f); }
				// мата қатпарлары: тізе мен шынтақ айналасында сақиналар, белде көлденең бүрмелер, жалпы жұмсақ толқын
				float Fold = 0.f;
				if (Wleg > 0.3f && C.B < 0.5f) { const float D = FMath::Min((P - KneeL).Size(), (P - KneeR).Size()); if (D < 22.f) Fold += 0.42f * FMath::Sin(D * 2.f * PI / 4.3f) * FMath::Exp(-D / 9.f); }
				if (Warm > 0.3f) { const float D = FMath::Min((P - ElbL).Size(), (P - ElbR).Size()); if (D < 16.f) Fold += 0.32f * FMath::Sin(D * 2.f * PI / 3.6f) * FMath::Exp(-D / 7.f); }
				if (Wtor > 0.5f && P.Z > BeltZ + 2.f && P.Z < BeltZ + 12.f) Fold += 0.22f * FMath::Sin((P.Z - BeltZ) * 2.f * PI / 3.f) * (1.f - (P.Z - BeltZ - 2.f) / 10.f);
				const float Cloth = FMath::Clamp(C.R + C.G, 0.f, 1.f);
				Fold += 0.22f * FMath::PerlinNoise3D(FVector(P) * 0.075f) * Cloth;
				VCol[V] = FVector4f(FMath::Clamp(C.R, 0.f, 1.f), FMath::Clamp(C.G, 0.f, 1.f), FMath::Clamp(C.B, 0.f, 1.f), FMath::Clamp(C.A, 0.f, 1.f));
				// UV1: (кеудеше аймағы, бастағы биіктік 0..1), UV2: (бет жағы −1..1, бүйір −1..1) — шаш, орамал, балаклава, көз ойығы
				const float Hh = Whead > 0.3f ? FMath::Clamp((P.Z - ChinZ) / FMath::Max(1.f, TopZ - ChinZ), 0.f, 1.f) : 0.f;
				// бет бедері: мұрын, қас доғасы, көз ойығы, бет сүйегі, ерін, иек, құлақ (манекеннің тегіс басына)
				float Face = 0.f;
				if (Whead > 0.5f)
				{
					const float Fr = (P.Y - HC.Y) / HR, La = (P.X - HC.X) / HR, Al = FMath::Abs(La);
					auto G = [](float X, float S) { return FMath::Exp(-(X * X) / (S * S)); };
					const float Front = FMath::Clamp((Fr - 0.55f) * 4.f, 0.f, 1.f);
					Face += 1.25f * G(La, 0.11f) * G(Hh - 0.46f, 0.07f) * Front;                 // мұрын ұшы
					Face += 0.55f * G(La, 0.08f) * G(Hh - 0.55f, 0.06f) * Front;                 // мұрын қыры
					Face += 0.45f * G(Hh - 0.62f, 0.035f) * (Al < 0.55f ? 1.f : 0.f) * Front;    // қас доғасы
					Face -= 0.4f * G(Al - 0.32f, 0.1f) * G(Hh - 0.545f, 0.045f) * Front;          // көз ойығы
					Face += 0.3f * G(Al - 0.5f, 0.14f) * G(Hh - 0.47f, 0.06f) * Front;            // бет сүйегі
					Face += 0.32f * G(La, 0.17f) * G(Hh - 0.355f, 0.028f) * Front;               // ерін
					Face -= 0.18f * G(La, 0.2f) * G(Hh - 0.31f, 0.018f) * Front;                 // ерін астындағы ойық
					Face += 0.4f * G(La, 0.22f) * G(Hh - 0.2f, 0.06f) * Front;                   // иек
					Face += 1.5f * G(Al - 0.97f, 0.07f) * G(Fr + 0.05f, 0.17f) * G(Hh - 0.5f, 0.085f); // құлақтар
				}
				NewP[V] = P + N * (C.Off + Fold * Cloth + Face);
				VUv[V] = FVector4f(C.Torso * (1.f - FMath::Clamp(Warm * 2.f, 0.f, 1.f)), Hh, (P.Y - HC.Y) / HR, (P.X - HC.X) / HR);
			}
			for (const FVertexID V : MD->Vertices().GetElementIDs())
			{
				Pos[V] = NewP[V];
				for (const FVertexInstanceID I : MD->GetVertexVertexInstanceIDs(V))
				{
					Col[I] = VCol[V];
					UV.Set(I, 1, FVector2f(VUv[V].X, VUv[V].Y));
					UV.Set(I, 2, FVector2f(VUv[V].Z, VUv[V].W));
				}
			}
			// нормальдарды жаңа пішіннен қайта есептеу (манекеннің бұлшықет бедері нормальдарда сақталған)
			// манекеннің панель жіктері «қатты қыр» ретінде белгіленген — тегіс мата үшін бәрін жұмсақ етеміз
			{ int32 NHard = 0; TEdgeAttributesRef<bool> Hard = A.GetEdgeHardnesses(); for (const FEdgeID E : MD->Edges().GetElementIDs()) { NHard += Hard[E] ? 1 : 0; Hard[E] = false; } UE_LOG(LogTemp, Display, TEXT("KAPHUM hard edges cleared: %d of %d"), NHard, MD->Edges().Num()); }
			FStaticMeshOperations::ComputeTriangleTangentsAndNormals(*MD, 0.f);
			FStaticMeshOperations::ComputeTangentsAndNormals(*MD, EComputeNTBsFlags::Normals | EComputeNTBsFlags::Tangents | EComputeNTBsFlags::WeightedNTBs);
			if (FSkeletalMeshLODInfo* LI = SK->GetLODInfo(L)) { LI->BuildSettings.bRecomputeNormals = true; LI->BuildSettings.bRecomputeTangents = true; }
			USkeletalMesh::FCommitMeshDescriptionParams Pm; Pm.bMarkPackageDirty = true;
			SK->CommitMeshDescription(L, Pm);
			Done++;
			UE_LOG(LogTemp, Display, TEXT("KAPHUM %s LOD%d: %d verts, head r=%.1f top=%.1f boot=%.1f belt=%.1f"), *Name, L, MD->Vertices().Num(), HR, TopZ, BootTop, BeltZ);
		}
		if (!Done) { UE_LOG(LogTemp, Warning, TEXT("KAPHUM %s: no mesh description"), *Name); return false; }
		SK->SetHasVertexColors(true);
		SK->SetVertexColorGuid(FGuid::NewGuid());
		SK->Build();
		SK->PostEditChange();
		{
			const FSkeletalMeshRenderData* RD = SK->GetResourceForRendering();
			const FSkeletalMeshRenderData* RS0 = Src->GetResourceForRendering();
			UE_LOG(LogTemp, Display, TEXT("KAPHUM %s render LODs %d (src %d), LOD0 verts %d (src %d), bounds %s (src %s)"), *Name, RD ? RD->LODRenderData.Num() : -1, RS0 ? RS0->LODRenderData.Num() : -1,
				RD && RD->LODRenderData.Num() ? (int32)RD->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer.GetNumVertices() : -1, RS0 && RS0->LODRenderData.Num() ? (int32)RS0->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer.GetNumVertices() : -1,
				*SK->GetImportedBounds().BoxExtent.ToString(), *Src->GetImportedBounds().BoxExtent.ToString());
			for (int32 L2 = 0; L2 < SK->GetLODNum(); L2++) if (const FSkeletalMeshLODInfo* LI = SK->GetLODInfo(L2)) UE_LOG(LogTemp, Display, TEXT("KAPHUM %s LODInfo %d: hasMD=%d recompN=%d reduction=%d"), *Name, L2, SK->HasMeshDescription(L2) ? 1 : 0, LI->BuildSettings.bRecomputeNormals ? 1 : 0, LI->bHasBeenSimplified ? 1 : 0);
		}
		return SaveAsset(SK);
	}

	/** Айналу денесі (Z осі): профиль (ρ, z), ρ-ны бұрышқа қарай созу (каска маңдайшасы т.б.) */
	static void LatheZ(FMeshBuild& B, const TArray<FVector2f>& Prof, int32 Seg, TFunction<float(float Ang, int32 Pi)> RhoK = nullptr, FVector2f Squash = FVector2f(1, 1))
	{
		const int32 Base = B.P.Num(), Np = Prof.Num();
		for (int32 s = 0; s <= Seg; s++)
		{
			const float Ang = s * 2.f * PI / Seg; const float Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
			for (int32 p = 0; p < Np; p++)
			{
				const float K = RhoK ? RhoK(Ang, p) : 1.f;
				const FVector2f T = (Prof[FMath::Min(p + 1, Np - 1)] - Prof[FMath::Max(p - 1, 0)]).GetSafeNormal();
				B.P.Add(V3(Ca * Prof[p].X * K * Squash.X, Sa * Prof[p].X * K * Squash.Y, Prof[p].Y));
				B.N.Add(V3(-Ca * T.Y / Squash.X, -Sa * T.Y / Squash.Y, T.X).GetSafeNormal());
				B.UV.Add(FVector2f(s / (float)Seg, p / (float)(Np - 1)));
			}
		}
		for (int32 s = 0; s < Seg; s++) for (int32 p = 0; p + 1 < Np; p++)
		{
			const int32 a = Base + s * Np + p, b = a + 1, c = a + Np, d = c + 1;
			B.Idx.Append({ a, b, c, b, d, c });
		}
	}
	/** Ось бойынша цилиндрдің SDF-і (C — орта, D — бағыт, R — радиус, H — жарты ұзындық) */
	static float CylA(const V3& P, const V3& C, V3 D, float R, float H)
	{
		D.Normalize(); const V3 Q = P - C; const float t = V3::DotProduct(Q, D);
		const float Rad = (Q - D * t).Size();
		const float Dx = Rad - R, Dy = FMath::Abs(t) - H;
		return FMath::Min(FMath::Max(Dx, Dy), 0.f) + FVector2f(FMath::Max(Dx, 0.f), FMath::Max(Dy, 0.f)).Size();
	}
}

bool UKAPEditorLibrary::CreateHumanAssets()
{
	using namespace KAPHGen;
	using namespace KAPVGen;
	bool bOk = true;
	// ---------------- денелер ----------------
	bOk &= MakeBody(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("SKM_KAP_BodyM"));
	bOk &= MakeBody(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"), TEXT("SKM_KAP_BodyF"));

	// ---------------- M_KAP_Human2: аймақтар вершина түсі мен UV1/UV2 бойынша ----------------
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Human2"));
		auto Comp = [&](UMaterialExpression* E, const TCHAR* Out, int32 X, int32 Y)
		{
			auto* Mu = Expr<UMaterialExpressionMultiply>(M, X, Y); Mu->ConstB = 1.f;
			UMaterialEditingLibrary::ConnectMaterialExpressions(E, Out, Mu, TEXT("A"));
			return (UMaterialExpression*)Mu;
		};
		auto* VC = Expr<UMaterialExpressionVertexColor>(M, -2600, -600);
		UMaterialExpression* R = Comp(VC, TEXT("R"), -2400, -700), *G = Comp(VC, TEXT("G"), -2400, -620), *Bc = Comp(VC, TEXT("B"), -2400, -540), *Ac = Comp(VC, TEXT("A"), -2400, -460);
		auto* T1 = Expr<UMaterialExpressionTextureCoordinate>(M, -2600, -300); T1->CoordinateIndex = 1;
		auto* T2 = Expr<UMaterialExpressionTextureCoordinate>(M, -2600, -150); T2->CoordinateIndex = 2;
		UMaterialExpression* Tor = MaskC(M, T1, true, false, false, -2400, -320), *HH = MaskC(M, T1, false, true, false, -2400, -260);
		UMaterialExpression* Fr = MaskC(M, T2, true, false, false, -2400, -170), *La = MaskC(M, T2, false, true, false, -2400, -110);
		auto* Jacket = VParam(M, TEXT("Jacket"), FLinearColor(0.75f, 0.28f, 0.04f), -1500, -900);
		auto* Pants = VParam(M, TEXT("Pants"), FLinearColor(0.12f, 0.12f, 0.13f), -1500, -800);
		auto* Boots = VParam(M, TEXT("Boots"), FLinearColor(0.03f, 0.025f, 0.02f), -1500, -700);
		auto* Skin = VParam(M, TEXT("Skin"), FLinearColor(0.55f, 0.36f, 0.24f), -1500, -600);
		auto* Hair = VParam(M, TEXT("Hair"), FLinearColor(0.02f, 0.015f, 0.01f), -1500, -500);
		auto* Glove = VParam(M, TEXT("Glove"), FLinearColor(0.08f, 0.07f, 0.05f), -1500, -400);
		auto* Vest = VParam(M, TEXT("Vest"), FLinearColor(0.85f, 0.42f, 0.03f), -1500, -300);
		auto* Stripe = VParam(M, TEXT("Stripe"), FLinearColor(0.8f, 0.8f, 0.7f), -1500, -200);
		UMaterialExpression* C = Lerp(M, Skin, Jacket, R, -1300, -900);
		C = Lerp(M, C, Vest, Mul(M, Mul(M, R, Tor, -1700, -350), SParam(M, TEXT("VestAmt"), 0.f, -1700, -300), -1550, -350), -1200, -850);
		C = Lerp(M, C, Pants, G, -1100, -800);
		// камуфляж (сарбаздар): UV0 бойынша екі шу дағы — киім денемен бірге жылжиды
		{
			auto* T0 = Expr<UMaterialExpressionTextureCoordinate>(M, -2600, 1500);
			UMaterialExpression* P0 = Op2<UMaterialExpressionAppendVector>(M, T0, K(M, 0.f, -2600, 1600), -2450, 1500);
			UMaterialExpression* P1 = Op2<UMaterialExpressionAdd>(M, P0, Col(M, FLinearColor(0.37f, 0.71f, 0.13f), -2600, 1700), -2450, 1650);
			UMaterialExpression* M1 = Ramp(M, NoiseAt(M, P0, 7.f, 2, -2300, 1500), 0.55f, 10.f, -2150, 1500);
			UMaterialExpression* M2 = Ramp(M, NoiseAt(M, P1, 9.f, 2, -2300, 1650), 0.6f, 10.f, -2150, 1650);
			UMaterialExpression* CamoK = Mul(M, SParam(M, TEXT("Camo"), 0.f, -1950, 1550), Sat(M, Op2<UMaterialExpressionAdd>(M, R, G, -1950, 1620), -1800, 1620), -1650, 1560);
			C = Lerp(M, C, VParam(M, TEXT("CamoA"), FLinearColor(0.16f, 0.18f, 0.1f), -1500, 1450), Mul(M, M1, CamoK, -1500, 1520), -1350, 1480);
			C = Lerp(M, C, VParam(M, TEXT("CamoB"), FLinearColor(0.42f, 0.37f, 0.24f), -1350, 1600), Mul(M, M2, CamoK, -1350, 1660), -1200, 1600);
		}
		C = Lerp(M, C, Boots, Bc, -1000, -750);
		C = Lerp(M, C, Lerp(M, Skin, Glove, SParam(M, TEXT("Gloves"), 1.f, -1500, -100), -1300, -100), Ac, -900, -700);
		// бет: қас, ерін, сақал (шаш/орамал/балаклава кейін үстінен жабады)
		{
			UMaterialExpression* HeadK = Sat(M, Mul(M, HH, K(M, 40.f, -2100, 2000), -1950, 2000), -1800, 2000);
			UMaterialExpression* AbsLa = Op1<UMaterialExpressionAbs>(M, La, -2100, 2100);
			auto Bell = [&](UMaterialExpression* X, float C0, float S, int32 Yy) { return Op1<UMaterialExpressionOneMinus>(M, Sat(M, Mul(M, Op1<UMaterialExpressionAbs>(M, SubK(M, X, C0, -2000, Yy), -1900, Yy), K(M, S, -1900, Yy + 50), -1800, Yy), -1700, Yy), -1600, Yy); };
			UMaterialExpression* Brow = Mul(M, Mul(M, Mul(M, Bell(HH, 0.625f, 40.f, 2200), Bell(AbsLa, 0.31f, 6.f, 2300), -1450, 2250), Ramp(M, Fr, 0.6f, 6.f, -1600, 2400), -1300, 2250), SParam(M, TEXT("Brows"), 1.f, -1300, 2350), -1150, 2250);
			C = Lerp(M, C, Mul(M, Hair, K(M, 0.7f, -1150, 2150), -1000, 2150), Mul(M, Brow, K(M, 0.85f, -1000, 2300), -900, 2250), -850, 2150);
			UMaterialExpression* Lip = Mul(M, Mul(M, Bell(HH, 0.355f, 45.f, 2500), Bell(La, 0.f, 6.5f, 2600), -1450, 2550), Ramp(M, Fr, 0.75f, 8.f, -1600, 2700), -1300, 2550);
			C = Lerp(M, C, Mul(M, Skin, Col(M, FLinearColor(0.62f, 0.42f, 0.4f), -1150, 2500), -1000, 2500), Mul(M, Lip, K(M, 0.6f, -1000, 2600), -900, 2550), -850, 2500);
			UMaterialExpression* BeardM = Mul(M, Mul(M, Mul(M, Ramp(M, HH, 0.44f, -14.f, -1600, 2800), Ramp(M, Fr, 0.1f, 4.f, -1600, 2900), -1450, 2850), HeadK, -1300, 2850), SParam(M, TEXT("Beard"), 0.f, -1300, 2950), -1150, 2850);
			C = Lerp(M, C, Hair, BeardM, -850, 2850);
		}
		// шаш (маңдайда жоғарырақ): HH > HairLine + 0.22·front
		UMaterialExpression* Line = Op2<UMaterialExpressionAdd>(M, SParam(M, TEXT("HairLine"), 0.68f, -2100, 0), Mul(M, Sat(M, Fr, -2250, 60), SParam(M, TEXT("HairFront"), 0.22f, -2250, 120), -2100, 60), -1950, 0);
		UMaterialExpression* HairM = Mul(M, Sat(M, Mul(M, Op2<UMaterialExpressionSubtract>(M, HH, Line, -1800, 0), K(M, 25.f, -1800, 60), -1650, 0), -1500, 0), SParam(M, TEXT("HairAmt"), 1.f, -1500, 60), -1350, 0);
		C = Lerp(M, C, Hair, HairM, -800, -650);
		// балаклава: бүкіл бас, көз саңылауынан басқа
		UMaterialExpression* HeadM = Sat(M, Mul(M, HH, K(M, 40.f, -2100, 200), -1950, 200), -1800, 200);
		UMaterialExpression* Slit = Mul(M, Op1<UMaterialExpressionOneMinus>(M, Sat(M, Mul(M, Op1<UMaterialExpressionAbs>(M, SubK(M, HH, 0.55f, -2100, 300), -1950, 300), K(M, 14.f, -1950, 360), -1800, 300), -1650, 300), -1500, 300), Ramp(M, Fr, 0.5f, 6.f, -1650, 400), -1350, 300);
		UMaterialExpression* Cov = Mul(M, Mul(M, SParam(M, TEXT("Cover"), 0.f, -1350, 200), HeadM, -1200, 200), Op1<UMaterialExpressionOneMinus>(M, Slit, -1200, 300), -1050, 220);
		C = Lerp(M, C, Hair, Cov, -700, -600);
		// көз ойықтары (бетті «тірі» етеді, ашық бетте ғана)
		UMaterialExpression* EyeX = Op1<UMaterialExpressionOneMinus>(M, Sat(M, Mul(M, Op1<UMaterialExpressionAbs>(M, SubK(M, Op1<UMaterialExpressionAbs>(M, La, -2100, 500), 0.33f, -1950, 500), -1800, 500), K(M, 8.f, -1800, 560), -1650, 500), -1500, 500), -1350, 500);
		UMaterialExpression* EyeZ = Op1<UMaterialExpressionOneMinus>(M, Sat(M, Mul(M, Op1<UMaterialExpressionAbs>(M, SubK(M, HH, 0.52f, -2100, 620), -1950, 620), K(M, 16.f, -1950, 680), -1800, 620), -1650, 620), -1500, 620);
		UMaterialExpression* Eye = Mul(M, Mul(M, Mul(M, EyeX, EyeZ, -1200, 550), Ramp(M, Fr, 0.55f, 5.f, -1350, 700), -1050, 560), SParam(M, TEXT("Eyes"), 0.5f, -1050, 650), -900, 560);
		Eye = Mul(M, Eye, Op1<UMaterialExpressionOneMinus>(M, Cov, -900, 650), -750, 560);
		C = Mul(M, C, Op1<UMaterialExpressionOneMinus>(M, Eye, -650, 560), -600, -550);
		// шағылыс жолақтары: кеудеде (Z≈31) және балтырда (Z≈−52), актерге қатысты
		auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -2600, 900);
		auto* AP = Expr<UMaterialExpressionActorPositionWS>(M, -2600, 1000);
		UMaterialExpression* Z = MaskC(M, Mul(M, Op2<UMaterialExpressionSubtract>(M, WP, AP, -2450, 950), SParam(M, TEXT("InvScale"), 1.f, -2450, 1050), -2300, 950), false, false, true, -2150, 950);
		auto Band = [&](float Cz, float W, int32 Y) { return Mul(M, Ramp(M, Z, Cz - W, 1.f, -2000, Y), Ramp(M, Z, Cz + W, -1.f, -2000, Y + 90), -1650, Y); };
		UMaterialExpression* Str = Mul(M, Op2<UMaterialExpressionAdd>(M, Mul(M, Band(31.f, 2.5f, 900), Mul(M, R, Tor, -1650, 1100), -1500, 950), Mul(M, Band(-52.f, 2.5f, 1200), G, -1500, 1200), -1350, 1000), SParam(M, TEXT("StripeAmt"), 0.f, -1350, 1100), -1200, 1000);
		C = Lerp(M, C, Stripe, Str, -500, -500);
		// кір/шаң
		UMaterialExpression* Dirt = Lerp(M, K(M, 0.78f, -700, 1300), K(M, 1.05f, -700, 1360), Noise(M, 0.02f, 3, -900, 1300), -500, 1300);
		UMaterialExpression* Base = Mul(M, C, Lerp(M, K(M, 1.f, -400, 1250), Dirt, SParam(M, TEXT("Grime"), 0.6f, -400, 1400), -250, 1300), -100, -400);
		UMaterialEditingLibrary::ConnectMaterialProperty(Base, TEXT(""), MP_BaseColor);
		UMaterialExpression* SkinM = Op1<UMaterialExpressionOneMinus>(M, Sat(M, Op2<UMaterialExpressionAdd>(M, Op2<UMaterialExpressionAdd>(M, R, G, -700, 1500), Op2<UMaterialExpressionAdd>(M, Bc, Op2<UMaterialExpressionAdd>(M, HairM, Cov, -850, 1600), -700, 1580), -550, 1520), -400, 1520), -250, 1520);
		UMaterialExpression* Rough = Lerp(M, Lerp(M, K(M, 0.9f, -400, 1650), K(M, 0.52f, -400, 1700), SkinM, -250, 1650), K(M, 0.35f, -250, 1750), Str, -100, 1650);
		UMaterialEditingLibrary::ConnectMaterialProperty(Rough, TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(Mul(M, Mul(M, Stripe, Str, -300, 1850), SParam(M, TEXT("StripeGlow"), 0.f, -300, 1950), -100, 1850), TEXT(""), MP_EmissiveColor);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		bOk &= SaveAsset(M);
	}

	// ---------------- қатты бөлшектер (табиғи жүйе: X — алға, Y — бүйір, Z — жоғары) ----------------
	{
		// құрылыс каскасы: созыңқы күмбез, алдында ұзын маңдайша, жоғарғы қыры
		const float Pr[][2] = { { 0.01f, 14.f }, { 4, 13.7f }, { 8, 12.3f }, { 11, 9.8f }, { 12.8f, 6.5f }, { 13.5f, 3.2f }, { 13.7f, 1.6f }, { 14.2f, 1.1f }, { 17.f, 0.5f }, { 17.6f, 0.1f }, { 17.2f, -0.35f }, { 13.6f, 0.5f }, { 12.9f, 1.f } };
		TArray<FVector2f> P; for (auto& X : Pr) P.Add(FVector2f(X[0], X[1]));
		FMeshBuild B; LatheZ(B, P, 56, [](float Ang, int32 Pi) { return (Pi >= 8 && Pi <= 10) ? 1.f + 0.32f * FMath::Max(0.f, FMath::Cos(Ang)) - 0.12f * FMath::Max(0.f, -FMath::Cos(Ang)) : 1.f; }, FVector2f(1.08f, 0.96f));
		// жоғарғы қыр (ортадағы жал)
		AddBox(B, V3(0, 0, 13.6f), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), V3(11.f, 1.2f, 0.8f));
		B.FixWinding();
		SaveMesh(TEXT("SM_KAP_HardHat"), B, false, false);
	}
	{
		// әскери дулыға (СШ-68 тектес): биік күмбез, шетке қарай кеңейген жиек
		const float Pr[][2] = { { 0.01f, 15.f }, { 5, 14.4f }, { 9.5f, 12.f }, { 12.4f, 8.5f }, { 13.8f, 4.5f }, { 14.3f, 1.f }, { 15.f, -1.2f }, { 15.8f, -2.3f }, { 15.4f, -2.8f }, { 14.f, -1.8f }, { 13.2f, 0.5f } };
		TArray<FVector2f> P; for (auto& X : Pr) P.Add(FVector2f(X[0], X[1]));
		FMeshBuild B; LatheZ(B, P, 56, nullptr, FVector2f(1.07f, 0.97f)); B.FixWinding();
		SaveMesh(TEXT("SM_KAP_Helmet"), B, false, false);
	}
	{
		// қазақ қалпағы: ақ киіз, қайырылған қара жиек (екі аймақ)
		const float Pr0[][2] = { { 0.01f, 15.5f }, { 4.f, 15.2f }, { 7.5f, 13.5f }, { 9.5f, 10.f }, { 10.3f, 6.f }, { 10.4f, 3.f } };
		const float Pr1[][2] = { { 10.4f, 3.f }, { 11.5f, 1.2f }, { 13.2f, 2.4f }, { 14.2f, 6.f }, { 14.f, 7.8f }, { 12.8f, 7.2f }, { 11.2f, 4.5f } };
		TArray<FVector2f> P0, P1; for (auto& X : Pr0) P0.Add(FVector2f(X[0], X[1])); for (auto& X : Pr1) P1.Add(FVector2f(X[0], X[1]));
		auto Split = [](float Ang, int32) { return 1.f - 0.05f * FMath::Abs(FMath::Sin(Ang)); };
		FMeshBuild B0, B1; LatheZ(B0, P0, 48, Split, FVector2f(1.06f, 0.95f)); LatheZ(B1, P1, 48, Split, FVector2f(1.06f, 0.95f));
		B0.FixWinding(); B1.FixWinding();
		SaveMesh(TEXT("SM_KAP_Kalpak_0"), B0, false, false);
		SaveMesh(TEXT("SM_KAP_Kalpak_3"), B1, false, false);
	}
	{
		// кепка: күмбез + алдыңғы күнқағар
		FSdf F = [](const V3& P) -> float
		{
			float D = FMath::Max(FVector3f(P.X / 1.07f, P.Y / 0.97f, P.Z).Size() - 12.2f, -P.Z + 0.5f);
			D = FMath::Max(D, -(FVector3f(P.X / 1.07f, P.Y / 0.97f, P.Z).Size() - 11.2f));                                     // қуыс
			const float Vis = FMath::Max(Rect2(P.X, P.Y, 6.f, 19.f, -9.5f, 9.5f, 7.f), FMath::Abs(P.Z - 1.2f + (P.X - 8.f) * 0.12f) - 0.55f);
			return Smin(D, Vis, 0.8f);
		};
		TArray<FMask> Mk;
		Mk.Add({ KAPVehSpec::Dark, [](const V3& P) { return 11.7f - P.Z; } });                                                   // түйме
		BuildShell(TEXT("SM_KAP_Cap"), F, Mk, V3(-15, -14, -2), V3(21, 14, 14), 0.4f);
	}
	{
		// респиратор (жартылай маска): резеңке бет бөлігі, екі сүзгі, алдыңғы клапан
		FSdf F = [](const V3& P) -> float
		{
			float Mk = RBox(P, -1.f, 8.5f, -6.5f, 6.5f, -8.f, 4.f, 4.5f);
			Mk = Smax(Mk, Plane(P, V3(8.5f, 0, 4.f), V3(0.6f, 0, 1)), 2.f);                                                       // мұрын үсті жіңішкереді
			Mk = Smax(Mk, -RBox(P, -3.f, 6.f, -5.f, 5.f, -6.5f, 2.5f, 3.5f), 0.6f);                                             // ішкі қуыс
			for (float S : { -1.f, 1.f }) Mk = Smin(Mk, CylA(P, V3(4.f, S * 6.8f, -4.f), V3(0.45f, S * 0.9f, -0.1f), 3.4f, 1.7f), 0.8f);
			Mk = Smin(Mk, CylA(P, V3(8.6f, 0, -4.5f), V3(1, 0, -0.2f), 2.f, 0.9f), 0.5f);
			return Mk;
		};
		TArray<FMask> Mk;
		Mk.Add({ KAPVehSpec::Second, [](const V3& P) { float D = 1e9f; for (float S : { -1.f, 1.f }) D = FMath::Min(D, CylA(P, V3(4.f, S * 6.8f, -4.f), V3(0.45f, S * 0.9f, -0.1f), 3.5f, 1.8f)); return D; } });
		Mk.Add({ KAPVehSpec::Dark, [](const V3& P) { return CylA(P, V3(8.6f, 0, -4.5f), V3(1, 0, -0.2f), 2.1f, 1.f); } });
		BuildShell(TEXT("SM_KAP_Respirator"), F, Mk, V3(-5, -13, -12), V3(13, 13, 8), 0.35f);
	}
	{
		// рюкзак (арқаға: X=0 — арқа, −X — сыртқа): негізгі қап, қақпақ, алдыңғы және бүйір қалталар, белбеу жіктері
		FSdf F = [](const V3& P) -> float
		{
			float Bp = RBox(P, -16.f, 0.f, -15.f, 15.f, -22.f, 22.f, 5.f);
			Bp = Smin(Bp, RBox(P, -17.5f, 1.f, -15.6f, 15.6f, 15.f, 24.f, 4.f), 1.f);                                          // қақпақ
			Bp = Smin(Bp, RBox(P, -21.f, -13.f, -10.f, 10.f, -18.f, 2.f, 3.f), 1.f);                                           // алдыңғы қалта
			for (float S : { -1.f, 1.f }) Bp = Smin(Bp, CylA(P, V3(-8.f, S * 16.5f, -9.f), V3(0, 0, 1), 4.f, 9.f), 1.2f);      // бүйір қалталар
			return Bp;
		};
		TArray<FMask> Mk;
		Mk.Add({ KAPVehSpec::Trim, [](const V3& P) { return FMath::Max(Rect2(P.Y, P.Z, -1.6f, 1.6f, 13.f, 17.f, 0.5f), -15.f - P.X); } }); // тоға
		Mk.Add({ KAPVehSpec::Dark, [](const V3& P) { return FMath::Min(FMath::Abs(P.Z - 15.f) - 0.5f, FMath::Max(FMath::Abs(FMath::Abs(P.Y) - 6.f) - 1.2f, FMath::Max(-21.5f - P.X, -13.f + P.X))); } }); // қақпақ жігі мен белдіктер
		BuildShell(TEXT("SM_KAP_Backpack"), F, Mk, V3(-24, -22, -25), V3(4, 22, 27), 0.6f);
	}
	{
		// бірінші тұлға қолы: қолғап киген жұдырық (буын бұдырлары, бүгілген саусақтар, бас бармақ, білезік)
		auto Seg = [](const V3& P, const V3& A, const V3& B, float R) { const V3 Pa = P - A, Ba = B - A; const float h = FMath::Clamp(V3::DotProduct(Pa, Ba) / Ba.SizeSquared(), 0.f, 1.f); return (Pa - Ba * h).Size() - R; };
		FSdf F = [Seg](const V3& P) -> float
		{
			float D = RBox(P, -4.f, 3.5f, -3.f, 3.f, -4.6f, 4.6f, 2.2f);
			for (int32 i = 0; i < 4; i++)
			{
				const float Z = -3.3f + i * 2.2f, R = i == 0 ? 0.9f : 1.05f;
				const float Fg = FMath::Min(FMath::Min(Seg(P, V3(2.5f, -2.6f, Z), V3(5.2f, -1.2f, Z), R), Seg(P, V3(5.2f, -1.2f, Z), V3(5.f, 1.8f, Z), R)), Seg(P, V3(5.f, 1.8f, Z), V3(3.2f, 3.f, Z), R));
				D = Smin(D, Fg, 0.7f);
			}
			D = Smin(D, FMath::Min(Seg(P, V3(-1.5f, 2.6f, 3.6f), V3(2.2f, 3.2f, 5.f), 1.25f), Seg(P, V3(2.2f, 3.2f, 5.f), V3(4.6f, 2.f, 4.6f), 1.1f)), 0.8f);
			D = Smin(D, RBox(P, -9.f, -3.f, -2.8f, 2.8f, -3.4f, 3.4f, 2.f), 1.2f);
			return D;
		};
		TArray<FMask> Mk;
		Mk.Add({ KAPVehSpec::Dark, [](const V3& P) { return FMath::Abs(P.X + 5.5f) - 0.5f; } });                                 // білезік жігі
		BuildShell(TEXT("SM_KAP_Fist"), F, Mk, V3(-10, -5, -7), V3(8, 6, 8), 0.25f);
	}
	{
		// жең: бірлік цилиндр (радиус 50, биіктік 100) — қатпарлары мен манжеті бар
		TArray<FVector2f> P;
		for (int32 i = 0; i <= 40; i++)
		{
			const float Z = -50.f + i * 2.5f;
			float Rho = 47.f + 2.2f * FMath::Sin(Z * 0.21f) + 1.2f * FMath::Sin(Z * 0.53f + 1.f);
			if (Z < -44.f) Rho += 3.5f;                                                                                          // манжет
			P.Add(FVector2f(Rho, Z));
		}
		P.Insert(FVector2f(0.01f, -50.f), 0); P.Add(FVector2f(0.01f, 50.f));
		// LatheZ профилі жоғарыдан төмен күтеді — кері ретпен
		Algo::Reverse(P);
		FMeshBuild B; LatheZ(B, P, 32); B.FixWinding();
		SaveMesh(TEXT("SM_KAP_Sleeve"), B, false, false);
	}
	{
		// полиция фуражкасы: сопақ төбе, жиек белбеуі, қара күнқағар, алтын белгі
		FSdf F = [](const V3& P) -> float
		{
			const V3 Q(P.X / 1.07f, P.Y, P.Z);
			float Crown = FMath::Max(V3(Q.X / 14.f, Q.Y / 12.6f, (Q.Z - 8.6f) / 3.4f).Size() - 1.f, Q.Z - 10.6f) * 3.f;
			const float Band = FMath::Max(FMath::Sqrt(Q.X * Q.X + Q.Y * Q.Y) - 10.9f, FMath::Max(-Q.Z, Q.Z - 7.8f));
			float D = Smin(Crown, Band, 2.5f);
			D = FMath::Max(D, -(FMath::Max(FMath::Sqrt(Q.X * Q.X + Q.Y * Q.Y) - 10.f, FMath::Max(-Q.Z - 1.f, Q.Z - 7.f))));            // ішкі қуыс
			const float Vis = FMath::Max(Rect2(P.X, P.Y, 6.f, 18.f, -9.5f, 9.5f, 8.f), FMath::Abs(P.Z - 1.f + (P.X - 8.f) * 0.28f) - 0.45f);
			D = Smin(D, Vis, 0.6f);
			D = Smin(D, CylA(P, V3(12.3f, 0, 8.6f), V3(1, 0, 0.2f), 1.7f, 0.5f), 0.3f);                                               // белгі
			return D;
		};
		TArray<FMask> Mk;
		Mk.Add({ KAPVehSpec::Trim, [](const V3& P) { return CylA(P, V3(12.3f, 0, 8.6f), V3(1, 0, 0.2f), 1.9f, 0.7f); } });
		Mk.Add({ KAPVehSpec::Dark, [](const V3& P) { return FMath::Max(Rect2(P.X, P.Y, 6.f, 18.5f, -10.f, 10.f, 8.f), FMath::Abs(P.Z - 1.f + (P.X - 8.f) * 0.28f) - 0.7f); } });
		Mk.Add({ KAPVehSpec::Second, [](const V3& P) { return FMath::Max(P.Z - 7.f, 0.2f - P.Z); } });
		BuildShell(TEXT("SM_KAP_PoliceCap"), F, Mk, V3(-17, -15, -4), V3(21, 15, 14), 0.35f);
	}
	{
		// атыс жалыны: алға созылған үш қиылысқан «жалын» жапырақшасы + алдыңғы жұлдыз (екі жақты)
		FMeshBuild B;
		auto Poly = [&](const TArray<FVector2f>& Pts, FVector2f Ctr, V3 Ax, V3 Ay, V3 Nn)
		{
			for (int32 Side = 0; Side < 2; Side++)
			{
				const V3 N = Side ? -Nn : Nn;
				const int32 Base = B.P.Num();
				B.P.Add(Ax * Ctr.X + Ay * Ctr.Y); B.N.Add(N); B.UV.Add(FVector2f(0.5f, 0.5f));
				for (const FVector2f& Q : Pts) { B.P.Add(Ax * Q.X + Ay * Q.Y); B.N.Add(N); B.UV.Add(FVector2f(Q.X / 16.f, Q.Y / 8.f + 0.5f)); }
				for (int32 i = 0; i < Pts.Num(); i++) B.Idx.Append({ Base, Base + 1 + i, Base + 1 + (i + 1) % Pts.Num() });
			}
		};
		const TArray<FVector2f> Flame = { FVector2f(0, 0.6f), FVector2f(3, 2.2f), FVector2f(5, 1.1f), FVector2f(8, 3.f), FVector2f(10, 0.9f), FVector2f(16, 0), FVector2f(10, -0.9f), FVector2f(8, -3.f), FVector2f(5, -1.1f), FVector2f(3, -2.2f), FVector2f(0, -0.6f) };
		for (int32 k = 0; k < 3; k++)
		{
			const float A = k * PI / 3.f;
			const V3 Up(0, FMath::Sin(A), FMath::Cos(A));
			Poly(Flame, FVector2f(4.f, 0), V3(1, 0, 0), Up, V3::CrossProduct(V3(1, 0, 0), Up));
		}
		TArray<FVector2f> Star;
		for (int32 i = 0; i < 16; i++) { const float A = i * PI / 8.f, Rr = (i % 2 == 0) ? 4.2f : 1.4f; Star.Add(FVector2f(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr)); }
		{
			const V3 Ax(0, 1, 0), Ay(0, 0, 1);
			for (int32 Side = 0; Side < 2; Side++)
			{
				const V3 N = Side ? V3(-1, 0, 0) : V3(1, 0, 0);
				const int32 Base = B.P.Num();
				B.P.Add(V3(1.f, 0, 0)); B.N.Add(N); B.UV.Add(FVector2f(0.5f, 0.5f));
				for (const FVector2f& Q : Star) { B.P.Add(V3(1.f, 0, 0) + Ax * Q.X + Ay * Q.Y); B.N.Add(N); B.UV.Add(FVector2f(Q.X / 8.f + 0.5f, Q.Y / 8.f + 0.5f)); }
				for (int32 i = 0; i < Star.Num(); i++) B.Idx.Append({ Base, Base + 1 + i, Base + 1 + (i + 1) % Star.Num() });
			}
		}
		B.FixWinding();
		SaveMesh(TEXT("SM_KAP_Muzzle"), B, false, false);
	}
	UE_LOG(LogTemp, Display, TEXT("KAPHUM done: %d"), bOk ? 1 : 0);
	return bOk;
}

// ---------------- белгілер үшін кирилл қаріпі (TextRender әдепкі қаріпінде кирилл жоқ) ----------------
#include "Factories/TrueTypeFontFactory.h"
#include "Engine/Font.h"
#include "Engine/FontImportOptions.h"
bool UKAPEditorLibrary::CreateSignFont()
{
	UTrueTypeFontFactory* Fac = NewObject<UTrueTypeFontFactory>();
	if (!Fac->ImportOptions) Fac->SetupFontImportOptions();
	FFontImportOptionsData& O = Fac->ImportOptions->Data;
	O.FontName = TEXT("Arial");
	O.Height = 48.f;
	O.bEnableAntialiasing = true;
	O.bEnableBold = true;
	O.bAlphaOnly = true;
	O.UnicodeRange = TEXT("0020-007E,00A0-00FF,0400-04FF,2013-2014,2026,2116,20B8");
	O.bUseDistanceFieldAlpha = true;
	O.DistanceFieldScaleFactor = 8;
	O.DistanceFieldScanRadiusScale = 1.f;
	O.TexturePageMaxHeight = 2048; O.TexturePageWidth = 1024;
	UPackage* Pkg = MakePkg(TEXT("/Game/KAP/Fonts/F_KAP_Sign"));
	UFont* F = Cast<UFont>(Fac->FactoryCreateNew(UFont::StaticClass(), Pkg, FName("F_KAP_Sign"), RF_Public | RF_Standalone, nullptr, GWarn));
	UE_LOG(LogTemp, Display, TEXT("KAPFONT %s chars=%d textures=%d"), F ? TEXT("ok") : TEXT("FAILED"), F ? F->Characters.Num() : 0, F ? F->Textures.Num() : 0);
	return F && SaveAsset(F);
}
