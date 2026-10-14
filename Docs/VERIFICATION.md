# Verification — honest status (2026-10-12)

## Verified in this environment ✅
- `node Tests/run_tests.mjs` → **25 passed, 0 failed**: exactly 50 unique cards, 8 attributes each in 1–100, rarity/artwork existence, weakness guarantee, all Greater/Less/tie rules, 30-card deck rule + duplicate-copy rule, AI information restriction (decision is a pure function of own card + public history), XP curve/level-up, upgrade-never-mutates-definition.
- `Data/validation_report.json` → PASS (50 cards, rarities: Common 8, Uncommon 7, Rare 13, Epic 11, Legendary 7, Mythic 4).
- 50 individual artwork PNGs exist (`Assets/Artwork/Cards/FCB-001..050`), plus card-back; names match `Data/artwork_manifest.json`.
- JS syntax-checked (`node --check`); web game served over HTTP and all screens wired to real handlers (no decorative buttons).

## NOT verified ⚠️
- **UE 5.8 C++ was not compiled** — no Unreal Engine exists in this sandbox. The C++ mirrors the tested JS rules line-for-line and was statically reviewed (delegate signatures, component spawning, GC pointers fixed), but treat the first VS build as a verification step; see INSTALL_UE58.md step 10. Presentation files (`FCBCardActor.cpp`, `UI/FCBWidgets.cpp`) carry the most point-release API risk.
- UE automation tests in `Tests/FCBAutomationTests.cpp` are written but unrun (same reason).
- Web build play-tested by code inspection + unit tests, not by a human clicking through in this environment.

## Known simplifications (stated, not hidden)
- Artwork is original **procedurally generated** cinematic silhouette art (Pillow), not hand-painted/commissioned illustration. Upgrade path: `Docs/ART_PROMPTS.md`.
- Abilities are card identity/flavor text; rounds are decided purely by the eight attributes, per the specified Greater/Less design.
- Card progression is one instance per definition (+ bonus copies count); multi-instance-per-card inventories are a documented extension point (`FCBOwnedCard` already carries per-instance fields).
- Web audio is synthesized (WebAudio); UE build exposes sound slots on card definitions but ships no `.wav` files.

## Round 2 (continuation) fixes
- UE: battle now uses the **selected active deck** (`GetActiveDeck`), not the first valid deck found.
- UE: real deck-editing APIs added (`AddCardToActiveDeck`/`RemoveCardFromActiveDeck` with owned-copy + 30-card validation, auto-save) and wired into the Collection/Deck-Builder widget (Add/Remove/New Deck/Validate buttons).
- UE: battlefield world scale fixed (table top z=-10, cards stand on it facing the camera, zones sit on the surface); card texture now applied to all material slots; front-face text uses the engine Roboto font instead of a null font.
- UE: UMG widgets build buttons **once** and only update labels (no per-round widget construction/name collisions); Main Menu now has functional Progression and Settings (difficulty cycle, persisted) entries.
- Web: direction picker hidden on AI-prediction rounds; 3D art paths are relative (works under sub-path hosting); upgrade toast fixed.
- New AI-generated menu key art: `Assets/Artwork/UI/key-art.jpg` (the 50 card illustrations remain the procedural set).

## Round 3 fixes (AI-illustrated card art + balance evidence)
- **8 marquee cards (all Mythic + key Legendaries) re-illustrated** with dedicated AI artwork: FCB-001 Emberclaw, FCB-004 Nyx, FCB-008 Ignivar, FCB-017 The Verdant Matriarch, FCB-026 Grendelok Worldgnawer, FCB-032 Aurora Vex Starforge, FCB-038 Noctis Umbral Wyrm, FCB-047 Aurelion Sky Sovereign. Filenames/paths unchanged, so `Data/cards.json` and `Data/artwork_manifest.json` still resolve; `artSource` per card is now recorded in the manifest (`ai-illustrated` vs `procedural-silhouette`).
- `Tools/generate_assets.py` has an `UPGRADED_ART` guard so regeneration never overwrites those 8 files.
- **Difficulty balance evidence (new, actually run):** `node Tools/balance_sim.mjs` plays 200 AI-vs-AI matches per pairing (both AIs only see their own card + public revealed history). Verified outcome over 200 matches each — expert vs easy 200–0, expert vs normal 197–2 (1 draw), hard vs easy 191–6 (3 draws), normal vs easy 163–26 (11 draws), expert vs hard 143–37 (20 draws). The 4 difficulty tiers are strictly, monotonically ordered.
- Remaining 42 cards still use the original procedural silhouette art (honestly labeled in the manifest); they can be upgraded the same way later.
