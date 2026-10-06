# Verification report — 2026-10-06

## Environment

- Linux sandbox, Python 3.11.2, Pygame 2.6.1, SDL 2.28.4.
- Isolated `.venv`; dependencies are not committed.
- UI testing: SDL dummy video/audio, real Pygame surfaces and event queue.
- Intended desktop runtime: Python 3.12+ / Windows, Linux or macOS.

## Executed successfully

```text
python -m compileall -q src main.py tests
python -m unittest discover -s tests -v
Ran 168 tests — OK
python main.py --headless --smoke 30 --screenshot docs/main-menu.png
Exit code: 0
```

32 core logic tests, 14 journal tests, 17 checkpoint tests, 22 achievement tests, 38 Pygame UI tests, 19 validation tests, 8 audio tests and 18 runtime/release-tool tests:

| Area | Checks |
|---|---|
| Database | 45 unique cards, 15 categories, six rarities, all 15 abilities, 1–100 values; invalid rows skipped; missing/malformed database safe |
| Deck | Create, rename, delete, selection, add/remove, 30 unique owned cards, invalid/overfull decks rejected |
| Draw | Two independent shuffled decks with 30 unique cards each |
| Comparisons | Greater, Less, no-winner tie, random tie, carry-over pot |
| Match | Ten-round Quick and Practice; thirty-round Classic; alternating AI selection; result and idempotent finalization |
| Rewards | Score, captured cards, win streak, combo rewards, XP, card/player level, capped upgrades, insufficient funds |
| AI | All four levels, choosing extreme high/low, public-only method signature; no hidden-card input |
| Ability | Every effect branch with seeded/injected RNG; silence, reflection recursion guard; next-round status expiry |
| Packs | All six pack types, currency deductions, duplicate protection within rarity, duplicate compensation, insufficient funds without mutation |
| Persistence | Default save creation, round-trip, corrupted/invalid save backup and reset, invalid settings normalization, write failure reporting |
| UI | Actual mouse events Main Menu → Play → Match → attribute → Greater → reveal → next round; keyboard/dialog input; full UI match → results |
| Timing | Timeout auto-selection; automatic AI selection in Classic |
| Collection | Search, category filter, rarity sorting, locked detail branch; upgrade and deck actions |
| Rendering | Main menu, selection, collection, decks, upgrades, packs, settings at 1280×720 and 1920×1080 |
| Assets | Missing images, missing optional font, all missing sound events and missing music are safe |
| Pack UI | Reveal animation, ten-card two-page presentation, saved purchase |
| Settings | Cycling all settings, display recreation, persisted settings |
| Exit | Save on quit |

Warnings/errors during the recovery tests are intentional assertions (corrupted save,
invalid rows, missing database, unwritable save destination), not test failures.

## Render inspection

Rendered actual application screens and inspected the images:

- `docs/main-menu.png`
- `docs/match.png`
- `docs/collection.png`
- A resolved battle was also rendered and inspected during development.

These screenshots are native Pygame output, not mockups. No browser server is required.

## Packaging and environment limitations

- The PyInstaller spec includes data, assets, config, and a windowed one-file executable.
- A local PyInstaller build was attempted; this sandbox's system Python lacks
  `libpython3.11.so.1.0`, so packaging stopped before producing a binary.
- Downloading Python 3.12 was attempted but the sandbox connection to the Python
  standalone distribution failed with TLS/certificate errors. Tests were therefore
  executed on the available Python 3.11.2, **not** on 3.12 locally.
- A Windows/Linux Python 3.12/3.13 CI matrix and Windows EXE artifact job are included,
  but this session did not push or run GitHub Actions.
- **No Windows executable has been built or verified here.** Build `CardGame.spec`
  on a Windows machine with the normal Python distribution, then test the executable.
- Headless tests do not verify real monitor fullscreen/DPI behavior, actual audio
  hardware, Windows SmartScreen, or long-duration human playtesting/balance.

## Deliberate v1 boundaries

- Art is procedural fallback. Nine synthesized effects and two synthesized music tracks are now bundled; there are no downloaded recordings.
- Captured cards are match points, not permanent ownership transfers.
- Practice has no progression or statistics changes.
- Matches can resume at the last successful round checkpoint; pending selections and their timer restart. Completed-round rewards are never replayed.
- HP is an auxiliary ability resource, not an elimination rule in these three modes.
- Mirror and Copy share the same nonrecursive copied-effect behavior.
- The public-distribution Expert AI is a heuristic, not a trained or cheating agent.
- Only English UI ships. Persian shaping/RTL and future online/mobile modes are not implemented.


## Follow-up verification — 2026-10-06

The continuation added validated rule loading, a data-only authoring checker,
original generated sound effects/music, immediate volume application, and a guarded
pack-opening UI with catalog pagination.

Additional executed checks:

- `python tools/validate_data.py --json`: PASS, 45 cards, 15 categories, six rarities,
  15 abilities, six packs, zero diagnostics for the shipped data.
- Invalid/negative prices, zero divisors, invalid XP thresholds, wrong JSON root
  types, malformed modes, invalid rarity RGB/borders, nonfinite numbers,
  bad ability parameters, unknown rarity weights and bad settings use the documented
  fallback/disable behavior. Corrupt-rule recovery was exercised through a complete match.
- CLI success/failure exit codes and JSON output were tested with temporary databases.
- File logger permission errors do not stop the application.
- All eleven bundled WAVs decode; no empty tracks, full-scale clipping, or large
  discontinuities at their first/last samples were detected.
- Both music tracks play under SDL dummy audio; switching, live volume/mute, missing
  devices, corrupt files and OGG-to-WAV fallback were tested.
- Audio generator reproducibility and clipping rejection were tested.
- Repeated pack-open actions do not deduct currency twice while reward presentation
  is active. Empty catalogs and catalogs containing more than six packs render safely.
- `python -m unittest discover -s tests -q`: **74 tests passed** on the available
  Python 3.11.2 / Pygame 2.6.1 environment.
- Native main-menu startup again passed the headless 60-frame smoke check.

`build_game.bat` now validates data and runs tests before invoking PyInstaller.
Windows CI waits explicitly for the windowed EXE process and checks its exit code.
Neither that BAT nor the Windows workflow was executed locally; no EXE validation
claim is made. Audio playback tests confirm decoding/mixer behavior, **not** human
listening quality on physical speakers. No new dependencies were added.


## Journal and field-guide verification — 2026-10-06

Added 21 tests, bringing the executed suite to **95 passing tests**:

- Completed Quick/Classic results and revealed-round details are recorded and survive
  an exact JSON save/load round trip. Classic's alternating chooser is preserved.
- Unfinished matches and default Practice do not create journal entries; Practice
  leaves the entire profile unchanged.
- Repeated finish/record calls do not duplicate records or rewards.
- A Quick journal contains only ten revealed opponent cards, with no remaining deck
  snapshot or RNG state. The journal explicitly whitelists its persisted fields.
- History is capped at the latest 50 completed matches.
- Older saves without `match_history` retain currency/progression and acquire an empty
  history. Invalid history containers or individual entries do not reset a valid save.
- Nonfinite values, invalid comparison data, malformed timestamps, bad round order,
  invalid scores and bad text are rejected; duplicate records are removed.
- Unknown fields are stripped. Round reward totals are recomputed for display only,
  never credited back to the profile.
- Actual mouse events open both new menu routes, operate the Greater/Less example,
  browse abilities, page/filter match history, and open the journal from Results.
- Viewing the journal or using the guide does not mutate rewards or progression.
- All three guide tabs and all six pages of a Classic journal render at both 1280×720
  and 1920×1080. Empty history and missing ability definitions are handled.

New rendered application screenshots were inspected:
`docs/field-guide.png` and `docs/match-journal.png`. All guide tabs were inspected.
The main-menu screenshot was refreshed to show the new navigation buttons.

Final checks: data validator PASS (zero shipped-data diagnostics), compileall PASS,
95 unit/UI tests PASS, and a 60-frame native headless startup PASS. The existing
limitations concerning physical audio/display devices, Python 3.12 local verification,
and Windows packaging are unchanged. This feature is a read-only journal, **not**
mid-match resume or interactive replay.


## Resume/checkpoint verification — 2026-10-06

Added **25 tests**, for a total of **120 passing tests**. Checks executed locally:

- Initial match save and exact checkpoint JSON round trip.
- Already-resolved rounds restore their results without running resolve/reward again.
- Continuing each of three modes at each of four AI difficulties after save/reload
  produces identical results, RNG states, statuses, scores and progression to the
  uninterrupted branch under the same choices.
- Pending Freeze/Poison and HP survive a restart.
- Battle-start attribute snapshots ignore upgrades made while suspended. Editing
  the selected deck does not alter the existing shuffled deck, and changing Settings
  difficulty does not change the saved opponent difficulty.
- Final resolved-round restoration finalizes once; finishing clears the checkpoint
  and records the match once. Discard retains previously-earned rewards.
- Old saves without a checkpoint still load. Invalid checkpoint types, malformed
  history, invalid numeric/state fields, changed rule fingerprints and already-recorded
  match IDs are rejected without resetting otherwise-valid progression.
- A simulated atomic replace failure leaves both the previous currency state and
  its corresponding checkpoint intact on disk.
- Mouse-event UI tests cover Suspend/Resume, replacement confirmation, Discard,
  resolved-result restoration, interrupted pre-reveal selection and Practice resume.
- The Play screen with Resume controls renders at both target resolutions; its native
  screenshot was inspected and saved to `docs/resume-match.png`.

Resume stores hidden future deck order internally in local JSON, but does not pass it
to the AI decision interface or public match journal. This is not encrypted storage
or an anti-tamper system. No pickle or executable-object serialization is used.

The timer and uncommitted selection reset on resume; unfinished animation playback
is not preserved. Battle rewards and the checkpoint share a single atomic profile
save. A failed save can lose in-memory progress since the last successful checkpoint,
but cannot write a new reward balance paired with an old checkpoint.

All prior tests remain passing. Physical-device testing and Windows EXE verification
are still outstanding; local execution remains Python 3.11.2 / Pygame 2.6.1.


## Offline achievements verification — 2026-10-06

Added **30 tests**, bringing the executed suite to **150 passing tests**.

- All 15 shipped definitions validate; none starts claimable on a fresh profile.
- Completing rewarded matches, opening packs, upgrading cards and reaching player/card
  levels update derived progress. Reading progress never mutates the profile.
- Older saves gain empty claim ledgers and recognize existing lifetime statistics
  retroactively. No rewards are credited merely by viewing an eligible achievement.
- Claim commits currency, lifetime coin statistics and the claim ID in one atomic save.
  Immediate repeat claims and repeat claims after reloading are rejected.
- Simulated write failure leaves both the in-memory profile and the prior disk file
  unchanged; the reward remains claimable and a retry succeeds.
- The transaction preserves shared references used by the audio manager, deck manager,
  progression service and battle. A claim made with an active match preserves its exact
  checkpoint and is restored alongside the awarded currency.
- Definition removal/reintroduction preserves previously-claimed IDs. Editing only the
  achievement definitions does not invalidate a battle fingerprint.
- Malformed ledger contents are sanitized without resetting valid currency. A completely
  lost ledger cannot reconstruct prior claims; this is not an anti-tamper system.
- Invalid metrics, malformed text, nonfinite/negative rewards, invalid thresholds and
  unsupported reward currencies disable the affected definitions.
- A missing achievements JSON disables only achievements; a full match still completes.
- Practice does not create new progress or rewards. Standalone metadata reads are read-only.
- Pygame mouse-event tests exercise the menu/settings entry points, disabled locked
  buttons, claim persistence, repeat clicks, retry after save failure, group/status
  filters and pagination. All pages, including claimed/ready/empty states, render at
  both 1280×720 and 1920×1080.

`docs/achievements.png` was rendered from the actual application after automated matches
in a disposable save; the user's persistent save was not used to create that screenshot.
The main-menu screenshot was refreshed to include the achievement navigation badge.

Final checks: 150 tests PASS, data validation PASS (45 cards / 15 abilities / six packs /
15 achievements, zero diagnostics), compileall PASS and 60-frame headless startup PASS.
No runtime dependency was added. Existing Windows EXE, Python 3.12 local verification
and physical-device testing limitations remain unchanged.


## Runtime self-test and release tooling — 2026-10-06

Added **18 tests**, for **168 passing tests** overall. Also executed the new source
runtime self-test successfully: **26 checks passed**, with `frozen: false`.

Actual report files:

- `docs/runtime-verification.json`: successful **source** execution under Python
  3.11.2 / Pygame 2.6.1. The temporary self-test profile is isolated from personal saves.
- `docs/build-environment.json`: **failed** native-build preflight. Exit code was 1,
  as expected: this sandbox has Python older than 3.12 and lacks the shared Python
  library required by PyInstaller. No artifact was produced by this check.

Python 3.12 downloads were retried using official/runtime-distributor endpoints.
GitHub release metadata was accessible, but binary downloads and other runtime
endpoints failed with TLS connection errors. No unverified substitute binary was run.

New tests cover:

- Full source self-test, including 26 data/asset/UI/match/resume/economy/save checks.
- Self-test invocation from an unrelated cwd with a report path containing spaces;
  the personal save's bytes remain unchanged.
- CLI validation, explicit failure exit codes, report-write errors, and report output
  when `sys.stdout` is absent (as with a windowed Windows executable).
- Missing prerequisites, failed/timed-out shared-library discovery, and check-only mode.
- Runtime report schema, required check names, strict success flags and source/frozen
  distinction; stale or incomplete reports cannot authorize publication.
- Failed/timed-out build stages, stage ordering, manifest hashes, ZIP contents and
  checksum verification **using mocked subprocesses and a fake binary in a temporary
  directory**. These tests are orchestration tests, not proof of a native build.

The release tool now stages the executable, waits for its self-test from a temporary
cwd outside the repository, validates its report, and only then publishes the binary,
ZIP, manifest and SHA-256. `requirements-build.txt` keeps packaging tooling separate
from runtime dependencies. Windows and Linux launch wrappers use the same tool.

GitHub Actions is configured to run source tests/self-test on Python 3.12 and 3.13,
and native release verification on both Windows and Linux under 3.12, with diagnostic
artifacts on failures and release artifacts only on success. **Actions was not run
in this session.** No native executable or release ZIP has been built or verified
here. The new source runtime checks do not certify physical audio, display hardware,
DPI, code signing or antivirus behavior.
