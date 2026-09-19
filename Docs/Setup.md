# Setting up and running the project

## Two ways to run it, and why both exist

**The headless harness — no engine needed.** The rules engine, the card database and the AI are plain C++
(no UMG, no Engine headers), so they compile and run with a bare toolchain:

```sh
Tools/mock_build.sh              # build
Tools/mock_build.sh run          # 177 checks against Content/Data/Generated
Tools/mock_build.sh run -v       # + per-test detail and every validator warning
Tools/mock_build.sh run --sim 200 --report   # AI ladder + duel matrix (Docs/Balance.md)
Tools/mock_build.sh clean
```

`Tools/MockUE/CoreMinimal.h` is a small shim that provides the handful of `FString` / `TArray` / `TMap` /
`FRandomStream` entry points the engine code uses, so the same `.cpp` files compile in both worlds. It is not a
UE reimplementation: if you find yourself adding behaviour to it, that behaviour probably belongs in
`FCBMatchRules.cpp` instead.

**The game.** Unreal Engine **5.8**, Windows or Android:

1. Generate the data (skip if `Content/Data/Generated/DT_Cards.csv` is present):
   `python3 Tools/generate_cards.py`
2. Generate project files and open `FantasyCardBattle.uproject`.
3. Press Play. `Config/DefaultEngine.ini` already points `GlobalDefaultGameMode` at
   `/Script/FantasyCardBattle.FCBGameMode`, the GameMode creates `AFCBPlayerController` and `AFCBDebugHud`
   from C++, and the input actions live in `Config/DefaultInput.ini`.

**There is nothing to create in the editor to play the game.** No widget blueprint, no input action assets, no
data tables: `UFCBGameInstance::ReloadCardData()` reads `Content/Data/Generated/*.csv` directly. If that
folder is missing it falls back to a labelled synthetic pool, so a fresh checkout still boots into a playable
table (the HUD prints `DEBUG POOL` and why).

## Optional editor content

To tune cards in a DataTable instead of a CSV, create the editor assets from the same files:

```sh
# headless
UnrealEditor-Cmd FantasyCardBattle.uproject -ExecutePythonScript="/Game/Python/import_content.py"
# or in the editor: Content Browser > Content/Python > right-click import_content.py > Run Script
python3 Content/Python/import_content.py --check     # validate the CSVs without the engine
```

It creates `/Game/Data/DT_Cards`, `DT_Factions`, `DT_Abilities`, the `FCB_Rules` data asset that points at them,
`FCB_AiProfiles`, and `/Game/Maps/L_FCB_Arena` with `AFCBGameMode` set as the level's default. It is idempotent:
re-running replaces rows rather than duplicating them, and it leaves an existing level alone.

`/Game/Maps/L_FCB_Arena` is only a convenience: `GlobalDefaultGameMode` is set project-wide, so any level -
including the empty one the editor opens on a fresh checkout - is already playable.

Once the tables exist, the loading order is: **rules asset (DataTables) → CSV directory → synthetic pool**.
Whichever path loaded is printed on the HUD (F1) and in `LogFCBData`, so "which cards am I actually playing"
is never a mystery.

## Editing data

Everything a designer touches is text:

| File | Contents |
|------|----------|
| `Tools/card_catalog.py` | authored data only: factions + doctrine, rarity bands, archetypes, ability definitions, the card list. **No logic.** |
| `Tools/generate_cards.py` | the generator/validator: jitter, coverage and fairness passes, band clamping, CSV/JSON/Markdown output |
| `Content/Data/Generated/*.csv` | what the game loads. Regenerated, never hand-edited |
| `Docs/Cards.md` | the readable card list, generated from the same rows |
| `Art/Briefs/*.md` | per-faction art briefs with the portrait paths the CSV already references |
| `Config/DefaultGame.ini` | tie rule, hand size, pot cap, AI profiles, HUD toggles |

Pipeline, always in this order:

```sh
python3 Tools/generate_cards.py     # writes CSV + Docs/Cards.md; fails loudly on bad data
Tools/mock_build.sh run             # the engine validator + rules tests on that data
Tools/mock_build.sh run --sim 200 --report   # if you touched stats, doctrine or abilities
```

`python3 Tools/generate_cards.py --check` validates without writing, for a quick "did I break anything" during
an edit. Determinism: the same catalog always produces byte-identical CSVs, so a data change that alters the
files is a change you actually made.

## Where things live

```
Source/FantasyCardBattle/Public/FCBTypes.h        enums, attributes, card def/instance, limits, display utils
Source/FantasyCardBattle/Public/FCBCardDatabase.h  CSV loader, derived stats, validator, filters
Source/FantasyCardBattle/Public/FCBMatchRules.h    match state, round resolver, tie and end rules
Source/FantasyCardBattle/Public/FCBAiAgent.h       four difficulty tiers, EV model, rollout re-ranking
Source/FantasyCardBattle/Public/FCBDataAssets.h    DataTable row structs, rules asset, settings, AI asset
Source/FantasyCardBattle/Public/FCBGameInstance.h  owns database + match + AI; the view API for widgets
Source/FantasyCardBattle/Public/FCBGameMode.h      GameMode, PlayerController, code-only debug HUD
Source/FantasyCardBattle/Private/Tests/            in-engine automation tests (Session Frontend)
Content/Python/import_content.py                   CSV -> DataTable / data asset / level
Tools/MockUE/SelfTest.cpp                          the headless test harness
```

The split that matters: `FCBTypes.h`, `FCBCardDatabase.*`, `FCBMatchRules.*`, `FCBAiAgent.*` never include a
header that the mock shim cannot provide. `FCBDataAssets.*` and up are Unreal-only. If you need a rule
visible in both, it belongs in the resolver, not in the widget layer.

## Tests

**Headless, no engine** - `Tools/mock_build.sh run`: 177 checks over the shipped CSVs (loader, validator,
coverage floors, rarity ladder, tie rules, every ability effect, the AI tiers, determinism). This is what
`Docs/Balance.md` numbers come from.

**In-engine** - Session Frontend > Automation Control, or from CI:

```
UnrealEditor-Cmd FantasyCardBattle.uproject -ExecCmds="Automation RunTests FantasyCardBattle;Quit" -unattended -nullrhi -log
```

| Test | What it pins |
|------|--------------|
| `FantasyCardBattle.Data.GeneratedDataIsValid` | the shipped CSVs load with zero fatal validator issues. Missing data is `AddWarning` + pass, so CI can build before it runs the generator |
| `FantasyCardBattle.Data.RulesAssetRoundTrip` | DataTable rows go through the same loader, validator and derived stats as the CSV path |
| `FantasyCardBattle.Data.IniConfigIsAbsorbed` | `Config/DefaultGame.ini` really reaches the class default objects. Probed through the AI profile array, which is empty unless the ini filled it - the one seam the headless harness cannot see, because it has no config system |
| `FantasyCardBattle.Match.SameSeedPlaysTheSameMatch` | seed + config replays a match byte for byte |
| `FantasyCardBattle.Match.CardsAreAlwaysAccountedFor` | deck + pot + burned + both hands + both piles equals the pool size after *every* round of four full matches |
| `FantasyCardBattle.Ui.NumberAndColourFormatting` | the number and `#RRGGBB` helpers the HUD and widgets share |

## First build on a fresh machine

1. Install **Unreal Engine 5.8** (Epic Games launcher; the Android toolchain only when you actually want to
   package for a phone).
2. `python3 Tools/generate_cards.py` in the repo root, so `Content/Data/Generated/` is not empty.
3. Right-click `FantasyCardBattle.uproject` -> **Generate Visual Studio project files**, then build the
   **FantasyCardBattleEditor** target in **Development Editor**. (On a Mac/Linux host the equivalent is
   `RunUBT.sh -projectfiles` and `FantasyCardBattleEditor -Development -TargetType=Editor`.)
4. Open the `.uproject`. If the editor complains that `/Game/Maps/L_FCB_Arena` does not exist, that is expected
   on a fresh checkout: create nothing, just open any level or an empty one - the project's
   `GlobalDefaultGameMode` is what matters, not the map.
5. Press Play. You should see the table drawn by `AFCBDebugHud`, your 12 cards, and `data: Content/Data/Generated:
   110 cards` on the F1 overlay. If it says `DEBUG POOL`, the CSV folder was not found (step 2, or the
   `CardDataDirectory` setting).
6. Run `FantasyCardBattle` in the Session Frontend. Six tests, all green.
7. Only if you want editor assets: `python3 Content/Python/import_content.py` from inside the editor
   (Content Browser > Content/Python > right-click > Run Script). Restart the editor afterwards so the new
   `/Game/Data/DT_*` assets are picked up by the rules asset.
8. Package for Windows when the above is green. Remember `Content/Data/Generated` in
   *Project Settings > Packaging > Additional Non-Asset Directories to Copy* if you are shipping the CSV path
   rather than cooked DataTables.

## Console commands and debugging

| Command | Effect |
|---------|--------|
| `Log LogFCBData Verbose` | every validator warning, not just the fatal ones |
| `Log LogFCBAI Verbose` | the AI's reasoning line for each move |
| `stat unit` | the AI's ~48-candidate scan is the only frame cost worth watching |
| `r.ScreenPercentage 100` | the debug HUD draws at viewport resolution; nothing to fix |

The seed is shown in the debug overlay (F1). `RngSeed` in `FFCBMatchConfig` plus that seed replays a match
exactly — the deal, the AI's blunders and the tie-breaks all come from the same `FRandomStream`, and
`FantasyCardBattle.Match.SameSeedPlaysTheSameMatch` fails the build if that ever stops being true.
