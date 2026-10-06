import os
os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['SDL_AUDIODRIVER'] = 'dummy'
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import copy
import shutil
import struct
import tempfile
import unittest
import wave
from pathlib import Path
from unittest.mock import patch
import pygame

from src.systems.resources import ROOT
from src.systems.sound_manager import SoundManager
from src.systems.data_validation import DEFAULT_SETTINGS
from tools.generate_audio import notes, write_wav


class AudioTests(unittest.TestCase):
    def setUp(self):
        pygame.init()
        self.temp = tempfile.TemporaryDirectory()
        self.settings = copy.deepcopy(DEFAULT_SETTINGS)
        self.sound = SoundManager(self.settings)

    def tearDown(self):
        pygame.quit()
        self.temp.cleanup()

    def test_every_shipped_effect_loads(self):
        for event in self.sound.EVENTS:
            self.sound.play(event)
            sample = self.sound.cache[f'assets/sounds/{event}.wav']
            self.assertIsNotNone(sample, event)
            self.assertGreater(sample.get_length(), .05)

    def test_music_tracks_play_and_switch(self):
        for name in ('menu', 'battle'):
            self.sound.music(name)
            self.assertEqual(self.sound.current_music, name)
            self.assertTrue(pygame.mixer.music.get_busy())

    def test_live_volume_and_mute(self):
        self.sound.music('menu')
        self.sound.play('win')
        self.settings['volume'] = .5
        self.settings['music_volume'] = .4
        self.settings['sound_volume'] = .6
        self.sound.refresh_volumes()
        self.assertAlmostEqual(pygame.mixer.music.get_volume(), .2, delta=.01)
        self.assertAlmostEqual(self.sound.cache['assets/sounds/win.wav'].get_volume(), .3, delta=.01)
        self.settings['volume'] = 0
        self.sound.refresh_volumes()
        self.assertEqual(pygame.mixer.music.get_volume(), 0)

    def test_missing_and_corrupt_sound_are_safe(self):
        root = Path(self.temp.name)
        (root/'bad.wav').write_bytes(b'not a wav')
        with patch('src.systems.sound_manager.ROOT', root):
            self.sound.play_file('bad.wav')
            self.sound.play_file('absent.wav')
            self.sound.music('missing')
        self.assertIsNone(self.sound.cache['bad.wav'])
        self.assertIsNone(self.sound.cache['absent.wav'])

    def test_corrupt_ogg_falls_back_to_bundled_wav(self):
        root = Path(self.temp.name)
        music = root/'assets/music'
        music.mkdir(parents=True)
        (music/'menu.ogg').write_bytes(b'corrupt')
        shutil.copy(ROOT/'assets/music/menu.wav', music/'menu.wav')
        with patch('src.systems.sound_manager.ROOT', root):
            self.sound.music('menu')
        self.assertTrue(pygame.mixer.music.get_busy())

    def test_audio_device_unavailable(self):
        pygame.mixer.quit()
        with patch('pygame.mixer.init', side_effect=pygame.error('no audio device')):
            sound = SoundManager(self.settings)
        self.assertFalse(sound.enabled)
        sound.play('win')
        sound.music('menu')
        sound.refresh_volumes()

    def test_bundled_pcm_has_no_clipping_or_empty_tracks(self):
        paths = list((ROOT/'assets/sounds').glob('*.wav'))+list((ROOT/'assets/music').glob('*.wav'))
        self.assertEqual(len(paths), 11)
        for path in paths:
            with wave.open(str(path), 'rb') as handle:
                self.assertEqual(handle.getsampwidth(), 2)
                self.assertEqual(handle.getframerate(), 22050)
                data = handle.readframes(handle.getnframes())
            values = [sample[0] for sample in struct.iter_unpack('<h', data)]
            self.assertGreater(max(values), 100)
            self.assertLess(max(abs(value) for value in values), 32113)
            self.assertLess(abs(values[0]), 100)
            self.assertLess(abs(values[-1]), 100)

    def test_generation_is_deterministic(self):
        samples = notes([(0, 440, .12)], .15)
        self.assertEqual(samples, notes([(0, 440, .12)], .15))
        a, b = Path(self.temp.name)/'a.wav', Path(self.temp.name)/'b.wav'
        write_wav(a, samples)
        write_wav(b, samples)
        self.assertEqual(a.read_bytes(), b.read_bytes())
        with self.assertRaises(ValueError):
            write_wav(a, [1.5])


if __name__ == '__main__':
    unittest.main()
