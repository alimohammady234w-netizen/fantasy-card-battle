import logging
import pygame
from src.systems.resources import ROOT


class SoundManager:
    EVENTS = ('click', 'reveal', 'flip', 'win', 'lose', 'tie', 'ability', 'pack_open', 'level_up')

    def __init__(self, settings):
        self.settings, self.cache = settings, {}
        self.enabled, self.current_music = False, None
        try:
            if not pygame.mixer.get_init():
                pygame.mixer.init()
            self.enabled = True
        except pygame.error as error:
            logging.warning('Audio disabled: %s', error)

    def play(self, name):
        self.play_file(f'assets/sounds/{name}.wav')

    def play_file(self, relative_path):
        if not self.enabled or not relative_path:
            return
        if relative_path not in self.cache:
            path = ROOT / relative_path
            try:
                self.cache[relative_path] = pygame.mixer.Sound(str(path)) if path.exists() else None
            except (pygame.error, OSError):
                self.cache[relative_path] = None
        sound = self.cache[relative_path]
        if sound:
            sound.set_volume(self.settings['volume'] * self.settings['sound_volume'])
            sound.play()

    def music(self, name):
        if not self.enabled:
            return
        self.refresh_volumes()
        if self.current_music == name:
            return
        self.current_music = name
        pygame.mixer.music.stop()
        paths = [ROOT / 'assets/music' / f'{name}.{ext}' for ext in ('ogg', 'wav')]
        for path in paths:
            if not path.exists():
                continue
            try:
                pygame.mixer.music.load(str(path))
                pygame.mixer.music.play(-1)
                break
            except (pygame.error, OSError) as error:
                logging.warning('Music unavailable: %s', error)

    def refresh_volumes(self):
        """Settings apply immediately, including music already playing."""
        if not self.enabled:
            return
        pygame.mixer.music.set_volume(self.settings['volume'] * self.settings['music_volume'])
        for sound in self.cache.values():
            if sound is not None:
                sound.set_volume(self.settings['volume'] * self.settings['sound_volume'])
