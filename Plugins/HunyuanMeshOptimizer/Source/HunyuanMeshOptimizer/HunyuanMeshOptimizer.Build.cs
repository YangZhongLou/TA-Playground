using UnrealBuildTool;

public class HunyuanMeshOptimizer : ModuleRules
{
	public HunyuanMeshOptimizer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"UnrealEd",
			"LevelEditor",
			"ToolMenus",
			"DesktopPlatform",
			"AssetTools",
			"Projects"
		});
	}
}
