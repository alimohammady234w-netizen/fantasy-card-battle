#include "FCBProgressionSubsystem.h"
#include "FCBSaveGame.h"
#include "FCBBattleRules.h"
#include "FCBCardDefinition.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "AssetRegistry/AssetRegistryModule.h"

static const FString SlotMain = TEXT("FCB_Save"), SlotBackup = TEXT("FCB_Save_Backup");

void UFCBProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); FString Report; LoadCardRegistry(Report); UE_LOG(LogTemp, Log, TEXT("[FCB] Registry: %s"), *Report); LoadOrInitialize(); }

bool UFCBProgressionSubsystem::LoadCardRegistry(FString& OutReport) {
	Registry.Reset(); TArray<FString> Problems; int32 Count = 0;
	if (const UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Cards.DT_Cards"))) {
		for (const auto& Pair : DT->GetRowMap()) { const FFCBCardTableRow* Row = (const FFCBCardTableRow*)Pair.Value;
			if (!Row) continue; ++Count;
			if (Registry.Contains(Row->CardId)) Problems.Add(FString::Printf(TEXT("Duplicate CardId %s"), *Row->CardId.ToString()));
			if (!Row->Attributes.IsValid()) Problems.Add(FString::Printf(TEXT("Invalid attributes on %s"), *Row->CardId.ToString()));
			if (Row->Artwork.IsNull()) Problems.Add(FString::Printf(TEXT("Missing artwork on %s"), *Row->CardId.ToString()));
			Registry.Add(Row->CardId, *Row); }
	} else Problems.Add(TEXT("DT_Cards not found at /Game/Data/DT_Cards (import Data/cards.csv - see INSTALL_UE58.md step 8)"));
	// Also register any Primary Data Assets of type Card (Asset Manager scan) - data-only expansion path.
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets; ARM.Get().GetAssetsByClass(UFCBCardDefinition::StaticClass()->GetClassPathName(), Assets);
	for (const FAssetData& AD : Assets) if (const UFCBCardDefinition* Def = Cast<UFCBCardDefinition>(AD.GetAsset())) { FString Err; if (Def->IsValidDefinition(Err) && !Registry.Contains(Def->CardId)) { FFCBCardTableRow Row; Row.CardId = Def->CardId; Row.DisplayName = Def->DisplayName; Row.Category = Def->Category; Row.Rarity = Def->Rarity; Row.Ability = Def->Ability; Row.Flavor = Def->Flavor; Row.Attributes = Def->Attributes; Row.Artwork = Def->Artwork; Registry.Add(Row.CardId, Row); ++Count; } }
	OutReport = FString::Printf(TEXT("%d definitions registered. %s"), Registry.Num(), Problems.IsEmpty() ? TEXT("Validation OK.") : *FString::Join(Problems, TEXT("; ")));
	return Problems.IsEmpty();
}
FFCBOwnedCard UFCBProgressionSubsystem::GetOwned(FName CardId) const { if (const FFCBOwnedCard* O = Owned.Find(CardId)) return *O; FFCBOwnedCard D; D.DefinitionId = CardId; return D; }
TMap<FName, int32> UFCBProgressionSubsystem::OwnedCopiesMap() const { TMap<FName, int32> M; for (const auto& P : Owned) M.Add(P.Key, P.Value.Copies); return M; }
void UFCBProgressionSubsystem::GrantCardXP(FName CardId, int32 XP) { FFCBOwnedCard& O = Owned.FindOrAdd(CardId); O.DefinitionId = CardId; O.XP = FMath::Max(0, O.XP + XP); }
bool UFCBProgressionSubsystem::TryUpgradeCard(FName CardId, FString& OutError) {
	FFCBOwnedCard* O = Owned.Find(CardId); if (!O) { OutError = TEXT("Card not owned."); return false; }
	if (O->Level >= FFCBBattleRules::MaxCardLevel) { OutError = TEXT("Already at max level."); return false; }
	const int32 Cost = FFCBBattleRules::XPForNextLevel(O->Level); if (O->XP < Cost) { OutError = FString::Printf(TEXT("Needs %d XP, has %d."), Cost, O->XP); return false; }
	O->XP -= Cost; ++O->Level; SaveNow(); OnProgressChanged.Broadcast(); return true; // base definition untouched
}
FGuid UFCBProgressionSubsystem::CreateDeck(const FString& Name) { FFCBDeck D; D.DeckId = FGuid::NewGuid(); D.Name = Name; Decks.Add(D); SaveNow(); return D.DeckId; }
bool UFCBProgressionSubsystem::DeleteDeck(FGuid Id) { if (Decks.Num() <= 1) return false; Decks.RemoveAll([&](const FFCBDeck& D) { return D.DeckId == Id; }); if (ActiveDeckId == Id && Decks.Num()) ActiveDeckId = Decks[0].DeckId; SaveNow(); return true; }
bool UFCBProgressionSubsystem::SetActiveDeck(FGuid Id) { if (!Decks.ContainsByPredicate([&](const FFCBDeck& D) { return D.DeckId == Id; })) return false; ActiveDeckId = Id; SaveNow(); return true; }
bool UFCBProgressionSubsystem::ValidateActiveDeck(TArray<FString>& OutErrors) const {
	const FFCBDeck* D = Decks.FindByPredicate([&](const FFCBDeck& X) { return X.DeckId == ActiveDeckId; });
	if (!D) { OutErrors.Add(TEXT("No active deck.")); return false; }
	return FFCBBattleRules::ValidateDeck(D->CardIds, const_cast<UFCBProgressionSubsystem*>(this)->OwnedCopiesMap(), FFCBBattleRules::DefaultDeckSize, OutErrors);
}
bool UFCBProgressionSubsystem::GetActiveDeck(FFCBDeck& OutDeck) const {
	const FFCBDeck* D = Decks.FindByPredicate([&](const FFCBDeck& X) { return X.DeckId == ActiveDeckId; });
	if (!D) return false; OutDeck = *D; return true;
}
bool UFCBProgressionSubsystem::AddCardToActiveDeck(FName CardId, FString& OutError) {
	FFCBDeck* D = Decks.FindByPredicate([&](const FFCBDeck& X) { return X.DeckId == ActiveDeckId; });
	if (!D) { OutError = TEXT("No active deck."); return false; }
	if (!Owned.Contains(CardId)) { OutError = TEXT("Card not owned."); return false; }
	if (D->CardIds.Num() >= FFCBBattleRules::DefaultDeckSize) { OutError = TEXT("Deck is full (30). Remove a card first."); return false; }
	int32 InDeck = 0; for (FName Id : D->CardIds) if (Id == CardId) ++InDeck;
	if (InDeck >= Owned[CardId].Copies) { OutError = TEXT("Not enough owned copies for another copy in this deck."); return false; }
	D->CardIds.Add(CardId); SaveNow(); OnProgressChanged.Broadcast(); return true;
}
bool UFCBProgressionSubsystem::RemoveCardFromActiveDeck(FName CardId, FString& OutError) {
	FFCBDeck* D = Decks.FindByPredicate([&](const FFCBDeck& X) { return X.DeckId == ActiveDeckId; });
	if (!D) { OutError = TEXT("No active deck."); return false; }
	if (D->CardIds.RemoveSingle(CardId) == 0) { OutError = TEXT("Card is not in the active deck."); return false; }
	SaveNow(); OnProgressChanged.Broadcast(); return true;
}
bool UFCBProgressionSubsystem::AwardMatchRewardsOnce(const FString& MatchId, bool bWon, bool bDraw, int32& OutXP, bool& bLeveled) {
	++MatchesPlayed; if (bWon) ++MatchesWon;
	if (RewardedMatchIds.Contains(MatchId)) { OutXP = 0; bLeveled = false; SaveNow(); return false; } // never double-award
	RewardedMatchIds.Add(MatchId); OutXP = bWon ? 120 : (bDraw ? 85 : 60);
	bLeveled = FFCBBattleRules::ApplyXP(PlayerLevel, PlayerXP, OutXP) > 0; SaveNow(); OnProgressChanged.Broadcast(); return true;
}
void UFCBProgressionSubsystem::InitNewGame() {
	Owned.Reset(); Decks.Reset(); PlayerLevel = 1; PlayerXP = 0;
	TArray<FName> Ids = GetAllCardIds(); Ids.Sort([](FName A, FName B) { return A.LexicalLess(B); });
	for (FName Id : Ids) { FFCBOwnedCard O; O.DefinitionId = Id; Owned.Add(Id, O); } // 1 copy of every definition
	FFCBDeck Starter; Starter.DeckId = FGuid::NewGuid(); Starter.Name = TEXT("Starter Deck");
	for (int32 i = 0; i < FMath::Min(FFCBBattleRules::DefaultDeckSize, Ids.Num()); ++i) Starter.CardIds.Add(Ids[i]);
	Decks.Add(Starter); ActiveDeckId = Starter.DeckId;
}
void UFCBProgressionSubsystem::LoadOrInitialize() {
	auto TryLoad = [&](const FString& Slot) -> bool {
		UFCBSaveGame* S = Cast<UFCBSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!S || S->SchemaVersion != UFCBSaveGame::CurrentSchema) return false;
		PlayerLevel = S->PlayerLevel; PlayerXP = S->PlayerXP; RoundWins = S->RoundWins; RoundLosses = S->RoundLosses; RoundTies = S->RoundTies;
		MatchesPlayed = S->MatchesPlayed; MatchesWon = S->MatchesWon; Difficulty = S->Difficulty; SoundVolume = S->SoundVolume; bEffectsEnabled = S->bEffectsEnabled;
		RewardedMatchIds = S->RewardedMatchIds; Owned.Reset(); for (const FFCBOwnedCard& O : S->OwnedCards) Owned.Add(O.DefinitionId, O);
		Decks = S->Decks; ActiveDeckId = S->ActiveDeckId;
		for (FName Id : GetAllCardIds()) if (!Owned.Contains(Id)) { FFCBOwnedCard O; O.DefinitionId = Id; Owned.Add(Id, O); } // grant future-expansion cards
		return true; };
	if (TryLoad(SlotMain)) return;
	if (TryLoad(SlotBackup)) { UE_LOG(LogTemp, Warning, TEXT("[FCB] Main save invalid; recovered from backup.")); return; }
	InitNewGame(); SaveNow(); // missing/corrupt save -> valid new game, never a crash
}
void UFCBProgressionSubsystem::SaveNow() {
	UFCBSaveGame* Old = Cast<UFCBSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotMain, 0));
	if (Old) UGameplayStatics::SaveGameToSlot(Old, SlotBackup, 0); // recoverable backup of last known-good save
	UFCBSaveGame* S = NewObject<UFCBSaveGame>();
	S->PlayerLevel = PlayerLevel; S->PlayerXP = PlayerXP; S->RoundWins = RoundWins; S->RoundLosses = RoundLosses; S->RoundTies = RoundTies;
	S->MatchesPlayed = MatchesPlayed; S->MatchesWon = MatchesWon; S->Difficulty = Difficulty; S->SoundVolume = SoundVolume; S->bEffectsEnabled = bEffectsEnabled;
	S->RewardedMatchIds = RewardedMatchIds; Owned.GenerateValueArray(S->OwnedCards); S->Decks = Decks; S->ActiveDeckId = ActiveDeckId;
	UGameplayStatics::SaveGameToSlot(S, SlotMain, 0);
}
void UFCBProgressionSubsystem::ResetProgress() { UGameplayStatics::DeleteGameInSlot(SlotMain, 0); UGameplayStatics::DeleteGameInSlot(SlotBackup, 0); InitNewGame(); SaveNow(); OnProgressChanged.Broadcast(); }
