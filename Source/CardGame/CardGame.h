// Fantasy Card Battle - CardGame module header

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Shared log category for the whole CardGame module.
// Filter the Output Log by "LogCardGame" to see all game infrastructure messages.
DECLARE_LOG_CATEGORY_EXTERN(LogCardGame, Log, All);

/**
 * Primary game module.
 * Loaded automatically by the engine at startup (see FantasyCardBattle.uproject).
 */
class FCardGameModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
