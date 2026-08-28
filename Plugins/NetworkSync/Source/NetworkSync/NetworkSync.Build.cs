using UnrealBuildTool;

public class NetworkSync : ModuleRules
{
	public NetworkSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"NetCore",
			"Sockets",
			"Mover",
		});
	}
}
