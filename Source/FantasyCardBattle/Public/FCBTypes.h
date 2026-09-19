// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBTypes.h - shared, engine-light value types for the card game.
//
// IMPORTANT: everything under Source/FantasyCardBattle/Public that starts with "FCB" (except the
// UDataTable/UDataAsset wrappers in FCBDataAssets.h) is deliberately kept free of Engine/UMG headers so it can
// also be compiled headless by Tools/MockUE. That lets us run automated balance simulations and unit tests
// without launching the editor. Do not add UMG/Engine includes here.

#pragma once

#include "CoreMinimal.h"

/** The four contestable attributes of a card (Top Trumps style). */
UENUM(BlueprintType)
enum class EFCBAttribute : uint8
{
	Age		UMETA(DisplayName = "Age"),
	Power	UMETA(DisplayName = "Power"),
	Speed	UMETA(DisplayName = "Speed"),
	Height	UMETA(DisplayName = "Height"),
	Max		UMETA(Hidden)
};

/** Rarity drives deck composition, pack odds and (see Docs/Balance.md) capture bonuses. */
UENUM(BlueprintType)
enum class EFCBRarity : uint8
{
	Common		UMETA(DisplayName = "Common"),
	Uncommon	UMETA(DisplayName = "Uncommon"),
	Rare		UMETA(DisplayName = "Rare"),
	Epic		UMETA(DisplayName = "Epic"),
	Legendary	UMETA(DisplayName = "Legendary"),
	Mythic		UMETA(DisplayName = "Mythic"),
	Max			UMETA(Hidden)
};

/** Factions / "worlds" a card belongs to. Each faction carries one doctrine modifier. */
UENUM(BlueprintType)
enum class EFCBFaction : uint8
{
	None			UMETA(DisplayName = "Unaligned"),
	Dragonkind		UMETA(DisplayName = "Dragonkind"),
	ArcaneOrders	UMETA(DisplayName = "Arcane Orders"),
	OrderOfTheBlade	UMETA(DisplayName = "Order of the Blade"),
	Ironhold		UMETA(DisplayName = "Ironhold"),
	FeyCourt		UMETA(DisplayName = "Fey Court"),
	UndyingLegion	UMETA(DisplayName = "Undying Legion"),
	AbyssalHorde	UMETA(DisplayName = "Abyssal Horde"),
	CelestialHost	UMETA(DisplayName = "Celestial Host"),
	Beastwild		UMETA(DisplayName = "Beastwild"),
	DeepwaterCourt	UMETA(DisplayName = "Deepwater Court"),
	Max				UMETA(Hidden)
};

/**
 * Data-driven ability effects. Adding a new keyword to the game = adding one entry here +
 * one branch in FCBMatchRules::ApplyAbilities(). Numbers/attributes all come from the card table,
 * so designers can retune without touching C++.
 */
UENUM(BlueprintType)
enum class EFCBAbilityEffect : uint8
{
	None = 0					UMETA(DisplayName = "None"),

	/**
	 * ParamA = flat amount, Attribute = the contested attribute it applies to.
	 * Legal only on Power/Speed (both live on the 1..120 band): a flat number is meaningless on Age
	 * (up to 999999) or Height (up to 9000), so the validator rejects those. Anything that must work on
	 * every attribute is authored as a percent effect instead.
	 */
	FlatBonusOnAttribute		UMETA(DisplayName = "Flat Bonus On Attribute"),
	/** ParamA = percent. +percent of the base value when that attribute is contested. */
	PercentBonusOnAttribute		UMETA(DisplayName = "Percent Bonus On Attribute"),
	/** Attribute = which attribute ties are won outright (Ancient-style tiebreaker). */
	WinsTiesOnAttribute			UMETA(DisplayName = "Wins Ties On Attribute"),
	/** Always active: the opponent's ability modifiers are void for this round (Spellbreak). */
	NullifyOpponentAbilities	UMETA(DisplayName = "Nullify Opponent Abilities"),
	/** ParamA = amount per card of my faction already in my capture pile (Oathkeeper). */
	BonusPerFactionInOwnPile	UMETA(DisplayName = "Bonus Per Faction Card In Pile"),
	/** ParamA = amount, Faction = target. Bonus against that faction (Giant-Slayer, bane effects). */
	BonusAgainstFaction			UMETA(DisplayName = "Bonus Against Faction"),
	/** ParamA = amount, ParamB = threshold, Attribute = opponent's stat. Bonus if opponent's stat > ParamB. */
	BonusIfOpponentStatAbove	UMETA(DisplayName = "Bonus If Opponent Stat Above"),
	/** ParamA = percent on *whatever* attribute is contested, but only while leading (First Charge). */
	PercentBonusWhenLeading		UMETA(DisplayName = "Percent Bonus When Leading"),
	/** ParamA = percent on whatever attribute is contested, but only while defending (Bulwark Oath). */
	PercentBonusWhenDefending	UMETA(DisplayName = "Percent Bonus When Defending"),
	/** ParamA = negative percent applied to every attribute (Cursed). Scale-free on purpose. */
	AlwaysPenalty				UMETA(DisplayName = "Always Penalty (Pct)"),
	/**
	 * ParamA = percent window. Defending and losing by at most ParamA% of the contested value flips the
	 * round into a win (Fey Trickery). Percentages keep it fair on every attribute scale.
	 */
	WinIfMarginWithin			UMETA(DisplayName = "Win If Narrow Margin"),
	/** On round win: also take the top card of the deck into hand (Sky Courier, Hoarder). */
	OnWinTakeDeckTop			UMETA(DisplayName = "On Win Take Deck Top"),
	/** On round win: opponent also loses their weakest card (lowest contested attribute) to your pile. */
	OnWinForceExtraCapture		UMETA(DisplayName = "On Win Force Extra Capture"),
	/** Once per card per match: when captured, returns to your hand instead (Undying). */
	OnLoseReturnToHandOnce		UMETA(DisplayName = "On Lose Return To Hand (Once)"),
	/** On round win, pot cards count double towards your pile *score* only (not physical capture). */
	PotScoreBonusOnWin			UMETA(DisplayName = "Pot Score Bonus On Win"),
	/** Attribute = contested attribute is also applied to your *next* card this round? Reserved. */
	Max							UMETA(Hidden)
};

/** Bit flags carried on a card ability. */
namespace FCBAbilityFlags
{
	enum : uint32
	{
		None			= 0u,
		/** Shown as a green/passive keyword in the UI. */
		Passive			= 1u << 0,
		/** Only meaningful while the card is the *attacker*. */
		AttackerOnly	= 1u << 1,
		/** Only meaningful while the card is the *defender*. */
		DefenderOnly	= 1u << 2,
		/** Triggers after the round winner is decided. */
		TriggerOnWin	= 1u << 3,
		/** Triggers when this card would be captured. */
		TriggerOnLose	= 1u << 4,
		/** Once-per-match usage; the engine consumes a charge. */
		ConsumedOnce	= 1u << 5
	};
}

/** Named numeric limits for the four attributes; also used by the data validator in Tools/. */
namespace FCBLimits
{
	inline constexpr int32 AgeMin			= 0;
	inline constexpr int32 AgeMax			= 999999;
	inline constexpr int32 PowerMin			= 1;
	inline constexpr int32 PowerMax			= 120;
	inline constexpr int32 SpeedMin			= 1;
	inline constexpr int32 SpeedMax			= 120;
	inline constexpr int32 HeightCmMin		= 20;
	inline constexpr int32 HeightCmMax		= 9000;
}

/** The four stat values of a card. Integers only: comparison must never depend on float rounding. */
USTRUCT(BlueprintType)
struct FFCBAttributes
{
	GENERATED_BODY()

	/** Years since birth (or since the founding of the realm, for the undying). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 AgeYears = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 Power = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 Speed = 0;

	/** Centimetres. Integer centimetres keep Height comparisons exact for very small creatures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 HeightCm = 0;

	int32 Get(EFCBAttribute InAttribute) const
	{
		switch (InAttribute)
		{
		case EFCBAttribute::Age:		return AgeYears;
		case EFCBAttribute::Power:		return Power;
		case EFCBAttribute::Speed:		return Speed;
		case EFCBAttribute::Height:		return HeightCm;
		default:						return 0;
		}
	}

	void Set(EFCBAttribute InAttribute, int32 Value)
	{
		switch (InAttribute)
		{
		case EFCBAttribute::Age:		AgeYears = Value;		break;
		case EFCBAttribute::Power:		Power = Value;		break;
		case EFCBAttribute::Speed:		Speed = Value;		break;
		case EFCBAttribute::Height:		HeightCm = Value;		break;
		default: break;
		}
	}

	void Add(EFCBAttribute InAttribute, int32 Delta)
	{
		Set(InAttribute, Get(InAttribute) + Delta);
	}

	/** Adds Delta to every attribute (used by AlwaysPenalty). */
	void AddToAll(int32 Delta)
	{
		AgeYears += Delta; Power += Delta; Speed += Delta; HeightCm += Delta;
	}

	bool operator==(const FFCBAttributes& Other) const
	{
		return AgeYears == Other.AgeYears && Power == Other.Power
			&& Speed == Other.Speed && HeightCm == Other.HeightCm;
	}
};

/** One keyword ability as authored on a card row. */
USTRUCT(BlueprintType)
struct FFCBCardAbility
{
	GENERATED_BODY()

	/** Stable identifier, matches a row in DT_Abilities (used for tooltips and analytics). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	EFCBAbilityEffect Effect = EFCBAbilityEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	EFCBAttribute Attribute = EFCBAttribute::Power;

	/** Primary tuning knob; meaning depends on Effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	int32 ParamA = 0;

	/** Secondary tuning knob (thresholds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	int32 ParamB = 0;

	/** Only used by BonusAgainstFaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	EFCBFaction Faction = EFCBFaction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	uint32 Flags = 0u;

	bool IsNone() const { return Effect == EFCBAbilityEffect::None; }
};

/** Faction doctrine: one global, always-on modifier applied to every card of the faction. */
USTRUCT(BlueprintType)
struct FFCBFactionDoctrine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FString DoctrineName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FString Lore;

	/** Attribute the doctrine touches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	EFCBAttribute BonusAttribute = EFCBAttribute::Power;

	/** Percent of base value, applied and rounded at load time. Negative allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	int32 BonusPercent = 0;

	/** Trade-off attribute (e.g. Beastwild is fast but small). INDEX_NONE-style 0 percent = unused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	EFCBAttribute PenaltyAttribute = EFCBAttribute::Power;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	int32 PenaltyPercent = 0;

	/** Colour used by the UI for the card frame (RGBA8 packed, art-team override in DT_Factions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	FString FrameColor = TEXT("#8A8A8A");
};

/** Immutable definition, loaded from the card database once per session. */
USTRUCT()
struct FFCBCardDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	EFCBFaction Faction = EFCBFaction::None;

	UPROPERTY()
	EFCBRarity Rarity = EFCBRarity::Common;

	/** Raw printed values, doctrine NOT applied yet. */
	UPROPERTY()
	FFCBAttributes Base;

	UPROPERTY()
	FFCBAttributes Effective;

	UPROPERTY()
	TArray<FFCBCardAbility> Abilities;

	UPROPERTY()
	FString Flavor;

	/** Soft object path to the portrait texture, e.g. /Game/Art/Cards/T_AncientRedWyrm.T_AncientRedWyrm */
	UPROPERTY()
	FString PortraitPath;

	/** Collector number within the set, e.g. "Dragonkind 03/11". */
	UPROPERTY()
	int32 SetIndex = 0;

	UPROPERTY()
	int32 LoreEntryNumber = 0;
};

/** A mutable copy of a card that lives in a deck / hand / capture pile. */
USTRUCT(BlueprintType)
struct FFCBCardInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FName Id;

	/** Index into FFCBCardDatabase::Cards (INDEX_NONE when the instance is orphaned). */
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	int32 DefIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	EFCBFaction Faction = EFCBFaction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	EFCBRarity Rarity = EFCBRarity::Common;

	/** Stats as currently printed for this copy (abilities can add further temporary modifiers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FFCBAttributes Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	TArray<FFCBCardAbility> Abilities;

	/** Number of still-usable "once per match" charges (Undying, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 RemainingCharges = 1;

	/** Cumulative self-buffs that persist across rounds (Bloodthirst style stacks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	int32 BonusPowerStacks = 0;

	/** Localised reason string for debugging / reveal popups. */
	UPROPERTY()
	FString LastModifierNote;
};

/** Attribute display helper (single source of truth for UI + logs + CSV). */
namespace FCBAttributeUtil
{
	inline FString ToString(EFCBAttribute InAttribute)
	{
		switch (InAttribute)
		{
		case EFCBAttribute::Age:		return TEXT("Age");
		case EFCBAttribute::Power:		return TEXT("Power");
		case EFCBAttribute::Speed:		return TEXT("Speed");
		case EFCBAttribute::Height:		return TEXT("Height");
		default:						return TEXT("?");
		}
	}

	inline bool FromString(const FString& InString, EFCBAttribute& OutAttribute)
	{
		if (InString.Equals(TEXT("Age"), ESearchCase::IgnoreCase))		{ OutAttribute = EFCBAttribute::Age; return true; }
		if (InString.Equals(TEXT("Power"), ESearchCase::IgnoreCase))		{ OutAttribute = EFCBAttribute::Power; return true; }
		if (InString.Equals(TEXT("Speed"), ESearchCase::IgnoreCase))		{ OutAttribute = EFCBAttribute::Speed; return true; }
		if (InString.Equals(TEXT("Height"), ESearchCase::IgnoreCase)
			|| InString.Equals(TEXT("HeightCm"), ESearchCase::IgnoreCase))	{ OutAttribute = EFCBAttribute::Height; return true; }
		return false;
	}

	/** 1219999 -> "1,219,999". Mythic ages are six figures; without separators they are unreadable at a glance. */
	inline FString WithThousandsSeparator(int32 Value)
	{
		const bool bNegative = Value < 0;
		FString Digits = FString::FromInt(bNegative ? -Value : Value);
		FString Grouped;
		const int32 Len = Digits.Len();
		for (int32 Index = 0; Index < Len; ++Index)
		{
			if (Index > 0 && (Len - Index) % 3 == 0)
			{
				Grouped.AppendChar(TEXT(','));
			}
			Grouped.AppendChar(Digits[Index]);
		}
		return bNegative ? FString(TEXT("-")) + Grouped : Grouped;
	}

	inline FString FormatValue(EFCBAttribute InAttribute, int32 Value)
	{
		switch (InAttribute)
		{
		case EFCBAttribute::Age:		return FString::Printf(TEXT("%s yr"), *WithThousandsSeparator(Value));
		case EFCBAttribute::Power:		return FString::Printf(TEXT("%d"), Value);
		case EFCBAttribute::Speed:		return FString::Printf(TEXT("%d"), Value);
		case EFCBAttribute::Height:
			{
				// Render as metres with two decimals for readability, from integer centimetres.
				const int32 Metres = Value / 100;
				const int32 Rem = (Value % 100) < 0 ? -(Value % 100) : (Value % 100);
				return FString::Printf(TEXT("%d.%02d m"), Metres, Rem);
			}
		default:						return FString::Printf(TEXT("%d"), Value);
		}
	}
}

/** Rarity helpers shared by engine, AI and UI. */
namespace FCBRarityUtil
{
	inline FString ToString(EFCBRarity InRarity)
	{
		switch (InRarity)
		{
		case EFCBRarity::Common:		return TEXT("Common");
		case EFCBRarity::Uncommon:		return TEXT("Uncommon");
		case EFCBRarity::Rare:			return TEXT("Rare");
		case EFCBRarity::Epic:			return TEXT("Epic");
		case EFCBRarity::Legendary:		return TEXT("Legendary");
		case EFCBRarity::Mythic:		return TEXT("Mythic");
		default:						return TEXT("?");
		}
	}

	inline bool FromString(const FString& InString, EFCBRarity& OutRarity)
	{
		if (InString.Equals(TEXT("Common"), ESearchCase::IgnoreCase))		{ OutRarity = EFCBRarity::Common; return true; }
		if (InString.Equals(TEXT("Uncommon"), ESearchCase::IgnoreCase))		{ OutRarity = EFCBRarity::Uncommon; return true; }
		if (InString.Equals(TEXT("Rare"), ESearchCase::IgnoreCase))			{ OutRarity = EFCBRarity::Rare; return true; }
		if (InString.Equals(TEXT("Epic"), ESearchCase::IgnoreCase))			{ OutRarity = EFCBRarity::Epic; return true; }
		if (InString.Equals(TEXT("Legendary"), ESearchCase::IgnoreCase))		{ OutRarity = EFCBRarity::Legendary; return true; }
		if (InString.Equals(TEXT("Mythic"), ESearchCase::IgnoreCase))		{ OutRarity = EFCBRarity::Mythic; return true; }
		return false;
	}

	inline int32 ToIndex(EFCBRarity InRarity) { return static_cast<int32>(InRarity); }

	/**
	 * Tie-break priority when a rule needs a "bigger creature wins" fallback, and the
	 * score weight of a captured card (see Docs/Balance.md: rarer cards are worth slightly more
	 * so that a deck full of commons cannot out-score a deck full of legends on volume alone).
	 */
	inline int32 CaptureScore(EFCBRarity InRarity)
	{
		switch (InRarity)
		{
		case EFCBRarity::Common:		return 1;
		case EFCBRarity::Uncommon:		return 1;
		case EFCBRarity::Rare:			return 2;
		case EFCBRarity::Epic:			return 3;
		case EFCBRarity::Legendary:		return 4;
		case EFCBRarity::Mythic:		return 6;
		default:						return 1;
		}
	}
}
/** Faction helpers: canonical names used by DT_Factions, logs and tooltips. */
namespace FCBFactionUtil
{
	inline int32 Count() { return static_cast<int32>(EFCBFaction::Max) - 1; } // excludes None

	inline FString ToString(EFCBFaction InFaction)
	{
		switch (InFaction)
		{
		case EFCBFaction::None:				return TEXT("Unaligned");
		case EFCBFaction::Dragonkind:		return TEXT("Dragonkind");
		case EFCBFaction::ArcaneOrders:		return TEXT("Arcane Orders");
		case EFCBFaction::OrderOfTheBlade:	return TEXT("Order of the Blade");
		case EFCBFaction::Ironhold:			return TEXT("Ironhold");
		case EFCBFaction::FeyCourt:			return TEXT("Fey Court");
		case EFCBFaction::UndyingLegion:	return TEXT("Undying Legion");
		case EFCBFaction::AbyssalHorde:		return TEXT("Abyssal Horde");
		case EFCBFaction::CelestialHost:	return TEXT("Celestial Host");
		case EFCBFaction::Beastwild:		return TEXT("Beastwild");
		case EFCBFaction::DeepwaterCourt:	return TEXT("Deepwater Court");
		default:							return TEXT("?");
		}
	}

	/** Accepts the enum name ("DeepwaterCourt") or the display name ("Deepwater Court") - CSV friendly. */
	inline bool FromString(const FString& InString, EFCBFaction& OutFaction)
	{
		for (int32 Index = 0; Index <= static_cast<int32>(EFCBFaction::Max); ++Index)
		{
			const EFCBFaction Candidate = static_cast<EFCBFaction>(Index);
			if (InString.Equals(ToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutFaction = Candidate;
				return true;
			}
		}
		// Tolerate the enum-style spellings used by the DataTable importer.
		static const TCHAR* EnumNames[] = {
			TEXT("None"), TEXT("Dragonkind"), TEXT("ArcaneOrders"), TEXT("OrderOfTheBlade"),
			TEXT("Ironhold"), TEXT("FeyCourt"), TEXT("UndyingLegion"), TEXT("AbyssalHorde"),
			TEXT("CelestialHost"), TEXT("Beastwild"), TEXT("DeepwaterCourt")
		};
		for (int32 Index = 0; Index <= static_cast<int32>(EFCBFaction::DeepwaterCourt); ++Index)
		{
			if (InString.Equals(FString(EnumNames[Index]), ESearchCase::IgnoreCase))
			{
				OutFaction = static_cast<EFCBFaction>(Index);
				return true;
			}
		}
		return false;
	}

	/** Display text for the doctrine, used on the faction banner in the UI. */
	inline FString DefaultDoctrineName(EFCBFaction InFaction)
	{
		switch (InFaction)
		{
		case EFCBFaction::Dragonkind:		return TEXT("Wyrmskin");
		case EFCBFaction::ArcaneOrders:		return TEXT("Spellshift");
		case EFCBFaction::OrderOfTheBlade:return TEXT("Oathbound");
		case EFCBFaction::Ironhold:		return TEXT("Deepforged");
		case EFCBFaction::FeyCourt:		return TEXT("Feylight");
		case EFCBFaction::UndyingLegion:	return TEXT("Deathless");
		case EFCBFaction::AbyssalHorde:	return TEXT("Hellmark");
		case EFCBFaction::CelestialHost:	return TEXT("Ascendant");
		case EFCBFaction::Beastwild:		return TEXT("Untamed");
		case EFCBFaction::DeepwaterCourt:	return TEXT("Tidebound");
		default:							return TEXT("None");
		}
	}
}

/** Ability effect metadata: canonical names for DT_Abilities and the validator. */
namespace FCBAbilityUtil
{
	inline FString ToString(EFCBAbilityEffect InEffect)
	{
		switch (InEffect)
		{
		case EFCBAbilityEffect::None:						return TEXT("None");
		case EFCBAbilityEffect::FlatBonusOnAttribute:		return TEXT("Flat Bonus On Attribute");
		case EFCBAbilityEffect::PercentBonusOnAttribute:	return TEXT("Percent Bonus On Attribute");
		case EFCBAbilityEffect::WinsTiesOnAttribute:		return TEXT("Wins Ties On Attribute");
		case EFCBAbilityEffect::NullifyOpponentAbilities:	return TEXT("Nullify Opponent Abilities");
		case EFCBAbilityEffect::BonusPerFactionInOwnPile:	return TEXT("Bonus Per Faction Card In Pile");
		case EFCBAbilityEffect::BonusAgainstFaction:		return TEXT("Bonus Against Faction");
		case EFCBAbilityEffect::BonusIfOpponentStatAbove:	return TEXT("Bonus If Opponent Stat Above");
		case EFCBAbilityEffect::PercentBonusWhenLeading:		return TEXT("Flat Bonus When Leading");
		case EFCBAbilityEffect::PercentBonusWhenDefending:	return TEXT("Flat Bonus When Defending");
		case EFCBAbilityEffect::AlwaysPenalty:				return TEXT("Always Penalty");
		case EFCBAbilityEffect::WinIfMarginWithin:			return TEXT("Win If Margin Within");
		case EFCBAbilityEffect::OnWinTakeDeckTop:			return TEXT("On Win Take Deck Top");
		case EFCBAbilityEffect::OnWinForceExtraCapture:	return TEXT("On Win Force Extra Capture");
		case EFCBAbilityEffect::OnLoseReturnToHandOnce:	return TEXT("On Lose Return To Hand (Once)");
		case EFCBAbilityEffect::PotScoreBonusOnWin:		return TEXT("Pot Score Bonus On Win");
		default:											return TEXT("?");
		}
	}

	inline bool FromString(const FString& InString, EFCBAbilityEffect& OutEffect)
	{
		// Accepted spellings, in enum order: { display name, C++ identifier }. Both are valid in CSV files so
		// a designer can paste either the human text from DT_Abilities or the raw enum name.
		static const TCHAR* Names[][2] = {
			{ TEXT("None"), TEXT("None") },
			{ TEXT("Flat Bonus On Attribute"), TEXT("FlatBonusOnAttribute") },
			{ TEXT("Percent Bonus On Attribute"), TEXT("PercentBonusOnAttribute") },
			{ TEXT("Wins Ties On Attribute"), TEXT("WinsTiesOnAttribute") },
			{ TEXT("Nullify Opponent Abilities"), TEXT("NullifyOpponentAbilities") },
			{ TEXT("Percent Bonus Per Faction Trophy"), TEXT("BonusPerFactionInOwnPile") },
			{ TEXT("Percent Bonus Against Faction"), TEXT("BonusAgainstFaction") },
			{ TEXT("Percent Bonus If Opponent Stat Above"), TEXT("BonusIfOpponentStatAbove") },
			{ TEXT("Percent Bonus When Leading"), TEXT("PercentBonusWhenLeading") },
			{ TEXT("Percent Bonus When Defending"), TEXT("PercentBonusWhenDefending") },
			{ TEXT("Always Penalty (Pct)"), TEXT("AlwaysPenalty") },
			{ TEXT("Win If Narrow Margin"), TEXT("WinIfMarginWithin") },
			{ TEXT("On Win Take Deck Top"), TEXT("OnWinTakeDeckTop") },
			{ TEXT("On Win Force Extra Capture"), TEXT("OnWinForceExtraCapture") },
			{ TEXT("On Lose Return To Hand (Once)"), TEXT("OnLoseReturnToHandOnce") },
			{ TEXT("Pot Score Bonus On Win"), TEXT("PotScoreBonusOnWin") }
		};

		const int32 Count = static_cast<int32>(sizeof(Names) / sizeof(Names[0]));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FString Display(Names[Index][0]);
			const FString Identifier(Names[Index][1]);
			if (InString.Equals(Display, ESearchCase::IgnoreCase) || InString.Equals(Identifier, ESearchCase::IgnoreCase))
			{
				OutEffect = static_cast<EFCBAbilityEffect>(Index);
				return true;
			}
		}
		return false;
	}

	/**
	 * Flags are derived from the effect rather than authored, so a designer can never create an ability
	 * that triggers for the wrong seat (a classic data bug in card games).
	 */
	inline uint32 DerivedFlags(EFCBAbilityEffect InEffect)
	{
		switch (InEffect)
		{
		case EFCBAbilityEffect::PercentBonusWhenLeading:
			return FCBAbilityFlags::Passive | FCBAbilityFlags::AttackerOnly;
		case EFCBAbilityEffect::PercentBonusWhenDefending:
			return FCBAbilityFlags::Passive | FCBAbilityFlags::DefenderOnly;
		case EFCBAbilityEffect::WinIfMarginWithin:
			return FCBAbilityFlags::Passive | FCBAbilityFlags::DefenderOnly;
		case EFCBAbilityEffect::NullifyOpponentAbilities:
		case EFCBAbilityEffect::AlwaysPenalty:
		case EFCBAbilityEffect::WinsTiesOnAttribute:
			return FCBAbilityFlags::Passive;
		case EFCBAbilityEffect::OnWinTakeDeckTop:
		case EFCBAbilityEffect::OnWinForceExtraCapture:
		case EFCBAbilityEffect::PotScoreBonusOnWin:
			return FCBAbilityFlags::TriggerOnWin;
		case EFCBAbilityEffect::OnLoseReturnToHandOnce:
			return FCBAbilityFlags::TriggerOnLose | FCBAbilityFlags::ConsumedOnce;
		default:
			return FCBAbilityFlags::Passive;
		}
	}

	/** Effects that change the number being compared (as opposed to firing after the fact). */
	inline bool AffectsContestedValue(EFCBAbilityEffect InEffect)
	{
		switch (InEffect)
		{
		case EFCBAbilityEffect::FlatBonusOnAttribute:
		case EFCBAbilityEffect::PercentBonusOnAttribute:
		case EFCBAbilityEffect::BonusPerFactionInOwnPile:
		case EFCBAbilityEffect::BonusAgainstFaction:
		case EFCBAbilityEffect::BonusIfOpponentStatAbove:
		case EFCBAbilityEffect::PercentBonusWhenLeading:
		case EFCBAbilityEffect::PercentBonusWhenDefending:
		case EFCBAbilityEffect::AlwaysPenalty:
			return true;
		default:
			return false;
		}
	}

	/** True for effects that must be evaluated before the winner is known. */
	inline bool IsPreResolution(EFCBAbilityEffect InEffect)
	{
		return AffectsContestedValue(InEffect)
			|| InEffect == EFCBAbilityEffect::NullifyOpponentAbilities
			|| InEffect == EFCBAbilityEffect::WinsTiesOnAttribute
			|| InEffect == EFCBAbilityEffect::WinIfMarginWithin;
	}

	/** A rough power rating so the balancer can compare cards that have different stats. */
	inline int32 PowerCost(EFCBAbilityEffect InEffect)
	{
		switch (InEffect)
		{
		case EFCBAbilityEffect::None:						return 0;
		case EFCBAbilityEffect::AlwaysPenalty:				return -3;
		case EFCBAbilityEffect::PercentBonusOnAttribute:	return 2;
		case EFCBAbilityEffect::WinsTiesOnAttribute:		return 3;
		case EFCBAbilityEffect::NullifyOpponentAbilities:	return 4;
		case EFCBAbilityEffect::PercentBonusWhenDefending:	return 2;
		case EFCBAbilityEffect::WinIfMarginWithin:			return 4;
		case EFCBAbilityEffect::OnLoseReturnToHandOnce:	return 5;
		case EFCBAbilityEffect::OnWinForceExtraCapture:	return 6;
		case EFCBAbilityEffect::OnWinTakeDeckTop:			return 3;
		case EFCBAbilityEffect::PotScoreBonusOnWin:			return 2;
		default:											return 1;
		}
	}
}
