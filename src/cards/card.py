from dataclasses import dataclass
from src.systems.resources import ATTRIBUTES


@dataclass(frozen=True)
class Card:
    id: int
    name: str
    description: str
    category: str
    rarity: str
    stats: dict
    ability: str
    image: str = ''
    sound: str = ''
    tags: tuple = ()
    level: int = 1
    xp: int = 0

    @classmethod
    def from_dict(cls, data):
        stats = {key: int(data[key]) for key in ATTRIBUTES}
        if any(not 1 <= value <= 100 for value in stats.values()):
            raise ValueError('Attributes must be between 1 and 100')
        if not data['name'] or not data['category'] or int(data['id']) < 1:
            raise ValueError('Invalid card identity')
        return cls(int(data['id']), str(data['name']), str(data['description']),
                   str(data['category']), str(data['rarity']), stats, str(data['ability']),
                   str(data.get('image', '')), str(data.get('sound', '')),
                   tuple(data.get('tags', [])), max(1, int(data.get('level', 1))),
                   max(0, int(data.get('xp', 0))))

    def values(self, progress=None, level_bonus=1):
        progress = progress or {}
        level = progress.get('level', self.level)
        upgrades = progress.get('upgrades', {})
        return {key: min(100, value + (level - 1) * level_bonus + upgrades.get(key, 0))
                for key, value in self.stats.items()}
