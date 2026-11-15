#!/usr/bin/env python3
"""Coverage regression of projects/fc4sc_cov.

Runs the simulation once per seed with only a few items, so a single run does
not reach the coverage goal, then merges the UCIS XML files of all runs and
prints the merged report. FC4SC cannot merge runs itself; the merge is done
with pyucis (pip install pyucis).

    scripts/cov_regress.py <build-dir> [--seeds N] [--items N] [--goal PCT]

Exits with 1 if a run fails or the merged coverage is below the goal.
"""

import argparse
import contextlib
import io
import pathlib
import subprocess
import sys


def load_pyucis():
    try:
        # pyucis prints a banner when it is imported
        with contextlib.redirect_stdout(io.StringIO()):
            from ucis.mem.mem_factory import MemFactory
            from ucis.merge.db_merger import DbMerger
            from ucis.report.coverage_report_builder import CoverageReportBuilder
            from ucis.xml.xml_factory import XmlFactory
    except ImportError:
        sys.exit("cov_regress: pyucis not found (pip install pyucis)")
    return MemFactory, DbMerger, CoverageReportBuilder, XmlFactory


def print_report(report):
    for cg in report.covergroups:
        print(f"  covergroup {cg.name}: {cg.coverage:.2f}%")
        for cp in cg.coverpoints:
            print(f"    coverpoint {cp.name}: {cp.coverage:.2f}%")
        for cr in cg.crosses:
            print(f"    cross {cr.name}: {cr.coverage:.2f}%")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build_dir", type=pathlib.Path, help="CMake build directory")
    parser.add_argument("--seeds", type=int, default=16, help="number of runs, seeds 1..N (default 16)")
    parser.add_argument("--items", type=int, default=8, help="bus items per run (default 8)")
    parser.add_argument("--goal", type=float, default=100.0, help="merged coverage goal in percent (default 100)")
    args = parser.parse_args()

    build = args.build_dir.resolve()
    sim = build / "projects" / "fc4sc_cov" / "fc4sc_cov_sim"
    out = build / "cov_regress"
    if not sim.is_file():
        sys.exit(f"cov_regress: {sim} not found (configure with -DFC4SC_HOME=... and build)")

    MemFactory, DbMerger, CoverageReportBuilder, XmlFactory = load_pyucis()

    dbs = []
    for seed in range(1, args.seeds + 1):
        run_dir = out / f"seed_{seed}"
        run_dir.mkdir(parents=True, exist_ok=True)
        # goal 0: a single short run is not expected to reach full coverage
        with open(run_dir / "run.log", "w") as log:
            result = subprocess.run([str(sim), str(seed), str(args.items), "0"],
                                    cwd=run_dir, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode != 0:
            sys.exit(f"cov_regress: seed {seed} failed, see {run_dir / 'run.log'}")
        db = XmlFactory.read(str(run_dir / "fc4sc_cov_results.xml"))
        print(f"seed {seed:<3} {CoverageReportBuilder.build(db).coverage:6.2f}%")
        dbs.append(db)

    merged = MemFactory.create()
    DbMerger().merge(merged, dbs)
    merged_file = out / "merged.xml"
    XmlFactory.write(merged, str(merged_file))

    # report what was written, not what is in memory
    report = CoverageReportBuilder.build(XmlFactory.read(str(merged_file)))
    print(f"\nmerged ({args.seeds} seeds, {args.items} items each): {report.coverage:.2f}%  {merged_file}")
    print_report(report)

    if report.coverage < args.goal:
        sys.exit(f"cov_regress: merged coverage {report.coverage:.2f}% is below the goal of {args.goal:.2f}%")


if __name__ == "__main__":
    main()
