# Fantasy Card Battle — Exact Installation (UE 5.8 + playable web build)

## A. Play the finished game right now (no engine needed)
1. In the repo root run: `python3 -m http.server 8000`  2. Open `http://localhost:8000`
3. Main Menu → **Play Battle** (Starter Deck is pre-built, 30 cards) → pick an attribute → GREATER/LESS → **Lock In & Reveal**.
   Rounds alternate: on AI-prediction rounds the AI locks a prediction from *its own card + public revealed history only*, then you press Reveal.
4. Explore Collection (click a card → Upgrade when it has enough XP), Deck Builder (exactly 30, duplicates ≤ copies owned), Progression, Settings (difficulty/sound/effects).
Everything saves automatically to browser localStorage (`fcb_save_v1`, backup `fcb_save_v1_backup`). Fully offline after the page loads; Three.js is vendored in `js/three.module.min.js`.

## B. Build the Unreal Engine 5.8 C++ version
**Status: source-complete, NOT compile-verified in this environment (no UE 5.8 here — see Docs/VERIFICATION.md). Follow these steps on a Windows PC with UE 5.8 + Visual Studio 2022 (Desktop development with C++).**

1. Create project: Epic Launcher → UE 5.8 → New Project → Games → **Blank**, **C++**, Desktop, Scalable, no starter content. Name it exactly `FantasyCardBattle`. Close the editor.
2. Copy from this repo's `Unreal/FantasyCardBattle/` into your project folder: `FantasyCardBattle.uproject`, `Source/` (all of it), `Config/DefaultGame.ini`, `Config/DefaultEngine.ini`. Do not overwrite `Config/DefaultInput.ini` if you customized it. No existing repo files outside `Unreal/` are part of the UE build.
   - Files replaced in a fresh template: only the two Config files above. There is no template C++ to keep (Blank C++ creates `Source/FantasyCardBattle/` with a module .cpp/Build.cs — **replace** those with ours; keep your `.Target.cs` files only if names differ, otherwise use ours).
3. Identify names: module = `FantasyCardBattle` (must match your project name; if yours differs, rename the folder/Build.cs class/`IMPLEMENT_PRIMARY_GAME_MODULE` consistently).
4. Import artwork: copy `Assets/Artwork/Cards/*.png` into the editor (drag into Content Browser folder `Content/Cards/Art`) and `Assets/Artwork/UI/card-back.png` → `Content/Cards/Art`. Import settings: Texture Group = UI, Compression = UserInterface2D. PNGs in `Assets/` are *source* files — they are not `.uasset`s until imported here.
5. Import card data: Content Browser → `Content/Data` → Import → select repo `Data/cards.csv` → row struct `FFCBCardTableRow` → name the asset `DT_Cards`. Then open DT_Cards and, per row, set **Artwork** to the matching imported texture (`Data/artwork_manifest.json` maps CardId → file → `/Game/Cards/Art/<name>`).
   Alternative data-only path: create `UFCBCardDefinition` Primary Data Assets (Content → Miscellaneous → Data Asset → FCBCardDefinition); the registry merges both sources and rejects duplicate IDs.
6. Gameplay classes (already set in Config): GameMode `AFCBGameMode` builds the battlefield from engine primitives at BeginPlay, spawns both card actors + camera + lights; `AFCBPlayerController` creates the three C++ UMG widgets (menu/battle/collection+deck) — **no Widget Blueprints or maps need authoring**. Startup map `/Engine/Maps/Entry` is fine because everything is procedural.
7. Generate project files: right-click the `.uproject` → Generate Visual Studio project files. Open the `.sln`, set `FantasyCardBattle` as startup project, build **Development Editor | Win64**. Fix/report errors per step 10.
8. Launch: Play in Editor → Main Menu appears → Play. Verify: log shows `[FCB] Registry: 50 definitions registered. Validation OK.`
9. Verify features: complete a match (30 rounds), check `Saved/SaveGames/FCB_Save.sav` exists; restart — level/XP/decks persist. Run automation tests: Session Frontend → Automation → `FantasyCardBattle.Rules.*` → Run.
10. Reporting build errors: copy the first 5 errors from the VS Error List **with file/line**, plus your UE point version. Most likely first-build issues are engine point-release API drift in `FCBCardActor.cpp` (canvas render-target drawing) or `FCBWidgets.cpp` (programmatic UMG) — both are isolated presentation files; gameplay/rules/save/AI files have no exotic dependencies.
**Reset test progress (UE):** delete `Saved/SaveGames/FCB_Save.sav` and `FCB_Save_Backup.sav`, or call `ResetProgress()` on the progression subsystem.

## C. Adding cards later (no C++ changes)
Add a row to `Data/cards.csv` (new unique `FCB-###` ID, attributes 1–100), regenerate/extend art with `Tools/generate_assets.py` conventions, import the PNG, add the DataTable row, set its Artwork. Registry validation flags duplicates, out-of-range attributes, and missing art at startup. New IDs are granted to existing saves automatically (1 copy).
