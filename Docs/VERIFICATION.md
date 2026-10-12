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
