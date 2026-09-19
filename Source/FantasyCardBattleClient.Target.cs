// Copyright (c) Fantasy Card Battle. All rights reserved.
// Client target: used for "distribution only" builds (no editor content), which is what you
// want for a Steam/Android shipping build.

using UnrealBuildTool;
using System.Collections.Generic;

public class FantasyCardBattleClientTarget : TargetRules
{
	public FantasyCardBattleClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("FantasyCardBattle");
	}
}
