from src.systems.resources import ROOT, read_json


class Localization:
    """English source strings are stable fallback keys for future translation catalogs."""
    def __init__(self, locale='en'):
        self.messages = read_json(ROOT / 'data/locales' / f'{locale}.json', {})
        if not isinstance(self.messages, dict):
            self.messages = {}

    def text(self, source):
        return self.messages.get(str(source), str(source))
