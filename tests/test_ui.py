import os
os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['SDL_AUDIODRIVER'] = 'dummy'
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import copy
import tempfile
import unittest
from pathlib import Path
import pygame
from src.game import Game


class UITests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.app = Game(Path(self.temp.name)/'player.json')
        self.app.animation.update(1.)
        self.app.draw()

    def tearDown(self):
        pygame.quit()
        self.temp.cleanup()

    def click(self, x, y):
        self.app.draw()
        pygame.event.post(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=1, pos=(x, y)))
        self.app.events()
        self.app.draw()

    def test_main_menu_play_click_flow(self):
        self.click(200, 450)
        self.assertEqual(self.app.route, 'play')
        self.click(1030, 640)
        self.assertEqual(self.app.route, 'battle')
        screen = self.app.screen
        self.assertIsNone(screen.battle.result)
        self.click(465, 207)
        self.assertEqual(screen.attribute, 'power')
        self.click(410, 543)
        self.assertTrue(screen.revealing)
        screen.update(.7)
        self.app.draw()
        self.assertIsNotNone(screen.battle.result)
        self.click(1090, 666)
        self.assertEqual(screen.battle.index, 1)
        self.assertIsNone(screen.battle.result)

    def test_full_ui_match_to_result(self):
        self.app.manager.start('Quick Match')
        self.app.navigate('battle')
        for _ in range(10):
            screen = self.app.screen
            screen.attribute = 'age'
            screen.choose('Less')
            screen.update(.8)
            self.app.draw()
            screen.next_round()
        self.assertEqual(self.app.route, 'result')
        self.app.draw()
        self.app.home()
        self.assertEqual(self.app.route, 'menu')
        self.assertEqual(self.app.manager.profile['statistics']['matches_played'], 1)

    def test_timer_automatic_selection(self):
        self.app.manager.start('Quick Match')
        self.app.navigate('battle')
        self.app.screen.update(16)
        self.assertTrue(self.app.screen.revealing)
        self.app.screen.update(1)
        self.assertIsNotNone(self.app.manager.battle.result)

    def test_classic_ai_turn_automatic(self):
        self.app.manager.start('Classic Match')
        self.app.navigate('battle')
        screen = self.app.screen
        screen.attribute = 'power'
        screen.choose('Greater')
        screen.update(1)
        screen.next_round()
        screen.update(2)
        screen.update(1)
        self.assertIsNotNone(screen.battle.result)

    def test_hidden_card_art_does_not_load_ai_image(self):
        self.app.manager.start('Quick Match')
        self.app.navigate('battle')
        player, enemy = self.app.manager.battle.cards
        self.app.assets.images.clear()
        self.app.card_view.art_cache.clear()
        self.app.draw()
        if player.id != enemy.id:
            self.assertNotIn(enemy.image, self.app.assets.images)

    def test_all_screens_render_both_resolutions(self):
        for resolution in ([1280, 720], [1920, 1080]):
            self.app.manager.profile['settings']['resolution'] = resolution
            self.app.set_display()
            for route in ('menu', 'play', 'decks', 'collection', 'upgrades', 'packs', 'settings'):
                self.app.navigate(route)
                self.app.draw()
                self.assertEqual(self.app.window.get_size(), tuple(resolution))
                self.assertTrue(self.app.ui.buttons)

    def test_collection_search_filter_sort(self):
        self.app.navigate('collection')
        screen = self.app.screen
        screen.query = 'wolf'
        self.assertEqual([c.name for c in screen.filtered()], ['Silver Wolf'])
        screen.query = ''
        screen.category = 'Animals'
        self.assertEqual(len(screen.filtered()), 3)
        screen.sort = 'Rarity'
        screen.draw()
        screen.selected = 45
        self.app.draw()  # Locked detail branch.

    def test_deck_and_upgrade_ui_actions(self):
        self.app.navigate('decks')
        screen = self.app.screen
        screen.deck_action('create', 'Test')
        screen.toggle(1)
        screen.deck_action('rename', 'Edited')
        self.assertEqual(self.app.manager.profile['decks']['Edited'], [1])
        screen.deck_action('delete')
        self.app.navigate('upgrades')
        coins = self.app.manager.profile['coins']
        self.app.screen.upgrade(self.app.manager.database[1], 'power')
        self.assertEqual(self.app.manager.profile['coins'], coins-100)
        self.app.draw()

    def test_pack_open_animation_and_pages(self):
        self.app.navigate('packs')
        self.app.screen.open_pack('Mythic')
        self.app.draw()
        self.app.screen.update(3)
        self.app.draw()
        self.app.screen.page = 1
        self.app.draw()
        self.assertEqual(len(self.app.screen.rewards), 10)
        self.assertEqual(self.app.manager.saves.load()['statistics']['total_packs_opened'], 1)

    def test_pack_double_click_does_not_charge_twice(self):
        self.app.navigate('packs')
        self.app.screen.open_pack('Bronze')
        gems = self.app.manager.profile['gems']
        rewards = list(self.app.screen.rewards)
        with self.assertRaises(ValueError):
            self.app.screen.open_pack('Bronze')
        self.assertEqual(self.app.manager.profile['gems'], gems)
        self.assertEqual(self.app.screen.rewards, rewards)
        self.assertEqual(self.app.manager.profile['statistics']['total_packs_opened'], 1)

    def test_empty_pack_catalog_and_extra_pack_page(self):
        self.app.navigate('packs')
        self.app.manager.pack_data['Custom Pack'] = copy.deepcopy(self.app.manager.pack_data['Bronze'])
        self.app.screen.catalog_page = 1
        self.app.draw()
        self.assertEqual(self.app.screen.catalog_page, 1)
        self.app.manager.pack_data.clear()
        self.app.draw()
        self.assertEqual(self.app.screen.catalog_page, 0)

    def test_settings_persist(self):
        self.app.navigate('settings')
        for key in self.app.screen.OPTIONS:
            self.app.screen.change(key)
        self.app.draw()
        self.assertEqual(self.app.manager.profile, self.app.manager.saves.load())

    def test_missing_assets_and_audio(self):
        self.assertIsNone(self.app.assets.image('missing.png'))
        self.assertIsNotNone(self.app.assets.font(28))
        for name in self.app.sound.EVENTS:
            self.app.sound.play(name)
        self.app.sound.music('missing')
        self.app.draw()

    def test_dialog_input_and_confirmation(self):
        values = []
        self.app.prompt('Name', values.append)
        self.app.draw()
        pygame.event.post(pygame.event.Event(pygame.TEXTINPUT, text='New deck'))
        pygame.event.post(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_RETURN))
        self.app.events()
        self.assertEqual(values, ['New deck'])
        self.assertIsNone(self.app.modal)

    def test_quit_saves(self):
        self.app.manager.profile['coins'] = 7000
        self.app.quit()
        self.assertFalse(self.app.running)
        self.assertEqual(self.app.manager.saves.load()['coins'], 7000)


if __name__ == '__main__':
    unittest.main()
