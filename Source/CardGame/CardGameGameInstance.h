// Fantasy Card Battle - Main GameInstance (infrastructure only)

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CardGameGameInstance.generated.h"

/**
 * Main GameInstance of the project.
 *
 * Lives for the whole application session (survives level travel).
 * Phase 1 responsibilities: initialization/shutdown logging only.
 *
 * Later phases will host session-wide, save-related or data-driven services here
 * (NOT widget/UI references - gameplay state must stay separate from the UI).
 */
UCLASS()
class CARDGAME_API UCardGameGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;
};
