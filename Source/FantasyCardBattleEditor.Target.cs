// Fantasy Card Battle - Editor target (development / PIE)

using UnrealBuildTool;
using System.Collections.Generic;

public class FantasyCardBattleEditorTarget : TargetRules
{
	public FantasyCardBattleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		// Unreal Engine 5.8 build settings
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.Add("CardGame");
	}
}
