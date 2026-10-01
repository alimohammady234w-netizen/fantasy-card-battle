// Fantasy Card Battle - Player card collection subsystem implementation (Stage 3)

#include "Collection/CardCollectionSubsystem.h"

#include "CardGame.h"
#include "Save/CardGameSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"

// ---------------------------------------------------------------------------
// USubsystem lifecycle
// ---------------------------------------------------------------------------

void UCardCollectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Auto-load is safe by design: a missing/corrupt save results in an
	// empty collection (logged once) instead of a crash.
	LoadCollection();
}

void UCardCollectionSubsystem::Deinitialize()
{
	// Intentionally NO implicit save here: persistence stays explicit
	// (game code calls SaveCollection() at well-defined moments, e.g. after
	// granting cards or when leaving a meta screen).
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Card ownership operations
// ---------------------------------------------------------------------------

bool UCardCollectionSubsystem::AddCard(FName CardID)
{
	if (CardID.IsNone())
	{
		UE_LOG(LogCardGame, Warning, TEXT("AddCard rejected: CardID is empty."));
		return false;
	}

	UCardDataAsset* Definition = ResolveCardDefinition(CardID);
	if (Definition == nullptr)
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("AddCard rejected: card definition not found for CardID '%s'."),
			*CardID.ToString());
		return false;
	}

	FPlayerCardInstance NewInstance = FPlayerCardInstance::MakeNew(CardID);
	if (!CollectionData.AddCard(NewInstance))
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("AddCard failed: could not store instance for CardID '%s'."),
			*CardID.ToString());
		return false;
	}

	UE_LOG(LogCardGame, Log,
		TEXT("AddCard ok: %s (instance %s) - quantity now %d."),
		*CardID.ToString(),
		*NewInstance.UniqueInstanceID.ToString(),
		CollectionData.GetQuantity(CardID));
	return true;
}

int32 UCardCollectionSubsystem::RemoveCard(FName CardID, int32 Count)
{
	if (CardID.IsNone() || Count < 1)
	{
		return 0;
	}

	const int32 Removed = CollectionData.RemoveCards(CardID, Count);
	if (Removed == 0)
	{
		UE_LOG(LogCardGame, Log,
			TEXT("RemoveCard: no copies of '%s' owned - nothing removed."),
			*CardID.ToString());
	}
	else
	{
		UE_LOG(LogCardGame, Log,
			TEXT("RemoveCard: removed %d cop(y/ies) of '%s' - %d left."),
			Removed, *CardID.ToString(), CollectionData.GetQuantity(CardID));
	}
	return Removed;
}

bool UCardCollectionSubsystem::RemoveCardInstance(FGuid UniqueInstanceID)
{
	if (!UniqueInstanceID.IsValid())
	{
		UE_LOG(LogCardGame, Warning, TEXT("RemoveCardInstance rejected: invalid instance id."));
		return false;
	}

	if (!CollectionData.RemoveInstance(UniqueInstanceID))
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("RemoveCardInstance: instance %s not found."),
			*UniqueInstanceID.ToString());
		return false;
	}

	UE_LOG(LogCardGame, Log,
		TEXT("RemoveCardInstance: removed %s."), *UniqueInstanceID.ToString());
	return true;
}

// ---------------------------------------------------------------------------
// Ownership queries
// ---------------------------------------------------------------------------

bool UCardCollectionSubsystem::HasCard(FName CardID) const
{
	return CollectionData.HasCard(CardID);
}

bool UCardCollectionSubsystem::HasCardInstance(FGuid UniqueInstanceID) const
{
	return CollectionData.HasInstance(UniqueInstanceID);
}

int32 UCardCollectionSubsystem::GetCardQuantity(FName CardID) const
{
	return CollectionData.GetQuantity(CardID);
}

bool UCardCollectionSubsystem::GetCardInstance(FGuid UniqueInstanceID, FPlayerCardInstance& OutInstance) const
{
	const FPlayerCardInstance* Found = CollectionData.FindInstance(UniqueInstanceID);
	if (Found == nullptr)
	{
		return false;
	}

	OutInstance = *Found;
	return true;
}

TArray<FPlayerCardInstance> UCardCollectionSubsystem::GetCardInstances(FName CardID) const
{
	return CollectionData.GetCardsOf(CardID);
}

TArray<FPlayerCardInstance> UCardCollectionSubsystem::GetAllOwnedCards() const
{
	return CollectionData.OwnedCards;
}

TArray<FPlayerCardInstance> UCardCollectionSubsystem::GetCardsByCategory(ECardCategory Category) const
{
	TArray<FPlayerCardInstance> Result;

	// Per-call local map: unresolved (nullptr) results are remembered too,
	// so each CardID resolves its definition at most once per call.
	TMap<FName, UCardDataAsset*> ResolvedDefinitions;
	ResolvedDefinitions.Reserve(CollectionData.Num());

	for (const FPlayerCardInstance& Instance : CollectionData.OwnedCards)
	{
		UCardDataAsset** Cached = ResolvedDefinitions.Find(Instance.CardID);
		UCardDataAsset* Definition = nullptr;
		if (Cached != nullptr)
		{
			Definition = *Cached;
		}
		else
		{
			Definition = ResolveCardDefinition(Instance.CardID);
			ResolvedDefinitions.Add(Instance.CardID, Definition);
		}

		if (Definition != nullptr && Definition->Category == Category)
		{
			Result.Add(Instance);
		}
	}
	return Result;
}

TArray<FPlayerCardInstance> UCardCollectionSubsystem::GetCardsByRarity(ECardRarity Rarity) const
{
	TArray<FPlayerCardInstance> Result;

	TMap<FName, UCardDataAsset*> ResolvedDefinitions;
	ResolvedDefinitions.Reserve(CollectionData.Num());

	for (const FPlayerCardInstance& Instance : CollectionData.OwnedCards)
	{
		UCardDataAsset** Cached = ResolvedDefinitions.Find(Instance.CardID);
		UCardDataAsset* Definition = nullptr;
		if (Cached != nullptr)
		{
			Definition = *Cached;
		}
		else
		{
			Definition = ResolveCardDefinition(Instance.CardID);
			ResolvedDefinitions.Add(Instance.CardID, Definition);
		}

		if (Definition != nullptr && Definition->Rarity == Rarity)
		{
			Result.Add(Instance);
		}
	}
	return Result;
}

void UCardCollectionSubsystem::ClearCollection()
{
	const int32 Removed = CollectionData.Clear();
	UE_LOG(LogCardGame, Log, TEXT("ClearCollection: removed %d instance(s)."), Removed);
}

// ---------------------------------------------------------------------------
// Progression (data only)
// ---------------------------------------------------------------------------

bool UCardCollectionSubsystem::SetCardLevel(FGuid UniqueInstanceID, int32 NewLevel)
{
	if (!UniqueInstanceID.IsValid())
	{
		UE_LOG(LogCardGame, Warning, TEXT("SetCardLevel rejected: invalid instance id."));
		return false;
	}
	if (NewLevel < 1)
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("SetCardLevel rejected: level %d is below the minimum (1)."), NewLevel);
		return false;
	}
	if (!CollectionData.SetLevel(UniqueInstanceID, NewLevel))
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("SetCardLevel: instance %s not found."), *UniqueInstanceID.ToString());
		return false;
	}

	UE_LOG(LogCardGame, Log,
		TEXT("SetCardLevel: instance %s -> level %d."), *UniqueInstanceID.ToString(), NewLevel);
	return true;
}

bool UCardCollectionSubsystem::AddCardXP(FGuid UniqueInstanceID, int32 Amount)
{
	if (!UniqueInstanceID.IsValid())
	{
		UE_LOG(LogCardGame, Warning, TEXT("AddCardXP rejected: invalid instance id."));
		return false;
	}
	if (Amount < 0)
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("AddCardXP rejected: negative XP amount (%d)."), Amount);
		return false;
	}
	if (!CollectionData.AddXP(UniqueInstanceID, Amount))
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("AddCardXP: instance %s not found."), *UniqueInstanceID.ToString());
		return false;
	}

	const FPlayerCardInstance* Instance = CollectionData.FindInstance(UniqueInstanceID);
	UE_LOG(LogCardGame, Log,
		TEXT("AddCardXP: +%d XP to %s (total %d)."),
		Amount, *UniqueInstanceID.ToString(), Instance ? Instance->XP : 0);
	return true;
}

// ---------------------------------------------------------------------------
// Definition resolution:  PlayerCardInstance -> CardID -> UCardDataAsset
// ---------------------------------------------------------------------------

UCardDataAsset* UCardCollectionSubsystem::GetCardDefinition(FName CardID) const
{
	return ResolveCardDefinition(CardID);
}

UCardDataAsset* UCardCollectionSubsystem::ResolveCardDefinition(FName CardID) const
{
	if (CardID.IsNone())
	{
		return nullptr;
	}

	// 1) Weak cache - avoids repeated loads during normal use.
	if (const TWeakObjectPtr<UCardDataAsset>* Cached = DefinitionCache.Find(CardID))
	{
		if (Cached->IsValid())
		{
			return Cached->Get();
		}
	}

	UCardDataAsset* Resolved = nullptr;

	// 2) Conventional path used by the project layout + Stage 2 sample cards.
	const FString ObjectPath = FString::Printf(
		TEXT("/Game/Data/Cards/%s.%s"), *CardID.ToString(), *CardID.ToString());
	Resolved = Cast<UCardDataAsset>(FSoftObjectPath(ObjectPath).TryLoad());

	// 3) Asset Registry fallback - finds cards organized in subfolders.
	if (Resolved == nullptr)
	{
		if (FAssetRegistryModule* RegistryModule =
			FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
		{
			FARFilter Filter;
			Filter.ClassPaths.Add(UCardDataAsset::StaticClass()->GetClassPathName());
			Filter.PackagePaths.Add(FName(TEXT("/Game/Data")));
			Filter.bRecursivePaths = true;

			TArray<FAssetData> Assets;
			RegistryModule->Get().GetAssets(Filter, Assets);

			for (const FAssetData& Asset : Assets)
			{
				if (Asset.AssetName == CardID)
				{
					Resolved = Cast<UCardDataAsset>(Asset.GetAsset());
					break;
				}
			}
		}
	}

	if (Resolved != nullptr)
	{
		DefinitionCache.Add(CardID, Resolved);
	}
	else
	{
		UE_LOG(LogCardGame, Verbose,
			TEXT("ResolveCardDefinition: no UCardDataAsset named '%s' found."),
			*CardID.ToString());
	}

	return Resolved;
}

// ---------------------------------------------------------------------------
// Save / Load
// ---------------------------------------------------------------------------

bool UCardCollectionSubsystem::SaveCollection()
{
	USaveGame* NewSave = UGameplayStatics::CreateSaveGameObject(
		UCardGameSaveGame::StaticClass());
	UCardGameSaveGame* CardSave = Cast<UCardGameSaveGame>(NewSave);
	if (CardSave == nullptr)
	{
		UE_LOG(LogCardGame, Error, TEXT("SaveCollection failed: could not create SaveGame object."));
		return false;
	}

	CardSave->SaveVersion = UCardGameSaveGame::CurrentSaveVersion;
	CardSave->Collection = CollectionData;

	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		CardSave,
		UCardGameSaveGame::DefaultSaveSlotName,
		UCardGameSaveGame::DefaultSaveUserIndex);

	if (bSaved)
	{
		UE_LOG(LogCardGame, Log,
			TEXT("SaveCollection ok: slot '%s' (%d instances, version %d)."),
			*UCardGameSaveGame::DefaultSaveSlotName,
			CollectionData.Num(),
			UCardGameSaveGame::CurrentSaveVersion);
	}
	else
	{
		UE_LOG(LogCardGame, Error,
			TEXT("SaveCollection FAILED: slot '%s' (%d instances)."),
			*UCardGameSaveGame::DefaultSaveSlotName,
			CollectionData.Num());
	}
	return bSaved;
}

bool UCardCollectionSubsystem::LoadCollection()
{
	const FString& SlotName = UCardGameSaveGame::DefaultSaveSlotName;
	const int32 UserIndex = UCardGameSaveGame::DefaultSaveUserIndex;

	// No save yet -> start with an empty collection (normal first run).
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		CollectionData.Clear();
		UE_LOG(LogCardGame, Log,
			TEXT("LoadCollection: no save in slot '%s' - starting with an empty collection."),
			*SlotName);
		return false;
	}

	USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
	if (LoadedSave == nullptr)
	{
		CollectionData.Clear();
		UE_LOG(LogCardGame, Error,
			TEXT("LoadCollection: slot '%s' could not be read (corrupt/unavailable) - starting empty."),
			*SlotName);
		return false;
	}

	UCardGameSaveGame* CardSave = Cast<UCardGameSaveGame>(LoadedSave);
	if (CardSave == nullptr)
	{
		CollectionData.Clear();
		UE_LOG(LogCardGame, Error,
			TEXT("LoadCollection: slot '%s' contains an unexpected SaveGame class - starting empty."),
			*SlotName);
		return false;
	}

	MigrateSaveData(*CardSave);

	CollectionData = CardSave->Collection;

	const int32 FixedIssues = CollectionData.Sanitize();
	if (FixedIssues > 0)
	{
		UE_LOG(LogCardGame, Warning,
			TEXT("LoadCollection: repaired %d malformed entr(y/ies) after load."),
			FixedIssues);
	}

	UE_LOG(LogCardGame, Log,
		TEXT("LoadCollection ok: %d instance(s), save version %d."),
		CollectionData.Num(), CardSave->SaveVersion);
	return true;
}

void UCardCollectionSubsystem::MigrateSaveData(UCardGameSaveGame& Save) const
{
	// v1 is the initial format. When CurrentSaveVersion grows, add steps like:
	//   case 1: upgrade v1 fields to v2; [[fallthrough]];
	//   case 2: upgrade v2 fields to v3; ...
	if (Save.SaveVersion < 1)
	{
		// Pre-release/invalid version - treat as v1 baseline.
		UE_LOG(LogCardGame, Warning,
			TEXT("MigrateSaveData: save version %d invalid - treating as v1."),
			Save.SaveVersion);
		Save.SaveVersion = 1;
	}
	else if (Save.SaveVersion > UCardGameSaveGame::CurrentSaveVersion)
	{
		// Written by a newer build: tagged serialization keeps known fields,
		// unknown future fields are ignored - proceed best-effort with a warning.
		UE_LOG(LogCardGame, Warning,
			TEXT("MigrateSaveData: save version %d is newer than supported %d - loading best-effort."),
			Save.SaveVersion, UCardGameSaveGame::CurrentSaveVersion);
	}
	// Same version -> nothing to do.
}

// ---------------------------------------------------------------------------
// Development helpers
// ---------------------------------------------------------------------------

void UCardCollectionSubsystem::GrantDevTestCollection()
{
	// DEVELOPMENT/TEST DATA ONLY - replace with a real reward flow later.
	struct FTestGrant
	{
		FName CardID;
		int32 Count;
	};

	static const FTestGrant TestGrants[] =
	{
		{ FName(TEXT("CARD_ANIMAL_LION_001")),    3 }, // Lion x3
		{ FName(TEXT("CARD_ANIMAL_EAGLE_001")),   1 },
		{ FName(TEXT("CARD_DINOSAUR_TREX_001")),  4 }, // T-Rex x4
		{ FName(TEXT("CARD_FANTASY_DRAGON_001")), 2 }, // Dragon x2
		{ FName(TEXT("CARD_ROBOT_001")),          1 },
		{ FName(TEXT("CARD_PERSIA_WARRIOR_001")), 1 },
		{ FName(TEXT("CARD_EGYPT_PHARAOH_001")),  1 },
		{ FName(TEXT("CARD_GREECE_HERO_001")),    1 },
		{ FName(TEXT("CARD_SPACE_MONSTER_001")),  1 },
		{ FName(TEXT("CARD_FANTASY_GOLEM_001")),  1 },
	};

	int32 GrantedCopies = 0;
	int32 GrantedCardIds = 0;

	for (const FTestGrant& Grant : TestGrants)
	{
		int32 AddedForCard = 0;
		for (int32 Index = 0; Index < Grant.Count; ++Index)
		{
			if (AddCard(Grant.CardID))
			{
				++AddedForCard;
			}
		}

		if (AddedForCard > 0)
		{
			++GrantedCardIds;
			GrantedCopies += AddedForCard;
		}
	}

	UE_LOG(LogCardGame, Log,
		TEXT("GrantDevTestCollection: granted %d cop(y/ies) across %d card(s) - total instances now %d."),
		GrantedCopies, GrantedCardIds, CollectionData.Num());
}

void UCardCollectionSubsystem::DumpCollection() const
{
	UE_LOG(LogCardGame, Log,
		TEXT("Collection dump: %d instance(s)."), CollectionData.Num());

	for (const FPlayerCardInstance& Instance : CollectionData.OwnedCards)
	{
		UE_LOG(LogCardGame, Log,
			TEXT("  %s | %s | Level %d | XP %d | Qty %d"),
			*Instance.UniqueInstanceID.ToString(),
			*Instance.CardID.ToString(),
			Instance.Level,
			Instance.XP,
			Instance.Quantity);
	}
}

// ---------------------------------------------------------------------------
// Console commands (development / manual testing)
// ---------------------------------------------------------------------------

namespace CardCollectionConsole
{
	static UCardCollectionSubsystem* GetSubsystem(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		UGameInstance* GameInstance = World->GetGameInstance();
		return GameInstance ? GameInstance->GetSubsystem<UCardCollectionSubsystem>() : nullptr;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GCardGameGrantDevTestCardsCmd(
	TEXT("CardGame.Collection.GrantDevTestCards"),
	TEXT("Grants the Stage-3 development test collection (includes duplicates). Development only."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (UCardCollectionSubsystem* Subsystem = CardCollectionConsole::GetSubsystem(World))
			{
				Subsystem->GrantDevTestCollection();
			}
			else
			{
				UE_LOG(LogCardGame, Warning, TEXT("CardGame.Collection.GrantDevTestCards: no collection subsystem available."));
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GCardGameCollectionSaveCmd(
	TEXT("CardGame.Collection.Save"),
	TEXT("Saves the player card collection to the CardGameSave slot."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (UCardCollectionSubsystem* Subsystem = CardCollectionConsole::GetSubsystem(World))
			{
				Subsystem->SaveCollection();
			}
			else
			{
				UE_LOG(LogCardGame, Warning, TEXT("CardGame.Collection.Save: no collection subsystem available."));
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GCardGameCollectionLoadCmd(
	TEXT("CardGame.Collection.Load"),
	TEXT("Reloads the player card collection from the CardGameSave slot."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (UCardCollectionSubsystem* Subsystem = CardCollectionConsole::GetSubsystem(World))
			{
				Subsystem->LoadCollection();
			}
			else
			{
				UE_LOG(LogCardGame, Warning, TEXT("CardGame.Collection.Load: no collection subsystem available."));
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GCardGameCollectionDumpCmd(
	TEXT("CardGame.Collection.Dump"),
	TEXT("Writes every owned card instance to the log (LogCardGame)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (UCardCollectionSubsystem* Subsystem = CardCollectionConsole::GetSubsystem(World))
			{
				Subsystem->DumpCollection();
			}
			else
			{
				UE_LOG(LogCardGame, Warning, TEXT("CardGame.Collection.Dump: no collection subsystem available."));
			}
		}));
