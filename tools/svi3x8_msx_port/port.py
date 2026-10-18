"""Ports SVI-318/328 cassette games to MSX with the recipes in games/<name>/recipe.py.

    python port.py --list
    python port.py spectron --cas "Spectron (1983)(Spectravideo).cas"
    python port.py spectron --source-dir <folder with the SVI cassettes> --rom-dir <folder> --cas-dir <folder>

The original SVI cassette is not part of this tool; the recipe names the file it expects. The
work folder (assembler sources, generated includes and intermediate files) is a temporary
folder unless --work names one.
"""

import argparse
import importlib.util
import pathlib
import sys
import tempfile

import sviport

HERE = pathlib.Path(__file__).resolve().parent
GAMES = HERE / "games"


def recipes() -> list:
    return sorted(path.parent.name for path in GAMES.glob("*/recipe.py"))


def load_recipe(name: str):
    path = GAMES / name / "recipe.py"
    spec = importlib.util.spec_from_file_location(f"recipe_{name}", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def find_source(recipe, cas: pathlib.Path, source_dirs: list) -> pathlib.Path:
    if cas:
        return cas
    for folder in source_dirs:
        candidate = folder / recipe.SOURCE
        if candidate.is_file():
            return candidate
    raise sviport.PortError(f"original cassette not found: {recipe.SOURCE} (give --cas or --source-dir)")


def run(name: str, args) -> None:
    recipe = load_recipe(name)
    source = find_source(recipe, args.cas, args.source_dir)
    sources = sorted((GAMES / name).glob("*.ASM")) + sorted(sviport.TEMPLATES.glob("*.ASM"))
    with tempfile.TemporaryDirectory() as temporary:
        work = args.work / name if args.work else pathlib.Path(temporary)
        asm = sviport.Assembler(args.pasmo, work, sources)
        print(f"{name}: ", end="")
        outputs = recipe.build(source.read_bytes(), asm)
    for file_name, content in outputs.items():
        folder = args.cas_dir if file_name.lower().endswith(".cas") else args.rom_dir
        folder.mkdir(parents=True, exist_ok=True)
        (folder / file_name).write_bytes(content)
        print(f"  {folder / file_name}: {len(content)} bytes")


def main() -> int:
    parser = argparse.ArgumentParser(description="Ports SVI-318/328 cassette games to MSX (cassette and cartridge).")
    parser.add_argument("game", nargs="?", help="recipe name, or 'all'")
    parser.add_argument("--list", action="store_true", help="list the recipes and the cassettes they expect")
    parser.add_argument("--cas", type=pathlib.Path, help="original SVI cassette of the game")
    parser.add_argument("--source-dir", type=pathlib.Path, action="append", default=[],
                        help="folder to look for the original cassette in (may be given several times)")
    parser.add_argument("--rom-dir", type=pathlib.Path, default=pathlib.Path("."), help="folder for cartridge files (default: current folder)")
    parser.add_argument("--cas-dir", type=pathlib.Path, default=pathlib.Path("."), help="folder for MSX cassette files (default: current folder)")
    parser.add_argument("--pasmo", default="pasmo", help="pasmo executable (default: pasmo on the PATH)")
    parser.add_argument("--work", type=pathlib.Path, help="keep the work files in this folder")
    args = parser.parse_args()

    if args.list or not args.game:
        for name in recipes():
            recipe = load_recipe(name)
            print(f"{name:10} {recipe.SOURCE}  ->  {', '.join(recipe.OUTPUTS)}")
        return 0
    names = recipes() if args.game == "all" else [args.game]
    if args.cas and len(names) > 1:
        parser.error("--cas names the cassette of one game")
    for name in names:
        if name not in recipes():
            parser.error(f"unknown recipe: {name} (see --list)")
        try:
            run(name, args)
        except sviport.PortError as error:
            print(f"\n{name}: {error}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
