// Copyright (c) Fantasy Card Battle. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FantasyCardBattleTarget : TargetRules
{
	public FantasyCardBattleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bUsesSteam = false; // flip to true once the Steam AppId is issued (see Docs/Platforms.md)

		ExtraModuleNames.Add("FantasyCardBattle");
	}
}
