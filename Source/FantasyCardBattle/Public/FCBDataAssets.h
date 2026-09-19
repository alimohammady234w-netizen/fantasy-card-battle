// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBDataAssets.h - the Unreal-facing side of the card database.
//
// There is exactly ONE representation of a card: FFCBCardDef, produced by FFCBCardDatabase::LoadFromCsvStrings.
// Everything in this file exists to feed that loader:
//
//   * FFCBCardRow / FFCBAbilityRow / FFCBFactionRow are DataTable row structs whose property names match the
//     generated CSV columns one-for-one, so Content/Data/Generated/*.csv can be dragged into the Content
//     Browser without a translation layer, and so a table can be turned back into the same CSV text.
//   * UFCBMatchRulesAsset serialises its tables into that CSV text and hands it to the loader. The editor and
//     the headless harness therefore validate the same rows with the same rules - a value that only "works"
//     in one of the two paths is a bug we want caught.
//   * UFCBSettings / UFCBAiProfileAsset are read straight out of Config/DefaultGame.ini, so the project ships
//     playable with zero Content assets: no blueprint, no widget, no table required.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "FCBAiAgent.h"
#include "FCBMatchRules.h"
#include "FCBTypes.h"
#include "FCBDataAssets.generated.h"

class UDataTable;
class FFCBCardDatabase;
class UFCBMatchRulesAsset;
struct FFCBDataIssue;

/** Shared data-layer helpers (CSV emission for the tables, colour parsing for the frames). */
namespace FCBData
{
	/** "#RRGGBB" or "RRGGBB" from DT_Factions.FrameColor. Returns false, leaving OutColor alone, on junk. */
	FANTASYCARDBATTLE_API bool ParseHexColor(const FString& InHex, FLinearColor& OutColor);
}

/** ------------------------------------------------------------------ card row --------------------------------- */

/**
 * One card. Column names are the CSV header names from Tools/generate_cards.py; do not rename one side without
 * the other. Enum properties are stored as the *identifier* (Power, Dragonkind, Mythic) rather than the
 * display name, which is what UE's own CSV import expects.
 */
USTRUCT(BlueprintType)
struct FANTASYCARDBATTLE_API FFCBCardRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	EFCBFaction Faction = EFCBFaction::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	EFCBRarity Rarity = EFCBRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0", ClampMax = "999999"))
	int32 AgeYears = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1", ClampMax = "120"))
	int32 Power = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1", ClampMax = "120"))
	int32 Speed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "20", ClampMax = "9000"))
	int32 HeightCm = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	FText Flavor;

	/** Kept as a string on purpose: the CSV stores a /Game path and art is renamed far more than cards are. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	FString PortraitPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "0"))
	int32 SetIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "0"))
	int32 LoreEntryNumber = 0;

	/**
	 * Ability slots are flattened (Ability0*/Ability1*) instead of nested so that the CSV import maps columns
	 * directly. FCBCardDatabase re-expands them into FFCBCardAbility and applies the catalogue defaults.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability0")
	FName Ability0Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability0")
	FText Ability0Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability0")
	FText Ability0Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability0")
	EFCBAbilityEffect Ability0Effect = EFCBAbilityEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability0")
	EFCBAttribute Ability0Attribute = EFCBAttribute::Power;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability0")
	int32 Ability0ParamA = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability0")
	int32 Ability0ParamB = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability1")
	FName Ability1Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability1")
	FText Ability1Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability1")
	FText Ability1Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability1")
	EFCBAbilityEffect Ability1Effect = EFCBAbilityEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability1")
	EFCBAttribute Ability1Attribute = EFCBAttribute::Power;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability1")
	int32 Ability1ParamA = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability1")
	int32 Ability1ParamB = 0;
};

/** ---------------------------------------------------------------- faction row -------------------------------- */

USTRUCT(BlueprintType)
struct FANTASYCARDBATTLE_API FFCBFactionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FText DisplayName;

	/** Shown in the codex header, e.g. "Wyrmskin". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FText DoctrineName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FText Lore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Doctrine")
	EFCBAttribute BonusAttribute = EFCBAttribute::Power;

	/** Percent of the printed value; applied at load time, so the card table stays readable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Doctrine", meta = (ClampMin = "-50", ClampMax = "50"))
	int32 BonusPercent = 0;

	/** Every doctrine pays for its bonus with a penalty on another attribute. A bonus without a penalty is a
	 *  faction-wide stat stick, which the validator in FCBCardDatabase.cpp rejects as an error. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Doctrine")
	EFCBAttribute PenaltyAttribute = EFCBAttribute::Speed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Doctrine", meta = (ClampMin = "-50", ClampMax = "50"))
	int32 PenaltyPercent = 0;

	/** "#RRGGBB". A string, not FColor: art owns this value and edits it in the CSV. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FString FrameColor = TEXT("#8A8A8A");
};

/** ---------------------------------------------------------------- ability row -------------------------------- */

USTRUCT(BlueprintType)
struct FANTASYCARDBATTLE_API FFCBAbilityRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	EFCBAbilityEffect Effect = EFCBAbilityEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	EFCBAttribute Attribute = EFCBAttribute::Power;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	int32 DefaultParamA = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	int32 DefaultParamB = 0;

	/** Only meaningful for BonusAgainstFaction: the faction this ability is a bane against. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	EFCBFaction Faction = EFCBFaction::None;

	/** Cards below this rarity may not carry the ability (the validator enforces the same rule). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	EFCBRarity MinRarity = EFCBRarity::Common;
};

/** ----------------------------------------------------------------- settings ---------------------------------- */

/**
 * Project settings (Edit > Project Settings > Fantasy Card Battle). Configured from DefaultGame.ini so the
 * shipped defaults are reviewable in code review, not hidden inside a binary asset.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Fantasy Card Battle"))
class FANTASYCARDBATTLE_API UFCBSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The code-only HUD: draws hands, pot, log and AI explanation with no widget assets. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "HUD")
	bool bShowDebugHud = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match")
	bool bAutoDealOnMatchStart = true;

	/** Artificial pause before the AI plays, so a move is readable. 0 makes it instant. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0", ClampMax = "3"))
	float AiThinkDelaySeconds = 0.35f;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	bool bAnimateCardReveals = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0", ClampMax = "2"))
	float CardRevealSeconds = 0.18f;

	/** Difficulty of the AI that sits opposite a human player. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI")
	EFCBAiDifficulty AiDifficulty = EFCBAiDifficulty::Expert;

	/** Optional: the rules asset with DataTables. When None, CSVs under Content/Data/Generated are used. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UFCBMatchRulesAsset> RulesAsset;

	/** Content/Data/Generated relative to the project, the path Tools/generate_cards.py writes to. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString CardDataDirectory = TEXT("Content/Data/Generated");

	/**
	 * The class default object, which is what Config/DefaultGame.ini configures.
	 *
	 * No GetSectionName() override: on UDeveloperSettings that virtual is editor-only, and an override that
	 * disappears between a Development Editor and a Shipping build is a build break for a string that the
	 * UCLASS DisplayName metadata already provides in the Project Settings UI.
	 */
	static const UFCBSettings& Get();
};

/** ---------------------------------------------------------------- AI profiles -------------------------------- */

/**
 * One row of the AI ladder. Field names match Config/DefaultGame.ini's +Profiles entries.
 *
 * Every property here is marked config on purpose: an array of structs is read out of the ini by serialising
 * the struct's *own* properties, and the config archive skips anything without CPF_Config. Drop the flag and
 * DefaultGame.ini's +Profiles lines stop loading silently - the game still runs, it just ignores the shipped
 * tuning. FantasyCardBattle.Data.IniConfigIsAbsorbed is the test that catches that.
 */
USTRUCT(BlueprintType)
struct FANTASYCARDBATTLE_API FFCBAiProfileRow
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI")
	EFCBAiDifficulty Difficulty = EFCBAiDifficulty::Adept;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI")
	int32 Elo = 1200;

	/** Depth of the opponent-reply rollout (0 = none, 1 = Expert behaviour, 2 = Legendary). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0", ClampMax = "4"))
	int32 SearchPly = 0;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BlunderChancePercent = 0;

	/** Soft frame budget. Drives the UI think delay; the engine never clocks itself against wall time. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0"))
	int32 TimeBudgetMs = 0;

	/** Leave at -1 to inherit the preset; set to tune a single knob without copying the whole profile. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI (overrides)", meta = (ClampMin = "-1", ClampMax = "4"))
	float CardQualityWeightOverride = -1.f;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI (overrides)", meta = (ClampMin = "-1", ClampMax = "4"))
	float RiskAversionOverride = -1.f;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI (overrides)", meta = (ClampMin = "-1", ClampMax = "4"))
	float PotValueWeightOverride = -1.f;

	/** Fills InOutProfile from this row on top of the hard-coded preset for the same difficulty. */
	void ApplyTo(FFCBAiProfile& InOutProfile) const;
};

/**
 * The AI ladder. The CDO is configured from DefaultGame.ini, so the shipped tuning is data in the repository
 * even when no asset exists in Content/.
 */
UCLASS(config = Game, BlueprintType, Blueprintable, meta = (DisplayName = "FCB AI Profiles"))
class FANTASYCARDBATTLE_API UFCBAiProfileAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "AI")
	TArray<FFCBAiProfileRow> Profiles;

	bool FindRow(EFCBAiDifficulty InDifficulty, FFCBAiProfileRow& OutRow) const;

	/** Resolves a difficulty to a full profile: asset rows first, then the ini-configured CDO, then presets. */
	static void Resolve(const UFCBAiProfileAsset* InAsset, EFCBAiDifficulty InDifficulty, FFCBAiProfile& OutProfile);
};

/** ------------------------------------------------------------- rules asset ---------------------------------- */

/**
 * The data root of the project: which tables to use and what a fresh match looks like. Read from
 * Config/DefaultGame.ini by default, which is why the game is playable before any asset exists.
 */
UCLASS(config = Game, BlueprintType, Blueprintable, meta = (DisplayName = "FCB Match Rules"))
class FANTASYCARDBATTLE_API UFCBMatchRulesAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> CardTable;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> FactionTable;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> AbilityTable;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "3", ClampMax = "24"))
	int32 DefaultHandSize = 12;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match")
	EFCBTieRule DefaultTieRule = EFCBTieRule::PotClash;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "8"))
	int32 DefaultMaxRounds = 240;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1"))
	int32 DefaultPotCap = 12;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match")
	bool bDefaultEnableAbilities = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match")
	bool bDefaultBalanceSeats = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Match")
	EFCBEndCondition DefaultEndCondition = EFCBEndCondition::LastSeatStanding;

	/**
	 * Loads the tables and runs them through the same validator the headless harness uses.
	 *
	 * Deliberately not a UFUNCTION: FFCBCardDatabase is a plain C++ class (no UObject, no BlueprintType), which
	 * is what lets it be compiled headless for the balance harness. Widgets therefore talk to the game
	 * instance's views instead of the database; BlueprintCallable access here would mean wrapping the whole
	 * rules engine in Blueprint-visible types.
	 */
	bool BuildDatabase(FFCBCardDatabase& OutDatabase, FString& OutError) const;

	/**
	 * Fallback path: DT_*.csv in InDirectory. Returns false when the files are missing.
	 * InIssues optionally receives the validator's output, so tests and the -csv dump can report *why* a load
	 * failed instead of only that it failed.
	 */
	static bool LoadDatabaseFromCsvDirectory(const FString& InDirectory, FFCBCardDatabase& OutDatabase, FString& OutError,
		TArray<FFCBDataIssue>* OutIssues = nullptr);

	/** Highest-precedence asset: an explicitly loaded one, else the ini-configured CDO. */
	static const UFCBMatchRulesAsset* GetConfigured();

	void ApplyToConfig(FFCBMatchConfig& InOutConfig) const;

	/** Re-emit the table contents as the canonical CSV text (used by the importer tests and by -csv= dumps). */
	FString ExportCardTableToCsv(FString& OutError) const;

private:
	template <typename RowType>
	bool SerializeTable(const TSoftObjectPtr<UDataTable>& InTable, const TArray<FString>& InColumns, FString& OutCsv, FString& OutError) const;
};
