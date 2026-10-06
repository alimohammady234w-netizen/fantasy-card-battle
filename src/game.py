"""Native Pygame application; rules and persistence remain independent of SDL."""
import logging
import pygame
from src.game_manager import GameManager
from src.systems.asset_manager import AssetManager
from src.systems.sound_manager import SoundManager
from src.systems.animation_manager import AnimationManager
from src.systems.localization import Localization
from src.ui.widgets import UI, BG, TEXT, GOLD, MUTED, TEAL
from src.ui.card_view import CardView
from src.ui.menu import MenuScreen, PlayScreen
from src.ui.collection import LibraryScreen
from src.ui.match_screen import MatchScreen, ResultScreen
from src.ui.packs import PackScreen
from src.ui.settings import SettingsScreen
from src.ui.journal import JournalScreen
from src.ui.help_screen import HelpScreen
from src.ui.achievements import AchievementsScreen


class Game:
    def __init__(self, save_path=None):
        pygame.init()
        self.manager = GameManager(save_path)
        self.running = True
        self.canvas = pygame.Surface((1280, 720))
        self.window = None
        self.set_display()
        pygame.display.set_caption('ARCANA | Realms in Conflict')
        self.assets = AssetManager()
        self.sound = SoundManager(self.manager.profile['settings'])
        self.animation = AnimationManager()
        self.locale = Localization()
        self.ui = UI(self)
        self.card_view = CardView(self)
        self.clock = pygame.time.Clock()
        self.mouse = (-1, -1)
        self.toast, self.toast_time = '', 0.
        if self.manager.data_issues:
            self.notify(f'{len(self.manager.data_issues)} data warnings. Safe fallbacks applied; see game.log.')
        if self.manager.resume_warning:
            self.notify(self.manager.resume_warning)
        self.modal = None
        self.route = 'menu'
        self.screen = MenuScreen(self)
        self.viewport = pygame.Rect(0, 0, *self.window.get_size())
        self.background = self.make_background()
        pygame.key.start_text_input()

    def set_display(self):
        settings = self.manager.profile['settings']
        flags = pygame.FULLSCREEN if settings['fullscreen'] else pygame.RESIZABLE
        try:
            self.window = pygame.display.set_mode(settings['resolution'], flags)
        except pygame.error as error:
            logging.warning('Display fallback: %s', error)
            settings['fullscreen'] = False
            settings['resolution'] = [1280, 720]
            self.window = pygame.display.set_mode((1280, 720), pygame.RESIZABLE)

    def make_background(self):
        surface = pygame.Surface((1280, 720))
        for y in range(720):
            color = (12+int(5*y/720), 19+int(7*y/720), 28+int(7*y/720))
            pygame.draw.line(surface, color, (0, y), (1280, y))
        for x in range(-400, 1500, 90):
            pygame.draw.line(surface, (23, 35, 44), (x, 720), (x+420, 0))
        for radius in (180, 250, 320, 390, 460):
            pygame.draw.circle(surface, (25, 40, 47), (1030, 360), radius, 1)
        return surface

    def navigate(self, route):
        if route == 'exit':
            self.confirm('Save and exit the game?', self.quit)
            return
        screens = {'menu': MenuScreen, 'play': PlayScreen, 'packs': PackScreen,
                   'settings': SettingsScreen, 'battle': MatchScreen, 'result': ResultScreen,
                   'journal': JournalScreen, 'help': HelpScreen, 'achievements': AchievementsScreen}
        self.screen = LibraryScreen(self, route) if route in ('decks', 'collection', 'upgrades') else screens[route](self)
        self.route = route
        self.animation.reset()
        self.ui.buttons.clear()
        self.sound.music('battle' if route=='battle' else 'menu')

    def home(self):
        if self.route == 'battle' and not self.manager.battle.finished:
            self.confirm('Suspend this match? Resume it later from PLAY.', self.suspend_match)
        else:
            self.navigate('menu')

    def suspend_match(self):
        self.persist()
        self.navigate('menu')

    def resume_match(self):
        self.manager.resume()
        self.navigate('battle')

    def quit(self):
        self.persist()
        self.running = False

    def persist(self):
        if not self.manager.save():
            self.notify('Save failed. Check folder permissions. See game.log.')

    def notify(self, message):
        self.toast, self.toast_time = message, 4.

    def perform(self, action):
        try:
            action()
        except (ValueError, KeyError) as error:
            logging.info('Action rejected: %s', error)
            self.notify(str(error))

    def prompt(self, title, callback):
        self.modal = {'title': title, 'callback': callback, 'text': '', 'input': True}

    def confirm(self, title, callback):
        self.modal = {'title': title, 'callback': callback, 'text': '', 'input': False}

    def accept_modal(self):
        modal, self.modal = self.modal, None
        if modal:
            self.perform(lambda: modal['callback'](modal['text']) if modal['input'] else modal['callback']())

    def to_logical(self, pos):
        return ((pos[0]-self.viewport.x)*1280/self.viewport.w,
                (pos[1]-self.viewport.y)*720/self.viewport.h)

    def events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.quit()
            elif event.type == pygame.VIDEORESIZE and not self.manager.profile['settings']['fullscreen']:
                self.window = pygame.display.set_mode((max(640, event.w), max(360, event.h)), pygame.RESIZABLE)
            elif self.modal:
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        self.modal = None
                    elif event.key == pygame.K_RETURN:
                        self.accept_modal()
                    elif event.key == pygame.K_BACKSPACE and self.modal['input']:
                        self.modal['text'] = self.modal['text'][:-1]
                elif event.type == pygame.TEXTINPUT and self.modal['input']:
                    self.modal['text'] = (self.modal['text']+event.text)[:40]
                elif event.type == pygame.MOUSEBUTTONDOWN and event.button==1:
                    self.ui.dispatch(self.to_logical(event.pos))
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE and not getattr(self.screen, 'searching', False):
                self.home() if self.route!='menu' else self.navigate('exit')
            else:
                self.screen.event(event)
                if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                    self.ui.dispatch(self.to_logical(event.pos))

    def draw(self):
        self.canvas.blit(self.background, (0, 0))
        self.ui.buttons.clear()
        self.screen.draw()
        if self.animation.age < .18:
            veil = pygame.Surface((1280, 720))
            veil.fill(BG)
            veil.set_alpha(int(95*(1-self.animation.age/.18)))
            self.canvas.blit(veil, (0, 0))
        if self.modal:
            veil = pygame.Surface((1280, 720), pygame.SRCALPHA)
            veil.fill((2, 7, 13, 205))
            self.canvas.blit(veil, (0, 0))
            self.ui.buttons.clear()
            self.ui.panel((337, 231, 606, 263), (24, 37, 49), GOLD)
            self.ui.wrap(self.modal['title'], 368, 265, 541, 26, TEXT, 30)
            if self.modal['input']:
                self.ui.panel((367, 330, 543, 48))
                self.ui.text(self.modal['text']+'|', 383, 343, 25, TEAL)
            self.ui.button('CANCEL', (366, 415, 256, 46), lambda: setattr(self, 'modal', None))
            self.ui.button('CONFIRM', (645, 415, 267, 46), self.accept_modal, True)
        if self.toast_time > 0:
            self.ui.panel((220, 655, 840, 48), (32, 49, 61), GOLD)
            self.ui.text(self.toast[:100], 640, 671, 21, GOLD, center=True)
        size = self.window.get_size()
        scale = min(size[0]/1280, size[1]/720)
        scaled = (max(1, int(1280*scale)), max(1, int(720*scale)))
        self.viewport = pygame.Rect((size[0]-scaled[0])//2, (size[1]-scaled[1])//2, *scaled)
        self.window.fill(BG)
        self.window.blit(pygame.transform.smoothscale(self.canvas, scaled), self.viewport)
        pygame.display.flip()

    def run(self, max_frames=None, screenshot=None):
        frames = 0
        self.sound.music('menu')
        while self.running:
            dt = min(.1, self.clock.tick(60)/1000.)
            self.mouse = self.to_logical(pygame.mouse.get_pos())
            self.events()
            self.animation.update(dt, self.manager.profile['settings']['animation_speed'])
            self.toast_time = max(0, self.toast_time-dt)
            if not self.modal:
                self.screen.update(dt)
            self.draw()
            frames += 1
            if max_frames is not None and frames >= max_frames:
                break
        if screenshot:
            pygame.image.save(self.canvas, screenshot)
        self.persist()
        pygame.quit()
