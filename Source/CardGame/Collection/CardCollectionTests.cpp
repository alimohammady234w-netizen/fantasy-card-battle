// Fantasy Card Battle - Stage 3 automation tests (collection data layer)
//
// These tests cover the pure-data layer (FPlayerCardInstance /
// FPlayerCardCollection / UCardGameSaveGame round-trip), which needs no
// World or GameInstance. Subsystem-level flows (definition resolution,
// save slot through UCardCollectionSubsystem) are verified manually with
// the console commands documented in docs/phase-03-player-collection.md.

#include "CoreMinimal.h"
#include "Collection/PlayerCardInstance.h"
#include "Save/CardGameSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// Tests 1-4 (spec): add / duplicates / ownership / quantity / remove
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardCollectionAddRemoveTest,
	"CardGame.Collection.AddRemoveQuantity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCardCollectionAddRemoveTest::RunTest(const FString& Parameters)
{
	FPlayerCardCollection Collection;
	const FName Lion(TEXT("CARD_ANIMAL_LION_001"));

	// Ownership on empty collection
	TestFalse(TEXT("Empty collection: HasCard false"), Collection.HasCard(Lion));
	TestEqual(TEXT("Empty collection: quantity 0"), Collection.GetQuantity(Lion), 0);

	// Add first copy
	FPlayerCardInstance LionA = FPlayerCardInstance::MakeNew(Lion);
	TestTrue(TEXT("MakeNew creates a valid instance"), LionA.IsValid());
	TestEqual(TEXT("New instance Level defaults to 1"), LionA.Level, 1);
	TestEqual(TEXT("New instance XP defaults to 0"), LionA.XP, 0);
	TestEqual(TEXT("New instance Quantity defaults to 1"), LionA.Quantity, 1);

	TestTrue(TEXT("Add first Lion succeeds"), Collection.AddCard(LionA));
	TestTrue(TEXT("HasCard true after add"), Collection.HasCard(Lion));
	TestEqual(TEXT("Quantity after first add is 1"), Collection.GetQuantity(Lion), 1);
	TestEqual(TEXT("Collection count after first add"), Collection.Num(), 1);

	// Add second copy - duplicates allowed, ids must differ
	FPlayerCardInstance LionB = FPlayerCardInstance::MakeNew(Lion);
	TestTrue(TEXT("Instance ids are unique per copy"), LionA.UniqueInstanceID != LionB.UniqueInstanceID);
	TestTrue(TEXT("Add second Lion succeeds"), Collection.AddCard(LionB));
	TestEqual(TEXT("Quantity after second add is 2"), Collection.GetQuantity(Lion), 2);

	// Invalid additions fail safely
	TestFalse(TEXT("Duplicate instance id rejected"), Collection.AddCard(LionA));
	FPlayerCardInstance InvalidInstance; // zero GUID + empty CardID
	TestFalse(TEXT("Invalid instance rejected"), Collection.AddCard(InvalidInstance));
	TestEqual(TEXT("Rejected adds did not change count"), Collection.Num(), 2);

	// Removal
	TestEqual(TEXT("Remove one Lion returns 1"), Collection.RemoveCards(Lion, 1), 1);
	TestEqual(TEXT("Quantity after remove is 1"), Collection.GetQuantity(Lion), 1);
	TestEqual(TEXT("Remove more than owned clamps to remaining"), Collection.RemoveCards(Lion, 5), 1);
	TestFalse(TEXT("HasCard false after all copies removed"), Collection.HasCard(Lion));
	TestEqual(TEXT("Removing from empty returns 0"), Collection.RemoveCards(Lion, 1), 0);
	TestFalse(TEXT("Remove unknown instance fails safely"), Collection.RemoveInstance(FGuid::NewGuid()));
	TestFalse(TEXT("Remove invalid guid fails safely"), Collection.RemoveInstance(FGuid()));

	// Invalid inputs are no-ops
	TestEqual(TEXT("RemoveCard(None) is a no-op"), Collection.RemoveCards(NAME_None, 3), 0);
	TestEqual(TEXT("RemoveCard with Count<1 is a no-op"), Collection.RemoveCards(Lion, 0), 0);

	return true;
}

// ---------------------------------------------------------------------------
// Tests 5-6 (spec): XP updates / level rules / identity preservation
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardCollectionProgressionTest,
	"CardGame.Collection.LevelAndXP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCardCollectionProgressionTest::RunTest(const FString& Parameters)
{
	FPlayerCardCollection Collection;
	const FName Dragon(TEXT("CARD_FANTASY_DRAGON_001"));

	FPlayerCardInstance Instance = FPlayerCardInstance::MakeNew(Dragon);
	TestTrue(TEXT("Add dragon"), Collection.AddCard(Instance));

	const FGuid OriginalId = Instance.UniqueInstanceID;

	// XP
	TestTrue(TEXT("AddXP 10 accepted"), Collection.AddXP(OriginalId, 10));
	TestTrue(TEXT("AddXP 10 again accepted"), Collection.AddXP(OriginalId, 10));
	const FPlayerCardInstance* Found = Collection.FindInstance(OriginalId);
	TestNotNull(TEXT("Instance found after XP"), Found);
	if (Found)
	{
		TestEqual(TEXT("XP accumulated to 20"), Found->XP, 20);
	}

	TestFalse(TEXT("Negative XP rejected"), Collection.AddXP(OriginalId, -5));
	Found = Collection.FindInstance(OriginalId);
	if (Found)
	{
		TestEqual(TEXT("XP unchanged after rejected negative add"), Found->XP, 20);
	}

	TestFalse(TEXT("XP on unknown instance fails"), Collection.AddXP(FGuid::NewGuid(), 10));
	TestFalse(TEXT("XP with invalid guid fails"), Collection.AddXP(FGuid(), 10));

	// Overflow safety - saturates instead of wrapping
	TestTrue(TEXT("Huge XP add accepted"), Collection.AddXP(OriginalId, MAX_int32));
	Found = Collection.FindInstance(OriginalId);
	if (Found)
	{
		TestTrue(TEXT("XP saturated at MAX_int32"), Found->XP == MAX_int32);
	}

	// Level
	TestFalse(TEXT("Level 0 rejected"), Collection.SetLevel(OriginalId, 0));
	TestFalse(TEXT("Negative level rejected"), Collection.SetLevel(OriginalId, -3));
	TestFalse(TEXT("Level on unknown instance fails"), Collection.SetLevel(FGuid::NewGuid(), 5));

	TestTrue(TEXT("Set level 5 accepted"), Collection.SetLevel(OriginalId, 5));
	Found = Collection.FindInstance(OriginalId);
	TestNotNull(TEXT("Instance found after level change"), Found);
	if (Found)
	{
		TestEqual(TEXT("Level is 5"), Found->Level, 5);
		TestTrue(TEXT("Identity preserved (guid)"), Found->UniqueInstanceID == OriginalId);
		TestTrue(TEXT("Identity preserved (CardID)"), Found->CardID == Dragon);
	}

	// Copies stay independent (individual progression)
	FPlayerCardInstance SecondCopy = FPlayerCardInstance::MakeNew(Dragon);
	TestTrue(TEXT("Add second dragon copy"), Collection.AddCard(SecondCopy));
	const FPlayerCardInstance* SecondFound = Collection.FindInstance(SecondCopy.UniqueInstanceID);
	if (SecondFound)
	{
		TestEqual(TEXT("Second copy keeps its own level 1"), SecondFound->Level, 1);
		TestEqual(TEXT("Second copy keeps its own XP 0"), SecondFound->XP, 0);
	}

	return true;
}

// ---------------------------------------------------------------------------
// Test 7 (spec): save -> reload -> collection restored (round-trip)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardCollectionSaveLoadTest,
	"CardGame.Collection.SaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCardCollectionSaveLoadTest::RunTest(const FString& Parameters)
{
	const FString SlotName = TEXT("CardGameSave_AutomationTest");
	const int32 UserIndex = UCardGameSaveGame::DefaultSaveUserIndex;

	const FName Lion(TEXT("CARD_ANIMAL_LION_001"));
	const FName Dragon(TEXT("CARD_FANTASY_DRAGON_001"));

	// Build a small collection with duplicates + progression
	FPlayerCardCollection Original;
	const FPlayerCardInstance LionA = FPlayerCardInstance::MakeNew(Lion);
	const FPlayerCardInstance LionB = FPlayerCardInstance::MakeNew(Lion);
	const FPlayerCardInstance DragonA = FPlayerCardInstance::MakeNew(Dragon);
	TestTrue(TEXT("Add LionA"), Original.AddCard(LionA));
	TestTrue(TEXT("Add LionB"), Original.AddCard(LionB));
	TestTrue(TEXT("Add DragonA"), Original.AddCard(DragonA));
	TestTrue(TEXT("Progress LionB"), Original.SetLevel(LionB.UniqueInstanceID, 3));
	TestTrue(TEXT("XP LionB"), Original.AddXP(LionB.UniqueInstanceID, 150));

	// Save
	USaveGame* NewSave = UGameplayStatics::CreateSaveGameObject(UCardGameSaveGame::StaticClass());
	UCardGameSaveGame* CardSave = Cast<UCardGameSaveGame>(NewSave);
	TestNotNull(TEXT("SaveGame object created"), CardSave);
	if (CardSave == nullptr)
	{
		return false;
	}

	CardSave->SaveVersion = UCardGameSaveGame::CurrentSaveVersion;
	CardSave->Collection = Original;

	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex); // clean slate
	TestTrue(
		TEXT("SaveGameToSlot succeeds"),
		UGameplayStatics::SaveGameToSlot(CardSave, SlotName, UserIndex));

	// Load
	TestTrue(TEXT("Save exists"), UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex));
	USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
	UCardGameSaveGame* LoadedCardSave = Cast<UCardGameSaveGame>(LoadedSave);
	TestNotNull(TEXT("Loaded save has expected class"), LoadedCardSave);
	if (LoadedCardSave == nullptr)
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
		return false;
	}

	// Verify restored data
	TestEqual(TEXT("Save version round-trips"),
		LoadedCardSave->SaveVersion, UCardGameSaveGame::CurrentSaveVersion);
	TestEqual(TEXT("Lion quantity restored"),
		LoadedCardSave->Collection.GetQuantity(Lion), 2);
	TestEqual(TEXT("Dragon quantity restored"),
		LoadedCardSave->Collection.GetQuantity(Dragon), 1);
	TestEqual(TEXT("Total instance count restored"),
		LoadedCardSave->Collection.Num(), 3);

	const FPlayerCardInstance* RestoredLionB = LoadedCardSave->Collection.FindInstance(LionB.UniqueInstanceID);
	TestNotNull(TEXT("LionB identity (guid) survives save/load"), RestoredLionB);
	if (RestoredLionB)
	{
		TestEqual(TEXT("LionB level restored"), RestoredLionB->Level, 3);
		TestEqual(TEXT("LionB XP restored"), RestoredLionB->XP, 150);
		TestTrue(TEXT("LionB CardID restored"), RestoredLionB->CardID == Lion);
	}

	// Malformed save content must be repairable, not fatal
	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
	return true;
}

// ---------------------------------------------------------------------------
// Test 10 (spec): invalid data is detected and repaired - never crashes
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardCollectionInvalidDataTest,
	"CardGame.Collection.InvalidDataSanitize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCardCollectionInvalidDataTest::RunTest(const FString& Parameters)
{
	FPlayerCardCollection Collection;

	const FName Lion(TEXT("CARD_ANIMAL_LION_001"));
	const FName Dragon(TEXT("CARD_FANTASY_DRAGON_001"));
	const FName Robot(TEXT("CARD_ROBOT_001"));
	const FName T-Rex(TEXT("CARD_DINOSAUR_TREX_001"));

	// Valid baseline
	const FPlayerCardInstance Good = FPlayerCardInstance::MakeNew(Lion);
	TestTrue(TEXT("Add valid baseline"), Collection.AddCard(Good));

	// 1) Empty CardID (bypass AddCard validation on purpose - simulates bad save data)
	FPlayerCardInstance EmptyCardId;
	EmptyCardId.Level = 0;
	EmptyCardId.XP = -5;
	EmptyCardId.Quantity = 0;
	Collection.OwnedCards.Add(EmptyCardId);

	// 2) Valid CardID but invalid GUID + negative progression
	FPlayerCardInstance InvalidGuid;
	InvalidGuid.CardID = Dragon;
	InvalidGuid.Level = -2;
	InvalidGuid.XP = -10;
	Collection.OwnedCards.Add(InvalidGuid);

	// 3) Quantity 0 (zero copies - meaningless)
	FPlayerCardInstance ZeroQuantity = FPlayerCardInstance::MakeNew(TRex);
	ZeroQuantity.Quantity = 0;
	Collection.OwnedCards.Add(ZeroQuantity);

	// 4) Duplicate GUID
	FPlayerCardInstance DuplicateId = FPlayerCardInstance::MakeNew(Robot);
	DuplicateId.UniqueInstanceID = Good.UniqueInstanceID;
	Collection.OwnedCards.Add(DuplicateId);

	const int32 NumBefore = Collection.Num();
	TestEqual(TEXT("Four invalid entries injected (5 total)"), NumBefore, 5);

	const int32 FixedIssues = Collection.Sanitize();
	TestTrue(TEXT("Sanitize reported fixes"), FixedIssues > 0);

	// EmptyCardId and ZeroQuantity must be gone; the others repaired
	TestEqual(TEXT("Exactly two entries dropped"), Collection.Num(), NumBefore - 2);

	int32 RepairErrors = 0;
	for (const FPlayerCardInstance& Instance : Collection.OwnedCards)
	{
		if (!Instance.IsValid())
		{
			++RepairErrors;
		}
		if (Instance.Level < 1)
		{
			++RepairErrors;
		}
		if (Instance.XP < 0)
		{
			++RepairErrors;
		}
		if (Instance.Quantity < 1)
		{
			++RepairErrors;
		}
	}
	TestEqual(TEXT("No invalid values remain after Sanitize"), RepairErrors, 0);

	TestTrue(TEXT("Valid instance survived sanitize"), Collection.HasInstance(Good.UniqueInstanceID));
	TestEqual(TEXT("Lion quantity intact"), Collection.GetQuantity(Lion), 1);
	TestEqual(TEXT("Dragon quantity intact"), Collection.GetQuantity(Dragon), 1);
	TestEqual(TEXT("Robot quantity intact"), Collection.GetQuantity(Robot), 1);
	TestEqual(TEXT("Zero-quantity T-Rex removed"), Collection.GetQuantity(TRex), 0);

	// The duplicated instance must now carry its own regenerated id
	const TArray<FPlayerCardInstance> Robots = Collection.GetCardsOf(Robot);
	TestEqual(TEXT("Robot has exactly one instance"), Robots.Num(), 1);
	if (Robots.Num() == 1)
	{
		TestTrue(TEXT("Duplicate guid was regenerated"),
			Robots[0].UniqueInstanceID != Good.UniqueInstanceID);
	}

	// Idempotent: running sanitize again fixes nothing
	TestEqual(TEXT("Sanitize is idempotent"), Collection.Sanitize(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
