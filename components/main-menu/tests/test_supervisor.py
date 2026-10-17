"""Run the actual BusyBox supervisor in a temporary simulated board filesystem."""
import pathlib, re, subprocess, tempfile

BASE = pathlib.Path(__file__).resolve().parents[1]
PACKAGE = BASE/"cherrypi-main-menu" if (BASE/"cherrypi-main-menu").is_dir() else BASE

def run_case(mode):
    with tempfile.TemporaryDirectory(prefix="cherrypi-supervisor-") as directory:
        root = pathlib.Path(directory)
        for name in ("etc", "run", "tmp", "mnt/games/roms", "root/gpsp", "root/audio-tools", "usr/local/bin", "usr/local/sbin", "sbin", "proc/bus/input"):
            (root / name).mkdir(parents=True, exist_ok=True)
        def put(name, content, executable=False):
            p = root / name
            p.write_text(content)
            if executable: p.chmod(0o755)
        put("etc/cherrypi-autostart.conf", "VOLUME=45\n")
        put("proc/mounts", f"/dev/mmcblk0p3 {root}/mnt/games vfat rw 0 0\n")
        put("proc/bus/input/devices", 'Name="cherrypi-gamepad"\n')
        (root / "root/gpsp/gba_bios.bin").write_bytes(bytes(16384))
        for name in ("root/gpsp/gpsp", "root/audio-tools/amixer", "root/audio-tools/audio-mode", "usr/local/sbin/cherrypi-device-control"):
            put(name, "#!/bin/sh\nexit 0\n", True)
        put("root/gpsp/run-gpsp.sh", "#!/bin/sh\nexport SDL_AUDIODRIVER=dsp\n", True)
        put("sbin/getty", f"#!/bin/sh\necho getty >> '{root}/events'\nexit 0\n", True)
        put("sbin/poweroff", f"#!/bin/sh\necho poweroff >> '{root}/events'\nkill -TERM \"$PPID\"\n", True)
        if mode == "crash": body = "exit 1"
        elif mode == "poweroff": body = f"touch '{root}/run/cherrypi-poweroff-request'\nexit 0"
        else:
            body = f"""if [ \"$n\" -eq 1 ]; then
touch '{root}/run/cherrypi-launcher-game'
exit {7 if mode == 'game-error' else 0}
fi
{f"test -f '{root}/run/cherrypi-launcher-error' || exit 99" if mode == 'game-error' else ':'}
touch '{root}/run/cherrypi-autostart.paused'
exit 0"""
        put("usr/local/bin/cherrypi-launcher", f"""#!/bin/sh
n=0
[ ! -f '{root}/count' ] || n=$(cat '{root}/count')
n=$((n + 1))
echo "$n" > '{root}/count'
echo launch >> '{root}/events'
{body}
""", True)
        if mode == "recovery": put("mnt/games/DISABLE_GBA_AUTOSTART", "")
        source = (PACKAGE / "supervisor.sh").read_text()
        # Adapt only hardware paths, tty precondition and delays; keep lifecycle logic intact.
        source = re.sub(r'''/(?:usr/local|root|etc|run|tmp|mnt|proc|sbin)(?=/|[\s:"'])''', lambda m: str(root)+m[0], source)
        source = source.replace('[ -t 0 ]', 'true').replace("sleep 1", "sleep 0.01").replace("sleep 60", "sleep 0.01")
        source = source.replace("    sync\n", "    :\n")
        script = root / "supervisor.sh"
        script.write_text(source)
        result = subprocess.run(["busybox", "sh", str(script)], capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stdout + result.stderr
        events = (root / "events").read_text().splitlines()
        expected = {"game": ["launch", "launch", "getty"], "game-error": ["launch", "launch", "getty"], "crash": ["launch"]*3+["getty"], "poweroff": ["launch", "poweroff"], "recovery": ["getty"]}[mode]
        assert events == expected, (mode, events, result.stdout, result.stderr)
        assert not (root / "run/cherrypi-autostart.ready").exists()

for mode in ("game", "game-error", "crash", "poweroff", "recovery"):
    run_case(mode)
print("PASS: supervisor game exit/error return, repeated crash recovery, confirmed poweroff, SD recovery marker")
