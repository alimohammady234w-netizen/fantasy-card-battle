# Platforms

Target platforms: **Windows (Steam)** and **Android**. Both build from one rules engine; everything below is
about the parts that are not shared.

## Windows / Steam

* `Config/DefaultEngine.ini` selects DX12 first, DX11 second, Vulkan SM5 as the fallback. Nothing in the game
  needs a specific RHI: the whole table is 2D text and quads.
* `FantasyCardBattle.uproject` lists the `OnlineSubsystemSteam` plugin but leaves it **disabled**. v1 has no
  achievements, cloud saves, lobbies or matchmaking, so the shipped build does not link Steam at all.
  Enabling it is a plugin flag plus an `OnlineSubsystem` config section - no game-code change is needed,
  because nothing in `Source/FantasyCardBattle` includes an online header.
* Window: the game needs roughly 1100x700 to show 12 hand rows plus the detail column. Below that the debug HUD
  drops hand rows (`FCBHud::ComputeLayout` clamps `VisibleCards`) rather than overlapping text; the UMG pass
  replaces that with real scaling.
* Packaging: `Build > Package Project > Windows`. The game reads `Content/Data/Generated/*.csv` at runtime, so
  those files must be staged when you are *not* using DataTable assets: add them to
  `Project Packaging > Additional Non-Asset Directories to Copy`. When `import_content.py` has been run, the
  data lives in cooked `.uasset` form and no extra directory is needed.

## Android

From `Config/DefaultEngine.ini` (`[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]`):

| Setting | Value | Why |
|---------|-------|-----|
| `PackageName` | `com.fantasy.cardbattle` | |
| `MinSdkVersion` / `TargetSdkVersion` | 26 / 34 | Android 8.0 and up |
| `Orientation` | Landscape | the table is a wide layout; portrait has no sane hand+detail arrangement |
| `bBuildForArm64`, `bBuildForES31=False` | ARM64 only | no 32-bit builds for a 2026 release |
| `bSupportVulkan` | True | with `r.Mobile.AntiAliasing=2` (MSAA) as the desktop-matching default |
| `DeepLinkScheme` | `fantasy-card-battle` | reserved for a future "open this match" link |
| `r.Streaming.PoolSize` | 512 | card art is texture-heavy and geometry-light |

The mobile build defines `FCB_MOBILE_BUILD=1` (see `FantasyCardBattle.Build.cs`). It is used for two things:
the AI think delay is capped by `TimeBudgetMs` so a Legendary decision never blocks the render thread past a
frame budget, and the debug HUD's font scale goes up because 12 px text is unreadable on a 6" screen.

Input on touch: `Config/DefaultInput.ini` has `DefaultTouchInterface=None` and `bEnableTouchEmulation=False`.
The debug HUD's tap-to-confirm (tap a card row = select + declare its default attribute) is the only touch
path in v1; proper touch controls belong to the UMG widgets, where a tap lands on a card and a long-press opens
it. Do **not** turn on mouse emulation as a workaround: it double-fires every tap.

The whole match is one screen with no camera, so there is no safe-area layout problem — but the Android
gesture bars do eat the bottom row. Keep the last hand row's tap target above `48dp` from the bottom edge when
UMG lands.

## Deliberately not supported

Dedicated server (the game is hot-seat or vs AI; `ServerDefaultMap` is `/Engine/Maps/Entry` and nothing
registers a replicated game mode), iOS (no hardware to test on yet; the build would work - the project has no
platform-specific code outside `FCB_MOBILE_BUILD`), and consoles (the theme and the license situation of
"Top Trumps with fantasy art" need clearing before a first-party dev kit is worth the fee).

## Perf budget

A turn is: 12 cards x 4 attributes = 48 candidate moves, each evaluated against the remaining unknown
multiset (~98 entries) → ~4,700 round resolutions, each of which walks at most 2+2 abilities. On Expert that
is well under a millisecond on desktop and a few milliseconds on a mid-range phone; Legendary adds
`RolloutSamples` full-hand samples per top candidate, and is the only tier that can measure in tens of
milliseconds. If the AI ever needs to be cheaper, `MaxCandidatesToScore` is the knob (it truncates the scan
after sorting by a cheap heuristic), not a rewrite.
