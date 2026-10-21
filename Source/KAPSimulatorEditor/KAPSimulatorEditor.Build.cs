using UnrealBuildTool;

public class KAPSimulatorEditor : ModuleRules
{
	public KAPSimulatorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "KAPSimulator" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "Landscape", "LandscapeEditor", "AssetTools", "AssetRegistry", "MaterialEditor", "Foliage", "EditorScriptingUtilities", "LevelEditor", "Slate", "SlateCore", "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription", "AnimationCore", "PhysicsCore" });
	}
}
