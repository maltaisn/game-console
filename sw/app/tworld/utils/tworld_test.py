#!/usr/bin/env python3

import argparse
import time
from pathlib import Path
from typing import Optional, TextIO, Iterable

from tworld import TWException, LevelLoader, TileWorld, EndCause, Level, ActiveActor
from tworld_tws import SolutionLoader, Solution


def validate_solution(level: Level, solution: Solution, export_file: TextIO) -> EndCause:
    # step through the whole solution until done or game is over (should be both).
    state = TileWorld(level, silent=True)
    state.stepping = solution.stepping
    state.initial_random_slide_dir = solution.initial_random_slide_dir
    state.prng_value0 = solution.prng_seed
    state.restart()
    solution_iter = iter(solution)

    def step(dir: int) -> None:
        state.step(dir)
        if export_file:
            export_file.write(f"STEP TIME {state.current_time}\n")
            for i, act in enumerate(state.actors):
                if act.state == ActiveActor.STATE_HIDDEN and act.step <= 0:
                    continue
                tile = state.get_top_tile(act.x, act.y)
                stat = "anim" if act.step > 0 and act.state == ActiveActor.STATE_HIDDEN else "alive"
                export_file.write(f"[{i}] ({act.x},{act.y}) {tile}, {stat}, step={act.step}\n")
            export_file.write("\n")

    for direction in solution_iter:
        if state.is_game_over():
            break
        try:
            step(direction)
        except TWException as e:
            print(f"check failed: {e}")
            break
    else:
        # step until the end of solution, with no input.
        while state.current_time < solution.total_time:
            step(0)

    return state.end_cause


def validate_solutions(level_file: Path, solution_file: Path,
                       level_numbers: Optional[Iterable[int]],
                       export_dir: Optional[Path]) -> None:
    start_time = time.time()

    # load all levels and solutions
    level_loader = LevelLoader(level_file)
    sol_loader = SolutionLoader(solution_file)
    levels = []
    solutions = []
    for i in range(1, level_loader.level_count + 1):
        levels.append(level_loader.load(i))
        try:
            solutions.append(sol_loader.read_solution(i))
        except TWException as e:
            print(f"bad solution for level {i} ({e})")
            solutions.append(None)

    # load previous results for regression check
    results_filename = Path(f"{level_file.stem}.txt")
    if results_filename.exists():
        with open(results_filename, "r") as file:
            last_results = [int(line) == 1 for line in file.readlines()]
    else:
        last_results = [False] * level_loader.level_count

    if level_numbers is None:
        level_numbers = range(level_loader.level_count)

    success = 0
    for i in level_numbers:
        export_file = None
        if export_dir:
            export_file = open(export_dir / f"{level_file.stem.upper()}L{i + 1:03}.txt", "w")

        end_cause = validate_solution(levels[i], solutions[i], export_file)

        if export_file:
            export_file.close()

        passed = False
        print(f"{level_file.name}/{i + 1}: ", end="")
        if end_cause == EndCause.COMPLETE:
            print("passed")
            success += 1
            passed = True
        elif end_cause == EndCause.NONE:
            print("failed, exit not reached")
        else:
            print(f"failed, unexpected end ({end_cause.name})")

        if last_results[i] and not passed:
            print("--> REGRESSION")
        last_results[i] |= passed

    print("===============")
    print(f"{success} levels passed out of {len(level_numbers)}, "
          f"{success / len(level_numbers):.1%} pass rate")
    print(f"total time: {time.time() - start_time:.0f} seconds")

    with open(results_filename, "w") as file:
        file.writelines([f"{int(i)}\n" for i in last_results])
        file.close()


def main() -> None:
    args = parser.parse_args()

    # load level from level pack file
    level_file = Path(args.level_pack)
    if not level_file.exists():
        raise TWException("level pack doesn't exist")

    solution_file = Path(args.solution)
    if not solution_file.exists():
        raise TWException("solution file doesn't exist")

    # validate all levels (used as unit testing)
    if not solution_file:
        raise TWException("--validate requires passing a solution file")

    export_dir = args.export_dir
    if export_dir:
        export_dir = Path(export_dir)
        export_dir.mkdir(parents=True, exist_ok=True)

    levels = None
    if args.level:
        levels = [args.level - 1]

    validate_solutions(level_file, solution_file, levels, export_dir)


parser = argparse.ArgumentParser(description="Tile world level pack validation")
parser.add_argument(
    "level_pack", action="store", type=str,
    help="Level pack to validate")
parser.add_argument(
    "solution", action="store", type=str,
    help="TWS solution file for level pack used for validation.")
parser.add_argument(
    "--export-dir", action="store", type=str, dest="export_dir",
    help="Directory in which to export state of actors on all steps, for all levels.")
parser.add_argument(
    "--level", action="store", type=int, dest="level", default=None,
    help="Specific level to validate, all levels by default.")

if __name__ == '__main__':
    try:
        main()
    except TWException as e:
        print(f"ERROR: {e}")
        exit(1)
