"""Verify developer cache publication and inherited acceptance storage boundaries."""
import hashlib
import os
import pathlib
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "tools"))
import Bootstrap
import DevelopmentPaths
import TestEnvironment


class DevelopmentPathsTests(unittest.TestCase):
    def test_default_tool_cache_survives_runtime_isolation(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, clear=True), patch.object(tempfile, "tempdir", tempfile.tempdir):
            root = pathlib.Path(directory)
            os.environ.update(HYP_OUT_ROOT=str(root / "Outputs"), LOCALAPPDATA=str(root / "OriginalLocalData"))
            original = DevelopmentPaths.tool_cache_root()
            run = TestEnvironment.ensure_isolated_environment()
            self.assertEqual(DevelopmentPaths.tool_cache_root(), original)
            self.assertEqual(pathlib.Path(os.environ["LOCALAPPDATA"]), run)

    def test_locked_archive_import_and_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            legacy = root / "OldOut"
            cache = root / "ToolCache"
            (legacy / "downloads").mkdir(parents=True)
            cache.mkdir()
            archive = legacy / "downloads/library-v1.zip"
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("repo/include/Library.h", "locked source")
            checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
            entry = {"version": "v1", "sha256": checksum}
            destination = root / "Dependencies/library"
            destination.parent.mkdir()
            Bootstrap.prepare_package("library", entry, cache, destination, legacy, True)
            self.assertEqual((destination / "include/Library.h").read_text(), "locked source")
            self.assertTrue(archive.exists())
            Bootstrap.prepare_package("library", entry, cache, destination, legacy, True)
            (cache / archive.name).write_bytes(b"corrupt")
            with self.assertRaisesRegex(RuntimeError, "Checksum mismatch"):
                Bootstrap.prepare_package("library", entry, cache, root / "Other", legacy, True)
            self.assertFalse((root / "Other").exists())

    def test_cache_identity_and_inherited_roots(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, clear=True), patch.object(tempfile, "tempdir", tempfile.tempdir):
            root = pathlib.Path(directory)
            os.environ.update(HYP_OUT_ROOT=str(root / "Outputs"), HYP_TOOL_CACHE=str(root / "Tools"))
            first = DevelopmentPaths.dependency_root({"a": 1, "b": 2})
            self.assertEqual(first, DevelopmentPaths.dependency_root({"b": 2, "a": 1}))
            self.assertNotEqual(first, DevelopmentPaths.dependency_root({"a": 2, "b": 2}))
            run = TestEnvironment.ensure_isolated_environment()
            self.assertTrue(run.is_relative_to(root / "Outputs/tests"))
            self.assertEqual(run, TestEnvironment.ensure_isolated_environment())
            for name in ("HYP_USER_DATA_ROOT", "HYP_CACHE_ROOT", "HYP_STORAGE_SETTINGS", "HYP_DISCOVERY_ROOT"):
                self.assertTrue(pathlib.Path(os.environ[name]).is_relative_to(run))
            self.assertEqual(DevelopmentPaths.tool_cache_root(), root / "Tools")


if __name__ == "__main__":
    unittest.main()
