// Fantasy Card Battle - Card Data Asset implementation (Stage 2)

#include "CardDataAsset.h"

#include "CardGame.h"

#define LOCTEXT_NAMESPACE "CardDataAsset"

// ---------------------------------------------------------------------------
// Primary Asset Id strategy:  Card:<CardID>
// Compatible with a future UAssetManager-based card database. The Asset
// Manager configuration (future stage) only needs to register the "Card"
// primary asset type for this class - no rework required.
// ---------------------------------------------------------------------------
FPrimaryAssetId UCardDataAsset::GetPrimaryAssetId() const
{
	const FName AssetName = CardID.IsNone() ? GetFName() : CardID;
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("Card")), AssetName);
}

FPrimaryAssetId UCardDataAsset::GetCardPrimaryAssetId() const
{
	return GetPrimaryAssetId();
}

// ---------------------------------------------------------------------------
// Validation - clear error/warning messages, never crashes on invalid data.
// ---------------------------------------------------------------------------
bool UCardDataAsset::ValidateCardData(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	// --- Identification ---------------------------------------------------
	if (CardID.IsNone())
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("ErrCardID", "Card Data Validation Error: CardID is empty. (asset: {0})"),
			FText::FromName(GetFName())));
	}

	if (Name.IsEmpty())
	{
		OutErrors.Add(LOCTEXT("ErrName", "Card Data Validation Error: Name is empty."));
	}

	if (Description.IsEmpty())
	{
		OutWarnings.Add(LOCTEXT("WarnDescription", "Card Data Validation Warning: Description is empty."));
	}

	// --- Stats ------------------------------------------------------------
	if (Stats.Power <= 0)
	{
		OutErrors.Add(LOCTEXT("ErrPower", "Card Data Validation Error: Power must be greater than 0."));
	}
	if (Stats.Speed <= 0)
	{
		OutErrors.Add(LOCTEXT("ErrSpeed", "Card Data Validation Error: Speed must be greater than 0."));
	}
	if (Stats.Height <= 0)
	{
		OutErrors.Add(LOCTEXT("ErrHeight", "Card Data Validation Error: Height must be greater than 0."));
	}
	if (Stats.Defense <= 0)
	{
		OutErrors.Add(LOCTEXT("ErrDefense", "Card Data Validation Error: Defense must be greater than 0."));
	}
	if (Stats.Intelligence <= 0)
	{
		OutErrors.Add(LOCTEXT("ErrIntelligence", "Card Data Validation Error: Intelligence must be greater than 0."));
	}
	if (Stats.Stamina <= 0)
	{
		OutErrors.Add(LOCTEXT("ErrStamina", "Card Data Validation Error: Stamina must be greater than 0."));
	}
	if (Stats.Luck <= 0)
	{
		OutErrors.Add(LOCTEXT("ErrLuck", "Card Data Validation Error: Luck must be greater than 0."));
	}

	if (Stats.Age < 0)
	{
		OutErrors.Add(LOCTEXT("ErrAge", "Card Data Validation Error: Age must not be negative."));
	}
	else if (Stats.Age == 0)
	{
		OutWarnings.Add(LOCTEXT("WarnAge", "Card Data Validation Warning: Age is 0 - confirm this is intended."));
	}

	// --- Level / XP -------------------------------------------------------
	if (BaseLevel < 1)
	{
		OutErrors.Add(LOCTEXT("ErrLevel", "Card Data Validation Error: BaseLevel must be 1 or greater."));
	}
	if (BaseXP < 0)
	{
		OutErrors.Add(LOCTEXT("ErrXP", "Card Data Validation Error: BaseXP must not be negative."));
	}

	// --- Ability (data consistency only, no execution) ---------------------
	if (AbilityType != ECardAbilityType::None)
	{
		if (AbilityValue <= 0.f)
		{
			OutWarnings.Add(FText::Format(
				LOCTEXT("WarnAbilityValue", "Card Data Validation Warning: AbilityType is '{0}' but AbilityValue is not positive."),
				UCardTypesLibrary::GetAbilityDisplayName(AbilityType)));
		}
		if (AbilityDescription.IsEmpty())
		{
			OutWarnings.Add(LOCTEXT("WarnAbilityDesc", "Card Data Validation Warning: AbilityType is set but AbilityDescription is empty."));
		}
	}

	// --- Required references ----------------------------------------------
	if (CardImage.IsNull())
	{
		OutWarnings.Add(LOCTEXT("WarnImage", "Card Data Validation Warning: CardImage is not assigned."));
	}

	// --- Editor-only consistency check -------------------------------------
#if WITH_EDITOR
	if (!CardID.IsNone() && CardID != GetFName())
	{
		OutWarnings.Add(FText::Format(
			LOCTEXT("WarnIDMismatch", "Card Data Validation Warning: CardID ({0}) differs from asset name ({1}). Recommendation: keep them identical."),
			FText::FromName(CardID),
			FText::FromName(GetFName())));
	}
#endif

	return OutErrors.Num() == 0;
}

// ---------------------------------------------------------------------------
// CallInEditor button: validate + log (never crashes).
// ---------------------------------------------------------------------------
void UCardDataAsset::ValidateCardDataNow()
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bHasNoErrors = ValidateCardData(Errors, Warnings);

	for (const FText& Error : Errors)
	{
		UE_LOG(LogCardGame, Error, TEXT("[%s] %s"), *GetName(), *Error.ToString());
	}
	for (const FText& Warning : Warnings)
	{
		UE_LOG(LogCardGame, Warning, TEXT("[%s] %s"), *GetName(), *Warning.ToString());
	}

	if (bHasNoErrors && Warnings.Num() == 0)
	{
		UE_LOG(LogCardGame, Log, TEXT("[%s] Card data validation passed (0 errors, 0 warnings)."), *GetName());
	}
	else if (bHasNoErrors)
	{
		UE_LOG(LogCardGame, Log, TEXT("[%s] Card data validation: 0 errors, %d warning(s)."), *GetName(), Warnings.Num());
	}
	else
	{
		UE_LOG(LogCardGame, Error, TEXT("[%s] Card data validation FAILED: %d error(s), %d warning(s)."), *GetName(), Errors.Num(), Warnings.Num());
	}
}

#undef LOCTEXT_NAMESPACE
