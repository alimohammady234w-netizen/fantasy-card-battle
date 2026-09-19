// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBCardDatabase.h - loads the card set from CSV text (the exact files under Content/Data) and
// derives everything the match engine needs: doctrine-adjusted stats, strength percentiles, ability catalog.
//
// The same loader is used by the UDataTable-backed path (UFCBCardDatabaseAsset) and by the headless
// balance harness in Tools/, so designer CSV edits and shipped data can never drift apart.

#pragma once

#include "CoreMinimal.h"
#include "FCBTypes.h"

/** One problem found while loading or validating data. Fatal issues block a match from starting. */
USTRUCT(BlueprintType)
struct FFCBDataIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	int32 Line = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FString Field;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
	bool bFatal = true;

	FString ToString() const
	{
		return FString::Printf(TEXT("line %d [%s] %s%s"),
			Line, *Field, bFatal ? TEXT("FATAL: ") : TEXT("warn: "), *Message);
	}
};

/** A parsed deck constraint used by the deck builder / lobby options. */
USTRUCT(BlueprintType)
struct FFCBDeckFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deck")
	EFCBRarity MinRarity = EFCBRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deck")
	EFCBRarity MaxRarity = EFCBRarity::Mythic;

	/** EFCBFaction::None = any faction. Otherwise only these two factions are eligible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deck")
	EFCBFaction FactionA = EFCBFaction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deck")
	EFCBFaction FactionB = EFCBFaction::None;

	/** 0 = use every eligible card. Otherwise take this many (randomly, seeded). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deck")
	int32 MaxCards = 0;

	bool Passes(const FFCBCardDef& Def) const
	{
		const int32 RarityIndex = FCBRarityUtil::ToIndex(Def.Rarity);
		if (RarityIndex < FCBRarityUtil::ToIndex(MinRarity) || RarityIndex > FCBRarityUtil::ToIndex(MaxRarity))
		{
			return false;
		}
		if (FactionA != EFCBFaction::None && Def.Faction != FactionA && Def.Faction != FactionB)
		{
			return false;
		}
		return true;
	}
};

/**
 * Engine-side card database (pure C++, no UObject). Load once, share by reference.
 */
class FANTASYCARDBATTLE_API FFCBCardDatabase
{
public:
	/** All playable cards, index order == table row order (indexes are used by the AI). */
	TArray<FFCBCardDef> Cards;

	/** Indexed by (int)EFCBFaction; entry 0 (None) is the empty doctrine. */
	TArray<FFCBFactionDoctrine> DoctrineByFaction;

	/** Canonical ability definitions from DT_Abilities, keyed by ability id. */
	TMap<FName, FFCBCardAbility> AbilityCatalog;

	/** Parallel to Cards: 0..1 rating used for fair deck splitting and AI heuristics. */
	TArray<float> StrengthPercentile;

	void Reset();

	bool IsReady() const { return Cards.Num() > 0 && StrengthPercentile.Num() == Cards.Num(); }

	int32 GetCardCount() const { return Cards.Num(); }

	int32 FindIndexById(FName Id) const;
	const FFCBCardDef* FindById(FName Id) const { const int32 Index = FindIndexById(Id); return Cards.IsValidIndex(Index) ? &Cards[Index] : nullptr; }

	float GetStrengthPercentile(int32 CardIndex) const { return StrengthPercentile.IsValidIndex(CardIndex) ? StrengthPercentile[CardIndex] : 0.5f; }

	/** Percentile of a single attribute across the whole card pool (0..1); used by AI and UI "stat bars". */
	float GetAttributePercentile(EFCBAttribute InAttribute, int32 Value) const;

	/**
	 * Parses the CSV exports produced by Tools/generate_cards.py. Requires a header row whose names
	 * match the FFCBCardRow property names in FCBDataAssets.h (which is also what UE's DataTable
	 * importer looks at - single source of truth).
	 */
	bool LoadFromCsvStrings(
		const FString& CardsCsv,
		const FString& FactionsCsv,
		const FString& AbilitiesCsv,
		TArray<FFCBDataIssue>& OutIssues);

	/** Applies faction doctrine, computes strength percentiles and runs the design-rule validation pass. */
	bool ComputeDerived(TArray<FFCBDataIssue>& OutIssues);

	/**
	 * Fills OutCardIndexes with every card matching the filter (table order). When Filter.MaxCards > 0 the
	 * selection is randomised with the given seed, which is how "quick draft" lobbies are built.
	 * Returns the number of eligible cards found (0 == error).
	 */
	int32 CollectEligibleCards(const FFCBDeckFilter& Filter, TArray<int32>& OutCardIndexes) const;

	/** Overload that also honours MaxCards + seed. */
	int32 CollectEligibleCards(const FFCBDeckFilter& Filter, uint32 Seed, TArray<int32>& OutCardIndexes) const;

	int32 GetCardCountForFaction(EFCBFaction InFaction) const;
	int32 GetCardCountForRarity(EFCBRarity InRarity) const;

	/** Creates an in-play copy of a card: doctrine-adjusted stats, ability copies, once-per-match charges. */
	bool MakeInstance(int32 CardIndex, FFCBCardInstance& OutInstance) const;

	/** Split a single CSV line into cells, honouring double-quoted fields with "" escapes. */
	static void SplitCsvLine(const FString& Line, TArray<FString>& OutCells);

	/** Locate a header column, case-insensitive; returns false when missing. */
	static bool FindColumn(const TArray<FString>& Header, const TCHAR* ColumnName, int32& OutIndex);

	/** Test / tooling helper: synthesise a small balanced pool so unit tests do not depend on shipped content. */
	static void BuildSyntheticPool(int32 CardCount, FFCBCardDatabase& OutDatabase);

private:
	static float ComputeRawStrength(const FFCBCardDef& Def, const TArray<FFCBCardDef>& AllCards);
};
