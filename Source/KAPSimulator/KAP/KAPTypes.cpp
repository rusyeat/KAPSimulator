#include "KAPTypes.h"

namespace KAP
{
	FText RegionName(int32 R)
	{
		static const TCHAR* N[4] = { TEXT("1-аймақ · Ақсай"), TEXT("2-аймақ · Жосалы"), TEXT("3-аймақ · Кеніш орталығы"), TEXT("4-аймақ · Қаражар") };
		return FText::FromString(N[FMath::Clamp(R, 0, 3)]);
	}
	FText FaultName(EFaultType F)
	{
		switch (F)
		{
		case EFaultType::Pump: return FText::FromString(TEXT("Сорғы тоқтауы"));
		case EFaultType::Pipe: return FText::FromString(TEXT("Құбыр жарылуы"));
		case EFaultType::Electric: return FText::FromString(TEXT("Электр ақауы"));
		case EFaultType::Acid: return FText::FromString(TEXT("Қышқыл ағуы"));
		case EFaultType::Fire: return FText::FromString(TEXT("Өрт"));
		default: return FText::FromString(TEXT("—"));
		}
	}
	FText WellStateName(EWellState S)
	{
		switch (S)
		{
		case EWellState::Ok: return FText::FromString(TEXT("Жұмыс істейді"));
		case EWellState::Fault: return FText::FromString(TEXT("1-кезең: ақау"));
		case EWellState::Leak: return FText::FromString(TEXT("2-кезең: ағу"));
		case EWellState::Fire: return FText::FromString(TEXT("3-кезең: өрт"));
		default: return FText::FromString(TEXT("Жоғалды"));
		}
	}
	FText WeatherName(EWeatherType W)
	{
		switch (W)
		{
		case EWeatherType::Heat: return FText::FromString(TEXT("Ыстық толқыны"));
		case EWeatherType::Storm: return FText::FromString(TEXT("Құм дауылы"));
		case EWeatherType::Rain: return FText::FromString(TEXT("Жаңбыр"));
		case EWeatherType::Thunder: return FText::FromString(TEXT("Найзағайлы жаңбыр"));
		case EWeatherType::Cloudy: return FText::FromString(TEXT("Бұлтты күн"));
		case EWeatherType::Windy: return FText::FromString(TEXT("Қатты жел"));
		case EWeatherType::Fog: return FText::FromString(TEXT("Тұман"));
		default: return FText::FromString(TEXT("Ашық күн"));
		}
	}
	FString WindDirName(float Yaw)
	{
		static const TCHAR* N[8] = { TEXT("солтүстіктен"), TEXT("солтүстік-шығыстан"), TEXT("шығыстан"), TEXT("оңтүстік-шығыстан"), TEXT("оңтүстіктен"), TEXT("оңтүстік-батыстан"), TEXT("батыстан"), TEXT("солтүстік-батыстан") };
		const float Bearing = FMath::Fmod(Yaw + 270.f + 720.f, 360.f);
		return N[FMath::RoundToInt32(Bearing / 45.f) % 8];
	}
	FName FaultTool(EFaultType F)
	{
		switch (F)
		{
		case EFaultType::Pump: return "pump";
		case EFaultType::Pipe: return "pipe";
		case EFaultType::Electric: return "electric";
		case EFaultType::Acid: return "acid";
		case EFaultType::Fire: return "fire";
		default: return NAME_None;
		}
	}
	EFaultType FaultFromName(FName N)
	{
		if (N == "pump") return EFaultType::Pump;
		if (N == "pipe") return EFaultType::Pipe;
		if (N == "electric") return EFaultType::Electric;
		if (N == "acid") return EFaultType::Acid;
		if (N == "fire") return EFaultType::Fire;
		return EFaultType::None;
	}
	FString FmtTime(double Hours)
	{
		const int32 H = FMath::FloorToInt32(Hours) % 24;
		const int32 M = FMath::FloorToInt32(FMath::Frac(Hours) * 60.0);
		return FString::Printf(TEXT("%02d:%02d"), H, M);
	}
	FString FmtDur(double Hours)
	{
		if (!FMath::IsFinite(Hours) || Hours > 999) return TEXT("—");
		if (Hours < 1) return FString::Printf(TEXT("%d мин"), FMath::Max(1, FMath::RoundToInt32(Hours * 60)));
		const int32 H = FMath::FloorToInt32(Hours), M = FMath::RoundToInt32((Hours - H) * 60);
		return M ? FString::Printf(TEXT("%d сағ %d мин"), H, M) : FString::Printf(TEXT("%d сағ"), H);
	}
	TArray<FString> RepairSteps(EFaultType F)
	{
		switch (F)
		{
		case EFaultType::Pump: return { TEXT("part:pump_part"), TEXT("valve") };
		case EFaultType::Pipe: return { TEXT("valve"), TEXT("part:pipe"), TEXT("pressure") };
		case EFaultType::Electric: return { TEXT("wires:cable"), TEXT("pressure") };
		case EFaultType::Acid: return { TEXT("valve"), TEXT("part:sealant"), TEXT("pressure") };
		case EFaultType::Fire: return { TEXT("fire:extinguisher"), TEXT("wires") };
		default: return {};
		}
	}
}
