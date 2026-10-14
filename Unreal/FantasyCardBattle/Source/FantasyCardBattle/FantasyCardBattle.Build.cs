using UnrealBuildTool;
public class FantasyCardBattle : ModuleRules { public FantasyCardBattle(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "Niagara" }); } }
