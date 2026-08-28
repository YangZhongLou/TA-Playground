using UnrealBuildTool;
using System.Collections.Generic;

public class TAPlaygroundTarget : TargetRules
{
	public TAPlaygroundTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		bOverrideBuildEnvironment = true;
		ExtraModuleNames.Add("TAPlayground");
	}
}
