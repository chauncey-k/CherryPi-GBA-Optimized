"""Exercise installation and rollback without touching the board or host /root."""
import hashlib, os, pathlib, re, shutil, subprocess, tempfile
BASE = pathlib.Path(__file__).resolve().parents[1]
PACKAGE = BASE/"cherrypi-main-menu" if (BASE/"cherrypi-main-menu").is_dir() else BASE
with tempfile.TemporaryDirectory(prefix="cherrypi-install-") as directory:
    root = pathlib.Path(directory)
    for name in ("root/gpsp", "root/cherrypi-main-menu", "usr/local/bin", "usr/local/sbin", "etc", "mock-bin"):
        (root/name).mkdir(parents=True, exist_ok=True)
    package = root/"root/cherrypi-main-menu"
    for p in PACKAGE.iterdir():
        if p.is_file(): shutil.copy2(p,package/p.name)
    for name in ("source","tests"):
        shutil.copytree(BASE/name,package/name)
    shutil.copy2(BASE/"build.sh",package/"build.sh")
    def executable(name, data):
        p=root/name;p.write_text(data);p.chmod(0o755);return p
    original=b"#!/bin/sh\n# previous verified supervisor\n"
    target=executable("usr/local/sbin/cherrypi-autostart",original.decode())
    for name in ("root/gpsp/gpsp", "root/gpsp/run-gpsp.sh", "usr/local/sbin/cherrypi-device-control"):
        executable(name,"#!/bin/sh\nexit 0\n")
    (root/"etc/inittab").write_text(f"ttyS0::respawn:{target}\n")
    config=root/"etc/cherrypi-autostart.conf";config.write_text("VOLUME=45\nBRIGHTNESS=6\n")
    executable("mock-bin/pidof","#!/bin/sh\nexit 1\n")
    executable("mock-bin/sync","#!/bin/sh\nexit 0\n")
    env=dict(os.environ,PATH=str(root/"mock-bin")+":"+os.environ["PATH"])
    def script(name):
        s=(PACKAGE/name).read_text()
        s=re.sub(r'''/(?:usr/local|root|etc)(?=/|[\s:"'])''',lambda m:str(root)+m[0],s)
        s=s.replace('[ "$(id -u)" -eq 0 ]',"true")
        path=package/("test-"+name);path.write_text(s);return path
    install=script("install.sh");uninstall=script("uninstall.sh")
    def run(p,ok=True):
        r=subprocess.run(["busybox","sh",str(p)],env=env,capture_output=True,text=True)
        assert (r.returncode==0)==ok, r.stdout+r.stderr
    run(install)
    installed=(root/"usr/local/bin/cherrypi-launcher").read_bytes()
    assert installed==(package/"cherrypi-launcher").read_bytes()
    assert target.read_bytes()==(package/"supervisor.sh").read_bytes()
    run(install)
    assert (package/"state/supervisor.sh").read_bytes()==original
    run(uninstall)
    assert target.read_bytes()==original and not (root/"usr/local/bin/cherrypi-launcher").exists()
    assert config.read_text()=="VOLUME=45\nBRIGHTNESS=6\n"
    # A corrupted package must be rejected before replacing either target.
    with (package/"cherrypi-launcher").open("ab") as f:f.write(b"corrupt")
    run(install,False)
    assert target.read_bytes()==original
print("PASS: checksum rejection, install, idempotent backup, rollback, preserved configuration")
