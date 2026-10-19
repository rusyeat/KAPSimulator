#include "KAPSurvivalAttributeSet.h"

UKAPSurvivalAttributeSet::UKAPSurvivalAttributeSet()
{
	InitHealth(100.f); InitMaxHealth(100.f); InitWater(80.f); InitBodyTemp(36.6f); InitDose(0.f); InitStamina(100.f);
}

void UKAPSurvivalAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	else if (Attribute == GetMaxHealthAttribute()) NewValue = FMath::Clamp(NewValue, 30.f, 100.f);
	else if (Attribute == GetWaterAttribute() || Attribute == GetStaminaAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetBodyTempAttribute()) NewValue = FMath::Clamp(NewValue, 30.f, 43.f);
	else if (Attribute == GetDoseAttribute()) NewValue = FMath::Max(0.f, NewValue);
}
