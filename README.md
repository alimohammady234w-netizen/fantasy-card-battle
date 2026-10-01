# Fantasy Card Battle

A fantasy card battle game (Top Trumps style) for Android using Unreal Engine 5.8.
Single-player, fully offline — 100+ fantasy cards with unique attributes (Age, Power, Speed, Height),
played against an AI opponent.

## Engine & Platform

| Item | Value |
|---|---|
| Engine | Unreal Engine 5.8 |
| Language (Gameplay) | C++ (module: `CardGame`) |
| UI | UMG (added in a later phase) |
| Target platform | Android (Mobile / Scalable 3D or 2D) |
| Mode | Single Player, Offline (no online/backend) |

## Project Structure

```
FantasyCardBattle.uproject
Config/
  DefaultEngine.ini        # Maps & Modes, Mobile+Scalable, Android settings
  DefaultGame.ini          # Project metadata
  DefaultInput.ini         # Input defaults (no gameplay bindings yet)
Source/
  FantasyCardBattle.Target.cs          # Game target (Android packaging)
  FantasyCardBattleEditor.Target.cs    # Editor target (development/PIE)
  CardGame/                            # Primary runtime module
    CardGame.Build.cs
    CardGame.h / CardGame.cpp          # Module + LogCardGame category
    CardGameGameMode.h / .cpp          # ACardGameGameMode
    CardGameGameInstance.h / .cpp      # UCardGameGameInstance
    CardGamePlayerController.h / .cpp  # ACardGamePlayerController
Content/
  Blueprints/  Cards/  Characters/  UI/      Materials/  Textures/
  Icons/  Sounds/  Music/  VFX/  Data/  Maps/  (MainMenu, Battle created in editor)
docs/
  phase-01-setup.md       # Phase 1 setup, compile & test guide (Persian)
```

## Default Classes (Phase 1)

- **GameMode:** `ACardGameGameMode` (set as Global Default GameMode)
- **GameInstance:** `UCardGameGameInstance` (set in Maps & Misc)
- **PlayerController:** `ACardGamePlayerController` (assigned by the GameMode)
- **Maps:** `/Game/Maps/MainMenu` (default), `/Game/Maps/Battle`

Phase 1 builds **infrastructure only** — no card, battle, AI, deck, ability or shop systems.

## Card Database (Phase 2)

Data-driven card definitions (no battle/deck/UI yet):

- **`Source/CardGame/Cards/`** — `CardTypes.h/.cpp` (enums + native `Card.*` gameplay tags),
  `CardStats.h` (`FCardStats`), `CardDataAsset.h/.cpp` (`UCardDataAsset : UPrimaryDataAsset`)
- **Primary Asset Id:** `Card:<CardID>` (e.g. `Card:CARD_ANIMAL_LION_001`) — ready for a
  future `UAssetManager` card database
- **Sample cards:** run `Tools/Python/create_sample_cards.py` in the editor
  (`Tools → Execute Python Script…`) to generate 10 sample assets in `/Game/Data/Cards`
- **Docs:** `docs/phase-02-card-database.md`

## Quick Start

1. Install Unreal Engine 5.8 (Epic Games Launcher) with **Android** support.
2. Right-click `FantasyCardBattle.uproject` → **Generate Project Files**.
3. Open the generated solution and build **Development Editor / Win64**, or open the project
   directly in the editor and let it compile.
4. Create the two levels `Content/Maps/MainMenu` and `Content/Maps/Battle`
   (step-by-step: `docs/phase-01-setup.md`).
5. Press **Play** (Alt+P) and check the Output Log filtered by `LogCardGame`.

See `docs/phase-01-setup.md` for full instructions (Persian).
