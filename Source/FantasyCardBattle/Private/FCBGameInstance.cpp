// Copyright (c) Fantasy Card Battle. All rights reserved.

#include "FCBGameInstance.h"
#include "FantasyCardBattle.h"
#include "FCBCardDatabase.h"
#include "Engine/Engine.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"

void UFCBGameInstance::Init()
{
	Super::Init();

	if (!ReloadCardData())
	{
		// Not fatal: ReloadCardData installs a labelled debug pool so the project stays runnable, and the HUD
		// prints why the real data is missing.
		HandleDataError(DataNote);
	}

	FFCBMatchConfig Config;
	ApplySettingsToConfig(Config);
	Match.Configure(Config, &Database);

	if (UFCBSettings::Get().bAutoDealOnMatchStart)
	{
		StartNewMatch(FixedSeedOverride);
	}
}

void UFCBGameInstance::Shutdown()
{
	// UGameInstance::GetTimerManager forwards to the world's FTimerManager - a struct, not a UObject.
	if (FTimerManager* Timers = GetTimerManager())
	{
		Timers->ClearTimer(AiThinkTimer);
	}
	Super::Shutdown();
}

void UFCBGameInstance::RollSeedAndRestart()
{
	// Only the *seed* is random here; the match itself stays fully determined by it, so the number printed in
	// the debug HUD is enough to replay any game that was played after it.
	const uint32 Seed = static_cast<uint32>(FDateTime::Now().GetTicks() & 0x7fffffffu);
	StartNewMatch(static_cast<int32>(Seed));
}

bool UFCBGameInstance::ReloadCardData()
{
	const UFCBSettings& Settings = UFCBSettings::Get();
	const UFCBMatchRulesAsset* Rules = UFCBMatchRulesAsset::GetConfigured();
	FString Error;

	if (Rules && Rules->BuildDatabase(Database, Error))
	{
		DataNote = FString::Printf(TEXT("DataTables via %s: %d cards"), *Rules->GetName(), Database.GetCardCount());
		bDataUsable = true;
	}
	else if (UFCBMatchRulesAsset::LoadDatabaseFromCsvDirectory(Settings.CardDataDirectory, Database, Error))
	{
		DataNote = FString::Printf(TEXT("%s: %d cards"), *Settings.CardDataDirectory, Database.GetCardCount());
		bDataUsable = true;
	}
	else
	{
		// A synthetic pool keeps the HUD, the input path and the AI reachable even in a checkout with no
		// generated data. It is labelled loudly so nobody mistakes it for the shipped set.
		FFCBCardDatabase::BuildSyntheticPool(24, Database);
		DataNote = FString::Printf(TEXT("DEBUG POOL (no card data: %s)"), *Error);
		bDataUsable = false;
	}

	UFCBAiProfileAsset::Resolve(nullptr, AiDifficulty, AiProfile);
	UE_LOG(LogFCBData, Log, TEXT("Card data: %s"), *DataNote);
	BroadcastMatchUpdated();
	return bDataUsable;
}

void UFCBGameInstance::HandleDataError(const FString& InError)
{
	UE_LOG(LogFCBData, Error, TEXT("Card data unavailable: %s"), *InError);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0xFCC010u, 12.f, FColor::Orange,
			FString::Printf(TEXT("Fantasy Card Battle: %s"), *InError));
	}
}

void UFCBGameInstance::ApplySettingsToConfig(FFCBMatchConfig& InOutConfig) const
{
	if (const UFCBMatchRulesAsset* Rules = UFCBMatchRulesAsset::GetConfigured())
	{
		Rules->ApplyToConfig(InOutConfig);
	}

	InOutConfig.bSeatAIsHuman = true;
	InOutConfig.bSeatBIsHuman = false;
	InOutConfig.SeatAName = TEXT("You");
	InOutConfig.SeatBName = FString::Printf(TEXT("AI (%s)"),
		*StaticEnum<EFCBAiDifficulty>()->GetNameStringByValue(static_cast<int64>(AiDifficulty)));
}

bool UFCBGameInstance::StartNewMatch(int32 InSeedOverride)
{
	if (!Database.IsReady())
	{
		ReloadCardData();
	}

	FFCBMatchConfig Config;
	ApplySettingsToConfig(Config);

	if (InSeedOverride >= 0)
	{
		Config.RngSeed = static_cast<uint32>(InSeedOverride);
	}
	else if (Match.IsStarted())
	{
		// A plain "new match" without a seed override replays the same deal, which is what makes the
		// "run it again, I want to see that round" workflow work.
		Config.RngSeed = Match.GetConfig().RngSeed;
	}
	else
	{
		RollSeedAndRestart();
		return Match.IsStarted();
	}

	Match.Configure(Config, &Database);

	FString Error;
	if (!Match.StartMatch(Error))
	{
		HandleDataError(Error);
		return false;
	}

	SelectedHandIndex = 0;
	SelectedAttribute = EFCBAttribute::Power;
	AiChainLength = 0;
	LastAiExplanation.Reset();
	SelectedCardId = NAME_None;

	AiAgent.Initialize(&Database, FCBSeat::B, AiProfile, Config.RngSeed + 7777u);

	UE_LOG(LogFCB, Log, TEXT("New match | seed %u | %d cards | leader: %s"),
		Config.RngSeed, Database.GetCardCount(), *Match.GetState().Seats[Match.GetLeaderSeat()].Name);

	BroadcastMatchUpdated();
	ScheduleAiTurn();
	return true;
}

int32 UFCBGameInstance::FindDefIndex(FName InId) const
{
	return Database.FindIndexById(InId);
}

void UFCBGameInstance::BuildCardView(const FFCBCardInstance& InCard, int32 InHandIndex, FFCBCardView& OutView) const
{
	OutView = FFCBCardView();
	OutView.Id = InCard.Id;
	OutView.DisplayName = InCard.Name;
	OutView.FactionName = *FCBFactionUtil::ToString(InCard.Faction);
	OutView.RarityName = *FCBRarityUtil::ToString(InCard.Rarity);
	OutView.AgeYears = InCard.Stats.AgeYears;
	OutView.Power = InCard.Stats.Power;
	OutView.Speed = InCard.Stats.Speed;
	OutView.HeightCm = InCard.Stats.HeightCm;
	OutView.AgeText = FCBUi::FormatAttribute(EFCBAttribute::Age, InCard.Stats.AgeYears);
	OutView.PowerText = FCBUi::FormatAttribute(EFCBAttribute::Power, InCard.Stats.Power);
	OutView.SpeedText = FCBUi::FormatAttribute(EFCBAttribute::Speed, InCard.Stats.Speed);
	OutView.HeightText = FCBUi::FormatAttribute(EFCBAttribute::Height, InCard.Stats.HeightCm);
	OutView.HandIndex = InHandIndex;
	OutView.bIsTopOfHand = InHandIndex == 0;
	OutView.RemainingCharges = InCard.RemainingCharges;
	OutView.PowerStacks = InCard.BonusPowerStacks;
	OutView.StrengthPercentile = Database.GetStrengthPercentile(InCard.DefIndex);

	const int32 DefIndex = FindDefIndex(InCard.Id);
	if (const FFCBCardDef* Def = Database.Cards.IsValidIndex(DefIndex) ? &Database.Cards[DefIndex] : nullptr)
	{
		OutView.PortraitPath = Def->PortraitPath;
		if (Def->Abilities.IsValidIndex(0))
		{
			OutView.Ability0Name = Def->Abilities[0].DisplayName;
			OutView.Ability0Text = Def->Abilities[0].Description;
		}
		if (Def->Abilities.IsValidIndex(1))
		{
			OutView.Ability1Name = Def->Abilities[1].DisplayName;
			OutView.Ability1Text = Def->Abilities[1].Description;
		}
		OutView.StrengthPercentile = Database.GetStrengthPercentile(DefIndex);
	}

	// Frame colour comes from the faction doctrine, not the card, so the art team retints a whole faction by
	// editing one CSV row.
	const int32 FactionIndex = static_cast<int32>(InCard.Faction);
	if (Database.DoctrineByFaction.IsValidIndex(FactionIndex))
	{
		FCBData::ParseHexColor(Database.DoctrineByFaction[FactionIndex].FrameColor, OutView.FrameColor);
	}
}

void UFCBGameInstance::SetSelectedHandIndex(int32 InIndex)
{
	const FFCBMatchState& State = Match.GetState();
	if (!State.Seats.IsValidIndex(FCBSeat::A))
	{
		return;
	}
	const int32 Count = State.Seats[FCBSeat::A].Hand.Num();
	if (Count == 0)
	{
		SelectedHandIndex = 0;
		SelectedCardId = NAME_None;
		return;
	}
	// Wrap rather than clamp: with 12 cards, "next" past the end going back to the first is what a d-pad
	// player expects, and clamping makes the last card feel sticky.
	SelectedHandIndex = ((InIndex % Count) + Count) % Count;
	SelectedCardId = State.Seats[FCBSeat::A].Hand[SelectedHandIndex].Id;
	BroadcastMatchUpdated();
}

void UFCBGameInstance::MoveSelection(int32 Delta)
{
	SetSelectedHandIndex(SelectedHandIndex + Delta);
}

void UFCBGameInstance::SetSelectedAttribute(EFCBAttribute InAttribute)
{
	if (InAttribute == EFCBAttribute::Max)
	{
		return;
	}
	SelectedAttribute = InAttribute;

	// One keystroke per round keeps the debug HUD playable without a mouse: press S and the round is played on
	// Speed. ConfirmSelection is a no-op when it is not the player's turn, so this is safe to leave on.
	if (IsPlayersTurn())
	{
		ConfirmSelection();
	}
	else
	{
		BroadcastMatchUpdated();
	}
}

FFCBCardView UFCBGameInstance::GetSelectedCard() const
{
	FFCBCardView View;
	const FFCBMatchState& State = Match.GetState();
	if (State.Seats.IsValidIndex(FCBSeat::A) && State.Seats[FCBSeat::A].Hand.IsValidIndex(SelectedHandIndex))
	{
		BuildCardView(State.Seats[FCBSeat::A].Hand[SelectedHandIndex], SelectedHandIndex, View);
	}
	return View;
}

bool UFCBGameInstance::ProjectSelectedMove(FFCBRoundView& OutView) const
{
	const FFCBMove Move = FFCBMove::Make(FCBSeat::A, SelectedHandIndex, SelectedAttribute);
	FFCBRoundProjection Projection;
	if (!Match.ProjectMove(Move, Projection))
	{
		return false;
	}

	OutView = FFCBRoundView();
	OutView.RoundNumber = Match.GetRoundNumber() + 1;
	OutView.AttributeName = *FCBAttributeUtil::ToString(Projection.Attribute);
	OutView.AttackerPrinted = Projection.AttackerPrinted;
	OutView.DefenderPrinted = Projection.DefenderPrinted;
	OutView.AttackerValue = Projection.AttackerEffective;
	OutView.DefenderValue = Projection.DefenderEffective;
	OutView.ModifierNotes = Projection.Notes;
	OutView.OutcomeText = FCBRoundUtil::ToString(Projection.Outcome);
	OutView.Narration = FString::Printf(TEXT("%d vs %d"), Projection.AttackerEffective, Projection.DefenderEffective);

	if (const FFCBCardInstance* Card = Match.GetState().Seats[FCBSeat::A].Hand.IsValidIndex(SelectedHandIndex)
		? &Match.GetState().Seats[FCBSeat::A].Hand[SelectedHandIndex] : nullptr)
	{
		OutView.AttackerName = Card->Name;
	}
	int32 DefenderIndex = INDEX_NONE;
	if (Match.GetDefenderCardForMove(Move, DefenderIndex)
		&& Match.GetState().Seats.IsValidIndex(FCBSeat::B)
		&& Match.GetState().Seats[FCBSeat::B].Hand.IsValidIndex(DefenderIndex))
	{
		OutView.DefenderName = Match.GetState().Seats[FCBSeat::B].Hand[DefenderIndex].Name;
	}
	return true;
}

bool UFCBGameInstance::ConfirmSelection()
{
	if (!IsPlayersTurn())
	{
		return false;
	}

	FFCBMove Move = FFCBMove::Make(FCBSeat::A, SelectedHandIndex, SelectedAttribute);
	if (!Match.IsLegalMove(Move))
	{
		// The selected slot may have been emptied by an ability. Fall back to the same *card* if it is still in
		// hand, then to any legal move on the chosen attribute, so a keystroke is never silently dropped.
		TArray<FFCBMove> Legal;
		Match.GetLegalMoves(FCBSeat::A, Legal);
		bool bFound = false;
		for (const FFCBMove& Candidate : Legal)
		{
			const bool bSameCard = !SelectedCardId.IsNone()
				&& Match.GetState().Seats[FCBSeat::A].Hand.IsValidIndex(Candidate.HandIndex)
				&& Match.GetState().Seats[FCBSeat::A].Hand[Candidate.HandIndex].Id == SelectedCardId;
			if (bSameCard && Candidate.Attribute == SelectedAttribute)
			{
				Move = Candidate;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			for (const FFCBMove& Candidate : Legal)
			{
				if (Candidate.Attribute == SelectedAttribute)
				{
					Move = Candidate;
					bFound = true;
					break;
				}
			}
		}
		if (!bFound)
		{
			if (Legal.Num() == 0)
			{
				return false;
			}
			Move = Legal[0];
		}
		SelectedHandIndex = Move.HandIndex;
	}

	FString Error;
	if (!Match.PlayMove(Move, Error))
	{
		UE_LOG(LogFCB, Warning, TEXT("Move rejected: %s"), *Error);
		return false;
	}

	if (Match.GetState().History.Num() > 0)
	{
		BroadcastRound(Match.GetState().History.Last());
	}
	ClampSelectionToHand();
	BroadcastMatchUpdated();
	ScheduleAiTurn();
	return true;
}

bool UFCBGameInstance::PlayAiTurn()
{
	if (Match.IsOver() || Match.GetLeaderSeat() != FCBSeat::B)
	{
		return false;
	}

	FFCBMove Move;
	FString Why;
	if (!AiAgent.ChooseMove(Match, Move, Why))
	{
		UE_LOG(LogFCBAI, Warning, TEXT("AI could not choose a move: %s"), *Why);
		return false;
	}
	LastAiExplanation = Why;

	FString Error;
	if (!Match.PlayMove(Move, Error))
	{
		UE_LOG(LogFCB, Warning, TEXT("AI move rejected: %s"), *Error);
		return false;
	}

	if (Match.GetState().History.Num() > 0)
	{
		BroadcastRound(Match.GetState().History.Last());
	}
	ClampSelectionToHand();
	BroadcastMatchUpdated();
	UE_LOG(LogFCBAI, Verbose, TEXT("AI: %s"), *Why);

	// The AI keeps the lead while it wins, so it may be its turn again; ScheduleAiTurn handles both cases.
	ScheduleAiTurn();

	if (Match.IsOver())
	{
		UE_LOG(LogFCB, Log, TEXT("Match over: %s (%s)"),
			Match.GetState().WinnerSeat == INDEX_NONE ? TEXT("draw") : *Match.GetState().Seats[Match.GetState().WinnerSeat].Name,
			*Match.GetState().EndReason);
	}
	return true;
}

void UFCBGameInstance::ScheduleAiTurn()
{
	// UGameInstance::GetTimerManager forwards to the world's FTimerManager - a struct, not a UObject.
	if (FTimerManager* Timers = GetTimerManager())
	{
		Timers->ClearTimer(AiThinkTimer);
	}

	const int32 ChainLimit = FMath::Max(1, MaxConsecutiveAiTurns);
	if (Match.IsOver() || Match.GetLeaderSeat() != FCBSeat::B)
	{
		AiChainLength = 0;
		return;
	}
	if (AiChainLength >= ChainLimit)
	{
		UE_LOG(LogFCB, Warning, TEXT("AI chain limit (%d turns) reached; waiting for the player"), ChainLimit);
		return;
	}
	++AiChainLength;

	float Delay = UFCBSettings::Get().AiThinkDelaySeconds;
	if (AiProfile.TimeBudgetMs > 0)
	{
		Delay = FMath::Min(Delay, AiProfile.TimeBudgetMs / 1000.f);
	}

	if (Delay <= 0.f)
	{
		PlayAiTurn();
		return;
	}

	FTimerManager* Timers = GetTimerManager();
	if (!Timers)
	{
		// No timer manager means no tick to wait on (shutdown, or a commandlet running a match): play the
		// move now instead of scheduling a callback that would never fire.
		PlayAiTurn();
		return;
	}
	Timers->SetTimer(AiThinkTimer,
		FTimerDelegate::CreateUObject(this, &UFCBGameInstance::HandleAiThinkExpired), Delay, false);
}

void UFCBGameInstance::HandleAiThinkExpired()
{
	PlayAiTurn();
}

void UFCBGameInstance::ClampSelectionToHand()
{
	const FFCBMatchState& State = Match.GetState();
	const int32 Count = State.Seats.IsValidIndex(FCBSeat::A) ? State.Seats[FCBSeat::A].Hand.Num() : 0;
	if (Count == 0)
	{
		SelectedHandIndex = 0;
		SelectedCardId = NAME_None;
		return;
	}

	// Prefer following the card we were on (it may have shifted down by one when a card was captured).
	if (!SelectedCardId.IsNone())
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (State.Seats[FCBSeat::A].Hand[Index].Id == SelectedCardId)
			{
				SelectedHandIndex = Index;
				return;
			}
		}
	}
	SelectedHandIndex = FMath::Clamp(SelectedHandIndex, 0, Count - 1);
	SelectedCardId = State.Seats[FCBSeat::A].Hand.IsValidIndex(SelectedHandIndex)
		? State.Seats[FCBSeat::A].Hand[SelectedHandIndex].Id : NAME_None;
}

int32 UFCBGameInstance::GetScore(int32 Seat) const
{
	return Match.GetState().Seats.IsValidIndex(Seat) ? Match.GetState().Seats[Seat].Score : 0;
}

int32 UFCBGameInstance::GetHandCount(int32 Seat) const
{
	return Match.GetState().Seats.IsValidIndex(Seat) ? Match.GetState().Seats[Seat].Hand.Num() : 0;
}

void UFCBGameInstance::GetHandView(int32 Seat, TArray<FFCBCardView>& OutCards) const
{
	OutCards.Reset();
	if (!Match.GetState().Seats.IsValidIndex(Seat))
	{
		return;
	}
	const TArray<FFCBCardInstance>& Hand = Match.GetState().Seats[Seat].Hand;
	for (int32 Index = 0; Index < Hand.Num(); ++Index)
	{
		FFCBCardView View;
		BuildCardView(Hand[Index], Index, View);
		// The opponent's hand is secret: widgets get the frame, not the numbers.
		if (Seat != FCBSeat::A)
		{
			View.DisplayName = TEXT("face-down card");
			View.AgeYears = View.Power = View.Speed = View.HeightCm = 0;
			View.AgeText = View.PowerText = View.SpeedText = View.HeightText = TEXT("-");
			View.Ability0Name.Reset();
			View.Ability0Text.Reset();
			View.Ability1Name.Reset();
			View.Ability1Text.Reset();
			View.StrengthPercentile = 0.5f;
		}
		OutCards.Add(MoveTemp(View));
	}
}

void UFCBGameInstance::GetPileView(int32 Seat, int32 MaxCards, TArray<FFCBCardView>& OutCards) const
{
	OutCards.Reset();
	if (!Match.GetState().Seats.IsValidIndex(Seat))
	{
		return;
	}
	const TArray<FFCBCardInstance>& Pile = Match.GetState().Seats[Seat].Pile;
	const int32 Start = FMath::Max(0, Pile.Num() - FMath::Max(0, MaxCards));
	for (int32 Index = Start; Index < Pile.Num(); ++Index)
	{
		FFCBCardView View;
		BuildCardView(Pile[Index], INDEX_NONE, View);
		OutCards.Add(MoveTemp(View));
	}
}

FFCBCardView UFCBGameInstance::GetOpponentTopCard() const
{
	FFCBCardView View;
	const FFCBMatchState& State = Match.GetState();
	if (State.Seats.IsValidIndex(FCBSeat::B) && State.Seats[FCBSeat::B].Hand.Num() > 0)
	{
		// Deliberately thin: the player is supposed *not* to know what is coming. Faction and frame colour are
		// what a face-down card shows across the table.
		const FFCBCardInstance& Top = State.Seats[FCBSeat::B].Hand[0];
		View.FactionName = *FCBFactionUtil::ToString(Top.Faction);
		View.bIsTopOfHand = true;
		if (Database.DoctrineByFaction.IsValidIndex(static_cast<int32>(Top.Faction)))
		{
			FCBData::ParseHexColor(Database.DoctrineByFaction[static_cast<int32>(Top.Faction)].FrameColor, View.FrameColor);
		}
	}
	return View;
}

FString UFCBGameInstance::FormatSummary() const
{
	return Match.FormatState();
}

void UFCBGameInstance::BroadcastMatchUpdated()
{
	OnMatchUpdated.Broadcast();
}

void UFCBGameInstance::BroadcastRound(const FFCBRoundResult& Result)
{
	FFCBRoundView View;
	View.RoundNumber = Result.RoundNumber;
	View.AttackerName = Result.AttackerCardName;
	View.DefenderName = Result.DefenderCardName;
	View.AttributeName = *FCBAttributeUtil::ToString(Result.Projection.Attribute);
	View.OutcomeText = FCBRoundUtil::ToString(Result.Outcome);
	View.Narration = Result.Narration;
	View.ModifierNotes = Result.Projection.Notes;
	View.AttackerPrinted = Result.Projection.AttackerPrinted;
	View.DefenderPrinted = Result.Projection.DefenderPrinted;
	View.AttackerValue = Result.Projection.AttackerEffective;
	View.DefenderValue = Result.Projection.DefenderEffective;
	View.WinnerCardsGained = Result.WinnerCardsGained;
	View.PotCardsAdded = Result.PotCardsAdded;
	View.PotCardsAwarded = Result.PotCardsAwarded;
	View.ScoreA = Match.GetState().Seats[FCBSeat::A].Score;
	View.ScoreB = Match.GetState().Seats[FCBSeat::B].Score;

	OnRoundResolved.Broadcast(View);
}
