// Copyright (c) Fantasy Card Battle. All rights reserved.

#include "FCBCardDatabase.h"

namespace
{
	/** Trim + strip UTF-8 BOM and stray CR from a CSV cell. */
	FString CleanCell(const FString& InCell)
	{
		FString Out = InCell;
		while (Out.Len() > 0 && (Out[0] == TEXT(' ') || Out[0] == TEXT('\t') || Out[0] == TEXT('\xEF')))
		{
			Out = Out.Mid(1);
		}
		while (Out.Len() > 0 && (Out[Out.Len() - 1] == TEXT(' ') || Out[Out.Len() - 1] == TEXT('\t') || Out[Out.Len() - 1] == TEXT('\r')))
		{
			Out = Out.Left(Out.Len() - 1);
		}
		return Out;
	}

	bool ParseInt(const FString& Text, int32& OutValue)
	{
		if (Text.Len() == 0)
		{
			return false;
		}
		int32 Index = 0;
		if (Text.Len() > 0 && (Text[0] == TEXT('+') || Text[0] == TEXT('-')))
		{
			Index = 1;
		}
		if (Index >= Text.Len())
		{
			return false;
		}
		const bool bNegative = Text[0] == TEXT('-');

		int64 Value = 0;
		int32 Digits = 0;
		for (; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];
			if (Char < TEXT('0') || Char > TEXT('9'))
			{
				return false;
			}
			Value = Value * 10 + (Char - TEXT('0'));
			++Digits;
			if (Value > 0x7fffffffLL)
			{
				Value = 0x7fffffffLL;   // saturate instead of wrapping on absurd cells
			}
		}
		OutValue = static_cast<int32>(bNegative ? -Value : Value);
		return Digits > 0;
	}

	int32 ParseIntOr(const FString& Text, int32 Fallback)
	{
		int32 Value = Fallback;
		return ParseInt(Text, Value) ? Value : Fallback;
	}

	/** Splits on \n / \r\n and drops a trailing empty line. Kept here so the loader has no Engine deps. */
	void SplitLines(const FString& Text, TArray<FString>& OutLines)
	{
		OutLines.Reset();
		FString Current;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];
			if (Char == TEXT('\n'))
			{
				if (Current.Len() > 0 && Current[Current.Len() - 1] == TEXT('\r'))
				{
					Current = Current.Left(Current.Len() - 1);
				}
				OutLines.Add(Current);
				Current.Reset();
			}
			else
			{
				Current.AppendChar(Char);
			}
		}
		OutLines.Add(Current);

		// Strip a UTF-8 BOM if the CSV was saved by a Windows tool.
		if (OutLines.Num() > 0 && OutLines[0].Len() > 0 && OutLines[0][0] == TEXT('\xEF'))
		{
			OutLines[0] = OutLines[0].Mid(3);
		}
		while (OutLines.Num() > 0 && OutLines.Last().Len() == 0)
		{
			OutLines.RemoveAt(OutLines.Num() - 1);
		}
	}

	FFCBDataIssue MakeIssue(int32 Line, const TCHAR* Field, const FString& Message, bool bFatal)
	{
		FFCBDataIssue Issue;
		Issue.Line = Line;
		Issue.Field = FString(Field);
		Issue.Message = Message;
		Issue.bFatal = bFatal;
		return Issue;
	}
}

void FFCBCardDatabase::Reset()
{
	Cards.Reset();
	DoctrineByFaction.Reset();
	AbilityCatalog.Reset();
	StrengthPercentile.Reset();
}

void FFCBCardDatabase::SplitCsvLine(const FString& Line, TArray<FString>& OutCells)
{
	OutCells.Reset();
	FString Cell;
	bool bInQuotes = false;
	for (int32 Index = 0; Index < Line.Len(); ++Index)
	{
		const TCHAR Char = Line[Index];
		if (bInQuotes)
		{
			if (Char == TEXT('"'))
			{
				if (Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
				{
					Cell.AppendChar(TEXT('"'));
					++Index;   // escaped quote
				}
				else
				{
					bInQuotes = false;
				}
			}
			else
			{
				Cell.AppendChar(Char);
			}
		}
		else if (Char == TEXT('"'))
		{
			bInQuotes = true;
		}
		else if (Char == TEXT(','))
		{
			OutCells.Add(CleanCell(Cell));
			Cell.Reset();
		}
		else
		{
			Cell.AppendChar(Char);
		}
	}
	OutCells.Add(CleanCell(Cell));
}

bool FFCBCardDatabase::FindColumn(const TArray<FString>& Header, const TCHAR* ColumnName, int32& OutIndex)
{
	for (int32 Index = 0; Index < Header.Num(); ++Index)
	{
		if (Header[Index].Equals(FString(ColumnName), ESearchCase::IgnoreCase))
		{
			OutIndex = Index;
			return true;
		}
	}
	return false;
}

int32 FFCBCardDatabase::FindIndexById(FName Id) const
{
	if (Id.IsNone())
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (Cards[Index].Id == Id)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

float FFCBCardDatabase::GetAttributePercentile(EFCBAttribute InAttribute, int32 Value) const
{
	if (Cards.Num() == 0)
	{
		return 0.5f;
	}
	int32 Below = 0;
	int32 Equal = 0;
	for (const FFCBCardDef& Def : Cards)
	{
		const int32 Other = Def.Effective.Get(InAttribute);
		if (Other < Value)
		{
			++Below;
		}
		else if (Other == Value)
		{
			++Equal;
		}
	}
	// Mid-rank: a value tied with half the pool sits at 0.5.
	return (static_cast<float>(Below) + 0.5f * static_cast<float>(Equal)) / static_cast<float>(Cards.Num());
}

int32 FFCBCardDatabase::GetCardCountForFaction(EFCBFaction InFaction) const
{
	int32 Count = 0;
	for (const FFCBCardDef& Def : Cards)
	{
		if (Def.Faction == InFaction)
		{
			++Count;
		}
	}
	return Count;
}

bool FFCBCardDatabase::MakeInstance(int32 CardIndex, FFCBCardInstance& OutInstance) const
{
	if (!Cards.IsValidIndex(CardIndex))
	{
		return false;
	}
	const FFCBCardDef& Def = Cards[CardIndex];

	OutInstance = FFCBCardInstance();
	OutInstance.Id = Def.Id;
	OutInstance.DefIndex = CardIndex;
	OutInstance.Name = Def.Name;
	OutInstance.Faction = Def.Faction;
	OutInstance.Rarity = Def.Rarity;
	OutInstance.Stats = Def.Effective;   // doctrine is baked in at load time
	OutInstance.Abilities = Def.Abilities;
	OutInstance.RemainingCharges = 0;
	for (const FFCBCardAbility& Ability : OutInstance.Abilities)
	{
		if ((Ability.Flags & FCBAbilityFlags::ConsumedOnce) != 0u)
		{
			OutInstance.RemainingCharges = 1;
		}
	}
	return true;
}

int32 FFCBCardDatabase::GetCardCountForRarity(EFCBRarity InRarity) const
{
	int32 Count = 0;
	for (const FFCBCardDef& Def : Cards)
	{
		if (Def.Rarity == InRarity)
		{
			++Count;
		}
	}
	return Count;
}

bool FFCBCardDatabase::LoadFromCsvStrings(
	const FString& CardsCsv,
	const FString& FactionsCsv,
	const FString& AbilitiesCsv,
	TArray<FFCBDataIssue>& OutIssues)
{
	Reset();

	auto AddIssue = [&OutIssues](int32 Line, const TCHAR* Field, const FString& Message, bool bFatal)
	{
		FFCBDataIssue Issue;
		Issue.Line = Line;
		Issue.Field = FString(Field);
		Issue.Message = Message;
		Issue.bFatal = bFatal;
		OutIssues.Add(Issue);
	};

	// ---------------------------------------------------------------- factions / doctrines
	DoctrineByFaction.SetNum(static_cast<int32>(EFCBFaction::Max));
	for (int32 Index = 0; Index < DoctrineByFaction.Num(); ++Index)
	{
		FFCBFactionDoctrine& Doctrine = DoctrineByFaction[Index];
		Doctrine.Name = FCBFactionUtil::ToString(static_cast<EFCBFaction>(Index));
		Doctrine.DoctrineName = FCBFactionUtil::DefaultDoctrineName(static_cast<EFCBFaction>(Index));
	}

	TArray<FString> FactionLines;
	SplitLines(FactionsCsv, FactionLines);
	if (FactionLines.Num() > 1)
	{
		TArray<FString> Header;
		SplitCsvLine(FactionLines[0], Header);

		int32 ColFaction = INDEX_NONE, ColName = INDEX_NONE, ColDoctrine = INDEX_NONE, ColLore = INDEX_NONE;
		int32 ColAttr = INDEX_NONE, ColPercent = INDEX_NONE, ColColor = INDEX_NONE;
		int32 ColPenaltyAttr = INDEX_NONE, ColPenaltyPercent = INDEX_NONE;
		FindColumn(Header, TEXT("Faction"), ColFaction);
		FindColumn(Header, TEXT("Name"), ColName);
		if (ColFaction == INDEX_NONE)
		{
			ColFaction = ColName;   // UE DataTables key on the "Name" column; treat it as the faction id
		}
		FindColumn(Header, TEXT("DoctrineName"), ColDoctrine);
		FindColumn(Header, TEXT("Lore"), ColLore);
		FindColumn(Header, TEXT("BonusAttribute"), ColAttr);
		FindColumn(Header, TEXT("BonusPercent"), ColPercent);
		FindColumn(Header, TEXT("PenaltyAttribute"), ColPenaltyAttr);
		FindColumn(Header, TEXT("PenaltyPercent"), ColPenaltyPercent);
		FindColumn(Header, TEXT("FrameColor"), ColColor);

		if (ColFaction == INDEX_NONE || ColAttr == INDEX_NONE || ColPercent == INDEX_NONE)
		{
			AddIssue(1, TEXT("DT_Factions"), TEXT("missing a required column (Faction, BonusAttribute, BonusPercent)"), true);
		}
		else
		{
			for (int32 LineIndex = 1; LineIndex < FactionLines.Num(); ++LineIndex)
			{
				TArray<FString> Cells;
				SplitCsvLine(FactionLines[LineIndex], Cells);
				if (ColFaction >= Cells.Num() || Cells[ColFaction].Len() == 0)
				{
					continue;
				}
				EFCBFaction Faction = EFCBFaction::None;
				if (!FCBFactionUtil::FromString(Cells[ColFaction], Faction))
				{
					AddIssue(LineIndex + 1, TEXT("Faction"), FString::Printf(TEXT("unknown faction '%s'"), *Cells[ColFaction]), true);
					continue;
				}

				FFCBFactionDoctrine& Doctrine = DoctrineByFaction[FMath::Clamp(static_cast<int32>(Faction), 0, DoctrineByFaction.Num() - 1)];
				if (ColName >= 0 && ColName < Cells.Num() && Cells[ColName].Len() > 0)
				{
					Doctrine.Name = Cells[ColName];
				}
				if (ColDoctrine >= 0 && ColDoctrine < Cells.Num() && Cells[ColDoctrine].Len() > 0)
				{
					Doctrine.DoctrineName = Cells[ColDoctrine];
				}
				if (ColLore >= 0 && ColLore < Cells.Num())
				{
					Doctrine.Lore = Cells[ColLore];
				}
				if (ColColor >= 0 && ColColor < Cells.Num() && Cells[ColColor].Len() > 0)
				{
					Doctrine.FrameColor = Cells[ColColor];
				}
				if (FCBAttributeUtil::FromString(Cells[ColAttr], Doctrine.BonusAttribute) == false && Cells[ColAttr].Len() > 0)
				{
					AddIssue(LineIndex + 1, TEXT("BonusAttribute"), FString::Printf(TEXT("unknown attribute '%s'"), *Cells[ColAttr]), true);
				}
				Doctrine.BonusPercent = ParseIntOr(Cells[ColPercent], 0);
				if (ColPenaltyAttr >= 0 && ColPenaltyAttr < Cells.Num())
				{
					FCBAttributeUtil::FromString(Cells[ColPenaltyAttr], Doctrine.PenaltyAttribute);
				}
				if (ColPenaltyPercent >= 0 && ColPenaltyPercent < Cells.Num())
				{
					Doctrine.PenaltyPercent = ParseIntOr(Cells[ColPenaltyPercent], 0);
				}
				if (FMath::Abs(Doctrine.BonusPercent) > 25 || FMath::Abs(Doctrine.PenaltyPercent) > 25)
				{
					AddIssue(LineIndex + 1, TEXT("BonusPercent"), FString::Printf(
						TEXT("doctrine %+d%% / %+d%% exceeds the 25%% design budget"), Doctrine.BonusPercent, Doctrine.PenaltyPercent), false);
				}
			}
		}
	}

	// ---------------------------------------------------------------- ability catalogue
	TArray<FString> AbilityLines;
	SplitLines(AbilitiesCsv, AbilityLines);
	if (AbilityLines.Num() > 1)
	{
		TArray<FString> Header;
		SplitCsvLine(AbilityLines[0], Header);

		int32 ColId = INDEX_NONE, ColName = INDEX_NONE, ColText = INDEX_NONE, ColEffect = INDEX_NONE;
		int32 ColAttr = INDEX_NONE, ColDefaultA = INDEX_NONE, ColMinRarity = INDEX_NONE, ColAbilityFaction = INDEX_NONE;
		FindColumn(Header, TEXT("AbilityId"), ColId);
		FindColumn(Header, TEXT("Faction"), ColAbilityFaction);
		FindColumn(Header, TEXT("Name"), ColId);            // tolerate the DataTable row-name column
		FindColumn(Header, TEXT("DisplayName"), ColName);
		FindColumn(Header, TEXT("Description"), ColText);
		FindColumn(Header, TEXT("Effect"), ColEffect);
		FindColumn(Header, TEXT("Attribute"), ColAttr);
		FindColumn(Header, TEXT("DefaultParamA"), ColDefaultA);
		FindColumn(Header, TEXT("MinRarity"), ColMinRarity);

		for (int32 LineIndex = 1; LineIndex < AbilityLines.Num(); ++LineIndex)
		{
			TArray<FString> Cells;
			SplitCsvLine(AbilityLines[LineIndex], Cells);
			if (ColId < 0 || ColId >= Cells.Num() || Cells[ColId].Len() == 0)
			{
				continue;
			}
			FFCBCardAbility Ability;
			Ability.Id = FName(*Cells[ColId]);
			if (ColName >= 0 && ColName < Cells.Num()) { Ability.DisplayName = Cells[ColName]; }
			if (ColText >= 0 && ColText < Cells.Num()) { Ability.Description = Cells[ColText]; }
			if (ColEffect >= 0 && ColEffect < Cells.Num())
			{
				FCBAbilityUtil::FromString(Cells[ColEffect], Ability.Effect);
			}
			if (ColAttr >= 0 && ColAttr < Cells.Num())
			{
				FCBAttributeUtil::FromString(Cells[ColAttr], Ability.Attribute);
			}
			if (ColDefaultA >= 0 && ColDefaultA < Cells.Num())
			{
				Ability.ParamA = ParseIntOr(Cells[ColDefaultA], 0);
			}
			if (ColAbilityFaction >= 0 && ColAbilityFaction < Cells.Num())
			{
				FCBFactionUtil::FromString(Cells[ColAbilityFaction], Ability.Faction);
			}
			Ability.Flags = FCBAbilityUtil::DerivedFlags(Ability.Effect);
			AbilityCatalog.Add(Ability.Id, Ability);
		}
	}

	// ---------------------------------------------------------------- cards
	TArray<FString> CardLines;
	SplitLines(CardsCsv, CardLines);
	if (CardLines.Num() < 2)
	{
		AddIssue(0, TEXT("DT_Cards"), TEXT("card CSV is empty or has no header row"), true);
		return false;
	}

	TArray<FString> Header;
	SplitCsvLine(CardLines[0], Header);

	auto Col = [&Header](const TCHAR* Primary, const TCHAR* Alias)
	{
		int32 Index = INDEX_NONE;
		if (FindColumn(Header, Primary, Index))
		{
			return Index;
		}
		if (Alias != nullptr && FindColumn(Header, Alias, Index))
		{
			return Index;
		}
		return INDEX_NONE;
	};

	// "Name" is the DataTable row name (UE convention); the creature's shown name lives in DisplayName.
	const int32 ColId = Col(TEXT("Name"), TEXT("CardId"));
	const int32 ColName = Col(TEXT("DisplayName"), TEXT("CardName"));
	const int32 ColFaction = Col(TEXT("Faction"), nullptr);
	const int32 ColRarity = Col(TEXT("Rarity"), nullptr);
	const int32 ColAge = Col(TEXT("AgeYears"), TEXT("Age"));
	const int32 ColPower = Col(TEXT("Power"), nullptr);
	const int32 ColSpeed = Col(TEXT("Speed"), nullptr);
	const int32 ColHeight = Col(TEXT("HeightCm"), TEXT("Height"));
	const int32 ColFlavor = Col(TEXT("Flavor"), TEXT("FlavorText"));
	const int32 ColPortrait = Col(TEXT("PortraitPath"), TEXT("Portrait"));
	const int32 ColSetIndex = Col(TEXT("SetIndex"), nullptr);
	const int32 ColLoreNo = Col(TEXT("LoreEntryNumber"), nullptr);

	static const TCHAR* AbilitySuffixes[7] = {
		TEXT("Id"), TEXT("Name"), TEXT("Text"), TEXT("Effect"), TEXT("Attribute"), TEXT("ParamA"), TEXT("ParamB")
	};
	int32 AbilityCols[2][7];
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		for (int32 Field = 0; Field < 7; ++Field)
		{
			const FString Column = FString::Printf(TEXT("Ability%d%s"), Slot, AbilitySuffixes[Field]);
			int32 Index = INDEX_NONE;
			AbilityCols[Slot][Field] = FindColumn(Header, *Column, Index) ? Index : INDEX_NONE;
		}
	}

	if (ColId == INDEX_NONE || ColFaction == INDEX_NONE || ColRarity == INDEX_NONE
		|| ColAge == INDEX_NONE || ColPower == INDEX_NONE || ColSpeed == INDEX_NONE || ColHeight == INDEX_NONE)
	{
		AddIssue(1, TEXT("DT_Cards"),
			TEXT("missing a required column (Name, Faction, Rarity, AgeYears, Power, Speed, HeightCm)"), true);
		return false;
	}

	for (int32 LineIndex = 1; LineIndex < CardLines.Num(); ++LineIndex)
	{
		const FString& Line = CardLines[LineIndex];
		if (Line.Len() == 0)
		{
			continue;
		}
		TArray<FString> Cells;
		SplitCsvLine(Line, Cells);
		if (ColId >= Cells.Num() || Cells[ColId].Len() == 0)
		{
			continue;
		}

		auto CellAt = [&Cells](int32 Index) -> FString
		{
			return (Index >= 0 && Index < Cells.Num()) ? Cells[Index] : FString();
		};

		FFCBCardDef Def;
		const FString IdText = CellAt(ColId);
		Def.Id = FName(*IdText);
		if (FindIndexById(Def.Id) != INDEX_NONE)
		{
			AddIssue(LineIndex + 1, TEXT("Name"), FString::Printf(TEXT("duplicate card id '%s'"), *IdText), true);
			continue;
		}

		Def.Name = CellAt(ColName).Len() > 0 ? CellAt(ColName) : IdText;
		if (!FCBFactionUtil::FromString(CellAt(ColFaction), Def.Faction))
		{
			AddIssue(LineIndex + 1, TEXT("Faction"), FString::Printf(TEXT("unknown faction '%s'"), *CellAt(ColFaction)), true);
			continue;
		}
		if (!FCBRarityUtil::FromString(CellAt(ColRarity), Def.Rarity))
		{
			AddIssue(LineIndex + 1, TEXT("Rarity"), FString::Printf(TEXT("unknown rarity '%s'"), *CellAt(ColRarity)), true);
			continue;
		}

		Def.Base.AgeYears = ParseIntOr(CellAt(ColAge), 0);
		Def.Base.Power = ParseIntOr(CellAt(ColPower), 0);
		Def.Base.Speed = ParseIntOr(CellAt(ColSpeed), 0);
		Def.Base.HeightCm = ParseIntOr(CellAt(ColHeight), 0);
		Def.Effective = Def.Base;
		Def.Flavor = CellAt(ColFlavor);
		Def.PortraitPath = CellAt(ColPortrait);
		Def.SetIndex = ParseIntOr(CellAt(ColSetIndex), LineIndex);
		Def.LoreEntryNumber = ParseIntOr(CellAt(ColLoreNo), LineIndex);

		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			const FString AbilityId = CellAt(AbilityCols[Slot][0]);
			const FString EffectText = CellAt(AbilityCols[Slot][3]);
			if (AbilityId.Len() == 0 && EffectText.Len() == 0)
			{
				continue;   // empty ability slot
			}

			FFCBCardAbility Ability;
			Ability.Id = FName(*AbilityId);
			Ability.DisplayName = CellAt(AbilityCols[Slot][1]);
			Ability.Description = CellAt(AbilityCols[Slot][2]);
			if (!FCBAbilityUtil::FromString(EffectText, Ability.Effect))
			{
				AddIssue(LineIndex + 1, TEXT("Effect"),
					FString::Printf(TEXT("%s: unknown effect '%s'"), *AbilityId, *EffectText), true);
				continue;
			}
			FCBAttributeUtil::FromString(CellAt(AbilityCols[Slot][4]), Ability.Attribute);
			Ability.ParamA = ParseIntOr(CellAt(AbilityCols[Slot][5]), 0);
			Ability.ParamB = ParseIntOr(CellAt(AbilityCols[Slot][6]), 0);
			Ability.Faction = EFCBFaction::None;   // resolved below from the catalogue or the faction column
			Ability.Flags = FCBAbilityUtil::DerivedFlags(Ability.Effect);

			if (Ability.Id != NAME_None)
			{
				if (const FFCBCardAbility* Canonical = AbilityCatalog.Find(Ability.Id))
				{
					if (Canonical->Effect != Ability.Effect)
					{
						AddIssue(LineIndex + 1, TEXT("Effect"), FString::Printf(TEXT("%s: card says '%s' but DT_Abilities says '%s'"),
							*Ability.Id.ToString(),
							*FCBAbilityUtil::ToString(Ability.Effect),
							*FCBAbilityUtil::ToString(Canonical->Effect)), true);
					}
					if (Ability.DisplayName.Len() == 0) { Ability.DisplayName = Canonical->DisplayName; }
					if (Ability.Description.Len() == 0) { Ability.Description = Canonical->Description; }
					if (Ability.ParamA == 0 && Canonical->ParamA != 0) { Ability.ParamA = Canonical->ParamA; }
					// Bane-style factions live in DT_Abilities so a card row cannot typo a target faction.
					if (Ability.Faction == EFCBFaction::None) { Ability.Faction = Canonical->Faction; }
				}
				else
				{
					AddIssue(LineIndex + 1, TEXT("AbilityId"),
						FString::Printf(TEXT("'%s' is not defined in DT_Abilities"), *Ability.Id.ToString()), false);
				}
			}
			Def.Abilities.Add(Ability);
		}

		Cards.Add(MoveTemp(Def));
	}

	return Cards.Num() > 0;
}

bool FFCBCardDatabase::ComputeDerived(TArray<FFCBDataIssue>& OutIssues)
{
	StrengthPercentile.SetNumZeroed(Cards.Num());
	if (Cards.Num() == 0)
	{
		OutIssues.Add(MakeIssue(0, TEXT("DT_Cards"), TEXT("no cards loaded"), true));return false;
	}

	// ---- apply faction doctrine to produce the printed ("Effective") stats -------------------------
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		FFCBCardDef& Def = Cards[Index];
		Def.Effective = Def.Base;

		const int32 DoctrineIndex = FMath::Clamp(static_cast<int32>(Def.Faction), 0, DoctrineByFaction.Num() - 1);
		if (DoctrineByFaction.IsValidIndex(DoctrineIndex))
		{
			const FFCBFactionDoctrine& Doctrine = DoctrineByFaction[DoctrineIndex];
			if (Doctrine.BonusPercent != 0)
			{
				const int64 Scaled = static_cast<int64>(Def.Effective.Get(Doctrine.BonusAttribute)) * Doctrine.BonusPercent;
				Def.Effective.Add(Doctrine.BonusAttribute, static_cast<int32>((Scaled + (Scaled >= 0 ? 50 : -50)) / 100));
			}
			if (Doctrine.PenaltyPercent != 0)
			{
				const int64 Scaled = static_cast<int64>(Def.Effective.Get(Doctrine.PenaltyAttribute)) * Doctrine.PenaltyPercent;
				Def.Effective.Add(Doctrine.PenaltyAttribute, static_cast<int32>((Scaled + (Scaled >= 0 ? 50 : -50)) / 100));
			}
		}

		// Range validation against the published design limits (FCBLimits in FCBTypes.h).
		// The printed values are the contract with Docs/Balance.md, so they are fatal. Doctrine may push an
		// effective value slightly past a band (a wyrm at Power 112 with +8% reads 121) - that is a warning,
		// and Tools/generate_cards.py clamps it away before shipping.
		auto CheckRange = [Index, &Def, &OutIssues](const TCHAR* Field, int32 Base, int32 Effective, int32 Low, int32 High)
		{
			FFCBDataIssue Issue;
			Issue.Line = Index + 2;
			Issue.Field = FString(Field);
			if (Base < Low || Base > High)
			{
				Issue.Message = FString::Printf(TEXT("%s: printed %s %d outside [%d..%d]"), *Def.Name, Field, Base, Low, High);
				Issue.bFatal = true;
				OutIssues.Add(Issue);
			}
			else if (Effective < Low || Effective > High)
			{
				Issue.Message = FString::Printf(TEXT("%s: %s reads %d after doctrine [%d..%d]"), *Def.Name, Field, Effective, Low, High);
				Issue.bFatal = false;
				OutIssues.Add(Issue);
			}
		};
		CheckRange(TEXT("AgeYears"), Def.Base.AgeYears, Def.Effective.AgeYears, FCBLimits::AgeMin, FCBLimits::AgeMax);
		CheckRange(TEXT("Power"), Def.Base.Power, Def.Effective.Power, FCBLimits::PowerMin, FCBLimits::PowerMax);
		CheckRange(TEXT("Speed"), Def.Base.Speed, Def.Effective.Speed, FCBLimits::SpeedMin, FCBLimits::SpeedMax);
		CheckRange(TEXT("HeightCm"), Def.Base.HeightCm, Def.Effective.HeightCm, FCBLimits::HeightCmMin, FCBLimits::HeightCmMax);

		// Ability sanity checks - the same rules Tools/validate_cards.py enforces on export.
		for (const FFCBCardAbility& Ability : Def.Abilities)
		{
			const FString Tag = FString::Printf(TEXT("%s [%s]"), *Def.Name, *Ability.Id.ToString());

			auto AddIssue = [&OutIssues, Index, &Tag](const TCHAR* Field, const FString& Message, bool bFatal)
			{
				FFCBDataIssue Issue;
				Issue.Line = Index + 2;
				Issue.Field = FString(Field);
				Issue.Message = Tag + Message;
				Issue.bFatal = bFatal;
				OutIssues.Add(Issue);
			};


			switch (Ability.Effect)
			{
			case EFCBAbilityEffect::AlwaysPenalty:
				// Authored as a negative percent so it stays scale-free across all four attributes.
				if (Ability.ParamA >= 0)
				{
					AddIssue(TEXT("ParamA"), TEXT(" AlwaysPenalty is a percent: ParamA must be negative"), true);
				}
				else if (Ability.ParamA < -30)
				{
					AddIssue(TEXT("ParamA"), FString::Printf(TEXT(" penalty %+d%% is harsher than the -30%% floor"), Ability.ParamA), false);
				}
				break;

			case EFCBAbilityEffect::FlatBonusOnAttribute:
				// Flat numbers are only meaningful on the two 1..120 bands; Age/Height span orders of magnitude.
				if (Ability.Attribute != EFCBAttribute::Power && Ability.Attribute != EFCBAttribute::Speed)
				{
					AddIssue(TEXT("Attribute"), FString::Printf(TEXT(" flat bonus on %s is illegal (use a percent effect)"),
						*FCBAttributeUtil::ToString(Ability.Attribute)), true);
				}
				else if (Ability.ParamA <= 0)
				{
					AddIssue(TEXT("ParamA"), TEXT(" flat bonus must be positive (use AlwaysPenalty for a drawback)"), true);
				}
				else if (Ability.ParamA > 14)
				{
					AddIssue(TEXT("ParamA"), FString::Printf(TEXT(" flat bonus %+d is outside the +-14 budget"), Ability.ParamA), false);
				}
				break;

			case EFCBAbilityEffect::PercentBonusOnAttribute:
			case EFCBAbilityEffect::PercentBonusWhenLeading:
			case EFCBAbilityEffect::PercentBonusWhenDefending:
			case EFCBAbilityEffect::BonusPerFactionInOwnPile:
			case EFCBAbilityEffect::BonusAgainstFaction:
			case EFCBAbilityEffect::BonusIfOpponentStatAbove:
				if (Ability.ParamA <= 0)
				{
					AddIssue(TEXT("ParamA"), TEXT(" percent bonus must be positive"), true);
				}
				else if (Ability.ParamA > 40)
				{
					AddIssue(TEXT("ParamA"), FString::Printf(TEXT(" percent bonus %d%% exceeds the 40%% cap"), Ability.ParamA), false);
				}
				if (Ability.Effect == EFCBAbilityEffect::PercentBonusOnAttribute
					&& Ability.Attribute == EFCBAttribute::Max)
				{
					AddIssue(TEXT("Attribute"), TEXT(" percent bonus needs a contested attribute"), true);
				}
				break;

			default:
				break;
			}

			if (Ability.Effect == EFCBAbilityEffect::BonusAgainstFaction && Ability.Faction == EFCBFaction::None)
			{
				AddIssue(TEXT("Faction"), TEXT(" BonusAgainstFaction needs a target faction"), true);
			}
			if (Ability.Effect == EFCBAbilityEffect::WinIfMarginWithin && Ability.ParamA > 20)
			{
				AddIssue(TEXT("ParamA"), FString::Printf(TEXT(" clutch window %d%% is wider than the 20%% budget"), Ability.ParamA), false);
			}
			if (Ability.Effect == EFCBAbilityEffect::WinsTiesOnAttribute && Ability.Attribute == EFCBAttribute::Power)
			{
				AddIssue(TEXT("Attribute"), TEXT(" Power ties are rare by design; prefer Age/Height for tiebreakers"), false);
			}
			if (Ability.Effect == EFCBAbilityEffect::OnWinForceExtraCapture
				&& FCBRarityUtil::ToIndex(Def.Rarity) < FCBRarityUtil::ToIndex(EFCBRarity::Rare))
			{
				AddIssue(TEXT("Rarity"), TEXT(" extra capture should be Rare or above"), false);
			}
			if (Ability.Effect == EFCBAbilityEffect::OnLoseReturnToHandOnce
				&& FCBRarityUtil::ToIndex(Def.Rarity) < FCBRarityUtil::ToIndex(EFCBRarity::Epic))
			{
				AddIssue(TEXT("Rarity"), TEXT(" Undying should be Epic or above"), false);
			}
			if (Ability.Effect == EFCBAbilityEffect::NullifyOpponentAbilities
				&& FCBRarityUtil::ToIndex(Def.Rarity) < FCBRarityUtil::ToIndex(EFCBRarity::Epic))
			{
				AddIssue(TEXT("Rarity"), TEXT(" nullification should be Epic or above"), false);
			}
		}
	}

	// ---- strength model -----------------------------------------------------------------------------
	// Raw strength = mean percentile of the four attributes, nudged by ability cost. Percentiles are
	// computed from the pre-ability stats, then re-ranked so the final values are monotonic in [0..1].
	TArray<float> RawStrength;
	RawStrength.SetNum(Cards.Num());
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		RawStrength[Index] = ComputeRawStrength(Cards[Index], Cards);
	}

	// Rank -> percentile (ties share their mean rank).
	TArray<int32> Order;
	Order.SetNum(Cards.Num());
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		Order[Index] = Index;
	}
	for (int32 A = 1; A < Order.Num(); ++A)   // insertion sort: N is ~110, stable and dependency-free
	{
		int32 Value = Order[A];
		int32 B = A - 1;
		while (B >= 0 && RawStrength[Order[B]] > RawStrength[Value])
		{
			Order[B + 1] = Order[B];
			--B;
		}
		Order[B + 1] = Value;
	}
	const float Denom = FMath::Max(1, Cards.Num() - 1);
	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		StrengthPercentile[Order[Rank]] = static_cast<float>(Rank) / Denom;
	}

	// ---- design-rule pass: no hopeless cards, faction size budget -------------------------------
	int32 Hopeless = 0;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		int32 BeatsAtLeastOne = 0;
		for (int32 Other = 0; Other < Cards.Num(); ++Other)
		{
			if (Other == Index)
			{
				continue;
			}
			bool bCanBeat = false;
			for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
			{
				const EFCBAttribute Attribute = static_cast<EFCBAttribute>(AttrIndex);
				if (Cards[Index].Effective.Get(Attribute) > Cards[Other].Effective.Get(Attribute))
				{
					bCanBeat = true;
					break;
				}
			}
			if (bCanBeat)
			{
				++BeatsAtLeastOne;
			}
		}

		const float Coverage = static_cast<float>(BeatsAtLeastOne) / static_cast<float>(FMath::Max(1, Cards.Num() - 1));
		if (Coverage < 0.999f)
		{
			++Hopeless;
			FFCBDataIssue Issue;
			Issue.Line = Index + 2;
			Issue.Field = TEXT("Coverage");
			Issue.Message = FString::Printf(TEXT("%s can only beat %.1f%% of the pool - re-tune it"),
				*Cards[Index].Name, Coverage * 100.f);
			// Fatal only when a card is nearly unplayable: a designer can still ship a "trap" card on purpose.
			Issue.bFatal = Coverage < 0.7f;
			OutIssues.Add(Issue);
		}
	}

	for (int32 FactionIndex = 0; FactionIndex < DoctrineByFaction.Num(); ++FactionIndex)
	{
		const EFCBFaction Faction = static_cast<EFCBFaction>(FactionIndex);
		if (Faction == EFCBFaction::None)
		{
			continue;
		}
		const int32 Count = GetCardCountForFaction(Faction);
		if (Count > 0 && (Count < 8 || Count > 15))
		{
			FFCBDataIssue Issue;
			Issue.Line = 0;
			Issue.Field = TEXT("Faction");
			Issue.Message = FString::Printf(TEXT("%s has %d cards (design budget 8..15)"),
				*FCBFactionUtil::ToString(Faction), Count);
			Issue.bFatal = false;
			OutIssues.Add(Issue);
		}
	}

	int32 FatalCount = 0;
	for (const FFCBDataIssue& Issue : OutIssues)
	{
		if (Issue.bFatal)
		{
			++FatalCount;
		}
	}
	return FatalCount == 0;
}

float FFCBCardDatabase::ComputeRawStrength(const FFCBCardDef& Def, const TArray<FFCBCardDef>& AllCards)
{
	auto PercentileOf = [&AllCards](EFCBAttribute Attribute, int32 Value) -> float
	{
		if (AllCards.Num() <= 1)
		{
			return 0.5f;
		}
		int32 Below = 0;
		int32 Equal = 0;
		for (const FFCBCardDef& Other : AllCards)
		{
			const int32 OtherValue = Other.Effective.Get(Attribute);
			if (OtherValue < Value) { ++Below; }
			else if (OtherValue == Value) { ++Equal; }
		}
		return (static_cast<float>(Below) + 0.5f * static_cast<float>(Equal)) / static_cast<float>(AllCards.Num());
	};

	float Total = 0.f;
	for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
	{
		const EFCBAttribute Attribute = static_cast<EFCBAttribute>(AttrIndex);
		// Power and Speed are the "duel" attributes, so they carry more weight than Age / Height.
		const float Weight = (Attribute == EFCBAttribute::Power || Attribute == EFCBAttribute::Speed) ? 0.3f : 0.2f;
		Total += Weight * PercentileOf(Attribute, Def.Effective.Get(Attribute));
	}

	for (const FFCBCardAbility& Ability : Def.Abilities)
	{
		int32 Cost = FCBAbilityUtil::PowerCost(Ability.Effect);
		if (Ability.Effect == EFCBAbilityEffect::FlatBonusOnAttribute
			|| Ability.Effect == EFCBAbilityEffect::PercentBonusWhenLeading
			|| Ability.Effect == EFCBAbilityEffect::PercentBonusWhenDefending)
		{
			Cost += Ability.ParamA / 4;
		}
		Total += static_cast<float>(Cost) * 0.01f;
	}
	return Total;
}

int32 FFCBCardDatabase::CollectEligibleCards(const FFCBDeckFilter& Filter, TArray<int32>& OutCardIndexes) const
{
	OutCardIndexes.Reset();
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (Filter.Passes(Cards[Index]))
		{
			OutCardIndexes.Add(Index);
		}
	}
	return OutCardIndexes.Num();
}

int32 FFCBCardDatabase::CollectEligibleCards(const FFCBDeckFilter& Filter, uint32 Seed, TArray<int32>& OutCardIndexes) const
{
	CollectEligibleCards(Filter, OutCardIndexes);
	if (Filter.MaxCards <= 0 || OutCardIndexes.Num() <= Filter.MaxCards)
	{
		return OutCardIndexes.Num();
	}

	// Deterministic partial Fisher-Yates: take MaxCards distinct entries from the eligible pool.
	FRandomStream Rng(static_cast<int32>(Seed & 0x7fffffffu));
	for (int32 Index = 0; Index < Filter.MaxCards && Index < OutCardIndexes.Num(); ++Index)
	{
		const int32 SwapTo = Rng.RandRange(Index, OutCardIndexes.Num() - 1);
		OutCardIndexes.Swap(Index, SwapTo);
	}
	OutCardIndexes.SetNum(Filter.MaxCards);
	return OutCardIndexes.Num();
}

void FFCBCardDatabase::BuildSyntheticPool(int32 CardCount, FFCBCardDatabase& OutDatabase)
{
	OutDatabase.Reset();
	OutDatabase.DoctrineByFaction.SetNum(static_cast<int32>(EFCBFaction::Max));
	for (int32 Index = 0; Index < OutDatabase.DoctrineByFaction.Num(); ++Index)
	{
		OutDatabase.DoctrineByFaction[Index].Name = FCBFactionUtil::ToString(static_cast<EFCBFaction>(Index));
	}

	const int32 FactionCount = FCBFactionUtil::Count();
	for (int32 Index = 0; Index < CardCount; ++Index)
	{
		FFCBCardDef Def;
		Def.Id = FName(*FString::Printf(TEXT("Test_Card_%03d"), Index + 1));
		Def.Name = FString::Printf(TEXT("Test Card %d"), Index + 1);
		Def.Faction = static_cast<EFCBFaction>(1 + (Index % FactionCount));
		Def.Rarity = static_cast<EFCBRarity>(Index % static_cast<int32>(EFCBRarity::Max));
		// Spread all four attributes across their bands so every card beats somebody.
		Def.Base.AgeYears = 10 + (Index * 137) % 900;
		Def.Base.Power = FCBLimits::PowerMin + (Index * 7) % (FCBLimits::PowerMax - FCBLimits::PowerMin);
		Def.Base.Speed = FCBLimits::SpeedMin + (Index * 11) % (FCBLimits::SpeedMax - FCBLimits::SpeedMin);
		Def.Base.HeightCm = 40 + (Index * 233) % 2000;
		Def.Effective = Def.Base;
		OutDatabase.Cards.Add(MoveTemp(Def));
	}
	OutDatabase.StrengthPercentile.SetNumUninitialized(OutDatabase.Cards.Num());
	for (int32 Index = 0; Index < OutDatabase.Cards.Num(); ++Index)
	{
		OutDatabase.StrengthPercentile[Index] = OutDatabase.Cards.Num() > 1
			? static_cast<float>(Index) / static_cast<float>(OutDatabase.Cards.Num() - 1) : 0.5f;
	}
}
