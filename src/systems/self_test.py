"""Runtime deployment check, included in source and frozen builds.

Uses an isolated temporary profile and SDL dummy drivers. It never opens or alters
the user's personal save. This validates software wiring, not physical audio/video.
"""
import copy
import json
import os
import platform
import sys
import tempfile
import traceback
from pathlib import Path


def run_self_test():
    os.environ['SDL_VIDEODRIVER'] = 'dummy'
    os.environ['SDL_AUDIODRIVER'] = 'dummy'
    os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
    report = {'schema_version': 1, 'status': 'failed', 'frozen': bool(getattr(sys, 'frozen', False)),
              'python': platform.python_version(), 'platform': sys.platform, 'checks': []}
    pygame = None

    def check(name, condition, detail=''):
        report['checks'].append({'name': name, 'passed': bool(condition), 'detail': detail})
        if not condition:
            raise RuntimeError(f'{name}: {detail or "check failed"}')

    try:
        import pygame
        from src.game import Game
        from src.ai.ai_player import AIPlayer
        from src.game_manager import GameManager
        from src.battle.checkpoint import CheckpointCodec
        report['pygame'] = pygame.version.ver
        with tempfile.TemporaryDirectory(prefix='arcana-self-test-') as folder:
            save_path = Path(folder)/'saves'/'player.json'
            app = Game(save_path)
            manager = app.manager
            check('bundled_data', len(manager.database.cards) >= 40 and not manager.data_issues,
                  f'{len(manager.database.cards)} cards; {len(manager.data_issues)} data warnings')
            check('isolated_save', save_path.exists(), 'Temporary profile created without accessing personal save')
            check('font_fallback', app.assets.font(24) is not None)
            check('missing_image_fallback', app.assets.image('assets/cards/__self_test_missing__.png') is None)
            for name in app.sound.EVENTS:
                app.sound.play(name)
            check('bundled_sound_effects', app.sound.enabled and all(
                app.sound.cache.get(f'assets/sounds/{name}.wav') is not None for name in app.sound.EVENTS),
                'All nine shipped effects decoded through SDL dummy audio')
            for name in ('menu', 'battle'):
                app.sound.music(name)
                check('music_'+name, pygame.mixer.music.get_busy())
            routes = ('menu', 'play', 'decks', 'collection', 'upgrades', 'packs',
                      'settings', 'journal', 'help', 'achievements')
            for route in routes:
                app.navigate(route)
                app.draw()
            check('all_screens', True, ', '.join(routes))

            def click(x, y):
                app.draw()
                pos = (int(app.viewport.x+x*app.viewport.w/1280),
                       int(app.viewport.y+y*app.viewport.h/720))
                pygame.event.post(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=1, pos=pos))
                app.events()
                app.draw()

            app.navigate('menu')
            click(200, 450)
            check('menu_play_event', app.route == 'play')
            click(1040, 640)
            check('start_match_event', app.route == 'battle' and manager.can_resume)
            click(465, 207)
            click(410, 543)
            app.screen.update(.8)
            app.draw()
            check('first_round_event', manager.battle.result is not None)
            snapshot = CheckpointCodec.encode(manager.battle)
            old_coins, old_xp = manager.profile['coins'], manager.profile['statistics']['total_xp']
            app.quit()
            pygame.quit()
            app = Game(save_path)
            manager = app.manager
            app.resume_match()
            app.draw()
            check('checkpoint_restore', CheckpointCodec.encode(manager.battle) == snapshot)
            check('no_reward_replay', manager.profile['coins'] == old_coins
                  and manager.profile['statistics']['total_xp'] == old_xp)

            chooser = AIPlayer('Normal')

            def finish_match():
                while app.route == 'battle':
                    screen = app.screen
                    if not screen.battle.result:
                        move = chooser.choose(screen.battle.values()[0])
                        screen.attribute = move.attribute
                        screen.choose(move.direction)
                        screen.update(.8)
                    app.draw()
                    screen.next_round()
                app.draw()
                if app.route != 'result':
                    raise RuntimeError('Result screen was not reached')

            finish_match()
            check('quick_match_complete', manager.profile['statistics']['matches_played'] == 1)
            manager.start('Classic Match')
            app.navigate('battle')
            finish_match()
            check('classic_match_complete', manager.profile['statistics']['matches_played'] == 2
                  and len(manager.battle.history) == manager.config['modes']['Classic Match']['rounds'])
            before = copy.deepcopy(manager.profile)
            manager.start('Practice')
            app.navigate('battle')
            finish_match()
            check('practice_no_rewards', manager.profile == before)
            app.navigate('journal')
            app.draw()
            check('journal', len(manager.profile['match_history']) == 2)
            app.navigate('packs')
            app.screen.open_pack('Bronze')
            app.screen.update(2.)
            app.draw()
            check('pack_purchase', len(app.screen.rewards) == manager.pack_data['Bronze']['count'])
            reward = manager.claim_achievement('first_steps')
            check('achievement_claim', reward['coins'] >= 0 and 'first_steps' in manager.profile['claimed_achievements'])
            duplicate_blocked = False
            try:
                manager.claim_achievement('first_steps')
            except ValueError:
                duplicate_blocked = True
            check('duplicate_claim_blocked', duplicate_blocked)
            card = manager.database[next(iter(manager.database.cards))]
            state = manager.progression.card_state(card.id)
            values = card.values(state, manager.config['level_stat_bonus'])
            attr = next(key for key, value in values.items() if value < 100)
            manager.progression.upgrade(card, attr)
            check('card_upgrade', card.values(state, manager.config['level_stat_bonus'])[attr] == values[attr]+1)
            manager.decks.create('Release check')
            for card_id in list(manager.profile['unlocked_cards'])[:30]:
                manager.decks.toggle('Release check', card_id)
            manager.decks.rename('Release check', 'Verified deck')
            check('deck_workshop', manager.decks.validate(manager.decks.decks['Verified deck']))
            manager.decks.delete('Verified deck')
            for resolution in ([1280, 720], [1920, 1080]):
                manager.profile['settings']['resolution'] = resolution
                app.set_display()
                for route in routes:
                    app.navigate(route)
                    app.draw()
                check(f'resolution_{resolution[0]}', app.window.get_size() == tuple(resolution))
            check('save_write', manager.save(), manager.saves.last_error)
            loaded = GameManager(save_path)
            check('save_reload', loaded.profile == manager.profile and not loaded.can_resume)
            app.quit()
        report['status'] = 'passed'
    except Exception as error:
        report['error'] = f'{type(error).__name__}: {error}'
        report['traceback'] = traceback.format_exc()
    finally:
        if pygame is not None:
            pygame.quit()
    return report


def write_report(path, report):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix+'.tmp')
    temporary.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n', encoding='utf-8')
    temporary.replace(path)
