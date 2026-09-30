// Fantasy Card Battle - Game target (packaging / standalone build)

using UnrealBuildTool;
using System.Collections.Generic;

public class FantasyCardBattleTarget : TargetRules
{
	public FantasyCardBattleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

		// Unreal Engine 5.8 build settings
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.Add("CardGame");
	}
}
