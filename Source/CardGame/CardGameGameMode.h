// Fantasy Card Battle - Main GameMode (infrastructure only)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CardGameGameMode.generated.h"

/**
 * Main GameMode of the project.
 *
 * Phase 1 responsibilities (infrastructure only):
 * - Assign the project PlayerController class.
 * - Spawn players as SpectatorPawns (UI-driven card game, no direct pawn movement).
 *
 * Battle / Deck / AI / Shop systems are intentionally NOT implemented here.
 * They will be added in later phases, kept separate from the UI layer.
 */
UCLASS()
class CARDGAME_API ACardGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACardGameGameMode();

protected:
	virtual void BeginPlay() override;
};
