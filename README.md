# Fantasy Card Battle

A fantasy card battle game (Top Trumps style) for PC, Steam and Android using **Unreal Engine 5.8**. Two
players, an AI opponent with four difficulty tiers, 110 data-driven cards with Age / Power / Speed / Height
attributes, ten factions with always-on doctrine modifiers, and 24 abilities resolved by a single shared rules
engine.

The interesting part is not the theme: it is that **the card data, the rules and the balance are all testable
without the engine.** The same `FCBMatchRules.cpp` that the game runs is compiled by a shell script with `g++`
and checked against the shipped CSVs, so "this card is broken" and "this faction is over-tuned" are questions
a 20-second command answers.

## Try it in one command

```sh
Tools/mock_build.sh run                     # 177 checks against Content/Data/Generated, no engine needed
Tools/mock_build.sh run --sim 200 --report  # + AI-vs-AI ladder and the duel-matrix balance report
python3 Tools/generate_cards.py --check     # validate the authored catalog without writing files
```

## Play it

1. `python3 Tools/generate_cards.py` — writes `Content/Data/Generated/{DT_Cards,DT_Factions,DT_Abilities}.csv`,
   `cards.json`, [`Docs/Cards.md`](Docs/Cards.md) and the per-faction art briefs in `Art/Briefs/`.
2. Open `FantasyCardBattle.uproject` in UE 5.8, generate project files, press Play.

Nothing has to be created in the editor. `AFCBGameMode` spawns `AFCBPlayerController` and `AFCBDebugHud` from
C++, the HUD draws the whole table with canvas text, and the bindings come from `Config/DefaultInput.ini`.
`Content/Python/import_content.py` exists for people who want to tune cards in a DataTable instead of a CSV,
and the game is playable if it is never run.

Keys: `1`–`0`/`Q`/`W` pick a card, `←`/`→` cycle, `A` `P` `S` `H` declare Age/Power/Speed/Height (and play),
`Enter`/click confirms, `N` rematch the same deal, `R` new deal, `L` log, `F1` debug overlay.
[Docs/Setup.md](Docs/Setup.md) has the full list plus the Android and Steam notes.

## The rules, in four lines

You play a card face up and name one attribute. The opponent answers with the top card of their hand; higher
value on that attribute wins and takes **both** cards. Equal values clash: both cards go to a shared pot that
the next winner collects. Play ends when the deck and one hand run out. Faction doctrine and per-card
abilities modify the number being compared — [Docs/GameDesign.md](Docs/GameDesign.md) is the exact order of
resolution.

## Layout

```
Source/FantasyCardBattle/Public|Private/
  FCBTypes.h            enums, attributes, card def/instance, published limits, display formatting
  FCBCardDatabase.*     CSV loader, derived stats (strength percentiles), the data validator
  FCBMatchRules.*       match state, the round resolver, tie rules, end conditions
  FCBAiAgent.*          Novice/Adept/Expert/Legendary: scored EV plus rollout re-ranking
  FCBDataAssets.*       DataTable row structs, rules asset, project settings, AI profile asset
  FCBGameInstance.*     owns database + match + AI; the only API widgets and the HUD talk to
  FCBGameMode.*         GameMode, PlayerController, code-only debug HUD
  Private/Tests/        in-engine automation tests
Content/Data/Generated/ the shipped data: DT_Cards.csv, DT_Factions.csv, DT_Abilities.csv, cards.json
Content/Python/         import_content.py - CSVs to DataTables, data assets and the arena level
Tools/card_catalog.py   authored data: factions, bands, archetypes, abilities, the 110-card list
Tools/generate_cards.py generator + validator (deterministic, --check for CI)
Tools/MockUE/           the shim and the headless harness that make all of this testable
Tools/mock_build.sh     build and run the harness
Docs/                   GameDesign.md, Balance.md, Cards.md, Setup.md, Platforms.md, Telemetry.md
Art/Briefs/             per-faction art briefs, generated from the same rows the game loads
```

Two boundaries are load-bearing:

* **`FCBTypes.h` / `FCBCardDatabase` / `FCBMatchRules` / `FCBAiAgent` include nothing the mock shim cannot
  provide.** That is what makes the headless harness possible, and it is why the rules engine has no UMG or
  Engine includes in it.
* **There is exactly one place that resolves a round.** `FFCBMatch::ResolveRoundModifiers` +
  `FinalizeOutcome` are used by `PlayMove`, by the AI's 48-move scan, by the HUD's preview line and by the
  harness tests. A rule that only exists in the UI is a rule that will be wrong.

## Balance, in one table

From `Tools/mock_build.sh run --sim 200 --report`, measured through the real rules on the shipped data:

| Check | Result |
|-------|--------|
| Attribute fairness (is one attribute secretly best?) | 48.2% – 49.6% win share, 1.4 pt spread |
| Rarity pyramid (best-attribute play, adjacent tiers) | higher tier wins 81.9% – 98.9% of duels |
| Faction fairness (best-attribute duel share) | 86.4% – 91.9%, mean 89.4% |
| Playability floor (every card beats a slice of the pool) | worst card covers 72.5% (floor is 70%) |
| AI ladder | Novice 31.5% → Adept 47.0% → Expert 51.5% vs the tier above |
| Mirror fairness | 52.0% for the first leader, over 200 games |
| Clashes | ~0.3 per game at Expert, 1.0 at Novice |

[Docs/Balance.md](Docs/Balance.md) explains how each number is produced, what the generator will and will not
repair for you, and which of them are fatal errors versus warnings.

## Known gaps (v1 scope)

Hot-seat only (no simultaneous defender choice, no netplay), no constructed decks or collection screens, no
Steam achievements, English-only text, and a code-only HUD in place of the UMG card widgets. Each of those has
a hook already: `bDefenderPlaysTopCard`, `FFCBDeckFilter`, the disabled `OnlineSubsystemSteam` plugin, `FText`
on everything user-visible, and the view API on `UFCBGameInstance` that the widgets will consume.
