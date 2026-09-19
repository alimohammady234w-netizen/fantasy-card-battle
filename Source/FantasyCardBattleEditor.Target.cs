// Copyright (c) Fantasy Card Battle. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FantasyCardBattleEditorTarget : TargetRules
{
	public FantasyCardBattleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("FantasyCardBattle");
	}
}
