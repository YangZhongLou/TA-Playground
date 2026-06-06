using UnrealBuildTool;

public class TAPlayground : ModuleRules
{
	public TAPlayground(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore",
			"ProceduralMeshComponent"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
