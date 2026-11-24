"""Exercise ready and run with an existing library entry and a versioned pack."""

import contextlib
import hashlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import vitarecomp


class LaunchVersionTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)
        packs = root / "packs"
        pack = packs / "TEST00001"
        pack.mkdir(parents=True)
        self.entry = root / "decrypted" / "TEST00001"
        (self.entry / "elf").mkdir(parents=True)
        self.elf = self.entry / "elf" / "eboot.bin.elf"
        self.version = self.entry / "version"
        digest = hashlib.sha256(b"supported fixture").hexdigest()
        (pack / "pack.toml").write_text(
            'UPDATE_NOTE = "Install update 2.00."\n'
            f'[versions."2.00"]\nsha256 = "{digest}"\n',
            encoding="utf-8",
        )
        env = vitarecomp.ENV.copy()
        env.pop("FORCE", None)
        overrides = patch.multiple(vitarecomp, DUMP=str(root), PACKS=str(packs), ENV=env)
        overrides.start()
        self.addCleanup(overrides.stop)

    def test_unlisted_copy_stops_before_ready_or_run(self):
        self.elf.write_bytes(b"unlisted fixture")
        for command in ("ready", "run"):
            for metadata in ("", "2.00\n"):
                with self.subTest(command=command, metadata=metadata):
                    self.version.write_text(metadata, encoding="utf-8")
                    out = io.StringIO()
                    with patch("vitarecomp.run") as build, patch("vitarecomp.tee_run", return_value=0) as launch, patch("vitarecomp.Game.entry", return_value="0"):
                        with contextlib.redirect_stdout(out), self.assertRaises(SystemExit) as result:
                            vitarecomp.main([command, "TEST00001"])
                    self.assertEqual(result.exception.code, 1)
                    self.assertIn("Install update 2.00.", out.getvalue())
                    build.assert_not_called()
                    launch.assert_not_called()

    def test_supported_legacy_copy_records_version_and_runs(self):
        self.elf.write_bytes(b"supported fixture")
        with patch("vitarecomp.cmd_run") as run:
            vitarecomp.main(["run", "TEST00001"])
        self.assertEqual(run.call_args.args[0].VERSION, "2.00")
        self.assertEqual(self.version.read_text(encoding="utf-8").strip(), "2.00")

    def test_force_still_allows_unlisted_copy(self):
        self.elf.write_bytes(b"unlisted fixture")
        with patch.dict(vitarecomp.ENV, FORCE="1"):
            with contextlib.redirect_stdout(io.StringIO()), patch("vitarecomp.Game.entry", return_value="0"):
                with patch("vitarecomp.tee_run", return_value=0) as run, self.assertRaises(SystemExit) as result:
                    vitarecomp.main(["run", "TEST00001"])
        self.assertEqual(result.exception.code, 0)
        run.assert_called_once()

    def test_ready_imports_installed_supported_update_before_rebuilding(self):
        self.elf.write_bytes(b"unlisted fixture")
        update = self.entry / "patch0" / self.elf.name
        update.parent.mkdir()
        update.write_bytes(b"supported fixture")
        recomp = self.entry / "recomp"
        recomp.mkdir()
        built_stamp = recomp / ".built_version"
        built_stamp.write_text("previous build", encoding="utf-8")
        commands = []

        def step(argv):
            command = argv[-2]
            commands.append(command)
            if command == "import":
                self.elf.write_bytes(update.read_bytes())
            elif command == "analyze":
                self.assertFalse(built_stamp.exists())

        with contextlib.redirect_stdout(io.StringIO()), patch("vitarecomp.run", side_effect=step):
            with patch("vitarecomp.recompile_inputs", return_value="fixture"):
                vitarecomp.main(["ready", "TEST00001"])
        self.assertEqual(commands, ["import", "analyze", "recompile", "build"])
        self.assertEqual(self.version.read_text(encoding="utf-8").strip(), "2.00")

    def test_recompile_stamp_changes_with_the_imported_executable(self):
        self.elf.write_bytes(b"unlisted fixture")
        game = vitarecomp.Game("TEST00001", "ready")
        before = vitarecomp.recompile_inputs(game)
        self.elf.write_bytes(b"supported fixture")
        self.assertNotEqual(before, vitarecomp.recompile_inputs(game))

    def test_export_data_follows_the_imported_tree(self):
        self.elf.write_bytes(b"supported fixture")
        base = self.entry / "app0"
        layered = self.entry / "app0_patched"
        base.mkdir()
        layered.mkdir()
        runtime_app0 = self.entry / "recomp" / "fs" / "app0"
        runtime_app0.parent.mkdir(parents=True)
        game = vitarecomp.Game("TEST00001", "export-apk")
        for directory, content in ((base, "base data"), (layered, "updated data")):
            with self.subTest(content=content):
                (directory / "fixture.txt").write_text(content, encoding="utf-8")
                vitarecomp.link_dir(directory, runtime_app0)
                exported = Path(vitarecomp.export_app0(game))
                self.assertEqual((exported / "fixture.txt").read_text(encoding="utf-8"), content)


if __name__ == "__main__":
    unittest.main()
