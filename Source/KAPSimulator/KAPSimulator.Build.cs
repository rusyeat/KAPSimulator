// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KAPSimulator : ModuleRules
{
	public KAPSimulator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"Landscape",
			"AudioMixer",
			"AudioExtensions",
			"RenderCore",
			"RHI"
		});

		PublicIncludePaths.AddRange(new string[] {
			"KAPSimulator",
			"KAPSimulator/SportsCar",
			"KAPSimulator/OffroadCar",
			"KAPSimulator/Variant_OffRoad",
			"KAPSimulator/Variant_TimeTrial",
			"KAPSimulator/Variant_TimeTrial/UI"
		});
	}
}
