// ---------------------------------------------------------------------
// Жеке модельдер (SDF → surface nets, KAPVehicleGen.inl-дегі BuildShell):
//  • SM_KAP_Spruce_0 — шекара тауларындағы шырша (10 м, қабат-қабат бұтақтар, Nanite) + M_KAP_Spruce
//  • SM_KAP_SuitPile_0/1/3 — ұңғыма маңында тасталған қорғаныш костюмі: жерге құлап жиырылған мата,
//    шағылыс жолақтары, резеңке етік пен қолғап (бұрынғы қораптардан құралған «дене» орнына)
// KAPEditorLibrary.cpp ішіне KAPVehicleGen.inl мен KAPNatureGen.inl-ден кейін қосылады.
// ---------------------------------------------------------------------

namespace KAPPGen
{
	using KAPVGen::V3;
	/** A–B кесіндісіне дейінгі қашықтық − R; Zk — тік бағытта қысу (жалпақ түтік) */
	inline float Cap(const V3& P, const V3& A, const V3& B, float R, float Zk = 1.f)
	{
		const V3 Pa = P - A, Ba = B - A;
		const float H = FMath::Clamp(V3::DotProduct(Pa, Ba) / FMath::Max(1e-4f, Ba.SizeSquared()), 0.f, 1.f);
		V3 D = Pa - Ba * H; D.Z *= Zk;
		return D.Size() - R;
	}
	inline float Ell(const V3& P, const V3& C, const V3& R)
	{
		const V3 Q((P.X - C.X) / R.X, (P.Y - C.Y) / R.Y, (P.Z - C.Z) / R.Z);
		return (Q.Size() - 1.f) * FMath::Min(R.X, FMath::Min(R.Y, R.Z));
	}
}

bool UKAPEditorLibrary::CreatePropMeshes()
{
	using namespace KAPVGen;
	using namespace KAPPGen;
	int32 Tot = 0;
	// ================= шырша: 10 м, діңі 2 м-ге дейін көрінеді, 9 қабат бұтақ, шеттері ирек =================
	{
		FSdf F = [](const V3& P) -> float
		{
			const float Rho = FMath::Sqrt(P.X * P.X + P.Y * P.Y), Th = FMath::Atan2(P.Y, P.X);
			const float Trunk = FMath::Max(Rho - (15.f - P.Z * 0.008f), FMath::Max(-P.Z, P.Z - 900.f));
			float Crown = 1e9f;
			if (P.Z > 90.f && P.Z < 1010.f)
			{
				const float T = FMath::Clamp((P.Z - 120.f) / 880.f, 0.f, 1.f);
				const float Rb = 300.f * FMath::Pow(1.f - T, 1.05f) + 5.f;
				const float TierH = 880.f / 9.f;
				const float Tf = (P.Z - 120.f) / TierH, Fr = Tf - FMath::FloorToFloat(Tf);
				const float Skirt = 0.74f + 0.26f * FMath::Pow(1.f - Fr, 1.7f);            // әр қабат төменге қарай салбырайды
				const float Jag = 0.9f + 0.07f * FMath::Sin(7.f * Th + FMath::FloorToFloat(Tf) * 1.7f) + 0.04f * FMath::Sin(13.f * Th + P.Z * 0.05f);
				Crown = (Rho - Rb * Skirt * Jag) * 0.6f;
				Crown = FMath::Max(Crown, FMath::Max(120.f - P.Z, P.Z - 1000.f));
			}
			return Smin(Trunk, Crown, 10.f);
		};
		TArray<FMask> M;
		M.Add({ KAPVehSpec::Dark, [](const V3& P) { return FMath::Max(P.Z - 126.f, FMath::Sqrt(P.X * P.X + P.Y * P.Y) - 26.f); } }); // діңнің төменгі бөлігі
		Tot += BuildShell(TEXT("SM_KAP_Spruce"), F, M, V3(-320, -320, -14), V3(320, 320, 1014), 7.f);
	}
	// ================= тасталған қорғаныш костюмі (комбинезон): жерде жайылып, жиырылып жатыр =================
	{
		// адам пішіні емес: шешіліп лақтырылған комбинезон — жиырылған үйінді, бір балағы (етігімен) мен бір жеңі (қолғабымен) шығып жатыр
		FSdf F = [](const V3& P) -> float
		{
			const float Zk = 1.35f;
			float B = Ell(P, V3(0, 0, 6), V3(31, 25, 11));                                                   // негізгі үйінді
			B = Smin(B, Ell(P, V3(-20, 12, 5), V3(22, 17, 8.5f)), 6.f);                                      // бүктелген балақтар
			B = Smin(B, Ell(P, V3(22, -10, 9), V3(12, 11, 8)), 5.f);                                         // капюшон бүрмесі
			B = Smin(B, Cap(P, V3(-18, 10, 5), V3(-58, 24, 4), 7.5f, Zk), 4.f);                              // шығып жатқан балақ
			B = Smin(B, RBox(P, -76, -56, 15, 33, -2, 13, 4), 2.5f);                                         // етік
			B = Smin(B, Cap(P, V3(12, 16, 6), V3(30, 42, 3), 6.f, Zk), 4.f);                                 // жең
			B = Smin(B, Ell(P, V3(34, 47, 2.8f), V3(6.5f, 5, 3.5f)), 2.f);                                   // қолғап
			// мата қатпарлары: ірі бүрмелер мен ұсақ жыйырылу (үйіндінің үстінде күштірек)
			const float Fold = 2.2f * (KAPTerrain::Noise(P.X / 7.f + 3.f, P.Y / 7.f - 1.f + P.Z * 0.04f) - 0.5f)
				+ 0.9f * FMath::Sin(P.X * 0.45f + P.Y * 0.3f + 3.f * KAPTerrain::Noise(P.X / 15.f, P.Y / 15.f))
				+ 0.5f * (KAPTerrain::Noise(P.X / 3.f - 7.f, P.Y / 3.f + 2.f) - 0.5f);
			B += Fold * FMath::Clamp(P.Z / 4.f, 0.f, 1.f);
			return FMath::Max(B, -P.Z);                                                                       // жерге жабысқан табан
		};
		TArray<FMask> M;
		M.Add({ KAPVehSpec::Dark, [](const V3& P) { return FMath::Min(RBox(P, -79, -54, 12, 36, -3, 15, 2), Ell(P, V3(34, 47, 2.8f), V3(8, 6.5f, 5))); } });
		M.Add({ KAPVehSpec::Second, [](const V3& P) { return FMath::Max(FMath::Min(FMath::Abs(P.X - 0.35f * P.Y - 4.f) - 2.1f, FMath::Abs(P.X + 44.f) - 2.f), 5.f - P.Z); } }); // шағылыс жолақтары
		Tot += BuildShell(TEXT("SM_KAP_SuitPile"), F, M, V3(-84, -40, -3), V3(48, 58, 26), 1.1f);
	}
	// ================= M_KAP_Spruce: қою жасыл, әр ағаш өз реңкімен =================
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Spruce"));
		Usages(M);
		auto* PIR = Expr<UMaterialExpressionPerInstanceRandom>(M, -1400, 0);
		auto* N = Noise(M, 0.004f, 2, -1400, 200);
		auto* VN = Expr<UMaterialExpressionVertexNormalWS>(M, -1400, 400);
		UMaterialExpression* C = Lerp(M, Col(M, FLinearColor(0.028f, 0.05f, 0.026f), -1200, -100), Col(M, FLinearColor(0.06f, 0.085f, 0.04f), -1200, -40), PIR, -1050, -80);
		C = Mul(M, C, Op2<UMaterialExpressionAdd>(M, Mul(M, N, K(M, 0.45f, -1200, 260), -1050, 220), K(M, 0.78f, -1050, 290), -900, 220), -750, 0);
		// бұтақ қабатының үсті жарығырақ (күн түседі), асты қараңғы
		C = Mul(M, C, Op2<UMaterialExpressionAdd>(M, Mul(M, MaskC(M, VN, false, false, true, -1200, 400), K(M, 0.25f, -1050, 460), -900, 400), K(M, 0.9f, -900, 460), -750, 400), -600, 100);
		UMaterialEditingLibrary::ConnectMaterialProperty(C, TEXT(""), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.88f, -600, 300), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.25f, -600, 360), TEXT(""), MP_Specular);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
		if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/KAP/Meshes/SM_KAP_Spruce_0.SM_KAP_Spruce_0"))) { SM->SetMaterial(0, M); SM->PostEditChange(); SaveAsset(SM); }
	}
	// ================= M_KAP_Puff: бу/түтін/шаң бұлттары — шет жұмсақ, іші ағып жатқан шу (шар емес, үлпек), бетке жанасқан жері біртіндеп сөнеді =================
	{
		UMaterial* M = NewMaterial(TEXT("M_KAP_Puff"));
		M->BlendMode = BLEND_Translucent;
		// жарықпен боялады (күн түскен жағы ақ, көлеңкеде сұр) — жарықсыз ақ бу күндізгі сахнада сұр дақ болып көрінген
		M->SetShadingModel(MSM_DefaultLit);
		M->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;   // күн мен көлеңке пиксель бойынша (көлемдік тор тым күңгірт)
		M->TwoSided = false;
		auto* C = VParam(M, TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f), -1200, -200);
		auto* E = VParam(M, TEXT("Emissive"), FLinearColor::Black, -1200, -80);
		UMaterialEditingLibrary::ConnectMaterialProperty(Sat(M, C, -1050, -200), TEXT(""), MP_BaseColor);
		// бу жарықты шашыратады: көлеңкеде де ақшыл (аспан жарығының баламасы)
		UMaterialExpression* Amb = Mul(M, Sat(M, C, -1050, -120), SParam(M, TEXT("Ambient"), 0.35f, -1050, -60), -900, -120);
		UMaterialEditingLibrary::ConnectMaterialProperty(Op2<UMaterialExpressionAdd>(M, E, Amb, -750, -100), TEXT(""), MP_EmissiveColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 1.f, -900, -40), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(K(M, 0.f, -900, 20), TEXT(""), MP_Specular);
		// шет: камераға қарайтын жағы тығыз, шетіне қарай сөнеді (≈ cos²)
		auto* Fr = Expr<UMaterialExpressionFresnel>(M, -1200, 100); Fr->Exponent = 1.f;
		UMaterialExpression* Edge = Op1<UMaterialExpressionOneMinus>(M, Fr, -1050, 100);
		Edge = Mul(M, Edge, Edge, -900, 100);
		// ішкі үлпектер: әлем кеңістігіндегі шу жоғары қарай ағады (бу көтеріледі)
		auto* WP = Expr<UMaterialExpressionWorldPosition>(M, -1500, 300);
		auto* Tm = Expr<UMaterialExpressionTime>(M, -1500, 420);
		UMaterialExpression* Pos = Op2<UMaterialExpressionAdd>(M, WP, Mul(M, Tm, Col(M, FLinearColor(9.f, 4.f, -38.f), -1350, 460), -1200, 420), -1050, 300);
		auto* N1 = Noise(M, 0.018f, 3, -900, 300); N1->Position.Connect(0, Pos);
		auto* N2 = Noise(M, 0.05f, 2, -900, 450); N2->Position.Connect(0, Pos);
		UMaterialExpression* Wisp = Ramp(M, Op2<UMaterialExpressionAdd>(M, N1, Mul(M, N2, K(M, 0.45f, -750, 500), -600, 450), -450, 350), 0.42f, 1.6f, -300, 350);
		UMaterialExpression* Op = Mul(M, Mul(M, SParam(M, TEXT("Opacity"), 0.5f, -600, 200), Edge, -450, 150), Wisp, -300, 150);
		auto* DF = Expr<UMaterialExpressionDepthFade>(M, -150, 200); DF->FadeDistanceDefault = 45.f;
		DF->InOpacity.Connect(0, Op);
		UMaterialEditingLibrary::ConnectMaterialProperty(DF, TEXT(""), MP_Opacity);
		Usages(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveAsset(M);
	}
	UE_LOG(LogTemp, Display, TEXT("KAPPROPS done tris=%d"), Tot);
	return Tot > 0;
}
