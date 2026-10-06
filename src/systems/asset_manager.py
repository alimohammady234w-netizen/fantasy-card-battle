import logging
import pygame
from src.systems.resources import ROOT


class AssetManager:
    def __init__(self):
        self.images, self.fonts = {}, {}

    def image(self, path):
        if not path:
            return None
        if path not in self.images:
            try:
                self.images[path] = pygame.image.load(str(ROOT / path)).convert_alpha()
            except (pygame.error, OSError):
                logging.info('Using procedural art for %s', path)
                self.images[path] = None
        return self.images[path]

    def font(self, size, bold=False):
        key = size, bold
        if key not in self.fonts:
            path = ROOT / 'assets/fonts/Inter.ttf'
            try:
                font = pygame.font.Font(str(path) if path.exists() else None, size)
            except (pygame.error, OSError):
                font = pygame.font.Font(None, size)
            font.set_bold(bold)
            self.fonts[key] = font
        return self.fonts[key]
