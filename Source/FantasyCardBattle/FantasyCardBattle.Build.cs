// Copyright (c) Fantasy Card Battle. All rights reserved.

using UnrealBuildTool;

public class FantasyCardBattle : ModuleRules
{
	public FantasyCardBattle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[]
		{
			// Allows `#include "FCBMatchRules.h"` style includes from inside Source/FantasyCardBattle/Public.
			"FantasyCardBattle/Public"
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"
		});

		// The rules engine, AI and card database are intentionally free of UMG/Engine headers so they can be
		// compiled headless (see Tools/MockUE) for automated balance simulation and unit tests.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		bEnableExceptions = false;

		// Steam / Android shipping tweaks live in Config/*.ini and Docs/Platforms.md.
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicDefinitions.Add("FCB_MOBILE_BUILD=1");
		}
		else
		{
			PublicDefinitions.Add("FCB_MOBILE_BUILD=0");
		}
	}
}
