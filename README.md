# Fantasy Card Battle — Legendary Duel

A complete, playable, single-player offline fantasy card battle game: **50 unique cards × 8 attributes**, Greater/Less prediction duels against a 4-difficulty AI, 3D arcane battlefield, collection + 30-card deck builder, XP/levels/upgrades, and automatic local saving. No accounts, no network, no purchases.

## Play it
```bash
python3 -m http.server 8000   # then open http://localhost:8000
```
Main Menu → Play Battle → choose attribute + Greater/Less → Lock In & Reveal. (Serve over HTTP — `file://` blocks data loading.)

## Layout
- `index.html`, `styles.css`, `js/` — the playable game (`rules.js` pure tested rules, `app.js` UI/save/flow, `battle3d.js` Three.js table, vendored three.module.min.js)
- `Data/` — `cards.json` (canonical 50 cards), `cards.csv` (UE DataTable import), `artwork_manifest.json`, `validation_report.json`
- `Assets/Artwork/Cards/` — 50 individual card illustrations + `UI/card-back.png` (original procedural art; see `Docs/ART_PROMPTS.md` to upgrade)
- `Tools/generate_assets.py` — deterministic generator for data + art + validation report
- `Tests/run_tests.mjs` — `node Tests/run_tests.mjs` → 25 automated checks
- `Unreal/FantasyCardBattle/` — complete UE 5.8 C++ project source implementing the same architecture/data (rules, state machine, AI, progression/save subsystem, card actor, procedural battlefield, C++ UMG, automation tests)
- `Docs/` — `INSTALL_UE58.md` (exact beginner install steps), `VERIFICATION.md` (honest verified/unverified status), `ART_PROMPTS.md`

## Honest status
Web game: complete and tested (25/25). UE 5.8 C++: source-complete and statically reviewed, **not compile-verified here** (no engine in this environment) — first-build guidance in `Docs/INSTALL_UE58.md`.
