#!/usr/bin/env python3
# Copyright (c) Fantasy Card Battle. All rights reserved.
#
# import_content.py - turn the generated CSVs into the DataTalk / DataTable assets the editor uses.
#
# Run from the project root, either headless:
#
#   UnrealEditor-Cmd <project>.uproject -run=pythonscript -script="/Game/Python/import_content.py"
#
# or inside the editor:  Content Explorer > right-click Content/Python > Python > Run Script
# (the file also works with `python3 Content/Python/import_content.py --csv-only`, which needs no engine.)
#
# What it creates / refreshes (all idempotent - re-running replaces the rows, never duplicates them):
#   /Game/Data/DT_Cards       (row struct FFCBCardRow)
#   /Game/Data/DT_Factions    (row struct FFCBFactionRow)
#   /Game/Data/DT_Abilities   (row struct FFCBAbilityRow)
#   /Game/Data/FCB_Rules      (UFCBMatchRulesAsset pointing at the three tables)
#   /Game/Data/FCB_AiProfiles  (UFCBAiProfileAsset, mirroring Config/DefaultGame.ini)
#   /Game/Maps/L_FCB_Arena     (empty level whose WorldSettings use AFCBGameMode)
#
# The level step is the only part that needs a running editor: it is skipped (with a warning, not an error)
# when the script runs headless without a world. The game is playable without ANY of these assets - the engine
# reads Content/Data/Generated/*.csv directly (see Docs/Setup.md), which is why this script is a convenience
# for people who want to tune cards in a table instead of a text file.

from __future__ import annotations

import argparse
import csv
import os
import sys

PROJECT_CONTENT = "Data/Generated"

TABLES = [
    # (asset name, csv file, row struct class name, key column)
    ("DT_Cards", "DT_Cards.csv", "FCBCardRow", "Name"),
    ("DT_Factions", "DT_Factions.csv", "FCBFactionRow", "Faction"),
    ("DT_Abilities", "DT_Abilities.csv", "FCBAbilityRow", "AbilityId"),
]

# Columns that must be integers. Everything else is text unless it is an enum (see ENUM_COLUMNS) or FText.
INT_COLUMNS = {
    "AgeYears", "Power", "Speed", "HeightCm", "SetIndex", "LoreEntryNumber",
    "Ability0ParamA", "Ability0ParamB", "Ability1ParamA", "Ability1ParamB",
    "BonusPercent", "PenaltyPercent", "DefaultParamA", "DefaultParamB",
}

# column name -> python enum class name. Values in the CSV are enum *identifiers* (FlatBonusOnAttribute),
# which is also what UE's own CSV importer expects, so both paths agree on the data.
ENUM_COLUMNS = {
    "Faction": "EFCBFaction",
    "Rarity": "EFCBRarity",
    "Ability0Effect": "EFCBAbilityEffect",
    "Ability0Attribute": "EFCBAttribute",
    "Ability1Effect": "EFCBAbilityEffect",
    "Ability1Attribute": "EFCBAttribute",
    "Effect": "EFCBAbilityEffect",
    "Attribute": "EFCBAttribute",
    "BonusAttribute": "EFCBAttribute",
    "PenaltyAttribute": "EFCBAttribute",
    "MinRarity": "EFCBRarity",
}

# FText columns: everything a player can read, so localization works without re-importing.
TEXT_COLUMNS = {
    "DisplayName", "Flavor", "DoctrineName", "Lore", "Description",
    "Ability0Name", "Ability0Text", "Ability1Name", "Ability1Text",
}

RULES_DEFAULTS = {
    "default_hand_size": 12,
    "default_tie_rule": ("EFCBTieRule", "PotClash"),
    "default_max_rounds": 240,
    "default_pot_cap": 12,
    "b_default_enable_abilities": True,
    "b_default_balance_seats": True,
    "default_end_condition": ("EFCBEndCondition", "LastSeatStanding"),
}

AI_PROFILES = [
    # difficulty, elo, search ply, blunder %, time budget ms
    ("Novice", 800, 0, 28, 0),
    ("Adept", 1200, 0, 10, 250),
    ("Expert", 1600, 1, 2, 700),
    ("Legendary", 1900, 2, 0, 1400),
]

MAP_PATH = "/Game/Maps/L_FCB_Arena"


def squash(name: str) -> str:
    """Compare enum members regardless of UE's underscore-casing conventions."""
    return "".join(ch for ch in name.lower() if ch.isalnum())


class EnumIndex:
    """Lazily maps 'FlatBonusOnAttribute' -> unreal.EFCBAbilityEffect.FLAT_BONUS_ON_ATTRIBUTE."""

    def __init__(self, unreal_module):
        self.unreal = unreal_module
        self._cache = {}

    def convert(self, enum_class_name: str, value: str):
        value = (value or "").strip()
        if not value:
            return None
        cache = self._cache.setdefault(enum_class_name, {})
        if value in cache:
            return cache[value]

        enum_class = getattr(self.unreal, enum_class_name, None)
        if enum_class is None:
            raise RuntimeError("enum %s is not exposed to Python - is the FantasyCardBattle module loaded?" % enum_class_name)

        wanted = squash(value)
        for member_name in dir(enum_class):
            if member_name.startswith("_"):
                continue
            if squash(member_name) == wanted:
                member = getattr(enum_class, member_name)
                cache[value] = member
                return member
        raise RuntimeError("value %r not found on %s" % (value, enum_class_name))


def read_rows(csv_path: str):
    with open(csv_path, "r", newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            yield row


def build_row(unreal, enums: EnumIndex, row_struct_class, source: dict, row_name: str):
    """
    Create one row-struct instance and fill it from the CSV dict.

    Every cell is set through set_editor_property and any failure is reported with the row name and the CSV
    column: a silent skip here is how a card ends up in the game with Power 0.
    """
    row = row_struct_class()

    for column, value in source.items():
        column = (column or "").strip()
        if not column or column in ("Name",):
            continue  # the row key carries the name

        target = None
        if column in ENUM_COLUMNS:
            target = enums.convert(ENUM_COLUMNS[column], value)
            if target is None:
                continue
        elif column in INT_COLUMNS:
            try:
                target = int(str(value).replace(",", ""))
            except ValueError:
                raise RuntimeError("column %s of row %s is not an integer: %r" % (column, row_name, value))
        elif column in TEXT_COLUMNS:
            target = unreal.Text(str(value))
        elif column == "PortraitPath":
            # Kept as a string on the row so a missing texture is a warning in-game, not a broken import.
            target = str(value)
        elif column == "FrameColor":
            target = str(value)
        else:
            target = unreal.Name(str(value)) if column.endswith("Id") else str(value)

        property_name = to_snake(column)
        try:
            row.set_editor_property(property_name, target)
        except Exception as error:  # noqa: BLE001 - report which cell failed, that is the whole point
            # A CSV column the row struct does not know is almost always a rename on one side only.
            raise RuntimeError(
                "row %s, column %s: %s has no property '%s'. Rename the CSV column or add the property: "
                "dropping a cell silently is how a card ships with Power 0."
                % (row_name, column, type(row).__name__, property_name)
            ) from error
    return row


def to_snake(name: str) -> str:
    out = []
    for index, char in enumerate(name):
        if char.isupper() and index and not name[index - 1].isupper():
            out.append("_")
        out.append(char.lower())
    return "".join(out)


def ensure_folder(unreal, path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def import_tables(unreal, enums: EnumIndex, data_dir: str, dest_folder: str) -> dict:
    """Create or refresh the three DataTables. Returns {asset_name: path}."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = {}

    for asset_name, csv_file, row_struct_name, key_column in TABLES:
        csv_path = os.path.join(data_dir, csv_file)
        if not os.path.exists(csv_path):
            print("  skip %s: %s not found" % (asset_name, csv_path))
            continue

        row_struct = getattr(unreal, row_struct_name, None)
        if row_struct is None:
            raise RuntimeError("%s is not exposed to Python - build the editor target first" % row_struct_name)

        asset_path = "%s/%s" % (dest_folder, asset_name)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            table = unreal.EditorAssetLibrary.load_asset(asset_path)
        else:
            factory = unreal.DataTableFactoryNew()
            try:
                factory.set_editor_property("row_struct", row_struct.static_class())
            except Exception:  # noqa: BLE001 - older factory API has no settable row struct
                pass
            table = asset_tools.create_asset(asset_name, dest_folder, unreal.DataTable, factory)
            try:
                table.set_editor_property("row_struct", row_struct.static_class())
            except Exception as error:  # noqa: BLE001
                print("  note: could not set row struct on %s (%s); create it manually if import fails" % (asset_name, error))

        rows = {}
        count = 0
        for source in read_rows(csv_path):
            row_name = (source.get(key_column) or source.get("Name") or "").strip()
            if not row_name:
                continue
            rows[row_name] = build_row(unreal, enums, row_struct, source, row_name)
            count += 1

        table.set_editor_property("row_map", rows)
        unreal.EditorAssetLibrary.save_loaded_asset(table)
        created[asset_name] = (asset_path, count)
        print("  %-12s %4d rows -> %s" % (asset_name, count, asset_path))

    return created


def import_rules_asset(unreal, enums: EnumIndex, dest_folder: str, tables: dict) -> None:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_class = getattr(unreal, "FCBMatchRulesAsset", None)
    if asset_class is None:
        print("  skip FCB_Rules: UFCBMatchRulesAsset not exposed to Python")
        return

    asset_path = "%s/FCB_Rules" % dest_folder
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        rules = unreal.EditorAssetLibrary.load_asset(asset_path)
    else:
        factory = unreal.DataAssetFactoryNew()
        try:
            factory.set_editor_property("asset_class", asset_class.static_class())
        except Exception:  # noqa: BLE001
            pass
        rules = asset_tools.create_asset("FCB_Rules", dest_folder, asset_class, factory)

    soft = {
        "card_table": "DT_Cards",
        "faction_table": "DT_Factions",
        "ability_table": "DT_Abilities",
    }
    for property_name, table_name in soft.items():
        if table_name in tables:
            rules.set_editor_property(property_name, unreal.SoftObjectPath(tables[table_name][0]))

    for property_name, value in RULES_DEFAULTS.items():
        if isinstance(value, tuple):
            value = enums.convert(value[0], value[1])
        try:
            rules.set_editor_property(property_name, value)
        except Exception as error:  # noqa: BLE001
            print("  note: %s not set on FCB_Rules (%s)" % (property_name, error))

    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    print("  FCB_Rules ready (tables wired, defaults from Docs/Balance.md)")


def import_ai_profiles(unreal, enums: EnumIndex, dest_folder: str) -> None:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_class = getattr(unreal, "FCBAiProfileAsset", None)
    if asset_class is None:
        print("  skip FCB_AiProfiles: UFCBAiProfileAsset not exposed to Python")
        return

    asset_path = "%s/FCB_AiProfiles" % dest_folder
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    else:
        factory = unreal.DataAssetFactoryNew()
        try:
            factory.set_editor_property("asset_class", asset_class.static_class())
        except Exception:  # noqa: BLE001
            pass
        asset = asset_tools.create_asset("FCB_AiProfiles", dest_folder, asset_class, factory)

    row_class = getattr(unreal, "FCBAiProfileRow", None)
    if row_class is None:
        print("  skip FCB_AiProfiles: FFCBAiProfileRow is not exposed (it needs BlueprintType, see FCBDataAssets.h)")
        return

    rows = []
    for difficulty, elo, search_ply, blunder, budget in AI_PROFILES:
        row = row_class()
        row.set_editor_property("difficulty", enums.convert("EFCBAiDifficulty", difficulty))
        row.set_editor_property("elo", elo)
        row.set_editor_property("search_ply", search_ply)
        row.set_editor_property("blunder_chance_percent", blunder)
        row.set_editor_property("time_budget_ms", budget)
        rows.append(row)

    try:
        asset.set_editor_property("profiles", rows)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        print("  FCB_AiProfiles ready (%d tiers)" % len(rows))
    except Exception as error:  # noqa: BLE001
        print("  note: could not write the profile table (%s) - Config/DefaultGame.ini already carries these values" % error)


def create_level(unreal) -> None:
    """An empty level with the right GameMode. Needs a live editor session."""
    try:
        subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    except Exception as error:  # noqa: BLE001
        print("  skip %s: no LevelEditorSubsystem (%s)" % (MAP_PATH, error))
        return

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        print("  level %s already exists, leaving it alone" % MAP_PATH)
        return

    try:
        subsystem.new_level(MAP_PATH, unreal.World)

        world = None
        try:
            world = unreal.EditorLevelLibrary.get_editor_world()
        except Exception:  # noqa: BLE001 - this accessor has moved twice in UE5; the level itself is what matters
            pass

        if world is not None and hasattr(unreal, "FCBGameMode"):
            settings = world.get_world_settings()
            settings.set_editor_property("default_game_mode", unreal.FCBGameMode.static_class())
            print("  set the level's Default Game Mode to AFCBGameMode")

        try:
            unreal.EditorAssetLibrary.save_loaded_asset(unreal.EditorAssetLibrary.load_asset(MAP_PATH))
        except Exception:  # noqa: BLE001
            if hasattr(subsystem, "save_all"):
                subsystem.save_all()
        print("  created %s" % MAP_PATH)
    except Exception as error:  # noqa: BLE001
        print("  skip level creation (%s)." % error)
        print("    Create it by hand: File > New Level > save as Maps/L_FCB_Arena, then set")
        print("    WorldSettings > Default Game Mode to FCBGameMode. Docs/Setup.md has the same steps.")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Import Fantasy Card Battle generated data into UE assets.")
    parser.add_argument("--data-dir", default=None, help="folder holding DT_*.csv (default: <project>/Content/%s)" % PROJECT_CONTENT)
    parser.add_argument("--dest", default="/Game/Data", help="content folder for the tables")
    parser.add_argument("--skip-level", action="store_true", help="do not create the arena level")
    parser.add_argument("--check", action="store_true", help="only validate the CSVs, create nothing")
    args = parser.parse_args(argv)

    here = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(here))
    data_dir = args.data_dir or os.path.join(project_root, "Content", PROJECT_CONTENT)

    print("== Fantasy Card Battle content import ==")
    print("  csv folder: %s" % data_dir)

    # Validate first: a broken CSV should fail here, not halfway through asset creation.
    problems = 0
    for asset_name, csv_file, _row_struct, key_column in TABLES:
        path = os.path.join(data_dir, csv_file)
        if not os.path.exists(path):
            print("  MISSING %s" % path)
            print("    fix: python3 Tools/generate_cards.py")
            return 2
        with open(path, "r", newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            rows = list(reader)
        if not rows:
            print("  EMPTY %s" % csv_file)
            problems += 1
            continue
        missing = [column for column in (key_column, "Name") if column not in (reader.fieldnames or [])]
        if len(missing) == 2:
            print("  %s has no %s column" % (csv_file, key_column))
            problems += 1
        print("  %-12s %4d rows, %d columns" % (asset_name, len(rows), len(reader.fieldnames or [])))
    if problems:
        print("CSV validation failed (%d problem(s)); nothing was imported." % problems)
        return 1
    if args.check:
        print("  --check: OK")
        return 0

    try:
        import unreal  # type: ignore
    except ImportError:
        print("\nNot running inside UnrealEditor: the `unreal` module is only available there.")
        print("CSVs are valid; headless usage is fully supported without this script (see Docs/Setup.md).")
        print("To create the assets, run this from the editor or with -run=pythonscript.")
        return 0

    enums = EnumIndex(unreal)
    ensure_folder(unreal, args.dest)
    tables = import_tables(unreal, enums, data_dir, args.dest)
    import_rules_asset(unreal, enums, args.dest, tables)
    import_ai_profiles(unreal, enums, args.dest)
    if not args.skip_level:
        create_level(unreal)
    print("done.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
