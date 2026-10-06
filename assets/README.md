# Local art & audio

## Included audio (fully offline)

Nine short synthesized sound effects and two 16-second looping music tracks are
bundled as mono 16-bit PCM WAV at 22050 Hz. They are original procedural compositions,
not downloaded recordings or third-party samples.

- `sounds/click.wav`, `reveal.wav`, `flip.wav`, `win.wav`, `lose.wav`, `tie.wav`,
  `ability.wav`, `pack_open.wav`, `level_up.wav`.
- `music/menu.wav`: quiet pads and plucked arpeggios.
- `music/battle.wav`: related harmony with a faster arpeggio and a soft pulse.

Regenerate deterministically, using only the Python standard library:

```bash
python tools/generate_audio.py
```

The generator checks peak amplitude before writing, uses attack/release envelopes,
and uses a fixed seed for noise effects. Actual speaker/headphone quality still needs
human audition; automated tests check file decoding, boundaries, non-silence, mixer
playback, volume, and lack of digital clipping.

You may replace these files with your own licensed assets. SoundManager prefers
`menu.ogg` / `battle.ogg` when present, then falls back to WAV if missing or unreadable.
Volume controls apply immediately. Card-specific sounds may be supplied through the
card's `sound` field, relative to the project root.

## Images & fonts

The checkout uses deterministic procedural champion sigils and landscapes rendered
by Pygame. No downloaded fonts or third-party artwork are required.

- `cards/`: PNG/JPEG paths declared in `data/cards.json`.
- `fonts/Inter.ttf`: optional user-supplied font; otherwise the bundled Pygame font.
- `ui/`, `backgrounds/`, `icons/`, `effects/`: locations for future custom art.

Missing or unreadable images/audio use safe fallbacks. The game remains playable
with no audio device, with all sound assets removed, or without external fonts.
