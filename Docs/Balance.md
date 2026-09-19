# Fantasy Card Battle — balance

Every number here is produced by the data pipeline, not chosen by feel:

```
Tools/generate_cards.py          # authored catalog -> Content/Data/Generated/*.csv (+ Docs/Cards.md)
Tools/mock_build.sh run          # the rules engine against that data: 177 checks, 0 failures
Tools/mock_build.sh run --sim 200 --report   # AI-vs-AI matches + the duel matrix below
```

Re-measure after any data change. The tables below are from the current `Content/Data/Generated` state
(110 cards, 10 factions, 24 abilities). If you retune and the numbers move, this file is the thing to update.

## Hard limits (`FCBLimits`, enforced fatally by the loader)

| Attribute | Range | Unit | Notes |
|-----------|-------|------|-------|
| Age | 0 … 999,999 | years | printed with thousands separators above 999 |
| Power | 1 … 120 | — | the tightest band; the "everyone understands this one" axis |
| Speed | 1 … 120 | — | |
| Height | 20 … 9,000 | cm | 20 cm is a toad, 9 km is a mythic in the 50 m range |

**Printed vs effective.** The values in `DT_Cards.csv` are the *printed* ones and must sit inside the table
above — the validator treats a violation as fatal. Faction doctrine is then applied, so a card can read
outside a band after doctrine (a wyrm at Power 112 with +8% shows 121); that is a **warning**, not an error,
and `Tools/generate_cards.py` clamps it away before shipping. The distinction matters: the printed number is
the contract with the art and the card face, the effective number is the engine's business.

## Rarity

| Tier | Share of pool | Printed band (power / speed / age / height) |
|------|---------------|---------------------------------------------|
| Common | ~28% | 46 / 52 / 45 yr / 180 cm |
| Uncommon | ~26% | 55 / 58 / 80 / 205 |
| Rare | ~24% | 66 / 64 / 150 / 235 |
| Epic | ~13% | 78 / 70 / 400 / 270 |
| Legendary | ~7% | 92 / 76 / 900 / 310 |
| Mythic | ~2% | 108 / 82 / 3,000 / 400 |

Current mix: **Common 27, Uncommon 29, Rare 26, Epic 18, Legendary 8, Mythic 2.**
Measured after doctrine: age 24 … 965,075 (mean 10,539), power 19 … 118 (mean 69.4), speed 20 … 112
(mean 58.5), height 25 … 3,200 cm (mean 479.6).

The pyramid is enforced where it counts — `TestRarityLadderWinsItsDuels` puts both cards in front of each other,
each naming its own best attribute, which is how the game is actually played:

```
Common vs Uncommon    higher tier takes 98.2% of 783 duels
Uncommon vs Rare      higher tier takes 98.7% of 754 duels
Rare vs Epic          higher tier takes 98.9% of 468 duels
Epic vs Legendary     higher tier takes 81.9% of 144 duels
```

A note on the duel matrix's own rarity column: averaged over *all four* declared attributes, Legendary reads
slightly below Epic (68.9% vs 71.8%). That is not an inversion — Legendaries are flatter, Epics get their
match-level abilities (Undying, Grave Rite) — and it is the ladder test above that is the contract.

## Playability floor: coverage, not "no dominated cards"

Every card must beat **at least 70% of the pool on at least one attribute** (fatal in the loader; the
generator tunes to 78% and measures 72.5% as the current worst).

What is deliberately *not* a rule: "no card may be dominated". A card that is worse on all four axes than some
legend is normal Top Trumps — you dump it on a round you are going to lose anyway. An earlier revision of the
generator enforced the stricter rule and produced absurd retunes (a 110-Speed hatchling, dozens of Ages above
mythic values). `report_dominated` still lists them (88 of 110 cards are dominated by someone) as information
for the flavour writer, never as an error.

## Faction doctrine

Doctrine is one global, always-on modifier per faction, and it must come with a trade-off — the generator
warns on a bonus without a penalty, because that is a faction-wide stat stick.

| Faction | Bonus | Penalty | Best-attribute duel share |
|---------|-------|---------|---------------------------|
| Dragonkind | Power +8% | Speed −6% | 91.1% |
| Arcane Orders | Age +18% | Power −3% | 86.8% |
| Order of the Blade | Speed +6% | Height −4% | 88.7% |
| Ironhold | Height +13% | Speed −4% | 87.3% |
| Fey Court | Speed +8% | Power −6% | 91.0% |
| Undying Legion | Age +30% | Speed −2% | 86.4% |
| Abyssal Horde | Power +7% | Age −10% | 90.8% |
| Celestial Host | Height +6% | Power −7% | 90.7% |
| Beastwild | Speed +9% | Height −7% | 91.9% |
| Deepwater Court | Age +9% | Power −4% | 89.1% |

Mean share 89.4%; the harness flags a faction more than 3 points under it. The two at the bottom (Arcane
Orders, Undying Legion) are single-axis factions by design: their whole identity is Age, so they win the
duel exactly when the opponent cannot answer on Age, and lose every other one. `tune_fairness` in the
generator nudges a trailing faction's near-miss attribute (+2% steps, capped at 10 points per card) so a
faction can never drift more than this band through flavour drift.

A percentage is a weak lever on Age and Height, where the pool is spread over several orders of magnitude:
raising Undying's Age bonus from +22% to +30% moved its share by 1.5 points, while giving the same cards a
second competitive axis moved it by 3. That is why the fairness pass works on attributes rather than on the
doctrine table.

## Ties

| Attribute | Tie rate across the pool |
|-----------|--------------------------|
| Age | 0.23% |
| Power | 1.12% |
| Speed | 1.52% |
| Height | 0.33% |

The generator keeps ties in a band rather than at zero: a clash is the event the pot exists for, and the tuner
deliberately lets a handful of pairs collide on the two "small" axes (measured effect: ~0.3–1.0 clashes per
game at Expert). Age and Height stay near zero because their bands are wide and integer-centimetre precision
makes collisions rare by construction.

## Ability impact, measured

The duel matrix resolves all 110 × 109 × 4 pairings through the real rules, and reports how many extra
percentage points of duels an ability earns the card that carries it. Measured on single-ability cards only,
so the credit is attributable. Both seats are evaluated — a defender-side ability can only be seen from the
defender's chair.

| Ability | Δ win share | | Ability | Δ win share |
|---------|-------------|-|---------|-------------|
| Stormstride | +3.00 | | HoardMagic | 0.00 |
| BulwarkOath | +2.43 | | DeepRoots | 0.00 |
| Thickhide | +1.29 | | Undying | 0.00 |
| FirstCharge | +1.25 | | BaneOfDragons | 0.00 |
| ColossalProwess | +1.00 | | BaneOfTheDead | 0.00 |
| Slaughtering | +0.17 | | GraveRite | −0.50 |
| Surefooted | +0.14 | | TitanGirth | −0.50 |
| FeyTrickery | +0.50 | | AncientBlood | −1.00 |
| Oathkeeper | −2.00 | | ElderMemory | −1.50 |
| CursedRelic | −9.00 | | | |

How to read this table:

* **±3 points is the ceiling for a round-level ability.** Nothing that fires inside one round should move the
  needle more than that, or the game becomes "play the card with the asterisk".
* **The zeros are not failures.** `Undying`, `GraveRite`, `HoardMagic`, `DeepRoots` and both banes earn their
  worth at *match* level (a card that returns to hand is worth several rounds of tempo, a bane is worth the
  one draft pick where you expect dragons) which a single-round metric cannot see. They are measured by the
  AI ladder instead.
* **Negative is correct for costs.** `CursedRelic` is an always-penalty: it must lose duels, and −9 points is
  why the cards that carry it get compensating stats. `Oathkeeper` and `ElderMemory` are mildly negative on
  the round and pay for themselves with pot and tie rules.
* `BaneOfDragons` reads 0.00 because the one card with it already beats every Dragonkind card outright — a
  bane on a card that does not need it is a *design* smell in the card list, not a bug in the metric.

Attribute declare shares (the "is one attribute secretly better?" test) come out at Age 49.6%, Height 48.8%,
Speed 48.5%, Power 48.2% — a 1.4-point spread, so no attribute is a free win and the 3-point tripwire never
fires.

## AI tiers

`TestAiTierDecisionQuality` measures how much of the best available expected value each tier actually takes,
across 117 real mid-game positions (a match win-rate test would need ~500 games to be worth reading, because
the deal dominates the result):

| Tier | Share of best available EV |
|------|----------------------------|
| Novice | 0.57 |
| Adept | 1.05 |
| Expert | 1.00 |
| Legendary | 0.97 |

Adept above 1.0 is not an error: it scores with its own (pool-wide) model, so it is being measured against a
yardstick it partly wrote. Expert is the reference scorer, hence 1.00 by construction. What the test pins
down is the *ordering* — random play leaves 43% of the available EV on the table, and both counting heuristics
and rollouts recover it.

Full matches, 200 per pair, hand 12:

| Matchup | Seat A win rate | Avg rounds | Clashes/game |
|---------|-----------------|------------|--------------|
| Novice vs Adept | 31.5% | 54.2 | 0.6 |
| Adept vs Expert | 47.0% | 53.8 | 0.3 |
| Expert vs Legendary | 51.5% | 53.9 | 0.3 |
| Expert vs Expert (mirror) | 52.0% | 53.8 | 0.3 |
| Adept vs Adept, hand 7 | 50.5% | 53.9 | 0.3 |
| TortureTest vs Legendary | 19.5% | 54.4 | 0.7 |

Readings: the ladder is monotone; the mirror is ~50% with a two-point first-leader edge, which is the
opening-seat bonus being real but small; `TortureTest` (the bot that deliberately picks the move that is best
*for its opponent*) loses 4:1, which is the proof that the evaluation function and the outcome are pointed the
same way; hand size 7 is nearly as sharp as 12 and 18% faster to play, so it is the party default.

Legendary's edge over Expert is thin (+4.5 points, and 0.97 vs 1.00 on EV). The rollout re-ranks the top
candidates rather than searching deeper, which keeps a decision inside one frame on mobile. If it ever needs
to be stronger, the knob is `RolloutSamples`/`SearchPly` in `Config/DefaultGame.ini` — data, not code.

## The pipeline's own guard rails

`Tools/generate_cards.py` is deterministic (an md5-derived jitter, never Python's salted `hash()`), so the same
catalog always produces byte-identical CSVs, and `--check` validates without writing. It fails the build on:

* a card outside the printed bands, before or after doctrine;
* coverage under the 70% floor;
* a faction roster outside 8…15 cards;
* an ability used below its `MinRarity`;
* an unknown ability id, an unknown rarity or archetype, a duplicate row id, a card with three abilities;
* a flat (non-percent) bonus on Age or Height — on a log-spread axis "+12" means nothing, so those axes only
  accept percentages; and an ability whose parameter is zero, which is a copied row waiting to be filled;
* a doctrine with a bonus and no penalty.

It only *warns* on soft budgets: per-faction rarity counts, an inverted best-attribute share between two
factions, and a faction drifting 1.75 points from the fairness mean.
