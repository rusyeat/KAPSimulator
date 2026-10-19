// UKAPSurvivalAttributeSet — GAS атрибуттары: HP, Су, Дене температурасы, Доза, Төзімділік
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "KAPSurvivalAttributeSet.generated.h"

UCLASS()
class KAPSIMULATOR_API UKAPSurvivalAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	UKAPSurvivalAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Survival") FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UKAPSurvivalAttributeSet, Health);
	UPROPERTY(BlueprintReadOnly, Category = "Survival") FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UKAPSurvivalAttributeSet, MaxHealth);
	UPROPERTY(BlueprintReadOnly, Category = "Survival") FGameplayAttributeData Water;
	ATTRIBUTE_ACCESSORS_BASIC(UKAPSurvivalAttributeSet, Water);
	UPROPERTY(BlueprintReadOnly, Category = "Survival") FGameplayAttributeData BodyTemp;
	ATTRIBUTE_ACCESSORS_BASIC(UKAPSurvivalAttributeSet, BodyTemp);
	UPROPERTY(BlueprintReadOnly, Category = "Survival") FGameplayAttributeData Dose;
	ATTRIBUTE_ACCESSORS_BASIC(UKAPSurvivalAttributeSet, Dose);
	UPROPERTY(BlueprintReadOnly, Category = "Survival") FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UKAPSurvivalAttributeSet, Stamina);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
};
