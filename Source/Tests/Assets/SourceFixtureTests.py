"""Regression coverage for independent Engine and Game source fixture manifests."""
import hashlib
import json
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "tools"))
import PrepareTestSources


class SourceFixtureTests(unittest.TestCase):
    def test_clean_fixture_and_missing_engine_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            game = root / "Game"
            engine_cache = root / "EngineCache"
            output = root / "Fixtures"
            for manifest_path, cache, name in (
                    (game / "Metadata/Sources.json", game / ".cache/Sources", "Clear.hdr"),
                    (root / "Content/Metadata/DefaultSkySources.json", engine_cache, "Cloudy.hdr")):
                data = name.encode()
                source = cache / "Skies" / name
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(data)
                manifest_path.parent.mkdir(parents=True, exist_ok=True)
                manifest_path.write_text(json.dumps({
                    "recipes": {}, "downloads": [{"path": "Skies/" + name,
                    "sha256": hashlib.sha256(data).hexdigest(), "url": "https://unused.invalid"}]}))
            args = ["PrepareTestSources.py", "--assets-root", str(game),
                    "--engine-sky-cache", str(engine_cache), "--output", str(output)]
            with patch.object(PrepareTestSources, "ROOT", root), patch.object(sys, "argv", args):
                PrepareTestSources.main()
            self.assertEqual((output / "Skies/Cloudy.hdr").read_bytes(), b"Cloudy.hdr")
            self.assertEqual((output / "Skies/Clear.hdr").read_bytes(), b"Clear.hdr")
            args[-1] = str(root / "MissingFixtures")
            args[4] = str(root / "MissingEngineCache")
            with patch.object(PrepareTestSources, "ROOT", root), patch.object(sys, "argv", args):
                with self.assertRaisesRegex(RuntimeError, "Engine sky source fixtures are unavailable"):
                    PrepareTestSources.main()


if __name__ == "__main__":
    unittest.main()
