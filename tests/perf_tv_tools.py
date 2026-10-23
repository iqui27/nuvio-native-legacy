"""Headless CSV and route contract check; all device commands use a fake adb."""
import csv
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("perf_compare", ROOT / "tools/perf-compara.py")
perf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(perf)


def run(*args, **kwargs):
    return subprocess.run(args, cwd=ROOT, text=True, capture_output=True, **kwargs)


with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    row = dict(zip(perf.FIELDS, ("home", 2, 10, 20, 30, 0, 1, 2, 3, 4)))
    other = dict(row, frames=6, p50_ms=20, p95_ms=40, max_ms=60, over33=3, uploads_pf=3)
    grouped = perf.aggregate([row, other])["home"]
    assert grouped["frames"] == 8 and grouped["over33"] == 3
    assert grouped["p95_ms"] == 35 and grouped["max_ms"] == 60
    assert grouped["uploads_pf"] == 2.5
    for name, rows in (("a.csv", [row]), ("b.csv", [other])):
        with (tmp / name).open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=perf.FIELDS)
            writer.writeheader()
            writer.writerows(rows)
    result = run("python3", "tools/perf-compara.py", str(tmp / "a.csv"), str(tmp / "b.csv"))
    assert result.returncode == 0, result.stderr
    assert "+20.00" in result.stdout and "+2.000" in result.stdout
    (tmp / "bad.csv").write_text("secret_url\nPRIVATE\n")
    bad = run("python3", "tools/perf-compara.py", str(tmp / "bad.csv"), str(tmp / "a.csv"))
    assert bad.returncode == 2 and "PRIVATE" not in bad.stdout + bad.stderr

    bindir = tmp / "bin"
    bindir.mkdir()
    fake = bindir / "adb"
    fake.write_text('''#!/bin/bash
set -eu
[[ $1 == -s && $2 == mock-tv ]]
shift 2
printf '%s\\n' "$*" >> "$MOCK_CALLS"
case "$*" in
  'shell getprop debug.nuvio.perf') echo "${MOCK_ORIGINAL:-0}" ;;
  'logcat -v raw -s nuvio:I NVPerfCapture:I') exec cat "$MOCK_EVENTS" ;;
  'shell log -t NVPerfCapture '*)
    { printf '%s\\n' "$5"
      printf '%s\\n' 'PRIVATE_URL_DO_NOT_KEEP' '[perf-tv] screen=home frames=2 p50_ms=10.00 p95_ms=20.00 max_ms=20.00 over33=0 uploads_pf=1.000 text_pf=2.000 blur_pf=3.000 glass_pf=4.000'
    } > "$MOCK_EVENTS"
    ;;
  'shell input keyevent KEYCODE_F1') [[ ${MOCK_FAIL:-0} == 0 ]] ;;
  'shell input keyevent '*|'shell setprop debug.nuvio.perf '*|'shell am start -n '*) ;;
  *) exit 99 ;;
esac
''')
    fake.chmod(0o755)
    sleeper = bindir / "sleep"
    sleeper.write_text("#!/bin/bash\nexit 0\n")
    sleeper.chmod(0o755)
    events = tmp / "events"
    os.mkfifo(events)
    env = dict(os.environ, PATH=f"{bindir}:{os.environ['PATH']}",
               MOCK_CALLS=str(tmp / "calls"), MOCK_EVENTS=str(events))
    out = tmp / "capture"
    result = run("bash", "tools/perf-tv.sh", "mock-tv", str(out), env=env, timeout=40)
    assert result.returncode == 0, result.stdout + result.stderr
    assert len(perf.read_csv(out / "perf.csv")) == 1
    assert "MISSING:" in (out / "summary.txt").read_text()
    assert "PRIVATE" not in (out / "perf.log").read_text() + result.stdout + result.stderr
    calls = (tmp / "calls").read_text().splitlines()
    assert calls.count("shell input keyevent KEYCODE_F2") == 9
    assert calls.count("shell input keyevent KEYCODE_DPAD_RIGHT") == 24 * 6 + 8 * 6
    assert calls.count("shell input keyevent KEYCODE_F12") == 1
    assert "shell setprop debug.nuvio.perf 0" in calls
    assert not any("clear" in call or "uninstall" in call or "http" in call for call in calls)
    before = len(calls)
    again = run("bash", "tools/perf-tv.sh", "mock-tv", str(out), env=env)
    assert again.returncode == 2 and len((tmp / "calls").read_text().splitlines()) == before
    failed = run("bash", "tools/perf-tv.sh", "mock-tv", str(tmp / "interrupted"),
                 env=dict(env, MOCK_FAIL="1", MOCK_ORIGINAL="1"), timeout=40)
    assert failed.returncode != 0
    assert (tmp / "calls").read_text().splitlines()[-1] == "shell setprop debug.nuvio.perf 1"
print("perf-tv CSV/comparison/fake-device route: PASS")
