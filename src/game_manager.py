import logging
import copy
from src.battle.checkpoint import CheckpointCodec
from src.cards.card_database import CardDatabase
from src.deck.deck_manager import DeckManager
from src.progression.xp_system import Progression
from src.progression.achievement_system import AchievementSystem
from src.economy.pack_system import PackSystem
from src.systems.save_system import SaveSystem
from src.systems.resources import ROOT, read_json
from src.battle.battle_manager import BattleManager
from src.systems.data_validation import DataCatalog


class GameManager:
    def __init__(self, save_path=None):
        self.database = CardDatabase()
        self.catalog = DataCatalog(reader=read_json)
        self.catalog.check_cards(self.database)
        self.config = self.catalog.config
        self.rarities = self.catalog.rarities
        self.ability_data = self.catalog.abilities
        self.pack_data = self.catalog.packs
        self.data_issues = self.catalog.issues
        self.saves = SaveSystem(self.database, save_path)
        self.profile = self.saves.load()
        self.decks = DeckManager(self.profile, self.database, self.config.get('deck_size', 30))
        self.achievements = AchievementSystem(self.profile, self.catalog.achievements, self.database)
        self.progression = Progression(self.profile, self.config, self.database)
        self.packs = PackSystem(self.database, self.profile, self.pack_data, self.rarities,
                                duplicate_coins=self.config.get('duplicate_coins', 50))
        self.battle = None
        self.resume_warning = ''
        snapshot = self.profile.get('active_match')
        if snapshot is not None:
            try:
                self.battle = CheckpointCodec.decode(snapshot, self.database, self.config,
                                                     self.ability_data, self.progression)
                self.profile['active_match'] = CheckpointCodec.encode(self.battle)
            except (ValueError, TypeError, KeyError, OverflowError) as error:
                logging.warning('Discarding unusable match checkpoint: %s', error)
                self.resume_warning = 'Saved match unavailable; progression preserved. See game.log.'
                self.profile['active_match'] = None
                self.saves.save(self.profile)

    def claim_achievement(self, achievement_id):
        """Commit the claim marker and currency together before updating live objects.

        On write failure the reward remains unclaimed, without changing profile
        references used by the battle, progression, sound manager or deck editor.
        """
        rewards = self.achievements.reward_for_claim(achievement_id)
        candidate = copy.deepcopy(self.profile)
        candidate['coins'] += rewards['coins']
        candidate['gems'] += rewards['gems']
        candidate['statistics']['total_coins_earned'] += rewards['coins']
        candidate['claimed_achievements'].append(achievement_id)
        candidate['active_match'] = CheckpointCodec.encode(self.battle) if self.battle else None
        if not self.saves.save(candidate):
            raise ValueError('Reward not claimed: save failed. Check folder permissions and retry.')
        self.profile['coins'] = candidate['coins']
        self.profile['gems'] = candidate['gems']
        self.profile['statistics']['total_coins_earned'] = candidate['statistics']['total_coins_earned']
        self.profile['claimed_achievements'][:] = candidate['claimed_achievements']
        self.profile['active_match'] = candidate['active_match']
        return rewards

    @property
    def can_resume(self):
        return self.battle is not None and not self.battle.finished

    def resume(self):
        if not self.can_resume:
            raise ValueError('No unfinished match to continue.')
        return self.battle

    def discard_match(self):
        self.battle = None
        self.profile['active_match'] = None
        return self.save()

    def save(self):
        self.profile['active_match'] = CheckpointCodec.encode(self.battle) if self.battle else None
        return self.saves.save(self.profile)

    def start(self, mode):
        ids = self.profile['decks'][self.profile['selected_deck']]
        if not self.decks.validate(ids):
            raise ValueError('INVALID DECK: choose exactly 30 unique owned cards.')
        self.battle = BattleManager(self.database, ids, self.config, self.ability_data,
                                    self.progression, mode, self.profile['settings']['difficulty'])
        return self.battle
