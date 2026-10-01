"""
Fantasy Card Battle - Stage 2 sample card generator.

Creates 10 sample UCardDataAsset assets under /Game/Data/Cards, proving the
data-driven card database architecture works end-to-end.

HOW TO RUN (inside the Unreal Editor):
  1. Tools -> Execute Python Script...  -> select this file, OR
  2. Console command:  py "<project>/Tools/Python/create_sample_cards.py"

Requirements:
  - Python Editor Script Plugin + Editor Scripting Utilities
    (already enabled in FantasyCardBattle.uproject for the Editor target).
  - The C++ project must be compiled first (so UCardDataAsset exists).

Notes:
  - Idempotent: re-running updates the existing sample cards in place.
  - Card art/sounds/animations are intentionally NOT assigned here - those
    assets do not exist yet (Stage 2 ships data only, soft refs stay empty).
  - Tag assignment is best-effort: if a tag cannot be created from Python,
    the card is still created and a warning is printed.
"""

import re
import traceback

import unreal

TARGET_FOLDER = "/Game/Data/Cards"

# Numeric values of the C++ enums (must match CardTypes.h declaration order).
RARITY_INDEX = {
    "Common": 0, "Uncommon": 1, "Rare": 2, "Epic": 3, "Legendary": 4, "Mythic": 5,
}
CATEGORY_INDEX = {
    "Animals": 0, "Dinosaurs": 1, "Robots": 2,
    "AncientEgypt": 3, "AncientPersia": 4, "AncientGreece": 5, "AncientRome": 6,
    "Heroes": 7, "Villains": 8, "Fantasy": 9, "Mythology": 10,
    "SciFi": 11, "Space": 12, "Monsters": 13, "Magic": 14,
}
ABILITY_INDEX = {
    "None": 0, "Shield": 1, "Heal": 2, "Freeze": 3, "Poison": 4, "Curse": 5,
    "CriticalStrike": 6, "Mirror": 7, "Copy": 8, "DoubleAttack": 9, "Counter": 10,
    "Revive": 11, "Boost": 12, "Silence": 13, "Dodge": 14, "Rage": 15,
}

# ---------------------------------------------------------------------------
# Sample card data (test values only - easy to modify later).
# Lion / T-Rex / Dragon values come straight from the stage specification.
# ---------------------------------------------------------------------------
SAMPLE_CARDS = [
    {
        "id": "CARD_ANIMAL_LION_001",
        "name": "Lion",
        "description": "King of the savanna. A balanced predator with fierce stamina.",
        "category": "Animals",
        "rarity": "Rare",
        "stats": {"Power": 82, "Speed": 88, "Height": 12, "Defense": 70,
                  "Intelligence": 55, "Stamina": 85, "Luck": 60, "Age": 8},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Boost",
        "ability_value": 8.0,
        "ability_description": "Increases the selected stat by 8%.",
        "tags": ["Card.Element.Earth"],
    },
    {
        "id": "CARD_ANIMAL_EAGLE_001",
        "name": "Eagle",
        "description": "Soars above the battlefield and strikes without warning.",
        "category": "Animals",
        "rarity": "Uncommon",
        "stats": {"Power": 65, "Speed": 96, "Height": 3, "Defense": 50,
                  "Intelligence": 60, "Stamina": 70, "Luck": 75, "Age": 12},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Dodge",
        "ability_value": 15.0,
        "ability_description": "15% chance to dodge the incoming attack.",
        "tags": ["Card.Element.Air"],
    },
    {
        "id": "CARD_DINOSAUR_TREX_001",
        "name": "T-Rex",
        "description": "The tyrant king. Overwhelming power and raw defense.",
        "category": "Dinosaurs",
        "rarity": "Epic",
        "stats": {"Power": 97, "Speed": 72, "Height": 20, "Defense": 92,
                  "Intelligence": 45, "Stamina": 90, "Luck": 50, "Age": 6},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "CriticalStrike",
        "ability_value": 25.0,
        "ability_description": "25% chance to deal a critical strike.",
        "tags": ["Card.Role.Monster"],
    },
    {
        "id": "CARD_FANTASY_DRAGON_001",
        "name": "Dragon",
        "description": "An ancient wyrm of fire and legend. Elite in every stat.",
        "category": "Fantasy",
        "rarity": "Legendary",
        "stats": {"Power": 98, "Speed": 90, "Height": 30, "Defense": 95,
                  "Intelligence": 92, "Stamina": 96, "Luck": 85, "Age": 500},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "DoubleAttack",
        "ability_value": 20.0,
        "ability_description": "20% chance to attack twice in a round.",
        "tags": ["Card.Element.Fire", "Card.Role.LegendaryCreature"],
    },
    {
        "id": "CARD_ROBOT_001",
        "name": "Robot",
        "description": "A war machine built to endure. High defense and stamina.",
        "category": "Robots",
        "rarity": "Rare",
        "stats": {"Power": 75, "Speed": 60, "Height": 8, "Defense": 88,
                  "Intelligence": 80, "Stamina": 95, "Luck": 30, "Age": 5},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Shield",
        "ability_value": 25.0,
        "ability_description": "Absorbs the first 25 points of incoming damage.",
        "tags": ["Card.Trait.Mechanical"],
    },
    {
        "id": "CARD_PERSIA_WARRIOR_001",
        "name": "Persian Warrior",
        "description": "Veteran of the Immortals. Turns defense into offense.",
        "category": "AncientPersia",
        "rarity": "Rare",
        "stats": {"Power": 85, "Speed": 78, "Height": 10, "Defense": 75,
                  "Intelligence": 70, "Stamina": 82, "Luck": 55, "Age": 30},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Counter",
        "ability_value": 15.0,
        "ability_description": "Counters 15% of the damage received.",
        "tags": ["Card.Trait.Ancient", "Card.Role.Warrior"],
    },
    {
        "id": "CARD_EGYPT_PHARAOH_001",
        "name": "Egyptian Pharaoh",
        "description": "Ruler of eternity, blessed by the old gods.",
        "category": "AncientEgypt",
        "rarity": "Epic",
        "stats": {"Power": 70, "Speed": 55, "Height": 9, "Defense": 65,
                  "Intelligence": 95, "Stamina": 60, "Luck": 80, "Age": 40},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Curse",
        "ability_value": 10.0,
        "ability_description": "Curses the opponent, reducing their Power by 10%.",
        "tags": ["Card.Trait.Ancient"],
    },
    {
        "id": "CARD_GREECE_HERO_001",
        "name": "Greek Hero",
        "description": "Champion of the old sagas, favored by the gods.",
        "category": "AncientGreece",
        "rarity": "Epic",
        "stats": {"Power": 88, "Speed": 85, "Height": 11, "Defense": 72,
                  "Intelligence": 84, "Stamina": 80, "Luck": 65, "Age": 28},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Heal",
        "ability_value": 15.0,
        "ability_description": "Restores 15% of Stamina after a round.",
        "tags": ["Card.Trait.Ancient", "Card.Role.Warrior"],
    },
    {
        "id": "CARD_SPACE_MONSTER_001",
        "name": "Space Monster",
        "description": "A void-born leviathan. Its poison eats through armor.",
        "category": "Space",
        "rarity": "Legendary",
        "stats": {"Power": 94, "Speed": 76, "Height": 25, "Defense": 85,
                  "Intelligence": 66, "Stamina": 88, "Luck": 45, "Age": 120},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Poison",
        "ability_value": 12.0,
        "ability_description": "Poisons the opponent for 12 damage each round.",
        "tags": ["Card.Role.Monster"],
    },
    {
        "id": "CARD_FANTASY_GOLEM_001",
        "name": "Magic Golem",
        "description": "Animated by ancient runes. Reflects what it endures.",
        "category": "Magic",
        "rarity": "Rare",
        "stats": {"Power": 80, "Speed": 45, "Height": 15, "Defense": 96,
                  "Intelligence": 50, "Stamina": 90, "Luck": 40, "Age": 200},
        "base_level": 1,
        "base_xp": 0,
        "ability_type": "Mirror",
        "ability_value": 20.0,
        "ability_description": "Reflects 20% of received damage back to the attacker.",
        "tags": ["Card.Trait.Magical"],
    },
]


# ---------------------------------------------------------------------------
# Property / enum / struct helpers
#
# Unreal Python exposes C++ names as lower snake_case, but the exact
# conversion of consecutive capitals (e.g. "CardID") is not guaranteed.
# We therefore discover the python-visible name at runtime by normalizing
# both sides (strip non-alphanumerics, lowercase).
# ---------------------------------------------------------------------------

def _normalize(name):
    return re.sub(r"[^a-z0-9]", "", name.lower())


def _to_upper_snake(name):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()


def _resolve_prop(obj, cpp_name):
    """Return the python-visible attribute name for a C++ property name."""
    target = _normalize(cpp_name)
    candidates = [a for a in dir(obj)
                  if not a.startswith("_") and _normalize(a) == target]
    if not candidates:
        preview = [a for a in dir(obj) if not a.startswith("_")][:60]
        raise AttributeError(
            "Cannot find property '{}' on {}. Available: {}".format(
                cpp_name, type(obj).__name__, preview))
    # Prefer exact match, then the most likely property name.
    candidates.sort(key=lambda a: (a != cpp_name, len(a), a))
    return candidates[0]


def setp(obj, cpp_name, value):
    obj.set_editor_property(_resolve_prop(obj, cpp_name), value)


def getp(obj, cpp_name):
    return obj.get_editor_property(_resolve_prop(obj, cpp_name))


def _enum_value_ok(current, member_upper, fallback_index):
    enum_type = type(current)
    expected = getattr(enum_type, member_upper, None)
    if expected is not None:
        return current == expected
    try:
        return int(current) == fallback_index
    except Exception:
        return False


def set_enum(asset, cpp_name, enum_cpp_type, member_cpp_name, fallback_index):
    """Set an enum-class property with several strategies + verification."""
    prop = _resolve_prop(asset, cpp_name)
    member_upper = _to_upper_snake(member_cpp_name)

    # Strategy A: discover the enum type from the value currently in the property.
    try:
        enum_type = type(asset.get_editor_property(prop))
        value = getattr(enum_type, member_upper, None)
        if value is not None:
            asset.set_editor_property(prop, value)
            if _enum_value_ok(asset.get_editor_property(prop), member_upper, fallback_index):
                return
    except Exception:
        pass

    # Strategy B: the enum class exposed on the unreal module (E-prefix / without).
    for type_name in ("E" + enum_cpp_type, enum_cpp_type):
        try:
            enum_type = getattr(unreal, type_name, None)
            if enum_type is None:
                continue
            value = getattr(enum_type, member_upper, None)
            if value is None:
                continue
            asset.set_editor_property(prop, value)
            if _enum_value_ok(asset.get_editor_property(prop), member_upper, fallback_index):
                return
        except Exception:
            continue

    # Strategy C: raw numeric index (declaration order in CardTypes.h).
    try:
        asset.set_editor_property(prop, fallback_index)
        current = asset.get_editor_property(prop)
        try:
            if int(current) == fallback_index:
                return
        except Exception:
            if getattr(type(current), member_upper, None) == current:
                return
    except Exception:
        pass

    raise ValueError("Failed to set enum {} = {} (fallback index {})".format(
        cpp_name, member_cpp_name, fallback_index))


def set_stats(asset, stats_values):
    """Copy a complete FCardStats struct onto the asset and verify it."""
    prop = _resolve_prop(asset, "Stats")
    stats = asset.get_editor_property(prop)

    for cpp_name, value in stats_values.items():
        member = _resolve_prop(stats, cpp_name)
        try:
            setattr(stats, member, value)           # UScriptStruct member descriptor
        except Exception:
            stats.set_editor_property(member, value)  # fallback

    asset.set_editor_property(prop, stats)

    # Verify
    back = asset.get_editor_property(prop)
    problems = []
    for cpp_name, value in stats_values.items():
        member = _resolve_prop(back, cpp_name)
        got = getattr(back, member, None)
        if got is None:
            try:
                got = back.get_editor_property(member)
            except Exception:
                got = "<unreadable>"
        if got != value:
            problems.append("{}: expected {}, got {}".format(cpp_name, value, got))
    if problems:
        raise RuntimeError("Stats verification failed -> " + "; ".join(problems))


def set_tags(asset, tag_names):
    """Best-effort gameplay tag assignment (never fails the whole card)."""
    if not tag_names:
        return
    try:
        prop = _resolve_prop(asset, "CardTags")
        container = asset.get_editor_property(prop)
        for tag_name in tag_names:
            added = False
            try:
                tag = unreal.GameplayTag.request_gameplay_tag(tag_name, False)
                container.add_tag(tag)
                added = True
            except Exception:
                pass
            if not added:
                try:
                    container.add_tag(tag_name)
                    added = True
                except Exception:
                    pass
            if not added:
                print("  [warn] could not add tag: {}".format(tag_name))
        asset.set_editor_property(prop, container)
    except Exception as exc:
        print("  [warn] tag assignment skipped: {}".format(exc))


def get_card_data_asset_class():
    cls = getattr(unreal, "CardDataAsset", None)
    if cls is None:
        cls = unreal.load_class(None, "/Script/CardGame.CardDataAsset")
    if cls is None:
        raise RuntimeError("UCardDataAsset class not found - compile the C++ project first.")
    return cls


def create_or_load_asset(card_id):
    path = "{}/{}".format(TARGET_FOLDER, card_id)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = asset_tools.create_asset(
        card_id, TARGET_FOLDER, get_card_data_asset_class(), unreal.DataAssetFactory())
    if asset is None:
        raise RuntimeError("create_asset returned None for {}".format(card_id))
    return asset


def save_asset(asset):
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    except TypeError:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)


def populate_asset(asset, data):
    setp(asset, "CardID", data["id"])
    setp(asset, "Name", data["name"])
    setp(asset, "Description", data["description"])
    set_enum(asset, "Category", "ECardCategory",
             data["category"], CATEGORY_INDEX[data["category"]])
    set_enum(asset, "Rarity", "ECardRarity",
             data["rarity"], RARITY_INDEX[data["rarity"]])
    set_stats(asset, data["stats"])
    setp(asset, "BaseLevel", int(data.get("base_level", 1)))
    setp(asset, "BaseXP", int(data.get("base_xp", 0)))
    set_enum(asset, "AbilityType", "ECardAbilityType",
             data["ability_type"], ABILITY_INDEX[data["ability_type"]])
    setp(asset, "AbilityValue", float(data.get("ability_value", 0.0)))
    setp(asset, "AbilityDescription", data.get("ability_description", ""))
    set_tags(asset, data.get("tags", []))


def main():
    print("=" * 70)
    print("Fantasy Card Battle - creating {} sample cards in {}".format(
        len(SAMPLE_CARDS), TARGET_FOLDER))
    print("=" * 70)

    created = []
    updated = []
    failed = []

    for data in SAMPLE_CARDS:
        card_id = data["id"]
        try:
            was_existing = unreal.EditorAssetLibrary.does_asset_exist(
                "{}/{}".format(TARGET_FOLDER, card_id))
            asset = create_or_load_asset(card_id)
            populate_asset(asset, data)
            save_asset(asset)
            try:
                asset.validate_card_data_now()  # logs to Output Log (LogCardGame)
            except Exception:
                print("  [warn] could not invoke ValidateCardDataNow on {}".format(card_id))
            (updated if was_existing else created).append(card_id)
            print("[ok]   {} ({}, {})".format(card_id, data["rarity"], data["category"]))
        except Exception:
            failed.append(card_id)
            print("[FAIL] {}".format(card_id))
            traceback.print_exc()

    print("-" * 70)
    print("Created: {}, Updated: {}, Failed: {}".format(
        len(created), len(updated), len(failed)))
    if failed:
        print("FAILED CARDS: {}".format(", ".join(failed)))
    else:
        print("All sample cards created successfully.")
        print("Check Output Log, filter: LogCardGame  (validation results).")
    print("-" * 70)


main()
