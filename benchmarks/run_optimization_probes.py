#!/usr/bin/env python3
"""Run correctness-backed optimization probes and retain per-process RAM/timing logs."""
import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys
import tempfile
import time


def run_job(name, command, overrides, output, timeout):
    env = os.environ.copy()
    for key in ("QT_QUICK_BACKEND", "QSG_RHI_BACKEND", "QSG_RENDER_LOOP",
                "PHOTOCRAP_TEST_STACK_STREAMING", "PHOTOCRAP_TEST_STACK_SHARED_SCRATCH",
                "PHOTOCRAP_TEST_GL_CORE", "PHOTOCRAP_TEST_GL_LEGACY"):
        env.pop(key, None)
    env.update(overrides)
    env["LC_ALL"] = "C"
    log = output / (name + ".log")
    timed = command
    rss_source = None
    if Path("/usr/bin/time").is_file() and sys.platform in ("darwin", "linux"):
        timed = ["/usr/bin/time", "-l" if sys.platform == "darwin" else "-v", *command]
        rss_source = "time.ru_maxrss"
    start = time.perf_counter()
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen(timed, stdout=stream, stderr=subprocess.STDOUT, env=env,
                                   start_new_session=os.name == "posix")
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
            process.wait()
            code = 124
    elapsed = (time.perf_counter() - start) * 1000
    text = log.read_text(encoding="utf-8", errors="replace")
    rss = None
    if sys.platform == "darwin":
        match = re.search(r"^\s*(\d+)\s+maximum resident set size", text, re.M)
        if match:
            rss = int(match[1])
    elif sys.platform == "linux":
        match = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", text)
        if match:
            rss = int(match[1]) * 1024
    probes = []
    for line in text.splitlines():
        if "PHOTOCRAP_PROBE " in line:
            probes.append(json.loads(line.split("PHOTOCRAP_PROBE ", 1)[1]))
    validation_error = None
    requested = overrides.get("QSG_RHI_BACKEND")
    if name.endswith("-stack") and requested and code == 0:
        if not any(probe.get("kind") == "native-stack-frame" and probe.get("backend") == requested
                   for probe in probes):
            validation_error = "The requested native backend did not produce its frame probe"
            code = 125
    result = dict(name=name, command=command, environment=overrides, returncode=code,
                  validation_error=validation_error,
                  elapsed_ms=elapsed, peak_rss_bytes=rss, rss_source=rss_source if rss else None,
                  probes=probes, log=str(log))
    print(f"{name}: {'PASS' if code == 0 else 'FAIL'}; {elapsed:.0f} ms; "
          f"peak RSS {rss if rss is not None else 'unavailable'}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--backends", default="software,metal,opengl" if sys.platform == "darwin" else "software")
    parser.add_argument("--modes", default="normal,stream,shared")
    parser.add_argument("--skip-cpu", action="store_true")
    parser.add_argument("--dense", action="store_true", help="Include 24 MP / 16-bit disk-backed stress probes")
    parser.add_argument("--scratch-dir", type=Path,
                        help="Place the dense cache fixture on this volume (for example a slow disk)")
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()
    if args.scratch_dir and (not args.dense or args.skip_cpu):
        parser.error("--scratch-dir requires --dense with CPU probes enabled")
    backends = args.backends.split(",")
    modes = args.modes.split(",")
    if any(b not in ("software", "metal", "opengl", "vulkan", "d3d11", "d3d12") for b in backends):
        parser.error("Unknown backend")
    if any(m not in ("normal", "stream", "shared") for m in modes):
        parser.error("Unknown mode")
    build = args.build_dir.resolve()
    output = (args.output_dir or Path(tempfile.mkdtemp(prefix="photocrap-probes-"))).resolve()
    output.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    app = build / ("PhotoCrap" + suffix)
    if sys.platform == "darwin":
        app = build / "PhotoCrap.app/Contents/MacOS/PhotoCrap"
    jobs = []
    software = dict(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                    PHOTOCRAP_OPTIMIZATION_PROBES="1")
    if not args.skip_cpu:
        jobs += [("cpu-tools-history-io", [str(build / ("photocrap_tests" + suffix)),
                  "optimizationMemoryAndLatencyProbe", "denseWholeLayerMovesMatchNativePainter",
                  "fullGradientBandsMatchNativePainter"], software),
                 ("history-deltas", [str(build / ("photocrap_tiled_tests" + suffix)),
                  "smallNativeTileEditsStoreBoundedDeltas"], software),
                 ("streaming-writers", [str(build / ("photocrap_project_write_tests" + suffix))], software),
                 ("patch-preview", [str(build / ("photocrap_tests" + suffix)),
                  "patchAsyncSoftSelectionMatchesRepairReference"], software)]
        if args.dense:
            dense = dict(software, PHOTOCRAP_DENSE_MEMORY_TEST="1")
            if args.scratch_dir:
                scratch = args.scratch_dir.resolve()
                if not scratch.is_dir():
                    parser.error("--scratch-dir must be an existing directory")
                dense.update(TMPDIR=str(scratch), TMP=str(scratch), TEMP=str(scratch))
            jobs += [("dense-24mp-disk", [str(build / ("photocrap_tests" + suffix)),
                      "denseTwentyFourMegapixelPlanesSurviveOffloadAndReload"], dense),
                     ("dense-24mp-preview", [str(build / ("photocrap_tiled_tests" + suffix)),
                      "denseFastPreviewBenchmark"], dense)]
            phase_dir = Path(tempfile.mkdtemp(prefix="project-phase-", dir=output))
            project = phase_dir / "native-24mp.pcr"
            for phase in ("save", "open"):
                jobs.append((f"dense-project-{phase}",
                             [str(build / ("photocrap_tests" + suffix)), "denseProjectPhaseProbe"],
                             dict(software, PHOTOCRAP_PROJECT_BAND_PHASE=phase,
                                  PHOTOCRAP_PROJECT_BAND_PATH=str(project))))
    for backend in backends:
        for mode in modes:
            env = dict(PHOTOCRAP_OPTIMIZATION_PROBES="1")
            if backend == "software":
                env.update(software)
            else:
                env["QT_QPA_PLATFORM"] = "cocoa" if sys.platform == "darwin" else "windows" if os.name == "nt" else "xcb"
                env["QSG_RHI_BACKEND"] = backend
                env["QSG_INFO"] = "1"
            if backend == "opengl":
                env.update(PHOTOCRAP_TEST_GL_CORE="1", QSG_RENDER_LOOP="basic")
            if mode != "normal":
                env["PHOTOCRAP_TEST_STACK_STREAMING" if mode == "stream" else
                    "PHOTOCRAP_TEST_STACK_SHARED_SCRATCH"] = "1"
            jobs.append((f"{backend}-{mode}-stack", [str(app), "--smoke-test", "--stack-render-test"], env))
        jobs.append((f"{backend}-shared-spatial", [str(app), "--smoke-test", "--spatial-render-test"],
                     dict(env, PHOTOCRAP_TEST_STACK_SHARED_SCRATCH="1")))
    root = Path(__file__).resolve().parents[1]
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, text=True, capture_output=True, check=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root,
                                text=True, capture_output=True, check=True).stdout.strip())
    configuration = {}
    cache = build / "CMakeCache.txt"
    if cache.is_file():
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith(("CMAKE_BUILD_TYPE:", "Qt6_DIR:", "PHOTOCRAP_ENABLE_ACCELERATE:")):
                key, value = line.split("=", 1)
                configuration[key.split(":", 1)[0]] = value
    report = dict(schema=1, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  revision=revision, tracked_sources_dirty=dirty, configuration=configuration,
                  platform=platform.platform(), machine=platform.machine(),
                  build_dir=str(build), jobs=[])
    for name, command, env in jobs:
        report["jobs"].append(run_job(name, command, env, output, args.timeout))
        (output / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Report: {output / 'results.json'}")
    return int(any(job["returncode"] != 0 for job in report["jobs"]))


if __name__ == "__main__":
    raise SystemExit(main())
