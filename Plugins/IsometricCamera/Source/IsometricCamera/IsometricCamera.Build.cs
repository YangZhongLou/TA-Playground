using UnrealBuildTool;

public class IsometricCamera : ModuleRules
{
	public IsometricCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"CinematicCamera"
		});
	}
}
