// Fantasy Card Battle - CardGame runtime module build rules

using UnrealBuildTool;

public class CardGame : ModuleRules
{
	public CardGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core gameplay dependencies.
		// - GameplayTags: card tag system (Stage 2), used by public headers
		//   (CardTypes.h / CardDataAsset.h) so it must be a PUBLIC dependency.
		// - UI (UMG/Slate) and additional systems will be added
		//   as private dependencies in later phases.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Stage 3: CardID -> UCardDataAsset fallback lookup when cards are
			// organized in subfolders (runtime asset registry scan of /Game/Data).
			"AssetRegistry"
		});
	}
}
