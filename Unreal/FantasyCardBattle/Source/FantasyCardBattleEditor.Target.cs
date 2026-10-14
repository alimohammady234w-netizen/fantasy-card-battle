using UnrealBuildTool; using System.Collections.Generic;
public class FantasyCardBattleEditorTarget : TargetRules { public FantasyCardBattleEditorTarget(TargetInfo Target) : base(Target) { Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.V5; IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4; ExtraModuleNames.Add("FantasyCardBattle"); } }
