#!/usr/bin/env python3
"""Writes 3ds/core/pd_tables_gen.c from the Android app's own Kotlin tables.

The app's tables are the source of truth (species, moves, type chart, map
sections); this only re-emits them as C arrays so the 3DS build never drifts
from them. Rerun after changing any of the files in SOURCES:

    python3 3ds/tools/gen_tables.py
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "app/src/main/kotlin/com/pokedaisy/app/companion/data")
OUT = os.path.join(ROOT, "3ds/core/pd_tables_gen.c")

SOURCES = {
    "species": "SpeciesNames.kt",
    "moves": "MoveData.kt",
    "types": "TypeChartUtil.kt",
    "chart": "TypeChart.kt",
    "mapsec_firered": "MapSecData.kt",
    "mapsec_emerald": "MapSecDataEmerald.kt",
    # Item names exactly as each game's bag prints them. Descriptions are the
    # games' prose: read from the player's ROM (pd_item_description), never bundled.
    "items": "ItemTextGame.kt",
    "gender": "GenderRatiosFireRed.kt",
    "species_types": "SpeciesTypes.kt",
    "exp": "ExpTables.kt",
    "move_stats": "MoveStatsGen.kt",
}
# The type chips' colours (AppColors.typeColors), outside the data package.
THEME = os.path.join(ROOT, "app/src/main/kotlin/com/pokedaisy/app/companion/ui/theme/Theme.kt")


def read(name):
    with open(os.path.join(DATA, SOURCES[name]), encoding="utf-8") as f:
        return f.read()


def block(text, decl):
    """The body of `val <decl> ... = mapOf( ... )`."""
    m = re.search(r"val " + re.escape(decl) + r"\b[^=]*=\s*mapOf\((.*?)\n\)", text, re.S)
    if not m:
        sys.exit(f"gen_tables: no `val {decl}` table found")
    return m.group(1)


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def name_array(c_name, entries):
    top = max(entries)
    lines = [f"const char* const {c_name}[{top + 1}] = {{"]
    for i in range(top + 1):
        lines.append(f"    {c_str(entries[i]) if i in entries else 'NULL'},")
    lines.append("};")
    lines.append(f"const int {c_name}_count = {top + 1};")
    return "\n".join(lines)


def main():
    species = {int(k): v for k, v in re.findall(r'(\d+) to "([^"]*)"', block(read("species"), "speciesNames"))}

    moves = {}
    for k, body in re.findall(r"(\d+) to MoveInfo\(([^)]*)\)", block(read("moves"), "moveData")):
        m = re.match(r'"([^"]*)"\s*,\s*(\d+)(?:\s*,\s*(\d+))?', body)
        moves[int(k)] = (m.group(1), int(m.group(2)), int(m.group(3) or 0))

    types = {int(k): v for k, v in re.findall(r'(\d+) to "([^"]*)"', block(read("types"), "typeNames"))}

    chart = [(int(k), int(v)) for k, v in re.findall(r"(\d+) to (\d+)", block(read("chart"), "typeEffectiveness"))]

    # In capitals with a small é, like the app's gameCase() for FireRed / Emerald.
    def mapsecs(name, decl):
        return {int(k): "".join(c if c == "é" else c.upper() for c in v)
                for k, v in re.findall(r'(\d+) to MapSecInfo\("([^"]*)"', block(read(name), decl))}

    fr_secs = mapsecs("mapsec_firered", "mapSecData")
    em_secs = mapsecs("mapsec_emerald", "mapSecDataEmerald")

    def strings(name, decl):
        return {int(k): v for k, v in re.findall(r'(\d+) to "([^"]*)"', block(read(name), decl))}

    item_tables = [
        ("pd_item_names_firered", strings("items", "itemNamesFireRedGame")),
        ("pd_item_names_emerald", strings("items", "itemNamesEmeraldGame")),
    ]

    # Each species' two types (the same twice for one type), by internal id.
    species_types = {int(k): (int(a), int(b)) for k, a, b in
                     re.findall(r"(\d+) to SpeciesTypes\((\d+), (\d+)\)", block(read("species_types"), "speciesTypeData"))}
    # The app's table files BULBASAUR's types under 0 (no species) and has no 1.
    if 0 in species_types and 1 not in species_types:
        species_types[1] = species_types.pop(0)

    m = re.search(r"val genderRatiosFireRed = intArrayOf\(([^)]*)\)", read("gender"))
    gender = [int(v) for v in m.group(1).split(",")]

    # The summary's EXP bar: each species' growth rate and the six curves.
    exp_text = read("exp")
    m = re.search(r"val gen3GrowthRates: ByteArray = byteArrayOf\(([^)]*)\)", exp_text)
    growth = [int(v) for v in m.group(1).replace("\n", " ").split(",") if v.strip()]
    m = re.search(r"val gen3ExperienceTables: Array<IntArray> = arrayOf\((.*?)\n\)", exp_text, re.S)
    curves = [[int(v) for v in body.split(",") if v.strip()] for body in re.findall(r"intArrayOf\(([^)]*)\)", m.group(1))]
    if len(curves) != 6 or any(len(cv) != 101 for cv in curves):
        sys.exit("gen_tables: gen3ExperienceTables isn't six curves of 101 levels")

    # The move advice's data (MoveAdvice.kt's MoveStats): effect, accuracy, PP,
    # effect chance and priority, indexed by move id.
    m = re.search(r"val vanillaMoveStats: List<MoveStats> = listOf\((.*?)\n\)", read("move_stats"), re.S)
    move_stats = [(e, int(a), int(pp), int(ch), int(pr)) for e, a, pp, ch, pr in
                  re.findall(r'MoveStats\("([A-Z0-9_]+)", (\d+), (\d+), (\d+), (-?\d+)\)', m.group(1))]

    # AppColors.typeColors, by the type's id in the names above.
    with open(THEME, encoding="utf-8") as f:
        theme = f.read()
    m = re.search(r"typeColorsTitled = mapOf\((.*?)\n    \)", theme, re.S)
    colors_by_name = {n: int(c, 16) & 0xFFFFFF for n, c in re.findall(r'"([^"]+)" to Color\(0x([0-9A-Fa-f]{8})\)', m.group(1))}
    type_colors = {i: colors_by_name[n] for i, n in types.items() if n in colors_by_name}

    out = [
        "// Generated by 3ds/tools/gen_tables.py from the app's Kotlin tables",
        "// (" + ", ".join(sorted(SOURCES.values())) + ", ui/theme/Theme.kt) - do not hand-edit.",
        '#include "pd_tables.h"',
        "",
        name_array("pd_species_names", species),
        "",
    ]
    top = max(moves)
    out.append(f"const struct pd_move_info pd_moves[{top + 1}] = {{")
    for i in range(top + 1):
        if i in moves:
            n, t, p = moves[i]
            out.append(f"    {{ {c_str(n)}, {t}, {p} }},")
        else:
            out.append("    { NULL, 0, 0 },")
    out.append("};")
    out.append(f"const int pd_moves_count = {top + 1};")
    out.append("")
    out.append(name_array("pd_type_names", types))
    out.append("")
    out.append(f"const struct pd_type_pair pd_type_chart[{len(chart)}] = {{")
    for k, v in chart:
        out.append(f"    {{ {k // 100}, {k % 100}, {v} }},")
    out.append("};")
    out.append(f"const int pd_type_chart_count = {len(chart)};")
    out.append("")
    out.append(name_array("pd_mapsec_firered", fr_secs))
    out.append("")
    out.append(name_array("pd_mapsec_emerald", em_secs))
    out.append("")
    for c_name, table in item_tables:
        out.append(name_array(c_name, table))
        out.append("")
    out.append(f"const unsigned char pd_gender_ratios[{len(gender)}] = {{")
    for i in range(0, len(gender), 20):
        out.append("    " + ", ".join(str(v) for v in gender[i:i + 20]) + ",")
    out.append("};")
    out.append(f"const int pd_gender_ratios_count = {len(gender)};")
    out.append("")
    out.append(f"const unsigned char pd_growth_rates[{len(growth)}] = {{")
    for i in range(0, len(growth), 32):
        out.append("    " + ", ".join(str(v) for v in growth[i:i + 32]) + ",")
    out.append("};")
    out.append(f"const int pd_growth_rates_count = {len(growth)};")
    out.append("")
    out.append("const int pd_exp_tables[6][101] = {")
    for cv in curves:
        out.append("    {")
        for i in range(0, 101, 10):
            out.append("        " + ", ".join(str(v) for v in cv[i:i + 10]) + ",")
        out.append("    },")
    out.append("};")
    out.append("")
    out.append(f"const struct pd_move_stats pd_move_stats[{len(move_stats)}] = {{")
    for e, a, pp, ch, pr in move_stats:
        out.append(f"    {{ {c_str(e)}, {a}, {pp}, {ch}, {pr} }},")
    out.append("};")
    out.append(f"const int pd_move_stats_count = {len(move_stats)};")
    out.append("")
    top = max(types)
    out.append(f"const unsigned int pd_type_colors[{top + 1}] = {{")
    for i in range(top + 1):
        out.append(f"    0x{type_colors.get(i, 0):06X},")
    out.append("};")
    out.append("")
    top = max(species_types)
    out.append(f"const unsigned char pd_species_type_table[{top + 1}][2] = {{")
    for i in range(top + 1):
        a, b = species_types.get(i, (255, 255))
        out.append(f"    {{ {a}, {b} }},")
    out.append("};")
    out.append(f"const int pd_species_type_table_count = {top + 1};")
    out.append("")

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    print(f"wrote {os.path.relpath(OUT, ROOT)}: {len(species)} species, {len(moves)} moves, "
          f"{len(chart)} chart entries, {len(fr_secs)}/{len(em_secs)} map sections, "
          f"{len(item_tables[0][1])}/{len(item_tables[1][1])} items, {len(gender)} gender ratios")


if __name__ == "__main__":
    main()
