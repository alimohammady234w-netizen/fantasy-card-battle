// Copyright (c) Fantasy Card Battle. All Rights reserved.

#include "FCBDataAssets.h"
#include "FantasyCardBattle.h"
#include "FCBCardDatabase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/** ------------------------------------------------------------------ CSV writing -------------------------------- */

namespace FCBData
{
	/**
	 * Cell formatting. Kept as overloads rather than one giant ternary so adding a property to a row struct is
	 * a one-line change and cannot silently produce an unquoted comma.
	 */
	inline void AppendCell(FString& InOut, const FText& InValue)
	{
		InOut += TEXT('"');
		InOut += InValue.ToString().Replace(TEXT("\""), TEXT("\"\""));
		InOut += TEXT('"');
	}

	inline void AppendCell(FString& InOut, const FString& InValue)
	{
		InOut += TEXT('"');
		InOut += InValue.Replace(TEXT("\""), TEXT("\"\""));
		InOut += TEXT('"');
	}

	inline void AppendCell(FString& InOut, FName InValue)
	{
		InOut += InValue.ToString();
	}

	inline void AppendCell(FString& InOut, int32 InValue)
	{
		InOut += FString::FromInt(InValue);
	}

	inline void AppendCell(FString& InOut, int64 InValue)
	{
		InOut += FString::Printf(TEXT("%lld"), static_cast<long long>(InValue));
	}

	/**
	 * Enums are written as the *identifier*, not the DisplayName: this text is parsed both by UE's DataTable
	 * importer and by FFCBCardDatabase::LoadFromCsvStrings, and only the identifier is stable when the
	 * display names get localized. Written as one overload per enum - explicit beats SFINAE here.
	 */
	inline void AppendCell(FString& InOut, EFCBAttribute InValue)
	{
		InOut += StaticEnum<EFCBAttribute>()->GetNameStringByValue(static_cast<int64>(InValue));
	}

	inline void AppendCell(FString& InOut, EFCBRarity InValue)
	{
		InOut += StaticEnum<EFCBRarity>()->GetNameStringByValue(static_cast<int64>(InValue));
	}

	inline void AppendCell(FString& InOut, EFCBFaction InValue)
	{
		InOut += StaticEnum<EFCBFaction>()->GetNameStringByValue(static_cast<int64>(InValue));
	}

	inline void AppendCell(FString& InOut, EFCBAbilityEffect InValue)
	{
		InOut += StaticEnum<EFCBAbilityEffect>()->GetNameStringByValue(static_cast<int64>(InValue));
	}

	template <typename RowType>
	const RowType* RequireRow(const UDataTable& InTable, FName InRowName, FString& OutError)
	{
		const RowType* Row = InTable.FindRow<RowType>(InRowName, TEXT("FCBDataAssets"));
		if (!Row)
		{
			OutError = FString::Printf(TEXT("Row '%s' in %s is not a %s - the table's Row Struct has changed."),
				*InRowName.ToString(), *InTable.GetName(), *RowType::StaticStruct()->GetName());
		}
		return Row;
	}

	/** The CSV text the loader expects for a card table: canonical header, one line per row. */
	FString BuildCardCsv(const UDataTable& InTable, FString& OutError)
	{
		static const TCHAR* Columns[] = {
			TEXT("Name"), TEXT("DisplayName"), TEXT("Faction"), TEXT("Rarity"),
			TEXT("AgeYears"), TEXT("Power"), TEXT("Speed"), TEXT("HeightCm"),
			TEXT("Flavor"), TEXT("PortraitPath"), TEXT("SetIndex"), TEXT("LoreEntryNumber"),
			TEXT("Ability0Id"), TEXT("Ability0Name"), TEXT("Ability0Text"), TEXT("Ability0Effect"),
			TEXT("Ability0Attribute"), TEXT("Ability0ParamA"), TEXT("Ability0ParamB"),
			TEXT("Ability1Id"), TEXT("Ability1Name"), TEXT("Ability1Text"), TEXT("Ability1Effect"),
			TEXT("Ability1Attribute"), TEXT("Ability1ParamA"), TEXT("Ability1ParamB")
		};

		FString Csv;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Columns); ++Index)
		{
			if (Index > 0)
			{
				Csv += TEXT(",");
			}
			Csv += Columns[Index];
		}
		Csv += LINE_TERMINATOR;

		for (const FName& RowName : InTable.GetRowNames())
		{
			const FFCBCardRow* Row = RequireRow<FFCBCardRow>(InTable, RowName, OutError);
			if (!Row)
			{
				return FString();
			}

			auto Cell = [&Csv](bool& bFirst)
			{
				if (!bFirst)
				{
					Csv += TEXT(",");
				}
				bFirst = false;
			};

			bool bFirst = true;
			Cell(bFirst); Csv += RowName.ToString();
			Cell(bFirst); AppendCell(Csv, Row->DisplayName);
			Cell(bFirst); AppendCell(Csv, Row->Faction);
			Cell(bFirst); AppendCell(Csv, Row->Rarity);
			Cell(bFirst); AppendCell(Csv, Row->AgeYears);
			Cell(bFirst); AppendCell(Csv, Row->Power);
			Cell(bFirst); AppendCell(Csv, Row->Speed);
			Cell(bFirst); AppendCell(Csv, Row->HeightCm);
			Cell(bFirst); AppendCell(Csv, Row->Flavor);
			Cell(bFirst); AppendCell(Csv, Row->PortraitPath);
			Cell(bFirst); AppendCell(Csv, Row->SetIndex);
			Cell(bFirst); AppendCell(Csv, Row->LoreEntryNumber);

			// Ability slot 0
			Cell(bFirst); AppendCell(Csv, Row->Ability0Id);
			Cell(bFirst); AppendCell(Csv, Row->Ability0Name);
			Cell(bFirst); AppendCell(Csv, Row->Ability0Text);
			Cell(bFirst); AppendCell(Csv, Row->Ability0Effect);
			Cell(bFirst); AppendCell(Csv, Row->Ability0Attribute);
			Cell(bFirst); AppendCell(Csv, Row->Ability0ParamA);
			Cell(bFirst); AppendCell(Csv, Row->Ability0ParamB);

			// Ability slot 1
			Cell(bFirst); AppendCell(Csv, Row->Ability1Id);
			Cell(bFirst); AppendCell(Csv, Row->Ability1Name);
			Cell(bFirst); AppendCell(Csv, Row->Ability1Text);
			Cell(bFirst); AppendCell(Csv, Row->Ability1Effect);
			Cell(bFirst); AppendCell(Csv, Row->Ability1Attribute);
			Cell(bFirst); AppendCell(Csv, Row->Ability1ParamA);
			Cell(bFirst); AppendCell(Csv, Row->Ability1ParamB);

			Csv += LINE_TERMINATOR;
		}

		return Csv;
	}

	FString BuildFactionCsv(const UDataTable& InTable, FString& OutError)
	{
		FString Csv = TEXT("Name,Faction,DisplayName,DoctrineName,Lore,BonusAttribute,BonusPercent,PenaltyAttribute,PenaltyPercent,FrameColor") LINE_TERMINATOR;
		for (const FName& RowName : InTable.GetRowNames())
		{
			const FFCBFactionRow* Row = RequireRow<FFCBFactionRow>(InTable, RowName, OutError);
			if (!Row)
			{
				return FString();
			}
			Csv += RowName.ToString();
			Csv += TEXT(",");
			AppendCell(Csv, RowName);                       // the enum spelling doubles as the faction id
			Csv += TEXT(",");
			AppendCell(Csv, Row->DisplayName);
			Csv += TEXT(",");
			AppendCell(Csv, Row->DoctrineName);
			Csv += TEXT(",");
			AppendCell(Csv, Row->Lore);
			Csv += TEXT(",");
			AppendCell(Csv, Row->BonusAttribute);
			Csv += TEXT(",");
			AppendCell(Csv, Row->BonusPercent);
			Csv += TEXT(",");
			AppendCell(Csv, Row->PenaltyAttribute);
			Csv += TEXT(",");
			AppendCell(Csv, Row->PenaltyPercent);
			Csv += TEXT(",");
			AppendCell(Csv, Row->FrameColor);
			Csv += LINE_TERMINATOR;
		}
		return Csv;
	}

	FString BuildAbilityCsv(const UDataTable& InTable, FString& OutError)
	{
		FString Csv = TEXT("Name,AbilityId,DisplayName,Description,Effect,Attribute,DefaultParamA,DefaultParamB,Faction,MinRarity") LINE_TERMINATOR;
		for (const FName& RowName : InTable.GetRowNames())
		{
			const FFCBAbilityRow* Row = RequireRow<FFCBAbilityRow>(InTable, RowName, OutError);
			if (!Row)
			{
				return FString();
			}
			Csv += RowName.ToString();
			Csv += TEXT(",");
			AppendCell(Csv, RowName);
			Csv += TEXT(",");
			AppendCell(Csv, Row->DisplayName);
			Csv += TEXT(",");
			AppendCell(Csv, Row->Description);
			Csv += TEXT(",");
			AppendCell(Csv, Row->Effect);
			Csv += TEXT(",");
			AppendCell(Csv, Row->Attribute);
			Csv += TEXT(",");
			AppendCell(Csv, Row->DefaultParamA);
			Csv += TEXT(",");
			AppendCell(Csv, Row->DefaultParamB);
			Csv += TEXT(",");
			AppendCell(Csv, Row->Faction);
			Csv += TEXT(",");
			AppendCell(Csv, Row->MinRarity);
			Csv += LINE_TERMINATOR;
		}
		return Csv;
	}

	bool ParseHexColor(const FString& InHex, FLinearColor& OutColor)
	{
		FString Hex = InHex;
		Hex.TrimStartAndEndInline();
		Hex.RemoveFromStart(TEXT("#"));
		if (Hex.Len() != 6)
		{
			return false;
		}

		auto Nibble = [](TCHAR Char) -> int32
		{
			if (Char >= TEXT('0') && Char <= TEXT('9'))
			{
				return Char - TEXT('0');
			}
			const TCHAR Upper = FChar::ToUpper(Char);
			return (Upper >= TEXT('A') && Upper <= TEXT('F')) ? 10 + (Upper - TEXT('A')) : -1;
		};

		int32 Bytes[3] = { 0, 0, 0 };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const int32 High = Nibble(Hex[Index * 2]);
			const int32 Low = Nibble(Hex[Index * 2 + 1]);
			if (High < 0 || Low < 0)
			{
				return false;
			}
			Bytes[Index] = (High << 4) | Low;
		}

		OutColor = FLinearColor(Bytes[0] / 255.f, Bytes[1] / 255.f, Bytes[2] / 255.f, 1.f);
		return true;
	}

	bool LoadTextFile(const FString& InAbsoluteOrProjectRelativePath, FString& OutText)
	{
		const FString Absolute = FPaths::IsRelative(InAbsoluteOrProjectRelativePath)
			? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / InAbsoluteOrProjectRelativePath)
			: InAbsoluteOrProjectRelativePath;
		return FFileHelper::LoadFileToString(OutText, *Absolute);
	}
}

/** ------------------------------------------------------------------ settings ----------------------------------- */

const UFCBSettings& UFCBSettings::Get()
{
	return *GetDefault<UFCBSettings>();
}

FName UFCBSettings::GetSectionName() const
{
	return TEXT("Fantasy Card Battle");
}

/** ---------------------------------------------------------------- AI profiles -------------------------------- */

void FFCBAiProfileRow::ApplyTo(FFCBAiProfile& InOutProfile) const
{
	InOutProfile.Difficulty = Difficulty;
	InOutProfile.Elo = Elo;
	InOutProfile.RolloutSamples = SearchPly;
	InOutProfile.BlunderPercent = BlunderChancePercent;
	InOutProfile.TimeBudgetMs = TimeBudgetMs;
	if (CardQualityWeightOverride >= 0.f)
	{
		InOutProfile.CardQualityWeight = CardQualityWeightOverride;
	}
	if (RiskAversionOverride >= 0.f)
	{
		InOutProfile.RiskAversion = RiskAversionOverride;
	}
	if (PotValueWeightOverride >= 0.f)
	{
		InOutProfile.PotValueWeight = PotValueWeightOverride;
	}
}

bool UFCBAiProfileAsset::FindRow(EFCBAiDifficulty InDifficulty, FFCBAiProfileRow& OutRow) const
{
	for (const FFCBAiProfileRow& Row : Profiles)
	{
		if (Row.Difficulty == InDifficulty)
		{
			OutRow = Row;
			return true;
		}
	}
	return false;
}

void UFCBAiProfileAsset::Resolve(const UFCBAiProfileAsset* InAsset, EFCBAiDifficulty InDifficulty, FFCBAiProfile& OutProfile)
{
	// Preset first, so a partial table row only has to say what it wants to change.
	OutProfile = FFCBAiProfile::GetPreset(InDifficulty);

	FFCBAiProfileRow Row;
	bool bApplied = false;
	if (InAsset && InAsset->FindRow(InDifficulty, Row))
	{
		Row.ApplyTo(OutProfile);
		bApplied = true;
	}
	else if (GetDefault<UFCBAiProfileAsset>()->FindRow(InDifficulty, Row))
	{
		// The class default object carries Config/DefaultGame.ini's +Profiles entries.
		Row.ApplyTo(OutProfile);
		bApplied = true;
	}

	if (!bApplied)
	{
		UE_LOG(LogFCB, Verbose, TEXT("No AI profile row for %s; using the built-in preset"),
			*StaticEnum<EFCBAiDifficulty>()->GetNameStringByValue(static_cast<int64>(InDifficulty)));
	}
}

/** ---------------------------------------------------------------- rules asset --------------------------------- */

template <typename RowType>
bool UFCBMatchRulesAsset::SerializeTable(const TSoftObjectPtr<UDataTable>& InTable, const TArray<FString>& InColumns, FString& OutCsv, FString& OutError) const
{
	UNUSED(InColumns);
	if (InTable.IsNull())
	{
		OutError = TEXT("Table reference is empty.");
		return false;
	}

	const UDataTable* Table = InTable.LoadSynchronous();
	if (!Table)
	{
		OutError = FString::Printf(TEXT("Could not load %s."), *InTable.ToString());
		return false;
	}
	if (Table->GetRowNames().Num() == 0)
	{
		OutError = FString::Printf(TEXT("%s has no rows."), *Table->GetName());
		return false;
	}

	if constexpr (TIsSameType<RowType, FFCBCardRow>::Value)
	{
		OutCsv = FCBData::BuildCardCsv(*Table, OutError);
	}
	else if constexpr (TIsSameType<RowType, FFCBFactionRow>::Value)
	{
		OutCsv = FCBData::BuildFactionCsv(*Table, OutError);
	}
	else
	{
		OutCsv = FCBData::BuildAbilityCsv(*Table, OutError);
	}
	return OutError.Len() == 0;
}

bool UFCBMatchRulesAsset::BuildDatabase(FFCBCardDatabase& OutDatabase, FString& OutError) const
{
	FString CardsCsv, FactionsCsv, AbilitiesCsv;
	TArray<FString> Unused;

	if (!SerializeTable<FFCBFactionRow>(FactionTable, Unused, FactionsCsv, OutError))
	{
		return false;
	}
	if (!SerializeTable<FFCBAbilityRow>(AbilityTable, Unused, AbilitiesCsv, OutError))
	{
		return false;
	}
	if (!SerializeTable<FFCBCardRow>(CardTable, Unused, CardsCsv, OutError))
	{
		return false;
	}

	TArray<FFCBDataIssue> Issues;
	OutDatabase.LoadFromCsvStrings(CardsCsv, FactionsCsv, AbilitiesCsv, Issues);
	const bool bValid = OutDatabase.ComputeDerived(Issues);

	int32 FatalCount = 0;
	for (const FFCBDataIssue& Issue : Issues)
	{
		if (Issue.bFatal)
		{
			++FatalCount;
			UE_LOG(LogFCBData, Error, TEXT("DT_Cards/DT_Factions/DT_Abilities: %s"), *Issue.ToString());
		}
		else
		{
			UE_LOG(LogFCBData, Warning, TEXT("%s"), *Issue.ToString());
		}
	}

	if (FatalCount > 0)
	{
		OutError = FString::Printf(TEXT("%d fatal data issue(s); see the LogFCBData output above."), FatalCount);
		return false;
	}
	if (!bValid)
	{
		OutError = TEXT("The card database reported itself invalid after validation.");
		return false;
	}

	UE_LOG(LogFCBData, Log, TEXT("Loaded %d cards from DataTables"), OutDatabase.GetCardCount());
	return true;
}

bool UFCBMatchRulesAsset::LoadDatabaseFromCsvDirectory(const FString& InDirectory, FFCBCardDatabase& OutDatabase, FString& OutError,
	TArray<FFCBDataIssue>* OutIssues)
{
	FString CardsCsv, FactionsCsv, AbilitiesCsv;
	const FString Directory = InDirectory.Len() > 0 ? InDirectory : TEXT("Content/Data/Generated");

	if (!FCBData::LoadTextFile(Directory / TEXT("DT_Cards.csv"), CardsCsv) || CardsCsv.Len() == 0)
	{
		OutError = FString::Printf(TEXT("Missing or empty %s/DT_Cards.csv (run: python3 Tools/generate_cards.py)"),
			*Directory);
		return false;
	}
	// Factions and abilities are optional here: the loader has built-in defaults for both, which keeps a
	// half-populated data folder usable instead of fatal.
	FCBData::LoadTextFile(Directory / TEXT("DT_Factions.csv"), FactionsCsv);
	FCBData::LoadTextFile(Directory / TEXT("DT_Abilities.csv"), AbilitiesCsv);

	TArray<FFCBDataIssue> Issues;
	OutDatabase.LoadFromCsvStrings(CardsCsv, FactionsCsv, AbilitiesCsv, Issues);
	const bool bValid = OutDatabase.ComputeDerived(Issues);

	if (OutIssues)
	{
		*OutIssues = Issues;
	}

	int32 FatalCount = 0;
	for (const FFCBDataIssue& Issue : Issues)
	{
		if (Issue.bFatal)
		{
			++FatalCount;
			UE_LOG(LogFCBData, Error, TEXT("%s: %s"), *Directory, *Issue.ToString());
		}
		else
		{
			UE_LOG(LogFCBData, Warning, TEXT("%s"), *Issue.ToString());
		}
	}

	if (FatalCount > 0 || !bValid)
	{
		OutError = FString::Printf(TEXT("%d fatal data issue(s) in %s"), FatalCount, *Directory);
		return false;
	}

	UE_LOG(LogFCBData, Log, TEXT("Loaded %d cards from %s"), OutDatabase.GetCardCount(), *Directory);
	return true;
}

const UFCBMatchRulesAsset* UFCBMatchRulesAsset::GetConfigured()
{
	const UGameInstance* GameInstance = nullptr;   // no dependency on a live instance: settings are enough
	(void)GameInstance;

	const TSoftObjectPtr<UFCBMatchRulesAsset>& Soft = UFCBSettings::Get().RulesAsset;
	if (!Soft.IsNull())
	{
		if (const UFCBMatchRulesAsset* Asset = Soft.LoadSynchronous())
		{
			return Asset;
		}
		UE_LOG(LogFCBData, Warning, TEXT("RulesAsset %s could not be loaded; falling back to the ini defaults"),
			*Soft.ToString());
	}
	return GetDefault<UFCBMatchRulesAsset>();
}

void UFCBMatchRulesAsset::ApplyToConfig(FFCBMatchConfig& InOutConfig) const
{
	InOutConfig.HandSize = DefaultHandSize;
	InOutConfig.TieRule = DefaultTieRule;
	InOutConfig.MaxRounds = DefaultMaxRounds;
	InOutConfig.PotCap = DefaultPotCap;
	InOutConfig.bEnableAbilities = bDefaultEnableAbilities;
	InOutConfig.bBalanceSeats = bDefaultBalanceSeats;
	InOutConfig.EndCondition = DefaultEndCondition;
}

FString UFCBMatchRulesAsset::ExportCardTableToCsv(FString& OutError) const
{
	TArray<FString> Unused;
	FString Csv;
	return SerializeTable<FFCBCardRow>(CardTable, Unused, Csv, OutError) ? Csv : FString();
}
