import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "main/boards/otto-robot/nailong_assets"


class NailongMaterialTests(unittest.TestCase):
    def test_images_keep_original_bytes_and_have_distinct_roles(self):
        manifest = json.loads((ASSETS / "nl_materials.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["images"]["nl_idle.raw"]["source"], "9.raw")
        self.assertEqual(manifest["images"]["nl_laugh.raw"]["source"], "2.raw")
        replies = [item["source"] for name, item in manifest["images"].items() if "reply" in name]
        self.assertEqual(set(replies), {f"{i}.raw" for i in (1, 3, 4, 5, 6, 7, 8)})
        for name, record in manifest["images"].items():
            data = (ASSETS / name).read_bytes()
            self.assertEqual(len(data), 240 * 240 * 2)
            self.assertEqual(hashlib.sha256(data).hexdigest(), record["sha256"])

    def test_laugh_is_mono_opus_with_five_seconds_of_audio(self):
        data = (ASSETS / "nl_laugh.ogg").read_bytes()
        offset = data.index(b"OpusHead")
        self.assertEqual(data[offset + 9], 1)
        self.assertEqual(struct.unpack_from("<I", data, offset + 12)[0], 16000)
        pre_skip = struct.unpack_from("<H", data, offset + 10)[0]
        pos, last_granule = 0, 0
        while pos < len(data):
            self.assertEqual(data[pos:pos + 4], b"OggS")
            granule = struct.unpack_from("<Q", data, pos + 6)[0]
            if granule != 0xffffffffffffffff:
                last_granule = granule
            count = data[pos + 26]
            pos += 27 + count + sum(data[pos + 27:pos + 27 + count])
        self.assertEqual(pos, len(data))
        self.assertAlmostEqual((last_granule - pre_skip) / 48000, 5.0, delta=0.02)


class NailongNativeTests(unittest.TestCase):
    def test_laugh_worker_during_cloud_tts_turn(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("Host g++ is not installed")
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            # Copy without rewriting, so local production display headers can
            # be replaced by host interfaces while testing the real worker.
            board = ROOT / "main/boards/otto-robot"
            for name in ("nailong_actions.cc", "nailong_actions.h", "nailong_motion.h"):
                shutil.copyfile(board / name, temporary / name)
            binary = temporary / ("laugh-test.exe" if os.name == "nt" else "laugh-test")
            command = [compiler, "-std=c++20", "-O2", "-pthread",
                       "-static-libstdc++", "-static-libgcc",
                       "-I", str(temporary), "-I", str(ROOT / "tests/host/nailong_action_stubs"),
                       "-I", str(ROOT / "main"), str(temporary / "nailong_actions.cc"),
                       str(ROOT / "tests/host/nailong_action_test.cc"), "-o", str(binary)]
            subprocess.run(command, check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=10)

    def test_scene_lifecycle_and_deferred_mcp_results(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("Host g++ is not installed")
        cjson = ROOT / "managed_components/espressif__cjson/cJSON"
        if not (cjson / "cJSON.c").exists():
            self.skipTest("Resolve project dependencies before running native MCP tests")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / ("nailong-test.exe" if os.name == "nt" else "nailong-test")
            command = [compiler, "-std=c++20", "-O2", "-static-libstdc++", "-static-libgcc",
                       "-I", str(ROOT / "main"), "-I", str(ROOT / "tests/host/include"),
                       "-I", str(cjson), str(ROOT / "tests/host/nailong_behavior_test.cc"),
                       str(cjson / "cJSON.c"), "-o", str(binary)]
            subprocess.run(command, check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
