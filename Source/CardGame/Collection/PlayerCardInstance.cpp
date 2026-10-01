// Fantasy Card Battle - Player-owned card data implementation (Stage 3)

#include "Collection/PlayerCardInstance.h"

// ---------------------------------------------------------------------------
// FPlayerCardInstance
// ---------------------------------------------------------------------------

FPlayerCardInstance FPlayerCardInstance::MakeNew(FName InCardID)
{
	FPlayerCardInstance Instance;
	Instance.UniqueInstanceID = FGuid::NewGuid();
	Instance.CardID = InCardID;
	Instance.Level = 1;
	Instance.XP = 0;
	Instance.Quantity = 1;
	return Instance;
}

// ---------------------------------------------------------------------------
// FPlayerCardCollection - mutating operations
// ---------------------------------------------------------------------------

bool FPlayerCardCollection::AddCard(const FPlayerCardInstance& Instance)
{
	if (!Instance.IsValid() || Instance.Quantity < 1)
	{
		return false;
	}

	if (FindInstance(Instance.UniqueInstanceID) != nullptr)
	{
		// Duplicate instance id - refuse to create ambiguous state.
		return false;
	}

	OwnedCards.Add(Instance);
	return true;
}

bool FPlayerCardCollection::RemoveInstance(const FGuid& UniqueInstanceID)
{
	if (!UniqueInstanceID.IsValid())
	{
		return false;
	}

	const int32 Index = OwnedCards.IndexOfByPredicate(
		[&UniqueInstanceID](const FPlayerCardInstance& Instance)
		{
			return Instance.UniqueInstanceID == UniqueInstanceID;
		});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	OwnedCards.RemoveAt(Index);
	return true;
}

int32 FPlayerCardCollection::RemoveCards(FName CardID, int32 Count)
{
	if (CardID.IsNone() || Count < 1)
	{
		return 0;
	}

	int32 Removed = 0;
	for (int32 Index = OwnedCards.Num() - 1; Index >= 0 && Removed < Count; --Index)
	{
		if (OwnedCards[Index].CardID == CardID)
		{
			OwnedCards.RemoveAt(Index);
			++Removed;
		}
	}
	return Removed;
}

int32 FPlayerCardCollection::Clear()
{
	const int32 Removed = OwnedCards.Num();
	OwnedCards.Reset();
	return Removed;
}

bool FPlayerCardCollection::SetLevel(const FGuid& UniqueInstanceID, int32 NewLevel)
{
	if (!UniqueInstanceID.IsValid() || NewLevel < 1)
	{
		return false;
	}

	FPlayerCardInstance* Instance = FindInstance(UniqueInstanceID);
	if (Instance == nullptr)
	{
		return false;
	}

	Instance->Level = NewLevel;
	return true;
}

bool FPlayerCardCollection::AddXP(const FGuid& UniqueInstanceID, int32 Amount)
{
	if (!UniqueInstanceID.IsValid() || Amount < 0)
	{
		return false;
	}

	FPlayerCardInstance* Instance = FindInstance(UniqueInstanceID);
	if (Instance == nullptr)
	{
		return false;
	}

	const int64 NewXP = static_cast<int64>(Instance->XP) + static_cast<int64>(Amount);
	Instance->XP = static_cast<int32>(FMath::Min<int64>(NewXP, static_cast<int64>(MAX_int32)));
	return true;
}

// ---------------------------------------------------------------------------
// FPlayerCardCollection - queries
// ---------------------------------------------------------------------------

FPlayerCardInstance* FPlayerCardCollection::FindInstance(const FGuid& UniqueInstanceID)
{
	if (!UniqueInstanceID.IsValid())
	{
		return nullptr;
	}

	return OwnedCards.FindByPredicate(
		[&UniqueInstanceID](const FPlayerCardInstance& Instance)
		{
			return Instance.UniqueInstanceID == UniqueInstanceID;
		});
}

const FPlayerCardInstance* FPlayerCardCollection::FindInstance(const FGuid& UniqueInstanceID) const
{
	if (!UniqueInstanceID.IsValid())
	{
		return nullptr;
	}

	return OwnedCards.FindByPredicate(
		[&UniqueInstanceID](const FPlayerCardInstance& Instance)
		{
			return Instance.UniqueInstanceID == UniqueInstanceID;
		});
}

TArray<FPlayerCardInstance> FPlayerCardCollection::GetCardsOf(FName CardID) const
{
	TArray<FPlayerCardInstance> Result;
	if (CardID.IsNone())
	{
		return Result;
	}

	Result.Reserve(OwnedCards.Num());
	for (const FPlayerCardInstance& Instance : OwnedCards)
	{
		if (Instance.CardID == CardID)
		{
			Result.Add(Instance);
		}
	}
	return Result;
}

bool FPlayerCardCollection::HasCard(FName CardID) const
{
	return GetQuantity(CardID) > 0;
}

bool FPlayerCardCollection::HasInstance(const FGuid& UniqueInstanceID) const
{
	return FindInstance(UniqueInstanceID) != nullptr;
}

int32 FPlayerCardCollection::GetQuantity(FName CardID) const
{
	if (CardID.IsNone())
	{
		return 0;
	}

	int32 Total = 0;
	for (const FPlayerCardInstance& Instance : OwnedCards)
	{
		if (Instance.CardID == CardID)
		{
			Total += Instance.Quantity;
		}
	}
	return Total;
}

// ---------------------------------------------------------------------------
// FPlayerCardCollection - validation
// ---------------------------------------------------------------------------

int32 FPlayerCardCollection::Sanitize()
{
	int32 Fixed = 0;

	// Pass 1: drop unusable entries, clamp progression values, fix missing ids.
	for (int32 Index = OwnedCards.Num() - 1; Index >= 0; --Index)
	{
		FPlayerCardInstance& Instance = OwnedCards[Index];

		if (Instance.CardID.IsNone())
		{
			OwnedCards.RemoveAt(Index);
			++Fixed;
			continue;
		}

		if (Instance.Quantity < 1)
		{
			// Represents zero copies - not a valid owned card.
			OwnedCards.RemoveAt(Index);
			++Fixed;
			continue;
		}

		if (Instance.Level < 1)
		{
			Instance.Level = 1;
			++Fixed;
		}

		if (Instance.XP < 0)
		{
			Instance.XP = 0;
			++Fixed;
		}

		if (!Instance.UniqueInstanceID.IsValid())
		{
			Instance.UniqueInstanceID = FGuid::NewGuid();
			++Fixed;
		}
	}

	// Pass 2: no two instances may share a UniqueInstanceID.
	TSet<FGuid> SeenIds;
	SeenIds.Reserve(OwnedCards.Num());
	for (FPlayerCardInstance& Instance : OwnedCards)
	{
		if (SeenIds.Contains(Instance.UniqueInstanceID))
		{
			Instance.UniqueInstanceID = FGuid::NewGuid();
			++Fixed;
		}
		SeenIds.Add(Instance.UniqueInstanceID);
	}

	return Fixed;
}
