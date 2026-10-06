import pygame

BG = (12, 19, 28)
PANEL = (21, 32, 43)
EDGE = (45, 62, 74)
TEXT = (230, 232, 225)
MUTED = (139, 157, 165)
GOLD = (224, 186, 111)
TEAL = (94, 207, 183)
RED = (238, 126, 130)


class UI:
    def __init__(self, app):
        self.app = app
        self.buttons = []

    @property
    def surface(self):
        return self.app.canvas

    def text(self, text, x, y, size=24, color=TEXT, bold=False, center=False):
        image = self.app.assets.font(size, bold).render(self.app.locale.text(text), True, color)
        rect = image.get_rect(midtop=(x, y)) if center else image.get_rect(topleft=(x, y))
        self.surface.blit(image, rect)
        return rect

    def wrap(self, text, x, y, width, size=21, color=MUTED, line_height=24):
        font = self.app.assets.font(size)
        line = ''
        for word in text.split():
            trial = (line + ' ' + word).strip()
            if font.size(trial)[0] > width and line:
                self.text(line, x, y, size, color)
                y += line_height
                line = word
            else:
                line = trial
        if line:
            self.text(line, x, y, size, color)
        return y + line_height

    def panel(self, rect, color=PANEL, border=EDGE, radius=14):
        rect = pygame.Rect(rect)
        pygame.draw.rect(self.surface, color, rect, border_radius=radius)
        if border:
            pygame.draw.rect(self.surface, border, rect, 1, border_radius=radius)

    def button(self, label, rect, action, primary=False, active=False, enabled=True, size=23):
        rect = pygame.Rect(rect)
        hover = rect.collidepoint(self.app.mouse) and enabled
        fill = GOLD if primary else (35, 64, 68) if active else (33, 48, 59) if hover else PANEL
        border = GOLD if primary or active else TEAL if hover else EDGE
        self.panel(rect, fill, border, 9)
        if hover:
            pygame.draw.line(self.surface, TEAL if not primary else TEXT,
                             (rect.x+12, rect.bottom-3), (rect.right-12, rect.bottom-3), 2)
        self.text(label, rect.centerx, rect.y+(rect.h-size)//2, size,
                  BG if primary else TEXT if enabled else (81, 99, 109), center=True, bold=primary)
        if enabled:
            self.buttons.append((rect, action))

    def bar(self, rect, ratio, color=TEAL):
        rect = pygame.Rect(rect)
        pygame.draw.rect(self.surface, EDGE, rect, border_radius=3)
        if ratio > 0:
            pygame.draw.rect(self.surface, color, (rect.x, rect.y, max(1, int(rect.w*min(1, ratio))), rect.h), border_radius=3)

    def header(self, title, subtitle=''):
        self.button('<  BACK', (36, 28, 104, 36), self.app.home, size=19)
        self.text(title, 162, 27, 34, bold=True)
        if subtitle:
            self.text(subtitle, 164, 67, 20, MUTED)
        p = self.app.manager.profile
        self.text(f"{p['coins']:,} coins    /    {p['gems']:,} gems", 985, 37, 22, GOLD)

    def dispatch(self, pos):
        for rect, action in reversed(self.buttons):
            if rect.collidepoint(pos):
                self.app.sound.play('click')
                self.app.perform(action)
                return
