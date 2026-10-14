#!/usr/bin/env python3
"""Generates the canonical card dataset (Data/cards.json + cards.csv for UE DataTable import),
50 distinct procedural card artworks (Assets/Artwork/Cards/*.png), card-back + emblem art,
an artwork manifest, and a validation report. Deterministic (seeded) — re-running is safe.

Art note: illustrations are ORIGINAL procedurally-drawn cinematic silhouettes (Pillow),
one unique composition/palette/silhouette per card. They are real delivered files, suitable
as import sources for UE textures and used directly by the playable web build.
"""
import csv, json, math, os, random
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ATTRS = ["power", "speed", "height", "defense", "intelligence", "stamina", "luck", "age"]
RARITY_BAND = {"Common": (38, 58), "Uncommon": (42, 63), "Rare": (47, 70),
               "Epic": (52, 78), "Legendary": (57, 86), "Mythic": (62, 92)}

# name, category, rarity, archetype(silhouette), element palette key, ability, flavor
CARDS = [
 ("Emberclaw, the Ash Dragon","Dragon","Legendary","dragon","fire","Inferno Breath","It does not hoard gold. It hoards last words."),
 ("Sylvaris, Thornback Behemoth","Legendary Beast","Epic","beast","nature","Regrowth","The forest walks when Sylvaris dreams."),
 ("Kael the Dawnblade","Warrior","Rare","knight","holy","Dawn Strike","His sword rises before the sun dares to."),
 ("Nyx, Mistress of Hollow Stars","Celestial","Mythic","celestial","void","Starfall Veil","She counts stars the way others count debts."),
 ("Brondar Stonefist","Ancient Guardian","Uncommon","golem","earth","Stone Skin","Mountains apologize when he stumbles."),
 ("Vespera Nightwing","Monster","Rare","bat","shadow","Silent Glide","You will hear her only in memory."),
 ("Thalor the Tidecaller","Wizard","Epic","wizard","water","Tidal Surge","The sea answers him out of habit."),
 ("Ignivar, Forge of Wrath","Construct","Legendary","golem","fire","Molten Core","Built to end a war. Still warming up."),
 ("Aelindra Moonweaver","Wizard","Rare","wizard","arcane","Moonweave","She knits moonlight into armor."),
 ("Gorath the Skullrender","Monster","Epic","beast","shadow","Frenzy","Diplomacy was tried once. Once."),
 ("Seraphine of the Ninth Choir","Celestial","Legendary","celestial","holy","Hymn of Dawn","Her silence has perfect pitch."),
 ("Kragg, Oathbound Colossus","Ancient Guardian","Rare","golem","earth","Oathshield","He swore to hold the pass. He is the pass."),
 ("Zephyra Stormdancer","Elemental","Uncommon","phoenix","storm","Tailwind","She has never once arrived on time, only early."),
 ("Morgath Hollowking","Warrior","Epic","knight","shadow","Grave Oath","His crown is heavy with unfinished business."),
 ("Pyrra Cinderheart","Elemental","Common","phoenix","fire","Ember Spark","Small flame. Enormous opinions."),
 ("Ulfgar Frostmaul","Warrior","Uncommon","knight","frost","Frostbite","Winter trained under him."),
 ("The Verdant Matriarch","Legendary Beast","Legendary","beast","nature","Wild Communion","Every root in the valley calls her mother."),
 ("Ossivane, Keeper of Tombs","Ancient Guardian","Epic","golem","void","Tomb Ward","Nothing leaves his archive without permission."),
 ("Lyrix Quickfingers","Warrior","Common","rogue","storm","Pickpocket","Your pockets feel lighter already."),
 ("Drakmor the Sunken Maw","Dragon","Epic","serpent","water","Abyssal Bite","Sailors pray to weather, not to him."),
 ("Celestine Dawn Herald","Celestial","Rare","celestial","holy","Radiant Herald",None),
 ("Bram Bleakwood Ranger","Warrior","Common","rogue","nature","Thorn Shot","The trees aim with him."),
 ("Vexalia, Duchess of Thorns","Wizard","Epic","wizard","nature","Thornbind","Every rose in her garden has drawn blood."),
 ("Tormund Ironveil","Warrior","Rare","knight","earth","Ironveil Stance","Armor first. Questions later."),
 ("Asha Ember Oracle","Wizard","Uncommon","wizard","fire","Ember Sight","She reads futures in cooling ash."),
 ("Grendelok Worldgnawer","Monster","Mythic","beast","void","Worldgnaw","Continents are just slow prey."),
 ("Sylphine Gale Duchess","Elemental","Rare","phoenix","storm","Gale Step","Court etiquette, at hurricane speed."),
 ("Korvath Chainbreaker","Warrior","Epic","knight","fire","Broken Chains","No lock has survived his introduction."),
 ("Isolde Wintermere Queen","Wizard","Legendary","wizard","frost","Eternal Winter","Spring filed a formal complaint."),
 ("Rumblefist Automaton IX","Construct","Uncommon","golem","storm","Overclock","Ninth of his line. First to improvise."),
 ("Fenwick Marshlight Wisp","Elemental","Common","wisp","nature","Marshlight","Follow the light, if you trust lights."),
 ("Aurora Vex Starforge","Construct","Legendary","celestial","arcane","Starforge","She hammers constellations into blades."),
 ("Skarn Venomtail","Monster","Uncommon","serpent","nature","Venom Tail","His handshake is a medical event."),
 ("Helka Doomcaller","Wizard","Rare","wizard","shadow","Doomcall","Prophecies fear being spoken by her."),
 ("Oberon Thistlecrown","Legendary Beast","Rare","stag","nature","Thistlecrown","The old forest still holds coronations."),
 ("Magmara Core Titan","Elemental","Legendary","golem","fire","Core Eruption","Geologists study him from very far away."),
 ("Yuna Silent Petal","Warrior","Uncommon","rogue","arcane","Silent Petal","The duel ended before the petal landed."),
 ("Noctis Umbral Wyrm","Dragon","Mythic","dragon","void","Umbral Devour","It eats light as an appetizer."),
 ("Petra Bastion Ward","Ancient Guardian","Common","golem","earth","Ward Stance","A wall with excellent manners."),
 ("Cassian Goldtongue","Warrior","Rare","rogue","holy","Golden Lie","His treaties sign themselves."),
 ("Rhea Stormmother","Elemental","Epic","phoenix","storm","Stormmother","Thunder asks her permission."),
 ("Vorath Deepcoil","Monster","Rare","serpent","water","Deepcoil","The trench has a heartbeat. It is his."),
 ("Elaria Sunpetal Druid","Wizard","Common","wizard","nature","Sunpetal Bloom","Kindness, weaponized gently."),
 ("Dain Copperbeard Smith","Construct","Common","golem","fire","Forgehand","He shoes horses and, once, a volcano."),
 ("Morwenna Gravebloom","Wizard","Epic","wizard","shadow","Gravebloom","Her garden only grows at funerals."),
 ("Talos Verdigris Sentinel","Construct","Rare","golem","arcane","Sentinel Protocol","Rust is just experience, oxidized."),
 ("Aurelion Sky Sovereign","Celestial","Mythic","celestial","holy","Sovereign Dawn","Even eclipses bow, briefly."),
 ("Nibbles the Terrible","Monster","Common","beast","nature","Terrible Nibbling","Do not let the name fool you. Or do."),
 ("Karash Dune Revenant","Warrior","Rare","knight","earth","Dune Revenant","The desert keeps what it buries. Mostly."),
 ("Selvara Crystaltide Sage","Wizard","Epic","wizard","water","Crystaltide","She argues with oceans. She wins."),
]
PALETTES = {  # sky top, sky bottom, glow, ground, silhouette, rim
 "fire":   ((38,10,8),(122,44,16),(255,140,50),(30,10,8),(16,8,10),(255,170,80)),
 "water":  ((6,22,44),(16,84,110),(90,200,255),(6,20,30),(8,16,26),(120,220,255)),
 "nature": ((10,34,20),(34,96,52),(150,255,160),(8,24,14),(10,20,14),(170,255,180)),
 "shadow": ((16,10,30),(52,30,84),(170,120,255),(12,8,22),(10,8,18),(190,150,255)),
 "holy":   ((44,38,16),(150,124,54),(255,226,140),(30,26,12),(22,18,12),(255,240,180)),
 "frost":  ((14,30,52),(70,130,170),(190,235,255),(16,30,44),(14,24,36),(210,245,255)),
 "storm":  ((20,20,44),(70,70,130),(180,190,255),(14,14,30),(12,12,24),(200,205,255)),
 "arcane": ((30,12,52),(96,44,140),(230,150,255),(20,10,36),(16,10,28),(240,180,255)),
 "void":   ((8,6,20),(36,20,64),(150,110,255),(6,5,14),(7,6,14),(170,130,255)),
 "earth":  ((34,26,14),(104,80,44),(230,190,120),(24,18,10),(18,14,10),(240,205,140)),
}

def make_stats(i, rarity):
    rng = random.Random(1000 + i)
    lo, hi = RARITY_BAND[rarity]
    stats = {}
    for a in ATTRS:
        if a == "age":
            stats[a] = int(rng.uniform(15, 98))  # Age is a normal comparable attribute (years/10 scale documented in Docs)
        else:
            stats[a] = int(rng.uniform(lo, hi))
    # Guarantee a real weakness + a real strength on every card (no unbeatable cards)
    weak = rng.choice([a for a in ATTRS if a != "age"]); stats[weak] = int(rng.uniform(12, 34))
    strong = rng.choice([a for a in ATTRS if a not in (weak, "age")]); stats[strong] = min(99, stats[strong] + int(rng.uniform(8, 20)))
    return stats

def font(sz, bold=False):
    p = "/usr/share/fonts/truetype/dejavu/DejaVuSans%s.ttf" % ("-Bold" if bold else "")
    return ImageFont.truetype(p, sz)

def silhouette(d, kind, cx, cy, s, fill, rim):
    """Draw a bold original silhouette (kind) centered cx,cy at scale s."""
    P = lambda pts: d.polygon(pts, fill=fill, outline=rim, width=3)
    E = lambda b: d.ellipse(b, fill=fill, outline=rim, width=3)
    if kind == "dragon":
        R = lambda pts: P([(cx+x*s, cy+y*s) for x, y in pts])
        R([(-.1,-.55),(-.95,-1.55),(-.25,-1.02),(.28,-1.4),(.32,-.6)])  # wing
        R([(-1.65,.75),(-1.15,.15),(-.95,-.35),(-.4,-.58),(.3,-.78),(.62,-1.12),(1.05,-1.08),(1.38,-.9),(1.42,-.72),(.88,-.62),(.55,-.32),(.72,.3),(.5,.62),(.58,1.05),(.38,1.05),(.18,.55),(-.35,.62),(-.3,1.05),(-.52,1.05),(-.68,.45),(-1.05,.55)])  # body
        E([cx+.92*s,cy-.94*s,cx+1.04*s,cy-.82*s]); P([(cx+.62*s,cy-1.1*s),(cx+.45*s,cy-1.38*s),(cx+.78*s,cy-1.16*s)])  # eye + horn
    elif kind == "beast2":
        pass
    elif kind == "knight":
        E([cx-.32*s,cy-1.25*s,cx+.32*s,cy-.6*s]); P([(cx-.62*s,cy-.6*s),(cx+.62*s,cy-.6*s),(cx+.5*s,cy+.9*s),(cx-.5*s,cy+.9*s)])
        P([(cx+.55*s,cy-.5*s),(cx+1.25*s,cy+.7*s),(cx+.95*s,cy+.85*s),(cx+.4*s,cy-.2*s)]); P([(cx-1.15*s,cy-.45*s),(cx-.55*s,cy-.45*s),(cx-.6*s,cy+.65*s),(cx-1.1*s,cy+.65*s)])
    elif kind == "wizard":
        P([(cx,cy-1.65*s),(cx-.42*s,cy-.75*s),(cx+.42*s,cy-.75*s)]); P([(cx-1.05*s,cy-.72*s),(cx+1.05*s,cy-.72*s),(cx+.75*s,cy+.95*s),(cx-.75*s,cy+.95*s)])
        P([(cx+.7*s,cy-.6*s),(cx+1.15*s,cy+.9*s),(cx+.92*s,cy+.95*s),(cx+.5*s,cy-.5*s)]); E([cx+.62*s,cy-.72*s,cx+.86*s,cy-.48*s])
    elif kind == "golem":
        P([(cx-.85*s,cy-1.05*s),(cx+.85*s,cy-1.05*s),(cx+1.05*s,cy+.95*s),(cx-1.05*s,cy+.95*s)])
        E([cx-.3*s,cy-.85*s,cx+.3*s,cy-.35*s]); P([(cx-1.5*s,cy-.6*s),(cx-1.0*s,cy-.55*s),(cx-1.05*s,cy+.6*s),(cx-1.5*s,cy+.5*s)]); P([(cx+1.0*s,cy-.55*s),(cx+1.5*s,cy-.6*s),(cx+1.5*s,cy+.5*s),(cx+1.05*s,cy+.6*s)])
    elif kind == "serpent":
        d.arc([cx-1.35*s,cy-.9*s,cx+.45*s,cy+.95*s],200,520,fill=fill,width=int(.42*s))
        d.arc([cx-.3*s,cy-1.15*s,cx+1.3*s,cy+.5*s],180,480,fill=fill,width=int(.36*s)); P([(cx+.75*s,cy-1.15*s),(cx+1.45*s,cy-.8*s),(cx+.85*s,cy-.5*s)])
    elif kind == "phoenix":
        P([(cx,cy-1.2*s),(cx-1.6*s,cy-.35*s),(cx-.55*s,cy+.05*s),(cx-1.25*s,cy+.75*s),(cx-.25*s,cy+.6*s),(cx,cy+1.25*s),(cx+.25*s,cy+.6*s),(cx+1.25*s,cy+.75*s),(cx+.55*s,cy+.05*s),(cx+1.6*s,cy-.35*s)]); E([cx-.16*s,cy-1.05*s,cx+.16*s,cy-.72*s])
    elif kind == "celestial":
        E([cx-.4*s,cy-1.3*s,cx+.4*s,cy-.5*s]); P([(cx-.7*s,cy-.5*s),(cx+.7*s,cy-.5*s),(cx+.45*s,cy+1.05*s),(cx-.45*s,cy+1.05*s)])
        P([(cx-.65*s,cy-.6*s),(cx-1.6*s,cy-1.05*s),(cx-1.15*s,cy-.15*s)]); P([(cx+.65*s,cy-.6*s),(cx+1.6*s,cy-1.05*s),(cx+1.15*s,cy-.15*s)])
        d.ellipse([cx-.55*s,cy-1.62*s,cx+.55*s,cy-1.38*s],outline=rim,width=5)
    elif kind == "stag":
        E([cx-.75*s,cy-.35*s,cx+.75*s,cy+.75*s]); E([cx+.45*s,cy-1.0*s,cx+1.0*s,cy-.45*s])
        for dx in (-.1,.35): P([(cx+(.55+dx)*s,cy-.95*s),(cx+(.35+dx)*s,cy-1.6*s),(cx+(.62+dx)*s,cy-1.55*s),(cx+(.75+dx)*s,cy-1.0*s)])
        P([(cx-.55*s,cy+.6*s),(cx-.45*s,cy+.6*s),(cx-.5*s,cy+1.2*s)]); P([(cx+.45*s,cy+.6*s),(cx+.55*s,cy+.6*s),(cx+.55*s,cy+1.2*s)])
    elif kind == "bat":
        P([(cx,cy-.55*s),(cx-1.65*s,cy-1.05*s),(cx-1.15*s,cy-.05*s),(cx-1.55*s,cy+.35*s),(cx-.6*s,cy+.35*s),(cx,cy+.95*s),(.6*s+cx,cy+.35*s),(cx+1.55*s,cy+.35*s),(cx+1.15*s,cy-.05*s),(cx+1.65*s,cy-1.05*s)])
    elif kind == "rogue":
        E([cx-.3*s,cy-1.2*s,cx+.3*s,cy-.58*s]); P([(cx-.55*s,cy-.55*s),(cx+.5*s,cy-.5*s),(cx+.85*s,cy+.95*s),(cx-.25*s,cy+.95*s)])
        P([(cx+.45*s,cy-.35*s),(cx+1.3*s,cy-.95*s),(cx+1.05*s,cy-.15*s)])
    elif kind == "wisp":
        E([cx-.55*s,cy-.95*s,cx+.55*s,cy+.15*s]); P([(cx-.4*s,cy+.05*s),(cx,cy+1.15*s),(cx+.4*s,cy+.05*s)]); E([cx-.18*s,cy-.62*s,cx-.04*s,cy-.46*s]); E([cx+.05*s,cy-.62*s,cx+.19*s,cy-.46*s])
    else:  # beast
        P([(cx-1.15*s,cy-.1*s),(cx-1.7*s,cy-.7*s),(cx-1.05*s,cy+.35*s)])
        E([cx-1.05*s,cy-.55*s,cx+.75*s,cy+.75*s]); E([cx+.5*s,cy-1.05*s,cx+1.25*s,cy-.3*s])
        for bx in (-.6,-.15,.3): P([(cx+bx*s,cy-.62*s),(cx+(bx+.16)*s,cy-1.0*s),(cx+(bx+.3)*s,cy-.6*s)])
        P([(cx+.65*s,cy-1.0*s),(cx+.6*s,cy-1.45*s),(cx+.85*s,cy-1.1*s)]); P([(cx+1.05*s,cy-1.0*s),(cx+1.15*s,cy-1.45*s),(cx+1.3*s,cy-1.05*s)])
        P([(cx-.8*s,cy+.55*s),(cx-.65*s,cy+.55*s),(cx-.7*s,cy+1.2*s)]); P([(cx+.5*s,cy+.55*s),(cx+.65*s,cy+.55*s),(cx+.7*s,cy+1.2*s)])

def make_art(cid, archetype, palette, seed):
    rng = random.Random(seed)
    top, bot, glow, ground, sil, rim = PALETTES[palette]
    W, H = 512, 768
    img = Image.new("RGB", (W, H), top)
    d = ImageDraw.Draw(img, "RGBA")
    for y in range(H):  # sky gradient
        t = y / H; d.line([(0, y), (W, y)], fill=tuple(int(top[i] + (bot[i]-top[i])*t) for i in range(3)))
    mx, my, mr = rng.randint(90, 420), rng.randint(70, 220), rng.randint(46, 84)  # moon / sun disc
    for r in range(mr*3, 0, -4): d.ellipse([mx-r, my-r, mx+r, my+r], fill=glow+(max(0, 26-r//6),))
    d.ellipse([mx-mr, my-mr, mx+mr, my+mr], fill=tuple(min(255, c+70) for c in glow))
    for _ in range(130):  # stars / embers
        x, y = rng.randint(0, W), rng.randint(0, int(H*.7)); d.ellipse([x, y, x+2, y+2], fill=(255, 255, 255, rng.randint(50, 190)))
    d.rectangle([0, int(H*.78), W, H], fill=ground)  # ground + glow pool
    d.ellipse([W*.12, H*.8, W*.88, H*.97], fill=glow+(36,))
    silhouette(d, archetype, W//2 + rng.randint(-20, 20), int(H*.56), 118, sil, rim)
    for _ in range(46):  # magic particles
        x, y = rng.randint(20, W-20), rng.randint(int(H*.25), int(H*.85)); r = rng.randint(1, 4)
        d.ellipse([x-r, y-r, x+r, y+r], fill=glow+(rng.randint(90, 220),))
    img = img.filter(ImageFilter.GaussianBlur(.6))
    v = Image.new("L", (W, H), 0); vd = ImageDraw.Draw(v)  # vignette
    vd.ellipse([-W*.35, -H*.25, W*1.35, H*1.25], fill=255); v = v.filter(ImageFilter.GaussianBlur(90))
    return Image.composite(img, Image.new("RGB", (W, H), (4, 4, 8)), v)

UPGRADED_ART = {"FCB-001","FCB-004","FCB-008","FCB-017","FCB-026","FCB-032","FCB-038","FCB-047"}  # AI-illustrated; never overwritten
def main():
    cards, manifest = [], []
    art_dir = os.path.join(ROOT, "Assets", "Artwork", "Cards"); os.makedirs(art_dir, exist_ok=True)
    for i, (name, cat, rarity, arch, pal, ability, flavor) in enumerate(CARDS):
        cid = "FCB-%03d" % (i + 1)
        slug = name.split(",")[0].split(" the ")[0].replace(" ", "_").replace("'", "")
        fn = "%s_%s.png" % (cid, slug)
        cards.append({"id": cid, "name": name, "category": cat, "rarity": rarity, "archetype": arch,
                      "element": pal, "ability": ability, "flavor": flavor or "A legend still being written.",
                      "description": "%s — %s of the %s. Ability: %s." % (name, cat, pal, ability),
                      "artwork": "Assets/Artwork/Cards/" + fn, "attributes": make_stats(i, rarity)})
        if cid not in UPGRADED_ART: make_art(cid, arch, pal, 5000 + i).save(os.path.join(art_dir, fn))  # preserve upgraded art
        manifest.append({"cardId": cid, "name": name, "artwork": "Assets/Artwork/Cards/" + fn,
                         "ueTextureImportPath": "/Game/Cards/Art/%s" % fn[:-4]})
    # Card back + UI emblem
    back = Image.new("RGB", (512, 768), (18, 12, 34)); bd = ImageDraw.Draw(back, "RGBA")
    bd.rectangle([18, 18, 494, 750], outline=(242, 213, 107), width=8); bd.rectangle([38, 38, 474, 730], outline=(143, 231, 255), width=3)
    bd.ellipse([156, 250, 356, 450], outline=(242, 213, 107), width=7); bd.text((256, 470), "CARD BATTLE", font=font(34, True), fill=(242, 213, 107), anchor="ma")
    back.save(os.path.join(ROOT, "Assets", "Artwork", "UI", "card-back.png")) if os.makedirs(os.path.join(ROOT, "Assets", "Artwork", "UI"), exist_ok=True) is None else None
    json.dump(cards, open(os.path.join(ROOT, "Data", "cards.json"), "w"), indent=1)
    json.dump(manifest, open(os.path.join(ROOT, "Data", "artwork_manifest.json"), "w"), indent=1)
    with open(os.path.join(ROOT, "Data", "cards.csv"), "w", newline="") as f:  # UE DataTable (FCardTableRow) import
        w = csv.writer(f); w.writerow(["Name", "CardId", "DisplayName", "Category", "Rarity", "Ability", "Flavor"] + [a.capitalize() for a in ATTRS] + ["ArtworkFile"])
        for c in cards: w.writerow([c["id"], c["id"], c["name"], c["category"], c["rarity"], c["ability"], c["flavor"]] + [c["attributes"][a] for a in ATTRS] + [c["artwork"]])
    ids = [c["id"] for c in cards]
    problems = []
    if len(cards) != 50: problems.append("count != 50")
    if len(set(ids)) != 50: problems.append("duplicate IDs")
    for c in cards:
        if sorted(c["attributes"]) != sorted(ATTRS): problems.append(c["id"] + " attrs")
        if any(not (1 <= v <= 100) for v in c["attributes"].values()): problems.append(c["id"] + " range")
        if not os.path.exists(os.path.join(ROOT, c["artwork"])): problems.append(c["id"] + " art missing")
    report = {"cardCount": len(cards), "uniqueIds": len(set(ids)), "rarities": {r: sum(1 for c in cards if c["rarity"] == r) for r in RARITY_BAND},
              "problems": problems, "status": "PASS" if not problems else "FAIL"}
    json.dump(report, open(os.path.join(ROOT, "Data", "validation_report.json"), "w"), indent=1)
    print(json.dumps(report, indent=1))

if __name__ == "__main__":
    main()
