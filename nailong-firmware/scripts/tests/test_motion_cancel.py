from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MotionCancellationTests(unittest.TestCase):
    def test_real_movements_cancel_without_more_servo_writes_or_reattaching(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("Host g++ is not installed")
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            for name in ("otto_movements.cc", "otto_movements.h"):
                shutil.copyfile(ROOT / "main/boards/otto-robot" / name, temporary / name)
            binary = temporary / "motion-cancel.exe"
            result = subprocess.run([
                compiler, "-std=c++20", "-O2", "-static-libstdc++", "-static-libgcc",
                "-I", str(temporary), "-I", str(ROOT / "tests/host/motion_stubs"),
                str(temporary / "otto_movements.cc"),
                str(ROOT / "tests/host/motion_cancel_test.cc"), "-o", str(binary),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
