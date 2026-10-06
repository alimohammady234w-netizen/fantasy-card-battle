"""Reproducible, original synthesized audio; no downloads or extra dependencies.

Run `python tools/generate_audio.py` to regenerate the bundled PCM WAV assets.
Small plucked tones, soft pads and filtered-noise swishes intentionally leave room
for gameplay rather than imitating a recorded orchestral soundtrack.
"""
import argparse
import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
TAU = math.tau
ROOT = Path(__file__).resolve().parents[1]


def envelope(t, duration, attack=.012, release=.06):
    if t < 0 or t >= duration:
        return 0.
    return min(1., t / attack, (duration-t) / release)


def tone(frequency, t):
    return math.sin(TAU*frequency*t) + .15*math.sin(TAU*frequency*2*t)


def notes(sequence, duration, gain=.22):
    """Each item: (start time, frequency, note length)."""
    output = []
    for i in range(int(RATE*duration)):
        t = i/RATE
        value = 0.
        for start, frequency, length in sequence:
            local = t-start
            if 0 <= local < length:
                value += tone(frequency, local)*envelope(local, length)*math.exp(-3*local/length)*gain
        output.append(value)
    return output


def swish(duration, start_frequency, end_frequency, seed):
    rng = random.Random(seed)
    output, noise = [], 0.
    for i in range(int(RATE*duration)):
        t = i/RATE
        noise = .85*noise+.15*rng.uniform(-1, 1)
        phase = TAU*(start_frequency*t+(end_frequency-start_frequency)*t*t/(2*duration))
        output.append((.1*math.sin(phase)+.24*noise)*envelope(t, duration, .02, .09))
    return output


def soundtrack(battle=False, duration=16):
    """Four original arpeggiated minor/major voicings, with a restrained battle pulse."""
    chords = ((130.81, 155.56, 196.00), (103.83, 130.81, 155.56),
              (116.54, 146.83, 174.61), (98.00, 130.81, 196.00))
    output = []
    for i in range(int(RATE*duration)):
        t = i/RATE
        chord_index = min(3, int(t/4))
        chord = chords[chord_index]
        local = t % 4
        pad = sum(tone(freq, t) for freq in chord)/3
        value = .13*pad*envelope(local, 4, .4, .5)
        step = .25 if battle else .5
        note_time = t % step
        freq = chord[int(t/step) % 3]*2
        value += .08*tone(freq, note_time)*envelope(note_time, step, .006, .035)*math.exp(-8*note_time)
        if battle:
            beat = t % .5
            # A pitched decaying pulse, not an audio recording or sampled drum.
            value += .12*math.sin(TAU*(65*beat+2*(1-math.exp(-30*beat))))*math.exp(-22*beat)*min(1., beat/.004)
        value *= envelope(t, duration, .2, .35)
        output.append(value)
    return output


def write_wav(path, samples):
    path.parent.mkdir(parents=True, exist_ok=True)
    # Fixed gain, no per-file normalization: designed relative levels are preserved.
    peak = max(abs(value) for value in samples)
    if peak >= .98:
        raise ValueError(f'Clipping risk in {path.name}: {peak}')
    pcm = b''.join(struct.pack('<h', round(max(-1, min(1, value))*32767)) for value in samples)
    with wave.open(str(path), 'wb') as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        handle.writeframes(pcm)


def generate(output):
    sounds = {
        'click': notes([(0, 760, .075)], .09, .12),
        'flip': swish(.18, 500, 190, 4),
        'reveal': notes([(0, 392, .25), (.08, 587.33, .27), (.16, 783.99, .29)], .48),
        'win': notes([(0, 523.25, .38), (.13, 659.25, .38), (.26, 783.99, .48)], .78),
        'lose': notes([(0, 293.66, .35), (.16, 233.08, .43), (.30, 196, .4)], .73, .18),
        'tie': notes([(0, 392, .25), (.20, 392, .26)], .49, .16),
        'ability': swish(.40, 180, 880, 12),
        'pack_open': notes([(i*.08, f, .42) for i, f in enumerate((261.63, 392, 523.25, 659.25, 783.99, 1046.50))], .88, .18),
        'level_up': notes([(i*.12, f, .46) for i, f in enumerate((392, 493.88, 587.33, 783.99))], .86),
    }
    for name, samples in sounds.items():
        write_wav(output/'sounds'/f'{name}.wav', samples)
    for name in ('menu', 'battle'):
        write_wav(output/'music'/f'{name}.wav', soundtrack(battle=name=='battle'))
    return len(sounds)+2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'assets')
    args = parser.parse_args()
    count = generate(args.output)
    print(f'Generated {count} original audio assets in {args.output}')


if __name__ == '__main__':
    main()
