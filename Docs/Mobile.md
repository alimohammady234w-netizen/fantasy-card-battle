# Mobile

The Android build is not a different game. It is the same match, the same rules engine and the same HUD, with
a layout and an input layer that assume a finger instead of a mouse. Everything that decides *where* a control
is lives in one engine-free file, [`Source/FantasyCardBattle/Public/FCBTouch.h`](../Source/FantasyCardBattle/Public/FCBTouch.h),
and is checked by `Tools/mock_build.sh run` on simulated phones, tablets and desktop windows.

Two properties are load-bearing, and both are tested:

* **A tap is unambiguous.** Its meaning comes from the rect it landed in, never from timing, ordering or a
  previous tap. There is no double-tap-to-confirm and no tap that plays a card you did not point at.
* **There is one layout.** The HUD draws from `FCBTouch::FTableLayout` and the player controller hit tests the
  same struct. A control that is drawn somewhere the hit test does not know about is the classic touch bug
  ("I tapped it and nothing happened"), and it cannot happen here.

## Touch map

| Gesture | Where | Command |
|---------|-------|---------|
| Tap | a value cell (`AGE` / `PWR` / `SPD` / `HGT`) | `DeclareAndPlay` - selects the card, declares that attribute and plays it. One tap, one round |
| Tap | the name strip of a row | `SelectCard` - opens the card in the detail column, plays nothing |
| Tap | an attribute chip on the detail card | `DeclareAttribute` - declares, waits for `PLAY` |
| Tap | `PLAY` | `Confirm` - plays the selected card on the declared attribute |
| Hold | anywhere on a hand row | `PeekCard` - the full card face, ability text and all. Modal: the next tap closes it |
| Swipe left / right | over the hand | Pages the hand (wraps) when it is paged, otherwise walks the selection |
| Tap | `NEW` / `DEAL` / `LOG` | New match / fresh deal / log, available even while the opponent is thinking |
| Tap | `<` / `>` | Hand page (only drawn when the hand is paged) |
| Tap | anywhere on the end screen | `NewMatch` - the whole end screen is one play-again target |
| Tap | empty table | nothing, deliberately: a stray tap in the middle of the table must not spend a card |

Keyboard and mouse resolve through exactly the same function (`FCBTouch::ResolveTap` /
`ResolveGesture`), so a desktop click on a value cell now plays that card too, and `←`/`→`/`PageUp`/`PageDown`
are the keyboard equivalent of the swipe.

## The hand grid

The hand is 12 cards. Twelve rows at a 48 dp touch target need 576 dp of height plus gaps, which no phone in
landscape has - so the grid is chosen from the space that is actually available, in this order:

1. **Fewest columns that fit the whole hand at the target.** A desktop window fits 12 rows, so it keeps its
   single column and the desktop layout does not change.
2. **The same, allowing rows to shrink to 92% of the target** (`FTuning::CompactTolerance`). A phone gets two
   columns of six at ~44-45 dp instead of a 48 dp grid that hides four cards behind a swipe. Forty-four dp is
   Apple's minimum; four visible cards is not a trade worth making for four dp.
3. **Paging.** Only when even that does not fit (`854x480` and below): the widest allowed grid, rows kept at the
   compact target, `<` `>` buttons in the status band and a swipe to turn the page.

Two constraints run through all three steps: a column is only added while its value cells stay at least 0.75 of a
touch target wide, and the detail column always keeps at least 30% of the width, so `PLAY`, the chips, the
preview and the ability text can never be squeezed out by a second column of cards.

### What that produces

Measured by `Tools/mock_build.sh run` (`TestTouchLayoutFitsEveryTarget`), 12 cards in hand:

| Viewport | Density | Grid | Rows | Value cell |
|----------|---------|------|------|------------|
| 2400x1080 (phone 20:9) | 3x | 2 x 6 | 136 px = 45 dp | 122 px = 41 dp |
| 1920x1080 (phone 16:9) | 3x | 2 x 6 | 136 px = 45 dp | 108 px = 36 dp |
| 1280x720 (small phone) | 2x | 2 x 6 | 88 px = 44 dp | 72 px = 36 dp |
| 2560x1600 (tablet) | 2x | 1 x 12 | 109 px = 55 dp | 261 px = 131 dp |
| 854x480 (old 480p) | 1.5x | 1 x 5, 3 pages | 67 px = 45 dp | 87 px = 58 dp |
| 1920x1080 (desktop) | 1x | 1 x 12 | 77 px | 208 px |
| 1100x700 (small window) | 1x | 2 x 6 | 101 px | 59 px |

The density is the game's own estimate of viewport pixels per device-independent pixel: the short side over
380, clamped to 1..3. A phone's short side is 360-420 dp, so that is the device's density; the cap stops a
tablet, whose short side is ~800 dp, from inflating its own targets. Desktop builds return 1.0 - a mouse hits a
48 px row perfectly well, and a small desktop window must not turn into a phone.

## Safe area

Notches, rounded corners and the Android gesture bar. The bottom is the one that matters: the gesture bar sits
exactly where the last hand row would be, which is why the fallback insets are asymmetric.

| | Left | Top | Right | Bottom |
|--|------|-----|-------|--------|
| Touch fallback (`FCBTouch::FallbackSafeArea(true)`) | 3% | 2% | 3% | 5% |
| Desktop | 0 | 0 | 0 | 0 |

`UFCBSettings.TouchSafeArea*Percent` overrides each edge independently; a negative value means "use the
fallback". Everything the player must hit - every value cell, the buttons, `PLAY` - is clamped inside the safe
rect by `FCBTouch::BuildLayout`, and `TestTouchControlsNeverOverlap` asserts it for every device in the table
above, mid-match and on the end screen.

The device's own cutout value is the one thing here that is not read yet: the hook is the four settings above
(a phone that reports a notch simply ships with the percentages), and wiring `FSlateApplication`'s safe zone
into `FCBHud::MakeSafeArea` in `FCBGameMode.cpp` is a one-line change that does not touch any of the tested
geometry. The layout already accepts per-edge insets as data.

## Gestures

| Threshold | Default | Why |
|-----------|---------|-----|
| Tap | < 0.35 s, < 24 px of travel | a still finger is a tap no matter how slow it is - no dead band between a tap and a hold, because a slow tap is still a tap |
| Hold | >= 0.45 s, still within 24 px | opens the card face. Only on a hand row; a hold anywhere else does nothing |
| Swipe | >= 5% of the short side, at least 72 px, < 0.8 s | device-relative, but floored so a small screen does not make swipes twitchy |
| Diagonal drag | ignored | horizontal must beat vertical by 1.4x; guessing between "page" and "inspect" is worse than doing nothing |

All four are in `FGestureConfig` and can be swept from `UFCBSettings`
(`TouchLongPressSeconds`, `TouchTapSlopPx`).

## Accessibility and deliberate limits

* **No accidental plays.** A tap on empty table is a no-op, `PLAY` is always on screen, and the value cells are
  the only thing that plays a card. `bTouchOneTapPlay=False` turns the value cells into declare-only, which
  makes `PLAY` the single confirm - the setting for a player who cannot reliably hit a value cell.
* **Nothing is hidden behind a gesture you have to know.** Paging has buttons as well as a swipe; the detail
  column shows the declared attribute as a lit chip; the last command is printed for 2.5 s at the bottom of the
  screen, *including the commands that did nothing* ("ignored: it is not a playable moment"). Silence is the
  worst feedback a touch interface can give.
* **One finger.** A second finger landing mid-gesture is ignored rather than retargeting the first. No pinch, no
  two-finger anything: every command is reachable one-handed in landscape.
* **Landscape only** (`Orientation=Landscape` in `Config/DefaultEngine.ini`). Portrait still lays out - the grid
  drops to a single column - but the hand-versus-detail split assumes a wide screen, and pretending otherwise
  would ship a worse layout than the one the tests cover.

## Testing

`Tools/mock_build.sh run` compiles `FCBTouch.cpp` with plain `g++` and runs seven touch tests before any engine
is involved:

| Test | What it pins |
|------|--------------|
| `TestTouchLayoutFitsEveryTarget` | for 7 viewports: no card or control outside the safe area, cells tile their row exactly, no cell narrower than 0.75 of a target or shorter than the compact target, no two rows overlapping |
| `TestTouchEveryCardAndAttributeIsReachable` | every card x every attribute, on every page, resolves to the right `DeclareAndPlay` from the centre of its cell - 12 cards x 4 attributes x 7 devices |
| `TestTouchLayoutSizesAcrossViewports` | the desktop keeps one column; a phone gets two without paging; the tiny viewport pages; the last page clamps |
| `TestTouchGestureClassification` | tap / slow tap / hold / four swipes / diagonal / over-long drag, at the thresholds |
| `TestTouchNeverPlaysUnasked` | the AI's turn, the end screen, the peek overlay, empty table, one-tap-play off - and that the `NEW`/`DEAL`/`LOG` buttons still work in all of them |
| `TestTouchControlsNeverOverlap` | pair-wise overlap of every interactive rect, mid-match and after the result, on all 7 viewports |
| `TestTouchTracksSelectionAndHandSize` | a hand that shrinks clamps the page, page-follows-selection, an empty hand draws nothing |

Two in-engine tests cover the seam the harness cannot see (it has no config system):
`FantasyCardBattle.Mobile.SettingsAreAbsorbed` proves `Config/DefaultGame.ini` reaches `UFCBSettings`, and
`FantasyCardBattle.Mobile.LayoutIsPlayable` re-runs the geometry against the shipped settings and prints the
grid it chose.

Adding a device is one row in `GDevices` in `Tools/MockUE/SelfTest.cpp` - if a value cell is unreachable there,
the harness fails before a phone ever runs it.

## On the device

```
Tools/mock_build.sh run                      # the geometry, before anything is packaged
UnrealEditor-Cmd FantasyCardBattle.uproject -ExecCmds="Automation RunTests FantasyCardBattle.Mobile;Quit" -unattended -nullrhi -log
```

Then package (`Build > Package Project > Android`), and check three things in this order:

1. **`F1` overlay** on the device: `layout: 2x6 grid, 12 cards/page ...` next to `safe L3.0% T2.0% R3.0% B5.0%`.
   If the grid says `paged` on a phone, the density or the safe area is off - tune `TouchTargetDp` or the
   `TouchSafeArea*Percent` settings rather than editing the maths.
2. **One tap on a value cell** plays that card. If it selects instead, `bTouchOneTapPlay` is off.
3. **The bottom row.** It must sit above the gesture bar; if it does not, the reported safe area is smaller than
   the device's and `TouchSafeAreaBottomPercent` needs a real value.

Tuning knobs, in the order to reach for them: `TouchTargetDp` (bigger grid, fewer columns),
`TouchSafeArea*Percent` (the gesture bar), `TouchTargetOverridePx` (a debug "pretend the target is this many
pixels" override), and only then `FCBTouch::FTuning` itself, which means re-running the harness because the
tests encode the invariants those numbers have to satisfy.
