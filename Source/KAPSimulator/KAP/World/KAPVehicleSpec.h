// Көліктердің көрінетін геометриясы: корпус мештері (редактор генераторы, KAPVehicleGen.inl) мен
// ойындағы дөңгелектер (KAPVehicleBody.cpp) бірдей сандарды қолданады — өзгертсеңіз, мештерді қайта генерациялаңыз.
#pragma once
#include "CoreMinimal.h"

struct FKAPVehSpec
{
	const TCHAR* Id;
	/** Көрінетін база (алдыңғы мен артқы осьтің арасы), см — физикалық база 303 см */
	float WB;
	/** Дөңгелек радиусы, см */
	float R;
	/** Дөңгелек ортасының бүйірлік қашықтығы (жарты із), см */
	float Track;
	/** Шина ені, см */
	float TireW;
};

namespace KAPVehSpec
{
	/** Шаблон көлігінің физикалық базасы (барлық көлікте бірдей) */
	constexpr float PhysWB = 303.f;
	inline const FKAPVehSpec* Find(const FString& Id)
	{
		static const FKAPVehSpec S[] = {
			{ TEXT("uaz469"), 240.f, 39.f, 72.f, 22.f },
			{ TEXT("bukhanka"), 232.f, 39.f, 73.f, 22.f },
			{ TEXT("niva"), 222.f, 34.f, 71.f, 19.f },
			{ TEXT("kamaz"), 303.f, 52.f, 100.f, 30.f },
			{ TEXT("water"), 303.f, 52.f, 100.f, 30.f },
			{ TEXT("btr"), 303.f, 56.f, 118.f, 34.f },
		};
		for (const FKAPVehSpec& X : S) if (Id == X.Id) return &X;
		return nullptr;
	}
	/** Корпус мешінің аймақтары (әрқайсысы — бөлек меш, өз материалымен) */
	enum ERegion : int32 { Paint = 0, Second = 1, Glass = 2, Dark = 3, Trim = 4, NumRegions = 5 };
}
