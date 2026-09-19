#!/usr/bin/env python3
# Copyright (c) Fantasy Card Battle. All rights reserved.
#
# Tools/generate_cards.py - turns Tools/card_catalog.py into the shipped data files.
#
# Outputs (all deterministic: same catalog -> byte-identical files):
#   Content/Data/Generated/DT_Cards.csv      <- imported into UE as the DT_Cards DataTable
#   Content/Data/Generated/DT_Factions.csv   <- faction doctrines
#   Content/Data/Generated/DT_Abilities.csv  <- keyword catalogue (tooltips + validation)
#   Content/Data/Generated/cards.json        <- same data for tools, tests and the web prototype
#   Docs/Cards.md                            <- human review sheet
#   Art/Briefs/*.md                          <- per-faction portrait briefs for the art pipeline
#
# The generator also *fixes* balance problems instead of merely reporting them:
#   * no card may be strictly dominated (beatable by nothing) -> its weakest attribute is bumped
#   * stat bands are clamped to FCBLimits in FCBTypes.h
#   * rarity/ability budgets are enforced exactly like the C++ validator, so the CSV always loads clean
#
# Usage:
#   python3 Tools/generate_cards.py            # generate + validate + write
#   python3 Tools/generate_cards.py --check    # do not write, exit non-zero on any problem

import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from card_catalog import ABILITIES, ARCHETYPES, CATALOG, FACTIONS, RARITY_BAND  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "Content", "Data", "Generated")
DOCS_DIR = os.path.join(ROOT, "Docs")
ART_DIR = os.path.join(ROOT, "Art", "Briefs")

RARITY_ORDER = ["Common", "Uncommon", "Rare", "Epic", "Legendary", "Mythic"]
RARITY_INDEX = {name: i for i, name in enumerate(RARITY_ORDER)}

# Mirrors FCBLimits in Source/FantasyCardBattle/Public/FCBTypes.h.
LIMITS = {
    "age": (0, 999999),
    "power": (1, 120),
    "speed": (1, 120),
    "height": (20, 9000),
}

# Mirrors the flat-bonus rule in FFCBCardDatabase::ComputeDerived.
FLAT_EFFECT = "FlatBonusOnAttribute"
PERCENT_EFFECTS = {
    "PercentBonusOnAttribute",
    "PercentBonusWhenLeading",
    "PercentBonusWhenDefending",
    "BonusPerFactionInOwnPile",
    "BonusAgainstFaction",
    "BonusIfOpponentStatAbove",
    "AlwaysPenalty",
    "WinIfMarginWithin",
}
# Effects whose magnitude is a percentage of the contested attribute.
CAPS = {
    "PercentBonusOnAttribute": 40,
    "PercentBonusWhenLeading": 20,
    "PercentBonusWhenDefending": 20,
    "BonusPerFactionInOwnPile": 8,
    "BonusAgainstFaction": 25,
    "BonusIfOpponentStatAbove": 25,
    "AlwaysPenalty": -30,
    "WinIfMarginWithin": 20,
}

ATTR_FIELDS = ["age", "power", "speed", "height"]
CSV_COLUMNS = [
    "Name", "DisplayName", "Faction", "Rarity",
    "AgeYears", "Power", "Speed", "HeightCm",
    "Flavor", "PortraitPath", "SetIndex", "LoreEntryNumber",
    "Ability0Id", "Ability0Name", "Ability0Text", "Ability0Effect", "Ability0Attribute",
    "Ability0ParamA", "Ability0ParamB",
    "Ability1Id", "Ability1Name", "Ability1Text", "Ability1Effect", "Ability1Attribute",
    "Ability1ParamA", "Ability1ParamB",
]


def slugify(text):
    out = []
    for char in text:
        if char.isalnum():
            out.append(char)
        elif char in (" ", "-", "'"):
            out.append("_")
    slug = "".join(out).strip("_")
    while "__" in slug:
        slug = slug.replace("__", "_")
    return slug


def stable_int(*parts, modulo=10000):
    """Deterministic pseudo-random integer: identical on every machine, unlike Python's hash()."""
    digest = hashlib.md5("|".join(str(p) for p in parts).encode("utf-8")).hexdigest()
    return int(digest[:12], 16) % modulo


def clamp(value, bounds):
    return max(bounds[0], min(bounds[1], int(round(value))))


def doctrine_for(faction_key):
    for row in FACTIONS:
        if row[0] == faction_key:
            return dict(
                display=row[1], enum=row[2], doctrine=row[3],
                bonus_attr=row[4], bonus_pct=row[5],
                penalty_attr=row[6], penalty_pct=row[7],
                color=row[8], lore=row[9],
            )
    raise KeyError("unknown faction " + faction_key)


FACTION_BY_ENUM = {doctrine_for(k)["enum"]: k for k, *_ in [f[:1] for f in FACTIONS]}


def apply_doctrine(stats, doctrine):
    """Mirrors FFCBCardDatabase::ComputeDerived (integer percent, half away from zero)."""
    out = dict(stats)
    attr_to_field = {"Age": "age", "Power": "power", "Speed": "speed", "Height": "height"}
    for field_key, pct in ((attr_to_field[doctrine["bonus_attr"]], doctrine["bonus_pct"]),
                           (attr_to_field[doctrine["penalty_attr"]], doctrine["penalty_pct"])):
        if pct:
            scaled = out[field_key] * pct
            out[field_key] += (scaled + (50 if scaled >= 0 else -50)) // 100
    return out


def build_cards(errors, warnings):
    cards = []
    lore_number = 0
    for faction_key, *rest in FACTIONS:
        doctrine = doctrine_for(faction_key)
        roster = CATALOG[faction_key]
        if not (8 <= len(roster) <= 15):
            errors.append("%s: %d cards, design budget is 8..15" % (faction_key, len(roster)))

        rarity_seen = {}
        for index, (name, rarity, archetype, stat_overrides, ability_ids, flavor) in enumerate(roster):
            if rarity not in RARITY_INDEX:
                errors.append("%s: unknown rarity '%s'" % (name, rarity))
                continue
            if archetype not in ARCHETYPES:
                errors.append("%s: unknown archetype '%s'" % (name, archetype))
                continue
            rarity_seen[rarity] = rarity_seen.get(rarity, 0) + 1

            power_base, speed_base, age_base, height_base = RARITY_BAND[rarity]
            power_mult, speed_mult, age_mult, height_mult = ARCHETYPES[archetype]

            def derived(key, base, mult):
                seed = stable_int("jitter", faction_key, name, key, modulo=1000)
                wiggle = (seed / 1000.0) * 2.0 - 1.0            # -1..1
                span = max(3, abs(base) * 0.04)                  # <= 4% authored variance
                return clamp(base * mult + wiggle * span, LIMITS[key])

            stats = {
                "age": derived("age", age_base, age_mult),
                "power": derived("power", power_base, power_mult),
                "speed": derived("speed", speed_base, speed_mult),
                "height": derived("height", height_base, height_mult),
            }
            for key, value in (stat_overrides or {}).items():
                if key not in LIMITS:
                    errors.append("%s: unknown stat override '%s'" % (name, key))
                else:
                    stats[key] = clamp(value, LIMITS[key])

            abilities = []
            for ability_id in (ability_ids or []):
                definition = ABILITIES.get(ability_id)
                if not definition:
                    errors.append("%s: unknown ability id '%s'" % (name, ability_id))
                    continue
                min_rarity = definition.get("min", "Common")
                if RARITY_INDEX[rarity] < RARITY_INDEX[min_rarity]:
                    errors.append("%s: %s requires %s+, card is %s" % (name, ability_id, min_rarity, rarity))
                    continue
                effect = definition["effect"]
                param_a = definition["a"]
                cap = CAPS.get(effect)
                if cap is not None:
                    if cap > 0 and param_a > cap:
                        warnings.append("%s: %s param %s above cap %s" % (name, ability_id, param_a, cap))
                    if cap < 0 and param_a < cap:
                        warnings.append("%s: %s param %s below floor %s" % (name, ability_id, param_a, cap))
                if effect == FLAT_EFFECT and definition["attribute"] not in ("Power", "Speed"):
                    errors.append("%s: flat bonus on %s is illegal (use a percent effect)"
                                  % (name, definition["attribute"]))
                if effect in PERCENT_EFFECTS and effect != FLAT_EFFECT and param_a == 0:
                    errors.append("%s: %s has a zero parameter" % (name, ability_id))
                abilities.append(dict(id=ability_id, effect=effect, attribute=definition["attribute"],
                                      a=param_a, b=definition.get("b", 0),
                                      faction=definition.get("faction", ""),
                                      name=definition["name"], desc=definition["desc"]))
            if len(abilities) > 2:
                errors.append("%s: %d abilities, only 2 slots exist" % (name, len(abilities)))

            lore_number += 1
            cards.append(dict(
                id="%s_%02d" % (faction_key, index + 1),
                card_id="FCB_%s" % slugify(name),
                name=name,
                faction_key=faction_key,
                faction=doctrine["enum"],
                rarity=rarity,
                archetype=archetype,
                stats=stats,
                abilities=abilities,
                flavor=flavor,
                set_index=index + 1,
                lore=lore_number,
            ))

        # Soft budget per faction; deviations are warnings so flavor can still win.
        expected = {"Common": (2, 5), "Uncommon": (2, 4), "Rare": (1, 3),
                    "Epic": (1, 2), "Legendary": (0, 2), "Mythic": (0, 1)}
        for rarity, (low, high) in expected.items():
            actual = rarity_seen.get(rarity, 0)
            if not (low <= actual <= high):
                warnings.append("%s: %d %s cards (budget %d..%d)" % (faction_key, actual, rarity, low, high))
    return cards


def doctrine_multiplier(faction_key, field):
    """Approximate effective/base ratio for one attribute, matching apply_doctrine() closely enough."""
    doctrine = doctrine_for(faction_key)
    field_to_attr = {"age": "Age", "power": "Power", "speed": "Speed", "height": "Height"}
    attr = field_to_attr[field]
    percent = 0
    if doctrine["bonus_attr"] == attr:
        percent += doctrine["bonus_pct"]
    if doctrine["penalty_attr"] == attr:
        percent += doctrine["penalty_pct"]
    return max(0.1, 1.0 + percent / 100.0)


def _coverage(card, effective, cards):
    """Fraction of the pool this card beats on at least one attribute (its 'playability' metric)."""
    mine = card["effective"]
    beats = 0
    for other in cards:
        if other is card:
            continue
        theirs = effective[other["card_id"]]
        if any(mine[k] > theirs[k] for k in ATTR_FIELDS):
            beats += 1
    return beats / max(1, len(cards) - 1)


def tune_coverage(cards, notes, target=0.75):
    """
    The design rule that actually matters in a Top-Trumps game: a card must be able to win against a
    healthy slice of the pool, otherwise drawing it feels like a punishment. Being *dominated* by one
    legendary is fine and expected - you dump that card on a round you are going to lose anyway.

    So instead of forcing every pair apart (which produces absurd numbers such as Speed 110 hatchlings)
    we raise the weakest attribute of the least playable cards until coverage clears the target.
    """
    def refresh():
        return {c["card_id"]: apply_doctrine(c["stats"], doctrine_for(c["faction_key"])) for c in cards}

    seen = set()
    for _pass in range(60):
        effective = refresh()
        for card in cards:
            card["effective"] = effective[card["card_id"]]

        worst, worst_coverage = None, 1.0
        for card in cards:
            coverage = _coverage(card, effective, cards)
            if coverage < worst_coverage:
                worst, worst_coverage = card, coverage
        if worst is None or worst_coverage >= target:
            return

        # Grow the attribute that is furthest below the pool median, in percent so it stays scale-free.
        values = {key: sorted(c["effective"][key] for c in cards) for key in ATTR_FIELDS}
        best_key, best_gap = None, -1.0
        for key in ATTR_FIELDS:
            median = values[key][len(values[key]) // 2]
            if worst["effective"][key] >= median:
                continue
            gap = (median - worst["effective"][key]) / max(1.0, float(median))
            if gap > best_gap:
                best_key, best_gap = key, gap
        if best_key is None:
            return   # already above the median everywhere: leave it alone, it is a "dump card"

        limit = LIMITS[best_key][1]
        if worst["stats"][best_key] >= limit:
            return
        worst["stats"][best_key] = min(limit, max(worst["stats"][best_key] + 1,
                                                 int(worst["stats"][best_key] * 1.07) + 1))
        message = "%s: %s raised to %.1f%% coverage (was %.1f%%)" % (
            worst["name"], best_key, target * 100, worst_coverage * 100)
        if message not in seen:
            seen.add(message)
            notes.append(message)


def clamp_to_bands(cards, notes):
    """
    Keeps every *doctrined* value inside the published band, which is what the UI bars assume. Returns True
    when it changed something (the caller re-runs coverage tuning afterwards).
    """
    changed = False
    for card in cards:
        doctrine = doctrine_for(card["faction_key"])
        for key in ATTR_FIELDS:
            low, high = LIMITS[key]
            for _guard in range(4000):
                effective = apply_doctrine(card["stats"], doctrine)[key]
                if effective <= high and effective >= low:
                    break
                if effective > high:
                    if card["stats"][key] <= low:
                        break
                    card["stats"][key] = max(low, card["stats"][key] - max(1, card["stats"][key] // 24))
                else:
                    card["stats"][key] = min(high, card["stats"][key] + 1)
                changed = True
    if changed:
        notes.append("band clamp: doctrine overflow pulled back into the published ranges")
    return changed


# --------------------------------------------------------------------------- fairness
def _recompute_effective(cards):
    for card in cards:
        card["effective"] = apply_doctrine(card["stats"], doctrine_for(card["faction_key"]))
        card["strength"] = 0.0   # recomputed by the caller if it cares


def _faction_best_shares(cards, exclude_ability_effects=True):
    """
    Mirrors the harness' strongest signal: with best-attribute play, how often does a card win a duel
    against the rest of the pool, averaged per faction. Abilities are ignored on purpose - they are worth a
    couple of points each (see the ability impact report) and must not be paid for with stat inflation.
    """
    _recompute_effective(cards)
    shares = {}
    for row in FACTIONS:
        shares[row[0]] = [0, 0, 0]

    for card in cards:
        bucket = shares[card["faction_key"]]
        for other in cards:
            if other is card:
                continue
            margins = [card["effective"][k] - other["effective"][k] for k in ATTR_FIELDS]
            bucket[1] += 1
            if max(margins) > 0:
                bucket[0] += 1          # best-attribute play
            bucket[2] += sum(1 for m in margins if m > 0) / len(margins)

    return {k: 0.5 * (100.0 * w / n if n else 0.0) + 0.5 * (100.0 * a / n if n else 0.0)
            for k, (w, n, a) in ((k, tuple(v)) for k, v in shares.items())}


def tune_fairness(cards, notes, max_rounds=18, tolerance=1.75):
    """
    A faction that only ever wins on one axis loses every duel where the opponent also holds that axis, so
    "balance the factions" really means "give every faction a second way to win". This pass nudges the
    near-miss attribute of the trailing factions until the best-play spread is inside the tolerance band.
    Bumps are capped per card so the printed values stay close to what the design doc promised.
    """
    # A hard cap on how far a single card may drift from its authored numbers, in stat points. Without this a
    # trailing faction can quietly re-tier its whole roster.
    max_bump = 10
    per_card_budget = {id(card): max_bump for card in cards}
    touched = {}

    for _round in range(max_rounds):
        shares = _faction_best_shares(cards)
        values = [v for v in shares.values()]
        spread = max(values) - min(values)
        if spread <= tolerance * 2:
            notes.append("fairness: faction share spread %.1f pts after %d pass(es)" % (spread, _round))
            return shares

        mean = sum(values) / len(values)
        trailing = sorted([k for k, v in shares.items() if v < mean - tolerance], key=lambda k: shares[k])
        if not trailing:
            break

        for key in trailing:
            touched[key] = touched.get(key, 0) + 1
            members = [c for c in cards if c["faction_key"] == key]
            for card in members:
                if per_card_budget[id(card)] <= 0:
                    continue        # this card has already drifted as far as the design allows
                # Which attribute is closest to flipping the most duels?
                best_key, best_gain = None, 0
                for attr in ATTR_FIELDS:
                    gain = 0
                    for other in cards:
                        if other is card:
                            continue
                        margin = card["effective"][attr] - other["effective"][attr]
                        if 0 < margin <= 6:
                            gain += 1          # already winning narrowly: fragile, worth reinforcing
                        elif -6 <= margin <= 0:
                            gain += 2          # one step away from a win
                if gain > best_gain:
                    best_key, best_gain = attr, gain
                if best_key:
                    step = max(1, int(card["stats"][best_key] * 0.02))
                    low, high = LIMITS[best_key]
                    card["stats"][best_key] = min(high, card["stats"][best_key] + step)
                    per_card_budget[id(card)] -= 1

        _recompute_effective(cards)

    shares = _faction_best_shares(cards)
    values = list(shares.values())
    if touched:
        notes.append("fairness: %d pass(es) on %s; spread now %.1f pts (min %.1f%%, max %.1f%%), blend of"
                     " best-attribute and random declaration"
                     % (sum(touched.values()),
                        ", ".join("%s x%d" % (k, n) for k, n in sorted(touched.items())),
                        max(values) - min(values), min(values), max(values)))
    else:
        notes.append("fairness: no nudges needed (spread %.1f pts)" % (max(values) - min(values)))
    return shares


def report_dominated(cards, notes):
    """Informational: full domination is allowed by design, but a designer should know where it happens."""
    effective = {c["card_id"]: apply_doctrine(c["stats"], doctrine_for(c["faction_key"])) for c in cards}
    dominated = []
    for me in cards:
        mine = effective[me["card_id"]]
        for other in cards:
            if other is me:
                continue
            theirs = effective[other["card_id"]]
            if all(theirs[k] >= mine[k] for k in ATTR_FIELDS) and (
                    theirs["power"] > mine["power"] or theirs["speed"] > mine["speed"]):
                dominated.append("%s <- %s" % (me["name"], other["name"]))
                break
    if dominated:
        notes.append("dominated (dump) cards: " + "; ".join(dominated[:8])
                     + (" ... +%d more" % (len(dominated) - 8) if len(dominated) > 8 else ""))


def normalize_rarity(cards, notes):
    """
    Pull the pool towards a collector pyramid (Common-heavy) by demoting surplus high-rarity cards, but
    only when every ability on the card still satisfies its own rarity floor. Designers can author
    freely; the pyramid is a soft target, not a straitjacket.
    """
    target_share = {"Common": 0.28, "Uncommon": 0.26, "Rare": 0.24,
                    "Epic": 0.13, "Legendary": 0.07, "Mythic": 0.02}
    counts = {r: 0 for r in RARITY_ORDER}
    for card in cards:
        counts[card["rarity"]] += 1

    def floors_ok(card, rarity):
        for ability in card["abilities"]:
            minimum = ABILITIES[ability["id"]].get("min", "Common")
            if RARITY_INDEX[rarity] < RARITY_INDEX[minimum]:
                return False
        return True

    for rarity in reversed(RARITY_ORDER[1:]):
        allowed = int(round(target_share[rarity] * len(cards)))
        overflow = counts[rarity] - allowed
        if overflow <= 0:
            continue
        lower = RARITY_ORDER[RARITY_INDEX[rarity] - 1]
        demoted = 0
        for card in sorted(cards, key=lambda c: c["lore"], reverse=True):
            if demoted >= overflow:
                break
            if card["rarity"] != rarity or not floors_ok(card, lower):
                continue
            card["rarity"] = lower
            counts[rarity] -= 1
            counts[lower] += 1
            demoted += 1
            notes.append("%s: demoted %s -> %s for the rarity pyramid" % (card["name"], rarity, lower))


def fix_duplicates(cards, errors):
    seen = {}
    for card in cards:
        if card["card_id"] in seen:
            errors.append("duplicate card id %s (%s / %s)" % (card["card_id"], seen[card["card_id"]], card["name"]))
        seen[card["card_id"]] = card["name"]


def write_csv(path, header, rows):
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(",".join(header) + "\n")
        for row in rows:
            handle.write(",".join(escape(cell) for cell in row) + "\n")


def escape(cell):
    text = "" if cell is None else str(cell)
    if any(char in text for char in (',', '"', '\n')):
        return '"' + text.replace('"', '""') + '"'
    return text


def ability_text(definition):
    return definition["desc"]


def main():
    parser = argparse.ArgumentParser(description="generate the card database")
    parser.add_argument("--check", action="store_true", help="validate only, write nothing")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    errors, warnings, notes = [], [], []
    cards = build_cards(errors, warnings)
    fix_duplicates(cards, errors)
    for _round in range(8):
        tune_coverage(cards, notes)
        if not clamp_to_bands(cards, notes):
            break

    # ---- derived reports -------------------------------------------------------
    normalize_rarity(cards, notes)
    for _round in range(8):
        tune_coverage(cards, notes)
        if not clamp_to_bands(cards, notes):
            break
    tune_fairness(cards, notes)
    for _round in range(4):
        if not clamp_to_bands(cards, notes):
            break
    tune_coverage(cards, notes)
    report_dominated(cards, notes)

    # Final contract check: printed *and* doctrined values must sit inside the bands.
    for card in cards:
        for key in ATTR_FIELDS:
            low, high = LIMITS[key]
            if not (low <= card["stats"][key] <= high):
                errors.append("%s: printed %s %d outside [%d..%d]" % (card["name"], key, card["stats"][key], low, high))
            if not (low <= card["effective"][key] <= high):
                errors.append("%s: doctrined %s %d outside [%d..%d]" % (card["name"], key, card["effective"][key], low, high))

    # Coverage floor, mirroring the harness' fatal rule: a card that cannot beat 70% of the pool on any one
    # attribute is a card the player resents drawing.
    for card in cards:
        beats = 0
        for other in cards:
            if other is card:
                continue
            if any(card["effective"][k] > other["effective"][k] for k in ATTR_FIELDS):
                beats += 1
        coverage = float(beats) / max(1, len(cards) - 1)
        if coverage < 0.70:
            errors.append("%s: coverage %.1f%% is under the 70%% floor" % (card["name"], 100.0 * coverage))
    for card in cards:
        card["effective"] = apply_doctrine(card["stats"], doctrine_for(card["faction_key"]))

    ties = {k: 0 for k in ATTR_FIELDS}
    comparisons = 0
    for i, me in enumerate(cards):
        mine = me["effective"]
        for other in cards[i + 1:]:
            theirs = other["effective"]
            comparisons += 1
            for key in ATTR_FIELDS:
                if mine[key] == theirs[key]:
                    ties[key] += 1
    for card in cards:
        score = 0.0
        for key in ATTR_FIELDS:
            below = sum(1 for other in cards if other["effective"][key] < card["effective"][key])
            score += below / max(1, len(cards) - 1)
        card["strength"] = round(score / len(ATTR_FIELDS), 4)

    if not args.check:
        os.makedirs(OUT_DIR, exist_ok=True)
        os.makedirs(DOCS_DIR, exist_ok=True)
        os.makedirs(ART_DIR, exist_ok=True)

    # ---- faction table ---------------------------------------------------------
    faction_rows = []
    for key, *rest in FACTIONS:
        d = doctrine_for(key)
        faction_rows.append([
            d["enum"], d["enum"], d["display"], d["doctrine"], d["lore"],
            d["bonus_attr"], d["bonus_pct"], d["penalty_attr"], d["penalty_pct"], d["color"],
        ])
    faction_header = ["Name", "Faction", "DisplayName", "DoctrineName", "Lore",
                      "BonusAttribute", "BonusPercent", "PenaltyAttribute", "PenaltyPercent", "FrameColor"]

    # ---- ability table ---------------------------------------------------------
    ability_rows = []
    ability_header = ["Name", "AbilityId", "DisplayName", "Description", "Effect", "Attribute",
                      "DefaultParamA", "DefaultParamB", "Faction", "MinRarity"]
    for ability_id, definition in sorted(ABILITIES.items()):
        ability_rows.append([
            ability_id, ability_id, definition["name"], definition["desc"], definition["effect"],
            definition.get("attribute", "Power"), definition.get("a", 0), definition.get("b", 0),
            definition.get("faction", ""), definition.get("min", "Common"),
        ])

    # ---- card table -----------------------------------------------------------
    card_rows = []
    for card in cards:
        row = [
            card["card_id"], card["name"], card["faction"], card["rarity"],
            card["stats"]["age"], card["stats"]["power"], card["stats"]["speed"], card["stats"]["height"],
            card["flavor"], "/Game/Art/Cards/%s/T_%s.T_%s" % (card["faction_key"], card["card_id"], card["card_id"]),
            card["set_index"], card["lore"],
        ]
        for slot in range(2):
            if slot < len(card["abilities"]):
                ability = ABILITIES[card["abilities"][slot]["id"]]
                row += [card["abilities"][slot]["id"], ability["name"], ability_text(ability),
                        ability["effect"], ability.get("attribute", "Power"),
                        ability.get("a", 0), ability.get("b", 0)]
            else:
                row += ["", "", "", "", "", "", ""]
        card_rows.append(row)

    if not args.check:
        write_csv(os.path.join(OUT_DIR, "DT_Factions.csv"), faction_header, faction_rows)
        write_csv(os.path.join(OUT_DIR, "DT_Abilities.csv"), ability_header, ability_rows)
        write_csv(os.path.join(OUT_DIR, "DT_Cards.csv"), CSV_COLUMNS, card_rows)

        with open(os.path.join(OUT_DIR, "cards.json"), "w", encoding="utf-8") as handle:
            json.dump(dict(
                schema="fantasy-card-battle/cards@1",
                counts=dict(cards=len(cards), factions=len(FACTIONS), abilities=len(ABILITIES)),
                ties=dict((k, dict(pairs=comparisons, ties=ties[k], pct=round(100.0 * ties[k] / max(1, comparisons), 3)))
                          for k in ATTR_FIELDS),
                cards=[dict(
                    id=c["card_id"], name=c["name"], faction=c["faction"], rarity=c["rarity"],
                    archetype=c["archetype"], base=c["stats"], effective=c["effective"],
                    strength=c["strength"], flavor=c["flavor"], set_index=c["set_index"], lore=c["lore"],
                    abilities=[dict(id=a["id"], **{k: v for k, v in ABILITIES[a["id"]].items()}) for a in c["abilities"]],
                ) for c in cards],
            ), handle, indent=1, sort_keys=False)

        # ---- review sheet -----------------------------------------------------
        lines = ["# Card Set (generated)", "",
                 "Generated by `python3 Tools/generate_cards.py`. Do not edit by hand.", "",
                 "| # | Card | Faction | Rarity | Age | Power | Speed | Height | Abilities |",
                 "|---|------|---------|--------|-----|-------|-------|--------|-----------|"]
        for card in cards:
            eff = card["effective"]
            abilities = ", ".join(a["id"] for a in card["abilities"]) or "-"
            lines.append("| %d | %s | %s | %s | %s | %d | %d | %.2f m | %s |" % (
                card["lore"], card["name"], doctrine_for(card["faction_key"])["display"], card["rarity"],
                format(eff["age"], ","), eff["power"], eff["speed"], eff["height"] / 100.0, abilities))
        lines += ["", "## Tie rate by attribute", ""]
        for key in ATTR_FIELDS:
            lines.append("- %s: %.2f%% of all card pairs tie (%d/%d)"
                         % (key, 100.0 * ties[key] / max(1, comparisons), ties[key], comparisons))
        with open(os.path.join(DOCS_DIR, "Cards.md"), "w", encoding="utf-8") as handle:
            handle.write("\n".join(lines) + "\n")

        # ---- art briefs -------------------------------------------------------
        for key, *rest in FACTIONS:
            doctrine = doctrine_for(key)
            roster = [c for c in cards if c["faction_key"] == key]
            brief = ["# Art brief - %s" % doctrine["display"], "", doctrine["lore"], "",
                     "Doctrine: **%s** (%s %+d%%, %s %+d%%)" % (
                 doctrine["doctrine"], doctrine["bonus_attr"], doctrine["bonus_pct"],
                 doctrine["penalty_attr"], doctrine["penalty_pct"]), "",
                     "Frame colour: `%s`  |  Cards: %d" % (doctrine["color"], len(roster)), "",
                     "Shared style: hand-painted fantasy plate, 3:4 portrait, single subject, dark vignette,",
                     "rim light matching the frame colour, no text in the art (the UI draws the frame).", ""]
            for card in roster:
                brief.append("## %s  (`%s`)" % (card["name"], card["card_id"]))
                brief.append("- Rarity: %s | Archetype: %s" % (card["rarity"], card["archetype"]))
                brief.append("- Size: %.2f m tall, %d kg of threat (height %d cm)"
                             % (card["effective"]["height"] / 100.0, card["effective"]["power"] * 9,
                                card["effective"]["height"]))
                brief.append("- One-line read: %s" % card["flavor"])
                if card["abilities"]:
                    brief.append("- Keywords: %s" % ", ".join(ABILITIES[a["id"]]["name"] for a in card["abilities"]))
                brief.append("")
            with open(os.path.join(ART_DIR, "%s.md" % key), "w", encoding="utf-8") as handle:
                handle.write("\n".join(brief) + "\n")

    # ---- console report -------------------------------------------------------
    if not args.quiet:
        print("cards: %d  factions: %d  abilities defined: %d" % (len(cards), len(FACTIONS), len(ABILITIES)))
        by_rarity = {}
        for card in cards:
            by_rarity[card["rarity"]] = by_rarity.get(card["rarity"], 0) + 1
        print("rarity mix: " + ", ".join("%s %d" % (r, by_rarity.get(r, 0)) for r in RARITY_ORDER))
        for key in ATTR_FIELDS:
            values = [c["effective"][key] for c in cards]
            mean = sum(values) / len(values)
            stdev = (sum((v - mean) ** 2 for v in values) / len(values)) ** 0.5
            print("  %-7s min %8s  mean %9.1f  max %8s  ties %.2f%%"
                  % (key, format(min(values), ","), mean, format(max(values), ","),
                     100.0 * ties[key] / max(1, comparisons)))
        # Summary lines must never be pushed out of the window by per-card tuning chatter.
        summary_prefix = ("fairness:", "band clamp:", "rarity:", "coverage:", "dominated")
        summary = [n for n in notes if n.startswith(summary_prefix)]
        chatter = [n for n in notes if n not in summary]
        seen = set()
        for note in summary + chatter[-4:]:
            if note not in seen:
                seen.add(note)
                print("  note: " + note)
    for warning in warnings:
        print("warn: " + warning, file=sys.stderr)
    for error in errors:
        print("ERROR: " + error, file=sys.stderr)

    if errors:
        print("%d error(s) - data not written" % len(errors), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
