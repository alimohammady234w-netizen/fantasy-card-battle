# Fantasy Card Battle — game design

The contract between the rules engine (`Source/FantasyCardBattle/Private/FCBMatchRules.cpp`) and everything
above it. If the code and this file disagree, the code is wrong — the numbers behind the rules are in
[Balance.md](Balance.md), and both are checked by `Tools/mock_build.sh run`.

## The game in one paragraph

Two players each hold a hand of cards. On your turn you put one card face up and name one of its four
attributes — **Age**, **Power**, **Speed**, **Height**. Your opponent is forced to answer with the top card of
their own hand, and the higher number on the named attribute wins: the winner collects *both* cards, and the
loser draws to refill. Whoever is holding the most cards when the deck and one hand run out has won the
match. Between the two hands sits a **pot** of cards collected by whoever wins the next clash.

That is classic Top Trumps with three additions: faction doctrines, per-card abilities, and the pot.

## Round resolution, exactly

For a move `(attacker seat, hand index, attribute)` against the defender's top card:

1. **Contested value** — start from the card's `Effective` attribute (printed value with the faction doctrine
   applied at load time), then add any persistent per-copy stacks (a Bloodthirst-style card carries
   `BonusPowerStacks` on the *instance*, not on the definition).
2. **Nullification** — resolved before any modifier: if the opponent holds `NullifyOpponentAbilities`, this
   card's abilities are inert for the round. First, because "does it fire at all" must be decided before
   "by how much".
3. **Ability pass** — each card's abilities are applied in their **authored slot order** (slot 0, then slot 1).
   The slot number is a real tuning knob: two percent abilities on one card stack additively because each one
   computes off the *printed* value, not off the running total.
4. **Floor** — a contested value never drops below 1, so a huge penalty cannot turn into a self-inflicted
   loss by wrapping the comparison.
5. **Compare** — greater wins. Equal is decided by the `WinsTiesOnAttribute` flags (one holder takes it; both
   holding it stays a clash).
6. **Tie rule** — only reached on a clash. `PotClash` / `SplitKeep` / `BurnNoCapture` keep it a clash;
   `HigherRarityWins` compares rarity; `LeaderWinsTies` hands it to the leader.
7. **Clutch** — a defender with `WinIfMarginWithin n` flips an attacker win of ≤ n points into a defender win.
   Deliberately last: it reads the final numbers, so both sides' modifiers change whether it fires.
8. **Triggers** — `OnWin*`, `OnLose*` and `PotScoreBonusOnWin` are applied once the outcome is known.

Steps 1–7 live in `FFCBMatch::ResolveRoundModifiers` plus `FFCBMatch::FinalizeOutcome`, both `static` and
taking the two cards plus both scores: the same functions the AI scans 48 candidate moves with, the HUD's
"what will happen" preview uses, and the harness unit-tests. **There is no second implementation of a round
anywhere in the project** — that was the whole point of putting the resolver on the match class instead of
inside `PlayMove`.

### What each effect knows

| Effect | Gate |
|--------|------|
| `FlatBonusOnAttribute` | the declared attribute only |
| `PercentBonusOnAttribute` | declared attribute; percent of the printed value |
| `BonusPerFactionInOwnPile` | own capture pile count for the card's faction |
| `BonusAgainstFaction` | the opponent card's faction |
| `BonusIfOpponentStatAbove` | a threshold on the opponent's same-attribute value |
| `PercentBonusWhenLeading` | `AttackerScore > DefenderScore`, attacker seat only |
| `PercentBonusWhenDefending` | defender seat only |
| `AlwaysPenalty` | nothing — it is a cost, and `ParamA` must be negative (the validator enforces it) |
| `WinsTiesOnAttribute` | consumed in step 5 |
| `WinIfMarginWithin` | consumed in step 7, defender seat only |
| `OnWinTakeDeckTop`, `OnWinForceExtraCapture`, `OnLoseReturnToHandOnce` | consumed in step 8; the return is capped by `RemainingCharges` |
| `PotScoreBonusOnWin` | consumed by the pot award |

Flags (`AttackerOnly`, `DefenderOnly`, `ConsumedOnce`, `TriggerOnWin`, `TriggerOnLose`) are **derived from the
effect** by `FCBAbilityUtil::DerivedFlags`, never authored. A designer cannot accidentally give a
defender-only ability to the attacker's column: the data does not carry that decision.

### Pot rules

A clash pushes both contested cards into a shared pot. The pot is capped (`PotCap`, default 12): when it is
full, the *oldest* cards leave the pot and are swallowed into the round winner's pile the next time somebody
wins a round. The player who wins the next non-clash round takes the whole pot. Losing a round while the pot
is non-empty is the game's biggest swing — which is why the AI prices pot cards separately from normal
captures (`PotValueWeight`) and why "feed the pot at the end of the deck" is a real mistake a Novice makes.

### End conditions

- `LastSeatStanding` (default): the first player whose hand cannot be refilled loses; the opponent's card
  count decides the rest. Games are therefore guaranteed to terminate and average ~54 rounds.
- `RoundLimitScore`: play `MaxRounds`, highest score wins.
- `FirstToScore`: someone reaching `TargetScore` ends it immediately.

Score is captures, optionally rarity-weighted (`bRarityScoreWeights`), plus `BonusScore` from pot bonuses and
ability extras. The UI shows both the raw card count and the score, because they tell different stories.

## Match setup

1. `FFCBCardDatabase::CollectEligibleCards(DeckFilter, seed, pool)` — the eligible slice of the pool.
2. Fisher–Yates shuffle on the *match* `FRandomStream` (seeded from `RngSeed`), so the deal is reproducible.
3. First `HandSize * 2` cards become the two opening hands; the rest is the shared draw pile.
4. `BalanceSeatHands` snake-drafts those 24 cards by strength (A,B,B,A,…) and re-shuffles **within** each
   hand. Measured effect: the opening-strength gap drops from ~15% to ~1%, which is the difference between a
   game decided by the deal and a game decided by the play.
5. Both seats get independent AI agents when they are not human, seeded from the match seed.

## Tie rules

| Rule | Behaviour | Where it belongs |
|------|-----------|------------------|
| `PotClash` (default) | Both cards to the pot, leader keeps the lead | The shipped ladder |
| `SplitKeep` | Both cards return to their owners' hands | Party / teaching mode: nothing is lost, so the game can stall out harmlessly |
| `BurnNoCapture` | Both cards to the burn pile | Speed mode: shrinks the match without rewarding either seat |
| `HigherRarityWins` | Rarer card takes it; same rarity stays a clash | Collector mode, makes Mythics feel like trumps |
| `LeaderWinsTies` | Leader takes it | Fastest games; also the harshest against the trailing seat |

The default tie rate is ~0.2%–1.5% per attribute (see Balance.md), which lands at roughly one clash every two
games. Ties are not a bug to eliminate: the pot exists to make them the most memorable round of the match.

## Input (v1, code-only)

The debug HUD and `AFCBPlayerController` are enough to play, using the legacy bindings in
`Config/DefaultInput.ini`. No widget or input-action asset is required.

| Key | Action |
|-----|--------|
| `1`…`9`, `0`, `Q`, `W` | pick hand card 1…12 |
| `←` / `→` | cycle the selected card |
| `A` / `P` / `S` / `H` | declare Age / Power / Speed / Height — and play it |
| `Enter`, `Space`, left click | confirm the selection |
| `N` | new match, same seed (rematch: same deal) |
| `R` | roll a new seed and start over |
| `L` | toggle the round log |
| `F1` | toggle the debug overlay (seed, RNG state, AI reasoning) |
| `Esc` | pause-ish (v1: shows the summary) |

Declaring an attribute plays immediately on purpose: with one key per round the game is playable one-handed,
and the confirm path stays available for mouse and touch.

## What v1 deliberately does not have

- **Simultaneous selection.** The defender plays their top card. `bDefenderPlaysTopCard` exists and the flag is
  documented in `FFCBMatchRules.h`; netplay and "choose your defender card" share that switch.
- **Decks.** Every match draws from the whole eligible pool with `DeckFilter` applied. No constructed decks,
  no collection, no crafting.
- **Draft mode.** `FFCBDeckFilter` is the seed of it, nothing more.
- **Real localisation.** All strings are English; the display text is `FText` where it reaches the player so a
  later localisation pass is a data change.
- **Steam.** The subsystem is disabled in the `.uproject`; no achievements, no cloud saves, no matchmaking.
