#!/usr/bin/env python3
"""ANDROID-PERF-08: the Android driver picker's package rules (runtime/platform/android/DriverPackage.java) on synthetic
ZIPs: the AdrenoTools layout accepted or refused, and which GPU family takes which package. No Android, no driver.

  python3 dev/test_android_driver.py     (Linux or WSL; a JDK: the Android toolchain's, ATIV_ANDROID_TOOLCHAIN or
                                          ~/toolchains/ativ-android, else javac on the PATH)
"""
import os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

at = os.environ.get("ATIV_ANDROID_TOOLCHAIN") or os.path.expanduser("~/toolchains/ativ-android")
jdk = os.path.join(at, "jdk", "bin")
javac, java = os.path.join(jdk, "javac"), os.path.join(jdk, "java")
if not os.path.isfile(javac): javac, java = shutil.which("javac"), shutil.which("java")
if not javac or not java:
    sys.exit("no JDK: bash runtime/platform/android/setup-toolchain.sh, or put javac on the PATH")

with tempfile.TemporaryDirectory() as tmp:
    classes, work = os.path.join(tmp, "classes"), os.path.join(tmp, "work")
    os.makedirs(work)
    subprocess.run([javac, "--release", "11", "-encoding", "UTF-8", "-d", classes,
                    os.path.join(ROOT, "runtime", "platform", "android", "DriverPackage.java"),
                    os.path.join(ROOT, "dev", "DriverPackageTest.java")], check=True)
    sys.exit(subprocess.run([java, "-cp", classes, "org.projectnativ.app.DriverPackageTest", work]).returncode)
