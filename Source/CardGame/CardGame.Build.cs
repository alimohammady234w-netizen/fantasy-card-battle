// Fantasy Card Battle - CardGame runtime module build rules

using UnrealBuildTool;

public class CardGame : ModuleRules
{
	public CardGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core gameplay dependencies.
		// UI (UMG/Slate), DataAsset and additional systems will be added
		// as private dependencies in later phases.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
