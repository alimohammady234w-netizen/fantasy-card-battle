"""Offline procedural illustrations are a deliberate asset fallback, not missing UI."""
import math
import random
import pygame
from src.ui.widgets import BG, PANEL, TEXT, MUTED, GOLD, TEAL, EDGE


class CardView:
    def __init__(self, app):
        self.app = app
        self.art_cache = {}

    def art(self, card, width, height, color):
        key = card.id, width, height
        if key in self.art_cache:
            return self.art_cache[key]
        image = self.app.assets.image(card.image)
        if image:
            result = pygame.transform.smoothscale(image, (width, height))
        else:
            result = pygame.Surface((width, height))
            rng = random.Random(card.id * 701)
            for y in range(height):
                mix = y / max(1, height)
                shade = tuple(int(v * (.22 - .12*mix)) + 8 for v in color)
                pygame.draw.line(result, shade, (0, y), (width, y))
            cx, cy = width//2, height//2
            for _ in range(36):
                x, y = rng.randrange(width), rng.randrange(height)
                pygame.draw.circle(result, tuple(int(v*.65) for v in color), (x, y), rng.choice([1, 1, 2]))
            radius = int(min(width, height)*.37)
            for extra in (0, 7, 20):
                pygame.draw.circle(result, tuple(int(v*.45) for v in color), (cx, cy), radius+extra, 1)
            # A unique faceted sigil for each champion, with layered landscape silhouettes.
            for layer in range(3):
                points = [(0, height)] + [(int(width*i/7), int(height*(.75+layer*.07))-rng.randrange(max(2, height//5))) for i in range(8)] + [(width, height)]
                pygame.draw.polygon(result, (14+layer*3, 23+layer*4, 31+layer*5), points)
            sides = 3 + card.id % 5
            points = [(cx+math.cos(i*math.tau/sides-math.pi/2)*radius,
                       cy+math.sin(i*math.tau/sides-math.pi/2)*radius) for i in range(sides)]
            pygame.draw.polygon(result, tuple(int(v*.27) for v in color), points)
            pygame.draw.polygon(result, color, points, 2)
            for point in points:
                pygame.draw.line(result, tuple(int(v*.7) for v in color), point, (cx, cy), 1)
                pygame.draw.circle(result, GOLD, point, 3)
            inner = [(cx, cy-radius*.63), (cx+radius*.3, cy), (cx, cy+radius*.63), (cx-radius*.3, cy)]
            pygame.draw.polygon(result, color, inner)
            pygame.draw.polygon(result, GOLD, inner, 2)
        self.art_cache[key] = result
        return result

    def draw(self, card, rect, hidden=False, compact=False, selected=False, highlight=None, outcome=None, progress=True, values_override=None):
        ui, surface = self.app.ui, self.app.canvas
        rect = pygame.Rect(rect)
        rarity = self.app.manager.rarities.get(card.rarity, {'color': GOLD, 'border': 2, 'glow': 15})
        color = tuple(rarity['color'])
        state = self.app.manager.profile['card_progress'].get(str(card.id), {}) if progress else {}
        if hidden:
            # A uniform back must not leak the hidden card's rarity through its border.
            color = (69, 117, 129)
            rarity = {'color': color, 'border': 2, 'glow': 15}
        glow = rarity.get('glow', 15) / 55
        if selected or outcome == 'win' or state.get('level', 1) >= 5:
            for spread in range(8, 0, -2):
                shade = tuple(int(v * (.12 + .08 * glow)) for v in color)
                pygame.draw.rect(surface, shade, rect.inflate(spread*2, spread*2), 2, border_radius=15)
        ui.panel(rect, (19, 29, 40), TEAL if selected else color, 13)
        pygame.draw.rect(surface, color, rect.inflate(-8, -8), min(5, rarity.get('border', 2)), border_radius=9)
        if hidden:
            center = rect.center
            for radius in (32, 47, 66):
                pygame.draw.circle(surface, color, center, radius, 1)
            diamond = [(center[0], center[1]-60), (center[0]+45, center[1]),
                       (center[0], center[1]+60), (center[0]-45, center[1])]
            pygame.draw.polygon(surface, color, diamond, 2)
            ui.text('?', center[0], center[1]-25, 62, GOLD, center=True)
            ui.text('UNREVEALED' if not compact else 'LOCKED', center[0], rect.bottom-42, 19, MUTED, center=True)
            return
        if compact:
            art_rect = pygame.Rect(rect.x+10, rect.y+10, rect.w-20, rect.h-70)
            surface.blit(self.art(card, art_rect.w, art_rect.h, color), art_rect)
            name = card.name if len(card.name) < 22 else card.name[:20]+'…'
            ui.text(name, rect.x+13, rect.bottom-52, 20, TEXT, True)
            ui.text(f"{card.rarity.upper()}  /  LV {state.get('level', card.level)}", rect.x+13, rect.bottom-28, 16, color)
            return
        ui.text(card.rarity.upper(), rect.x+19, rect.y+16, 17, color, True)
        ui.text(f"LV {state.get('level', card.level):02}", rect.right-62, rect.y+16, 17, GOLD)
        art_rect = pygame.Rect(rect.x+14, rect.y+43, rect.w-28, 155)
        surface.blit(self.art(card, art_rect.w, art_rect.h, color), art_rect)
        ui.text(card.name, rect.x+18, rect.y+210, 26, TEXT, True)
        ui.text(card.category.upper(), rect.x+18, rect.y+239, 17, MUTED)
        values = values_override if values_override is not None else card.values(state, self.app.manager.config.get('level_stat_bonus', 1))
        for i, (attribute, value) in enumerate(values.items()):
            x = rect.x+18 + (i % 2)*((rect.w-30)//2)
            y = rect.y+274+(i//2)*28
            if attribute == highlight:
                ui.panel((x-5, y-4, (rect.w-34)//2, 26), (37, 63, 63), None, 4)
            label = attribute[:5].upper()
            ui.text(label, x, y, 16, MUTED)
            ui.text(value, x+(rect.w-35)//2-32, y-1, 21, color if attribute==highlight else TEXT, True)
        pygame.draw.line(surface, EDGE, (rect.x+18, rect.y+392), (rect.right-18, rect.y+392))
        ui.text(card.ability.upper(), rect.x+18, rect.y+405, 20, color, True)
        if outcome == 'lose':
            veil = pygame.Surface(rect.size, pygame.SRCALPHA)
            veil.fill((8, 12, 22, 110))
            surface.blit(veil, rect)
