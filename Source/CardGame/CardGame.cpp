// Fantasy Card Battle - CardGame module implementation

#include "CardGame.h"

DEFINE_LOG_CATEGORY(LogCardGame);

void FCardGameModule::StartupModule()
{
	UE_LOG(LogCardGame, Log, TEXT("CardGame module started."));
}

void FCardGameModule::ShutdownModule()
{
	UE_LOG(LogCardGame, Log, TEXT("CardGame module shut down."));
}

IMPLEMENT_PRIMARY_GAME_MODULE(FCardGameModule, CardGame, "CardGame");
