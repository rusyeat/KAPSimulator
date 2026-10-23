// Жер бедері мен биомдардың ортақ сипаттамасы (бәрі метрмен).
// Редактор ландшафтты осыдан құрады (KAPEditorLibrary::CreateTerrain), ойын суды, көпірлерді, өсімдіктерді,
// жануарларды, батпақ пен қадам беттерін осыдан алады — екеуі әрқашан бірдей.
//
// Карта (2.42 км): шығыстағы таулардың шатқалынан өзен шығып, батысқа қарай иіріліп ағады, x < −640 м-де
// үш тармаққа бөлініп (атырап, қамысты батпақ), батыстағы көлге құяды. Құм тек екі жерде қалды:
// оңтүстіктегі оазис айналасы және солтүстік-шығыстағы «Құмды төбе». Қалғаны — шөпті дала, ауыл маңында
// және өзен жайылмасында егістік. Шекарада бөктерлер жартасты тауға ұласады (оның ары жағы — SM_KAP_Mountains).
#pragma once

#include "CoreMinimal.h"

namespace KAPTerrain
{
	// ---------------- шу ----------------
	inline float Hash(int32 I, int32 J)
	{
		uint32 H = (uint32)I * 374761393u + (uint32)J * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H = H ^ (H >> 16);
		return H / 4294967295.f;
	}
	inline float Noise(float X, float Z)
	{
		const int32 I = FMath::FloorToInt32(X), J = FMath::FloorToInt32(Z);
		const float Fx = X - I, Fz = Z - J, U = Fx * Fx * (3 - 2 * Fx), V = Fz * Fz * (3 - 2 * Fz);
		const float A = Hash(I, J), B = Hash(I + 1, J), C = Hash(I, J + 1), D = Hash(I + 1, J + 1);
		return A + (B - A) * U + (C - A) * V + (A - B - C + D) * U * V;
	}
	inline float Fbm(float X, float Z, int32 O) { float S = 0, A = 0.5f, F = 1; for (int32 k = 0; k < O; k++) { S += A * Noise(X * F, Z * F); F *= 2; A *= 0.5f; } return S; }
	inline float Ridged(float X, float Z, int32 O) { float S = 0, A = 0.5f, F = 1; for (int32 k = 0; k < O; k++) { const float N = 1.f - FMath::Abs(2.f * Noise(X * F, Z * F) - 1.f); S += A * N * N; F *= 2.03f; A *= 0.5f; } return S; }
	inline float Smooth(float E0, float E1, float X) { const float T = FMath::Clamp((X - E0) / (E1 - E0), 0.f, 1.f); return T * T * (3 - 2 * T); }

	// ---------------- өзен, атырап, көл, оазис ----------------
	constexpr float DeltaX0 = -640.f;            // атырап басталатын жер
	constexpr float DeltaX1 = -900.f;            // тармақтар көлге құятын жер
	constexpr float LakeX = -960.f, LakeY = 15.f, LakeR = 120.f;
	constexpr float OasisX = 20.f, OasisY = -460.f, OasisR = 30.f;
	constexpr float MainHalfW = 14.f;            // өзеннің су беті (жарты ені)
	constexpr float BranchHalfW = 7.f;

	inline float RiverY(float X) { return 60.f + 120.f * FMath::Sin(X / 260.f) + 10.f * FMath::Sin(X / 83.f + 0.7f); }
	inline float RiverSlope(float X) { return 120.f / 260.f * FMath::Cos(X / 260.f) + 10.f / 83.f * FMath::Cos(X / 83.f + 0.7f); }
	/** Аңғар табаны: шығыс биік, батыс (көл) аласа — су шығыстан батысқа ағады */
	inline float ValleyZ(float X) { return 0.012f * FMath::Clamp(X, DeltaX1, 1300.f); }
	inline float WaterZ(float X) { return ValleyZ(X) - 0.9f; }
	inline float LakeZ() { return WaterZ(DeltaX1); }

	/** Атырап тармағының ортаңғы сызығы (k = 0, 1, 2), X < DeltaX0 */
	inline float BranchY(int32 K, float X)
	{
		const float T = FMath::Clamp((DeltaX0 - X) / (DeltaX0 - DeltaX1), 0.f, 1.2f);
		const float S = 95.f * FMath::Sin(FMath::Min(T, 1.f) * UE_HALF_PI);
		const float Yc = RiverY(DeltaX0) + (DeltaX0 - X) * 0.1f;
		return Yc + (K - 1) * S + (22.f * FMath::Sin(X / 70.f + 2.1f * K) + 6.f * FMath::Sin(X / 23.f + K)) * FMath::Min(T, 1.f); // иректелген тармақтар
	}
	/** Ең жақын су арнасы: қашықтық (м) және сол арнаның жарты ені; арна жоқ болса — үлкен сан */
	inline float ChannelDist(float X, float Y, float& OutHalfW)
	{
		OutHalfW = MainHalfW;
		float Best = 1e6f;
		if (X >= DeltaX0 - 30.f)
		{
			const float S = RiverSlope(X);
			Best = FMath::Abs(Y - RiverY(X)) / FMath::Sqrt(1.f + S * S);
			if (X < DeltaX0) Best += (DeltaX0 - X) * 0.6f; // басты арна атырапқа кіргенде тармақтарға бөлінеді
		}
		if (X < DeltaX0 + 10.f && X > LakeX - LakeR)
			for (int32 K = 0; K < 3; K++)
			{
				const float D = FMath::Abs(Y - BranchY(K, X));
				const float W = K == 1 ? 9.f : BranchHalfW;
				if (D - W < Best - OutHalfW) { Best = D; OutHalfW = W; }
			}
		return Best;
	}
	/** Ландшафттың жарты ені (м); одан ары — SM_KAP_Mountains */
	constexpr float MapHalf = 1209.6f;
	/** Шығыс шатқалы (X > MapHalf): өзен таулардың арасынан шығады; шетте ландшафт арнасымен бір бағытта */
	inline float GorgeY(float X)
	{
		const float D = X - MapHalf;
		return RiverY(MapHalf) + RiverSlope(MapHalf) * 700.f * FMath::Sin(D / 700.f) + 40.f * (1.f - FMath::Cos(D / 900.f));
	}
	inline float GorgeFloor(float X) { return ValleyZ(MapHalf) - 2.f + (X - MapHalf) * 0.012f; }
	inline float GorgeWaterZ(float X) { return GorgeFloor(X) + 1.1f; }
	/** Көл мен оазис жағасы шеңбер емес — бұрышқа қарай толқынды (табиғи пішін) */
	inline float LakeWobble(float A) { return 20.f * FMath::Sin(3.f * A + 1.f) + 11.f * FMath::Sin(5.f * A + 2.3f) + 6.f * FMath::Sin(9.f * A + 0.4f); }
	inline float OasisWobble(float A) { return 4.f * FMath::Sin(3.f * A + 0.5f) + 2.5f * FMath::Sin(7.f * A + 1.f); }
	inline float LakeDist(float X, float Y) { const float Dx = X - LakeX, Dy = Y - LakeY; return FMath::Sqrt(Dx * Dx + Dy * Dy) - LakeWobble(FMath::Atan2(Dy, Dx)); }
	inline float OasisDist(float X, float Y) { const float Dx = X - OasisX, Dy = Y - OasisY; return FMath::Sqrt(Dx * Dx + Dy * Dy) - OasisWobble(FMath::Atan2(Dy, Dx)); }

	// ---------------- биомдар ----------------
	/** Құм төбелері (0..1): оңтүстіктегі оазис шөлі және солтүстік-шығыстағы «Құмды төбе» */
	inline float DuneMask(float X, float Y)
	{
		const float N = 0.18f * (Fbm(X / 260.f + 7.1f, Y / 260.f - 2.3f, 3) - 0.5f);
		const float A = FMath::Sqrt(FMath::Square((X - 60.f) / 480.f) + FMath::Square((Y + 440.f) / 260.f)) + N;
		const float B = FMath::Sqrt(FMath::Square((X - 600.f) / 320.f) + FMath::Square((Y - 1000.f) / 220.f)) + N;
		float M = FMath::Max(1.f - Smooth(0.72f, 1.f, A), 1.f - Smooth(0.68f, 1.f, B));
		M *= Smooth(55.f, 120.f, OasisDist(X, Y)); // оазистің айналасы — тегіс
		return M;
	}
	/** Шекара бөктерлері (0..1): 1080 м-ден басталып, шетінде тік жартасқа айналады; өзен шатқалы мен көл жағасында — жоқ */
	inline float FoothillMask(float X, float Y)
	{
		const float DInf = FMath::Max(FMath::Abs(X), FMath::Abs(Y));
		float M = Smooth(1080.f, 1195.f, DInf);
		float Hw; const float Dc = ChannelDist(X, Y, Hw);
		if (X > 850.f) M *= Smooth(28.f, 85.f, Dc);
		M *= Smooth(LakeR + 10.f, LakeR + 70.f, LakeDist(X, Y));
		return M;
	}
	/** Тұрақты ылғал (0..1): өзен жағасы, атырап батпағы, көл мен оазис жағасы */
	inline float WetMask(float X, float Y)
	{
		float Hw; const float Dc = ChannelDist(X, Y, Hw);
		float W = 1.f - Smooth(Hw * 1.5f, Hw * 1.5f + 18.f, Dc);
		if (X < DeltaX0 && X > LakeX) // тармақтар арасы — қамысты батпақ
		{
			const float Band = 1.f - Smooth(100.f, 135.f, FMath::Abs(Y - BranchY(1, X)));
			W = FMath::Max(W, Band * (0.55f + 0.45f * Fbm(X / 40.f, Y / 40.f, 2)) * Smooth(0.f, 60.f, DeltaX0 - X));
		}
		W = FMath::Max(W, 1.f - Smooth(LakeR, LakeR + 22.f, LakeDist(X, Y)));
		W = FMath::Max(W, 1.f - Smooth(OasisR + 4.f, OasisR + 28.f, OasisDist(X, Y)));
		return FMath::Clamp(W, 0.f, 1.f);
	}

	// ---------------- егістік ----------------
	/** Егін түрі: 0 — бидай (сары), 1 — жас егін (жасыл), 2 — жыртылған жер */
	struct FField { float X0, Y0, X1, Y1; int32 Crop; };
	inline TArray<FField>& FieldStore() { static TArray<FField> F; return F; }
	/** Егіс алқаптары: ауылдар маңы және өзен жайылмасы; жолдан, нысаннан, судан, құмнан аулақ.
	 *  RoadDist / SiteClear — шақырушы береді (редактор CSV-ден, ойын графтан) — нәтиже екеуінде бірдей */
	inline void BuildFields(TFunctionRef<float(float, float)> RoadDist, TFunctionRef<bool(float, float, float)> SiteClear)
	{
		TArray<FField>& Out = FieldStore();
		Out.Reset();
		auto TryRect = [&](float Cx, float Cy, float Wd, float Ht, int32 Crop)
		{
			const float X0 = Cx - Wd / 2, X1 = Cx + Wd / 2, Y0 = Cy - Ht / 2, Y1 = Cy + Ht / 2;
			const float Px[5] = { X0, X1, X0, X1, Cx }, Py[5] = { Y0, Y0, Y1, Y1, Cy };
			for (int32 i = 0; i < 5; i++)
			{
				float Hw;
				if (RoadDist(Px[i], Py[i]) < 11.f || !SiteClear(Px[i], Py[i], 18.f)) return;
				if (ChannelDist(Px[i], Py[i], Hw) < Hw * 1.5f + 14.f || DuneMask(Px[i], Py[i]) > 0.15f || FoothillMask(Px[i], Py[i]) > 0.02f) return;
				if (LakeDist(Px[i], Py[i]) < LakeR + 40.f || OasisDist(Px[i], Py[i]) < 90.f) return;
			}
			for (const FField& F : Out) if (X0 < F.X1 + 5.f && X1 > F.X0 - 5.f && Y0 < F.Y1 + 5.f && Y1 > F.Y0 - 5.f) return;
			Out.Add({ X0, Y0, X1, Y1, Crop });
		};
		// ауыл маңы (Ақсай, Жосалы): сақина бойымен
		const float Vx[2] = { -520.f, 800.f }, Vy[2] = { 900.f, 850.f };
		for (int32 v = 0; v < 2; v++)
			for (int32 r = 0; r < 3; r++)
				for (int32 a = 0; a < 14; a++)
				{
					const float Ang = a / 14.f * UE_TWO_PI + r * 0.37f, Rad = 130.f + r * 75.f;
					TryRect(Vx[v] + FMath::Cos(Ang) * Rad, Vy[v] + FMath::Sin(Ang) * Rad, 74.f, 46.f, (a + r + v) % 3);
				}
		// өзен жайылмасы: екі жағалау бойымен
		for (float X = -560.f; X <= 620.f; X += 82.f)
			for (int32 s = -1; s <= 1; s += 2)
				for (int32 r = 0; r < 2; r++)
					TryRect(X, RiverY(X) + s * (62.f + r * 56.f), 74.f, 46.f, (int32)(Hash((int32)X, s * 7 + r) * 2.99f));
	}
	/** Егістік маскасы: 0 — жоқ, (Crop+1)/3 — бар; шеті 2 м жұмсақ */
	inline float FieldMask(float X, float Y, int32* OutCrop = nullptr)
	{
		for (const FField& F : FieldStore())
			if (X > F.X0 - 2.f && X < F.X1 + 2.f && Y > F.Y0 - 2.f && Y < F.Y1 + 2.f)
			{
				const float E = FMath::Min(FMath::Min(X - F.X0, F.X1 - X), FMath::Min(Y - F.Y0, F.Y1 - Y));
				if (OutCrop) *OutCrop = F.Crop;
				return Smooth(-2.f, 2.f, E);
			}
		return 0.f;
	}

	// ---------------- биіктік ----------------
	/** Ландшафт биіктігі (м), нысандар тегістелмеген күйінде */
	inline float Height(float X, float Y)
	{
		// дала: аңғардан 2.5 м жоғары, баяу толқынды, ұсақ бұдырлар
		float H = ValleyZ(X) * 0.85f + 2.5f + 6.f * (Fbm(X / 700.f + 3.1f, Y / 700.f - 1.7f, 4) - 0.5f) + 2.2f * (Fbm(X / 110.f, Y / 110.f, 3) - 0.5f) + 0.3f * (Fbm(X / 13.f, Y / 13.f, 2) - 0.5f);
		// құм төбелері (бұрынғы бедердің жұмсартылғаны)
		const float Dm = DuneMask(X, Y);
		if (Dm > 0.f)
		{
			const float Warp = 3.f * Fbm(X / 300.f, Y / 300.f, 2);
			const float R = 1.f - FMath::Abs(FMath::Sin((X * 0.72f + Y * 0.31f) / 46.f + Warp));
			H += Dm * (9.f * Fbm(X / 260.f + 11.3f, Y / 260.f - 4.1f, 4) + R * R * 4.f + 0.5f * Fbm(X / 12.f, Y / 12.f, 2));
		}
		// өзен аңғары: жайылма тегістеледі, арна қазылады
		float Hw; const float Dc = ChannelDist(X, Y, Hw);
		const float Valley = ValleyZ(X) + 0.35f * (Fbm(X / 35.f, Y / 35.f, 2) - 0.5f);
		H = FMath::Lerp(H, Valley, 1.f - Smooth(Hw * 2.f + 12.f, Hw * 2.f + 75.f, Dc));
		const float Wb = Hw * 1.45f;
		if (Dc < Wb) H = FMath::Min(H, Valley - (Hw > 10.f ? 2.4f : 1.5f) * (1.f - FMath::Square(Dc / Wb)));
		// көл: жағасы тегіс, ортасы 3.5 м терең
		const float Ld = LakeDist(X, Y);
		H = FMath::Lerp(H, ValleyZ(DeltaX1), 1.f - Smooth(LakeR, LakeR + 60.f, Ld));
		if (Ld < LakeR + 8.f) H = FMath::Min(H, LakeZ() + 0.6f - 3.5f * (1.f - FMath::Square(FMath::Min(Ld / (LakeR + 8.f), 1.f))) - 0.4f);
		// оазис: айналасы тегіс, ортасында бұлақ көлшігі
		const float Od = OasisDist(X, Y);
		const float Ho = 2.5f + 6.f * (Fbm(OasisX / 700.f + 3.1f, OasisY / 700.f - 1.7f, 4) - 0.5f);
		H = FMath::Lerp(H, Ho, 1.f - Smooth(45.f, 110.f, Od));
		if (Od < OasisR + 4.f) H = FMath::Min(H, Ho - 0.4f - 2.2f * (1.f - FMath::Square(Od / (OasisR + 4.f))));
		// шекара: алдымен баяу көтеріледі, сосын тік жартасты бөктер
		const float DInf = FMath::Max(FMath::Abs(X), FMath::Abs(Y));
		H += Smooth(860.f, 1010.f, DInf) * 10.f * Smooth(LakeR + 20.f, LakeR + 80.f, Ld);
		const float Fm = FoothillMask(X, Y);
		if (Fm > 0.f) H += Fm * (38.f + 30.f * Ridged(X / 160.f + 4.4f, Y / 160.f - 9.2f, 5)) + Fm * Fm * 8.f * (Fbm(X / 18.f, Y / 18.f, 3) - 0.5f); // жайпақ бөктер (тау шетке қарай біртіндеп биіктейді)
		return H;
	}

	/** Су беті: бар болса true, OutZ — су деңгейі (м), OutDepth — шамамен тереңдік */
	inline bool WaterAt(float X, float Y, float& OutZ, float* OutDepth = nullptr)
	{
		float Hw; const float Dc = ChannelDist(X, Y, Hw);
		if (Dc < Hw) { OutZ = WaterZ(X); if (OutDepth) *OutDepth = (Hw > 10.f ? 1.5f : 0.6f) * (1.f - FMath::Square(Dc / Hw)); return true; }
		const float Ld = LakeDist(X, Y);
		if (Ld < LakeR) { OutZ = LakeZ(); if (OutDepth) *OutDepth = 3.f * (1.f - FMath::Square(Ld / LakeR)); return true; }
		const float Od = OasisDist(X, Y);
		if (Od < OasisR)
		{
			OutZ = 2.5f + 6.f * (Fbm(OasisX / 700.f + 3.1f, OasisY / 700.f - 1.7f, 4) - 0.5f) - 1.2f;
			if (OutDepth) *OutDepth = 1.4f * (1.f - FMath::Square(Od / OasisR));
			return true;
		}
		return false;
	}
}
