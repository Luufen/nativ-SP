"""A built game as an APK for the player's own Android device: vitarecomp export-apk (EXPORT-APK A3).

The APK holds the compiled game (lib/<abi>/libmain.so, made by runtime/build.py TARGET=android-<abi>) and its images
from the player's copy: it is for their own device, never for anyone else. The player copies it to the device and
installs it. The game's files (app0) come one of two ways (SetupActivity puts them into
Android/data/<package>/files/fs/app0 at the first start): inside the APK (assets/nativ-data/app0.zip) when they are
small enough, else as a data file beside it (<name>-data.zip) that the player copies too and picks there. Signed with
a key of the player's own, made once. The pieces, each testable without a game (dev/test_export_apk.py):

  package_id()    local.nativ.<title id>: one app per game
  manifest_text() the AndroidManifest.xml: SDL's Activity settings, our NativActivity (runtime/platform/android)
  launch_text()   assets/nativ/launch.txt: the game's arguments and environment, as `vitarecomp run` gives them
  build_apk()     aapt2 (manifest, icon, assets), javac + d8 (NativActivity with SDL's Java side), the libraries,
                  zipalign, apksigner -- the Android toolchain's (runtime/platform/android/setup-toolchain.sh)
"""
import hashlib
import os
import re
import shutil
import subprocess
import tempfile
import zipfile
from xml.sax.saxutils import escape

NOTICE = ("This APK holds the compiled game and the game's files from YOUR copy: it is private, for your own device. "
          "Don't share or upload it.")
BUILD_TOOLS = "36.0.0"
PLATFORM = "36"
MIN_SDK = 29
ABIS = {"arm64": "arm64-v8a", "x86_64": "x86_64"}
ACTIVITY = "org.projectnativ.app.NativActivity"   # (the game: SDL's Activity)
SETUP = "org.projectnativ.app.SetupActivity"      # (the launcher's entry: the game's files first, then the game)
INSIDE_MAX = 1_500_000_000   # app0 up to this size goes inside the APK (export-apk --data auto); bigger: a data file apart
KEY_ALIAS = "nativ"
KEY_PASS = "nativ-local"   # (a key that signs the player's own sideloaded apps on their own machine: not a secret kept)


def toolchain(at=None):
    at = at or os.environ.get("ATIV_ANDROID_TOOLCHAIN") or os.path.expanduser("~/toolchains/ativ-android")
    bt = os.path.join(at, "sdk", "build-tools", BUILD_TOOLS)
    t = {"root": at, "aapt2": os.path.join(bt, "aapt2"), "d8": os.path.join(bt, "d8"), "zipalign": os.path.join(bt, "zipalign"),
         "apksigner": os.path.join(bt, "apksigner"), "jar": os.path.join(at, "sdk", "platforms", "android-" + PLATFORM, "android.jar"),
         "java_home": os.path.join(at, "jdk"), "sdl_jar": os.path.join(at, "sdl-java", "classes.jar"),
         "strip": os.path.join(at, "ndk", "toolchains", "llvm", "prebuilt", "linux-x86_64", "bin", "llvm-strip")}
    missing = [k for k in ("aapt2", "d8", "zipalign", "apksigner", "jar", "sdl_jar") if not os.path.isfile(t[k])]
    if missing or not os.path.isfile(os.path.join(t["java_home"], "bin", "javac")):
        raise RuntimeError(f"no Android toolchain at {at} ({', '.join(missing) or 'jdk'} missing): "
                           "bash runtime/platform/android/setup-toolchain.sh")
    return t


def package_id(title):
    """local.nativ.<title id in lower case>; ValueError for anything but a title id"""
    if not re.fullmatch(r"[A-Z]{4}\d{5}", title or ""): raise ValueError(f"not a title id: {title!r}")
    return "local.nativ." + title.lower()


def manifest_text(package, label, version_code=1, version_name="1.0", target_sdk=int(PLATFORM)):
    return f"""<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="{package}"
    android:versionCode="{int(version_code)}" android:versionName="{escape(version_name, {'"': '&quot;'})}" android:installLocation="auto">
    <uses-sdk android:minSdkVersion="{MIN_SDK}" android:targetSdkVersion="{target_sdk}" />
    <uses-feature android:glEsVersion="0x00020000" />
    <uses-feature android:name="android.hardware.touchscreen" android:required="false" />
    <uses-feature android:name="android.hardware.gamepad" android:required="false" />
    <uses-feature android:name="android.hardware.bluetooth" android:required="false" />
    <uses-feature android:name="android.hardware.usb.host" android:required="false" />
    <uses-feature android:name="android.hardware.type.pc" android:required="false" />
    <uses-permission android:name="android.permission.VIBRATE" />
    <application android:label="{escape(label, {'"': '&quot;'})}" android:icon="@mipmap/icon" android:allowBackup="false"
        android:theme="@android:style/Theme.NoTitleBar.Fullscreen" android:hardwareAccelerated="true"
        android:extractNativeLibs="true" android:enableOnBackInvokedCallback="false">
        <activity android:name="{SETUP}" android:label="{escape(label, {'"': '&quot;'})}" android:exported="true"
            android:launchMode="singleTask" android:screenOrientation="sensorLandscape"
            android:configChanges="orientation|screenSize|keyboardHidden"
            android:theme="@android:style/Theme.DeviceDefault.NoActionBar.Fullscreen">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
            <intent-filter>
                <action android:name="android.intent.action.VIEW" />
                <category android:name="android.intent.category.DEFAULT" />
                <data android:scheme="content" android:mimeType="application/zip" />
            </intent-filter>
        </activity>
        <activity android:name="{ACTIVITY}" android:label="{escape(label, {'"': '&quot;'})}"
            android:process=":game" android:screenOrientation="sensorLandscape"
            android:configChanges="layoutDirection|locale|grammaticalGender|fontScale|fontWeightAdjustment|orientation|uiMode|screenLayout|screenSize|smallestScreenSize|keyboard|keyboardHidden|navigation"
            android:preferMinimalPostProcessing="true" android:exported="false">
        </activity>
    </application>
</manifest>
"""


def launch_text(args, env):
    """launch.txt: "arg <value>" per argument, "env <NAME>=<value>" per setting; no line breaks inside a value"""
    lines = []
    for a in args:
        if "\n" in str(a): raise ValueError("a line break in an argument")
        lines.append(f"arg {a}")
    for k, v in env:
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", k) or "\n" in str(v): raise ValueError(f"not a setting: {k}")
        lines.append(f"env {k}={v}")
    return "\n".join(lines) + "\n"


def app0_files(app0):
    """(relative path with /, size) of every file under app0, sorted"""
    out = []
    for d, sub, names in os.walk(app0):
        sub.sort()
        for n in sorted(names):
            p = os.path.join(d, n)
            out.append((os.path.relpath(p, app0).replace(os.sep, "/"), os.path.getsize(p)))
    return sorted(out)


def data_id(app0, title):
    """which game files these are: the title, every file's path and size, and eboot.bin's content (sha256, 32 hex)"""
    h = hashlib.sha256(title.encode() + b"\0")
    for rel, size in app0_files(app0): h.update(f"{rel}\0{size}\n".encode())
    eboot = os.path.join(app0, "eboot.bin")
    if os.path.isfile(eboot):
        with open(eboot, "rb") as f:
            for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()[:32]


def write_data_zip(app0, out, title, did):
    """app0 as the zip SetupActivity unpacks: nativ-data.txt first (title, id), then app0/... stored (game files are
    mostly compressed already; storing keeps the export and the unpacking fast); zip64 for big games"""
    tmp = out + ".part"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_STORED, allowZip64=True) as z:
        z.writestr("nativ-data.txt", f"title {title}\nid {did}\n")
        for rel, _size in app0_files(app0): z.write(os.path.join(app0, rel), "app0/" + rel)
    os.replace(tmp, out)
    return out


def data_text(mode, did, file, size):
    """assets/nativ/data.txt: how the game's files come (SetupActivity)"""
    if mode not in ("inside", "apart"): raise ValueError(mode)
    return f"mode {mode}\nid {did}\nfile {file}\nbytes {int(size)}\n"


def ensure_key(keystore, t, run=subprocess.run):
    """the player's signing key, made once (keytool: RSA 2048, 10000 days)"""
    if os.path.isfile(keystore): return False
    os.makedirs(os.path.dirname(os.path.abspath(keystore)), exist_ok=True)
    run([os.path.join(t["java_home"], "bin", "keytool"), "-genkeypair", "-keystore", keystore, "-storetype", "PKCS12",
         "-alias", KEY_ALIAS, "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
         "-storepass", KEY_PASS, "-keypass", KEY_PASS, "-dname", "CN=Project Nativ local export"],
        check=True, capture_output=True)
    return True


def build_apk(out, *, package, label, libs, assets, icon, keystore, t, version_code=1, version_name="1.0", work=None, extra_assets=()):
    """libs: {abi: [paths of .so files]}; assets: (source, name) pairs under assets/nativ/; extra_assets: (source, path)
    pairs under assets/ (the game's files inside: nativ-data/app0.zip); icon: a PNG"""
    env = dict(os.environ, JAVA_HOME=t["java_home"], PATH=os.path.join(t["java_home"], "bin") + os.pathsep + os.environ.get("PATH", ""))
    def run(argv): subprocess.run(argv, check=True, env=env)
    w = work or tempfile.mkdtemp(prefix="nativ-apk-")
    try:
        platform_src = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "runtime", "platform", "android"))
        java_sources = [os.path.join(platform_src, name) for name in ("NativActivity.java", "SetupActivity.java", "DriverStore.java")]
        libs = {abi: list(files) for abi, files in libs.items()}
        if "arm64-v8a" in libs:
            driver_work = os.path.abspath(os.path.join(w, "driver"))
            run(["bash", os.path.join(platform_src, "driver", "build.sh"), os.path.abspath(t["root"]), driver_work])
            driver_build = os.path.join(driver_work, "build")
            libs["arm64-v8a"] += [os.path.join(driver_build, "libnativ_vulkan.so")]
            libs["arm64-v8a"] += [os.path.join(driver_build, "adrenotools", "src", "hook", name)
                                  for name in ("libmain_hook.so", "libhook_impl.so")]
            source = os.path.join(driver_work, "source")
            extra_assets = [*extra_assets,
                            (os.path.join(source, "LICENSE"), "licenses/libadrenotools.txt"),
                            (os.path.join(source, "lib", "linkernsbypass", "LICENSE"), "licenses/liblinkernsbypass.txt")]
        os.makedirs(os.path.join(w, "res", "mipmap"), exist_ok=True)
        shutil.copyfile(icon, os.path.join(w, "res", "mipmap", "icon.png"))
        a = os.path.join(w, "assets", "nativ"); os.makedirs(a, exist_ok=True)
        for src, name in assets: shutil.copyfile(src, os.path.join(a, name))
        for src, rel in extra_assets:
            p = os.path.join(w, "assets", rel); os.makedirs(os.path.dirname(p), exist_ok=True)
            try: os.link(src, p)   # (a big zip: no copy where a link will do)
            except OSError: shutil.copyfile(src, p)
        with open(os.path.join(w, "AndroidManifest.xml"), "w", encoding="utf-8", newline="\n") as f:
            f.write(manifest_text(package, label, version_code, version_name))
        run([t["aapt2"], "compile", "-o", os.path.join(w, "res.zip"), "--dir", os.path.join(w, "res")])
        base = os.path.join(w, "base.apk")
        run([t["aapt2"], "link", "-o", base, "-I", t["jar"], "--manifest", os.path.join(w, "AndroidManifest.xml"),
             "-A", os.path.join(w, "assets"), "--min-sdk-version", str(MIN_SDK), "--target-sdk-version", PLATFORM,
             "--version-code", str(version_code), "--version-name", version_name, "-0", "zip", os.path.join(w, "res.zip")])   # (-0 zip: the data zip stored as it is)
        cls = os.path.join(w, "classes"); os.makedirs(cls, exist_ok=True)
        run([os.path.join(t["java_home"], "bin", "javac"), "-nowarn", "--release", "11", "-encoding", "UTF-8",
             "-classpath", t["jar"] + os.pathsep + t["sdl_jar"], "-d", cls] + java_sources)
        ours = os.path.join(w, "ours.jar")
        run([os.path.join(t["java_home"], "bin", "jar"), "cf", ours, "-C", cls, "."])
        dex = os.path.join(w, "dex"); os.makedirs(dex, exist_ok=True)
        run([t["d8"], "--release", "--min-api", str(MIN_SDK), "--lib", t["jar"], "--output", dex, t["sdl_jar"], ours])
        with zipfile.ZipFile(base, "a", zipfile.ZIP_DEFLATED) as z:
            z.write(os.path.join(dex, "classes.dex"), "classes.dex")
            for abi, files in libs.items():
                for f in files:
                    s = os.path.join(w, "strip-" + abi + "-" + os.path.basename(f))
                    run([t["strip"], "--strip-debug", "-o", s, f])
                    z.write(s, f"lib/{abi}/{os.path.basename(f)}")
        aligned = os.path.join(w, "aligned.apk")
        run([t["zipalign"], "-f", "-P", "16", "4", base, aligned])
        ensure_key(keystore, t)
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        run([t["apksigner"], "sign", "--ks", keystore, "--ks-key-alias", KEY_ALIAS, "--ks-pass", "pass:" + KEY_PASS,
             "--key-pass", "pass:" + KEY_PASS, "--out", out, aligned])
        if os.path.exists(out + ".idsig"): os.remove(out + ".idsig")
        run([t["apksigner"], "verify", out])
    finally:
        if not work: shutil.rmtree(w, ignore_errors=True)
    return out
