using UnrealBuildTool;
using System.Collections.Generic;

public class TAPlaygroundEditorTarget : TargetRules
{
	public TAPlaygroundEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		ExtraModuleNames.Add("TA-Playground");
	}
}
