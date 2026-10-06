ARCANA - Realms in Conflict
Offline single-player card battle

RUN
Windows: extract the ZIP, then open CardGame.exe.
Linux: extract the ZIP, run chmod +x CardGame if needed, then ./CardGame.
The packaged game does not require Python or an internet connection.
Builds are native to their OS/architecture; a Linux binary is not a Windows EXE.

FIRST MATCH
PLAY > Quick Match > START MATCH.
Choose an attribute, then Greater or Less. The opponent's card is revealed and
abilities resolve. The winner captures both cards as match points.
Keys 1-8 select attributes, G/L select the direction, Enter advances rounds.
Escape returns to the menu with confirmation. PLAY > RESUME continues a saved match.
Unconfirmed selections and their timer restart on resume.

MORE FEATURES
HOW TO PLAY: interactive rulebook and ability reference.
MATCH JOURNAL: completed match history, never the unrevealed deck.
ACHIEVEMENTS: progress and one-time rewards; use CLAIM when ready.
Deck Builder, Collection, Upgrades, Packs and Settings are available from the menu.

LOCAL DATA
Windows: %LOCALAPPDATA%\ArcanaCardGame\saves\player_save.json
Linux: ~/ArcanaCardGame/saves/player_save.json
The game.log file is in the parent ArcanaCardGame folder.
Only run one instance per save. Back up your save before replacing game data.
Local JSON saves are not encrypted or anti-tamper protected.

DEPLOYMENT SELF-TEST (DOES NOT USE YOUR PERSONAL SAVE)
Windows PowerShell:
  $p = Start-Process .\CardGame.exe -ArgumentList '--self-test','--report','runtime.json' -Wait -PassThru
  $p.ExitCode
Linux:
  ./CardGame --self-test --report runtime.json
The test uses dummy audio/video and an isolated temporary profile. It checks data,
assets, UI, all match modes, resume, progression, packs, achievements and persistence.
It does not certify physical display/audio hardware behavior.

INTEGRITY AND TRUST
manifest.json contains the executable SHA-256, build versions and runtime check count.
runtime-report.json contains checks run on the packaged executable during the build.
The adjacent .zip.sha256 file verifies the archive. Checksums detect file changes;
they do not replace a trusted publisher signature. The executable is unsigned unless
the distributor separately code-signs it. Do not bypass OS warnings for unknown files.

TROUBLESHOOTING
If a window cannot open, use a desktop session with a working graphics driver.
If audio is unavailable the game continues silently. Check Settings volume controls.
If saving fails, check folder permissions and the log. If a checkpoint no longer
matches the installed card/rule data, it is discarded without resetting progression.
Before public distribution, also test normal desktop launch, sound, fullscreen,
Windows DPI behavior, save/restart, and OS security prompts on real hardware.
