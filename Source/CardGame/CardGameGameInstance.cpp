// Fantasy Card Battle - Main GameInstance implementation

#include "CardGameGameInstance.h"

#include "CardGame.h"

void UCardGameGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogCardGame, Log, TEXT("UCardGameGameInstance::Init - game instance initialized."));
}

void UCardGameGameInstance::Shutdown()
{
	UE_LOG(LogCardGame, Log, TEXT("UCardGameGameInstance::Shutdown - game instance shutting down."));

	Super::Shutdown();
}
