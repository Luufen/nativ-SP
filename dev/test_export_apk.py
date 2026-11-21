#!/usr/bin/env python3
"""EXPORT-APK A3: pipeline/apk.py -- the package id, manifest, launch file and a real APK made with the Android toolchain.

  python3 dev/test_export_apk.py     (Linux or WSL; the APK checks need runtime/platform/android/setup-toolchain.sh's
                                      toolchain, ATIV_ANDROID_TOOLCHAIN or ~/toolchains/ativ-android: skipped without)

The APK is built from the toolchain's own check library (check/libmain-arm64-v8a.so, a stand-in for a game's) and made-up
assets; nothing of a game is used.
"""
import os, re, subprocess, sys, tempfile, zipfile
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "pipeline"))
import apk  # noqa: E402

checks = fails = 0
A = "{http://schemas.android.com/apk/res/android}"


def check(what, ok, detail=""):
    global checks, fails
    checks += 1; fails += not ok
    print(f"{what:<80} {'ok' if ok else 'FAILED'}{('  (' + str(detail)[-300:] + ')') if detail and not ok else ''}")


def raises(f, *a):
    try: f(*a); return False
    except ValueError: return True


check("package id: local.nativ.<title id in lower case>", apk.package_id("PCSZ00042") == "local.nativ.pcsz00042")
check("package id: anything but a title id is refused", raises(apk.package_id, "../x") and raises(apk.package_id, "pcsz00042") and raises(apk.package_id, ""))
m = ET.fromstring(apk.manifest_text("local.nativ.pcsz00042", 'Game & "Two" <X>', 3, "1.00"))
act = m.find("application/activity")
check("manifest: package, version, min SDK 29", m.get("package") == "local.nativ.pcsz00042" and m.get(A + "versionCode") == "3"
      and m.get(A + "versionName") == "1.00" and m.find("uses-sdk").get(A + "minSdkVersion") == "29")
check("manifest: the label as given (& \" < > escaped in the file)", m.find("application").get(A + "label") == 'Game & "Two" <X>')
acts = {a.get(A + "name"): a for a in m.findall("application/activity")}
check("manifest: SetupActivity is the launcher entry (and opens a zip), landscape", apk.SETUP in acts and acts[apk.SETUP].get(A + "screenOrientation") == "sensorLandscape"
      and any(c.get(A + "name") == "android.intent.category.LAUNCHER" for c in acts[apk.SETUP].iter("category"))
      and any(d.get(A + "mimeType") == "application/zip" for d in acts[apk.SETUP].iter("data")))
check("manifest: NativActivity (the game) is not exported and has no launcher entry", apk.ACTIVITY in acts and acts[apk.ACTIVITY].get(A + "exported") == "false"
      and not list(acts[apk.ACTIVITY].iter("category")))
lt = apk.launch_text(["$FILES/game.img", "81000000", "100000000000"], [("RECOMP_TITLE", "PCSZ00042"), ("A", "x=y")])
check("launch.txt: an arg line per argument, an env line per setting", lt == "arg $FILES/game.img\narg 81000000\narg 100000000000\nenv RECOMP_TITLE=PCSZ00042\nenv A=x=y\n", lt)
check("launch.txt: a line break in a value or a bad name is refused",
      raises(apk.launch_text, ["a\nb"], []) and raises(apk.launch_text, [], [("A B", "1")]) and raises(apk.launch_text, [], [("A", "1\n2")]))
with tempfile.TemporaryDirectory() as tmp:   # the game's files: their id, the data zip, data.txt
    a0 = os.path.join(tmp, "app0"); os.makedirs(os.path.join(a0, "sce_sys")); os.makedirs(os.path.join(a0, "data", "sub"))
    open(os.path.join(a0, "eboot.bin"), "wb").write(b"\x7fSCE" + bytes(60))
    open(os.path.join(a0, "sce_sys", "param.sfo"), "wb").write(b"\0PSF" + bytes(28))
    open(os.path.join(a0, "data", "sub", ".hidden"), "wb").write(b"x" * 1000)   # (a dot name: aapt2 would drop it as an asset)
    i1 = apk.data_id(a0, "PCSZ00042")
    check("data id: 32 hex, the same twice, another for another title", re.fullmatch(r"[0-9a-f]{32}", i1) and apk.data_id(a0, "PCSZ00042") == i1 and apk.data_id(a0, "PCSZ00043") != i1)
    open(os.path.join(a0, "eboot.bin"), "wb").write(b"\x7fSCE" + bytes(59) + b"\1")
    check("data id: eboot.bin's content changes it (same size)", apk.data_id(a0, "PCSZ00042") != i1)
    z = apk.write_data_zip(a0, os.path.join(tmp, "g-data.zip"), "PCSZ00042", "abc123")
    zf = zipfile.ZipFile(z); names = zf.namelist()
    check("data zip: nativ-data.txt first (title, id), then app0/... every file, stored", names[0] == "nativ-data.txt"
          and zf.read(names[0]) == b"title PCSZ00042\nid abc123\n" and sorted(names[1:]) == ["app0/data/sub/.hidden", "app0/eboot.bin", "app0/sce_sys/param.sfo"]
          and all(i.compress_type == zipfile.ZIP_STORED for i in zf.infolist()), names)
    check("data.txt: mode, id, file, bytes; a wrong mode is refused", apk.data_text("apart", "abc", "g-data.zip", 1064) == "mode apart\nid abc\nfile g-data.zip\nbytes 1064\n"
          and raises(apk.data_text, "both", "a", "b", 1))
setup = open(os.path.join(ROOT, "runtime", "platform", "android", "SetupActivity.java"), encoding="utf-8").read()
check("SetupActivity: data.txt's mode / id, the APK's zip or a picked one, nativ-data.txt checked first, app0.id",
      '"nativ/data.txt"' in setup and '"nativ-data/app0.zip"' in setup and "ACTION_OPEN_DOCUMENT" in setup and '"nativ-data.txt"' in setup
      and '"app0.id"' in setup and "NativActivity.class" in setup and 'startsWith("app0/")' in setup)
java = open(os.path.join(ROOT, "runtime", "platform", "android", "NativActivity.java"), encoding="utf-8").read()
check("NativActivity reads launch.txt's arg / env lines and names the data folder",
      'startsWith("arg ")' in java and 'startsWith("env ")' in java and "NATIV_DATA" in java and "extends SDLActivity" in java)

try: t = apk.toolchain()
except RuntimeError as e: t = None; print(f"({e}: the APK checks are skipped)")
if t:
    stand_in = os.path.join(t["root"], "check", "libmain-arm64-v8a.so")
    with tempfile.TemporaryDirectory() as tmp:
        lib = os.path.join(tmp, "arm64-v8a"); os.makedirs(lib)
        for src, n in ((stand_in, "libmain.so"), (os.path.join(t["root"], "arm64-v8a", "SDL3", "lib", "libSDL3.so"), "libSDL3.so")):
            with open(src, "rb") as s, open(os.path.join(lib, n), "wb") as d: d.write(s.read())
        img = os.path.join(tmp, "game.img"); open(img, "wb").write(os.urandom(4096))
        launch = os.path.join(tmp, "launch.txt"); open(launch, "w").write(lt)
        stamp = os.path.join(tmp, "stamp.txt"); open(stamp, "w").write("1\n")
        icon = os.path.join(ROOT, "assets", "brand", "png", "icon-256.png")
        ks = os.path.join(tmp, "key", "export.keystore"); out = os.path.join(tmp, "out", "game.apk")
        kw = dict(package="local.nativ.pcsz00001", label="Test & Game", libs={"arm64-v8a": [os.path.join(lib, "libSDL3.so"), os.path.join(lib, "libmain.so")]},
                  assets=[(img, "game.img"), (launch, "launch.txt"), (stamp, "stamp.txt")], icon=icon, keystore=ks, t=t,
                  version_name="1.00")
        dz = os.path.join(tmp, "app0.zip"); zipfile.ZipFile(dz, "w").writestr("nativ-data.txt", "title PCSZ00001\nid x\n")
        kw["extra_assets"] = [(dz, "nativ-data/app0.zip")]
        try: apk.build_apk(out, **kw); err = ""
        except subprocess.CalledProcessError as e: err = str(e)
        check("build_apk: an APK made, signed and verified (apksigner verify)", os.path.isfile(out) and not err, err)
        if os.path.isfile(out):
            names = zipfile.ZipFile(out).namelist()
            check("APK: classes.dex, both libraries for arm64-v8a, the assets",
                  {"classes.dex", "lib/arm64-v8a/libmain.so", "lib/arm64-v8a/libSDL3.so", "assets/nativ/game.img",
                   "assets/nativ/launch.txt", "assets/nativ/stamp.txt", "assets/nativ-data/app0.zip", "AndroidManifest.xml"} <= set(names), names)
            check("APK: the game's files zip stored as it is (not compressed again)", zipfile.ZipFile(out).getinfo("assets/nativ-data/app0.zip").compress_type == zipfile.ZIP_STORED)
            check("APK: ARM64 Vulkan bridge, its hooks, and their notices are present",
                  {"lib/arm64-v8a/libnativ_vulkan.so", "lib/arm64-v8a/libmain_hook.so", "lib/arm64-v8a/libhook_impl.so",
                   "assets/licenses/libadrenotools.txt", "assets/licenses/liblinkernsbypass.txt"} <= set(names), names)
            check("APK: no .idsig left beside it", not os.path.exists(out + ".idsig"))
            dex = zipfile.ZipFile(out).read("classes.dex")
            check("classes.dex: NativActivity, SetupActivity and SDL's SDLActivity", b"Lorg/projectnativ/app/NativActivity;" in dex and b"Lorg/projectnativ/app/SetupActivity;" in dex and b"Lorg/libsdl/app/SDLActivity;" in dex)
            b = subprocess.run([t["aapt2"], "dump", "badging", out], capture_output=True, text=True).stdout
            check("aapt2 badging: package, label, SDK 29, launchable SetupActivity, native code arm64-v8a",
                  "package: name='local.nativ.pcsz00001'" in b and "application-label:'Test & Game'" in b and re.search(r"(?m)^(min)?[sS]dkVersion:'29'", b)
                  and f"launchable-activity: name='{apk.SETUP}'" in b and "native-code: 'arm64-v8a'" in b, b[:600])
            env = dict(os.environ, JAVA_HOME=t["java_home"], PATH=os.path.join(t["java_home"], "bin") + os.pathsep + os.environ.get("PATH", ""))
            cert = lambda p: re.findall(r"SHA-256 digest: (\w+)", subprocess.run([t["apksigner"], "verify", "--print-certs", p], capture_output=True, text=True, env=env).stdout)
            c1, kt = cert(out), os.path.getmtime(ks)
            out2 = os.path.join(tmp, "out", "again.apk"); apk.build_apk(out2, **kw)
            check("the key is made once: a second APK is signed with the same certificate", c1 and cert(out2) == c1 and os.path.getmtime(ks) == kt, (c1, cert(out2)))

print(f"{checks} checks, {'all passed' if not fails else str(fails) + ' failed'}")
sys.exit(1 if fails else 0)
