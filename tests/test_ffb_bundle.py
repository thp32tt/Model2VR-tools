import importlib.util
import tempfile
import unittest
import zipfile
from pathlib import Path

MODULE = Path(__file__).resolve().parents[1] / "tools" / "assemble_model2_ffb.py"
spec = importlib.util.spec_from_file_location("ffb_bundle", MODULE)
ffb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ffb)


class FFBBundleTests(unittest.TestCase):
    def test_ini_game_id_is_scoped_to_settings(self):
        self.assertTrue(ffb.contains_game_id_25("[Settings]\nGameId=25\n[Other]\nGameId=12\n"))
        self.assertFalse(ffb.contains_game_id_25("[Settings]\nGameId=20\n[Other]\nGameId=25\n"))
        self.assertFalse(ffb.contains_game_id_25("[Settings]\nGameId=250\n"))

    def test_key_update_preserves_other_sections(self):
        text = "[Settings]\nGameId=25\nInputDeviceWheelEnable=0\n[Daytona]\nGain=30\n"
        updated = ffb.set_key(text, "Settings", "InputDeviceWheelEnable", 1)
        self.assertIn("InputDeviceWheelEnable=1", updated)
        self.assertIn("[Daytona]\nGain=30", updated)
        self.assertEqual(updated.count("InputDeviceWheelEnable="), 1)

    def test_combined_package_is_single_owner_and_non_destructive(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source"
            binary = source / "Release.Win32" / "M2Emulator" / "dinput8.dll"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"single-ffb-owner")
            sdl = source / "packages" / "win-x86" / "SDL2.dll"
            sdl.parent.mkdir(parents=True)
            sdl.write_bytes(b"single-sdl-backend")
            (source / "LICENSE").write_text("GPL-3", encoding="utf-8")
            (source / "MODEL2_MODERN_FFB.md").write_text("Modern FFB v2", encoding="utf-8")
            official = root / "upstream.zip"
            with zipfile.ZipFile(official, "w") as z:
                z.writestr("Package/Other/FFBPlugin.ini", "[Settings]\nGameId=20\n")
                z.writestr("Package/Model2/FFBPlugin.ini",
                           "[Settings]\nGameId=25\nInputDeviceWheelEnable=0\n"
                           "[Model2Modern]\nConstantGain=91\n")
                z.writestr("Package/Model2/FFBPluginGUI.exe", "existing-gui")
                z.writestr("Package/Model2/MetroFramework.dll", "runtime")
            target = root / "bundle" / "FFB"
            ffb.assemble(official, source, target)
            self.assertEqual((target / "dinput8.dll").read_bytes(), b"single-ffb-owner")
            self.assertEqual((target / "SDL2.dll").read_bytes(), b"single-sdl-backend")
            content = (target / "FFBPlugin.ini").read_text(encoding="utf-8")
            self.assertTrue(ffb.contains_game_id_25(content))
            self.assertIn("ConstantGain=91", content)
            self.assertIn("InputDeviceWheelEnable=1", content)
            self.assertIn("Model2ModernFFB=1", content)
            self.assertIn("[Model2Modern.Daytona]", content)
            self.assertTrue((target / "LICENSE-FFBArcadePlugin-GPLv3.txt").is_file())
            with self.assertRaises(ValueError):
                ffb.assemble(official, source, target)

    def test_archive_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source"
            compiled = source / "Release.Win32" / "M2Emulator" / "dinput8.dll"
            compiled.parent.mkdir(parents=True)
            compiled.write_bytes(b"dinput")
            zipfile_path = root / "untrusted.zip"
            with zipfile.ZipFile(zipfile_path, "w") as z:
                z.writestr("../bad.txt", "malicious")
            with self.assertRaises(ValueError):
                ffb.assemble(zipfile_path, source, root / "FFB")
            self.assertFalse((root / "bad.txt").exists())


if __name__ == "__main__":
    unittest.main()
