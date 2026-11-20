from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import build_default_assets


class StopAssetTests(unittest.TestCase):
    def config(self, extra):
        text = '\n'.join(['CONFIG_USE_CUSTOM_WAKE_WORD=y',
                          'CONFIG_CUSTOM_WAKE_WORD="ni hao nai long"',
                          'CONFIG_CUSTOM_WAKE_WORD_DISPLAY="你好奶龙"',
                          'CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=10', extra])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sdkconfig"
            path.write_text(text, encoding="utf-8")
            return build_default_assets.read_custom_wake_word_from_sdkconfig(str(path))

    def test_stop_metadata_contains_separate_probability_and_action(self):
        config = self.config('CONFIG_ENABLE_LOCAL_STOP_COMMAND=y\nCONFIG_LOCAL_STOP_COMMAND_THRESHOLD=35')
        info = build_default_assets.make_multinet_model_info(config, "cn")
        self.assertEqual(info["threshold"], .1)
        self.assertEqual(info["stop_threshold"], .35)
        self.assertEqual(info["commands"][1], {"command": "ting xia", "text": "停下", "action": "stop"})

    def test_disabled_profile_keeps_only_wake_command(self):
        info = build_default_assets.make_multinet_model_info(self.config('# CONFIG_ENABLE_LOCAL_STOP_COMMAND is not set'), "cn")
        self.assertEqual(len(info["commands"]), 1)
        self.assertEqual(info["commands"][0]["action"], "wake")
        self.assertNotIn("stop_threshold", info)

    def test_configured_stop_threshold_reaches_the_resource_index(self):
        config = self.config('CONFIG_ENABLE_LOCAL_STOP_COMMAND=y\nCONFIG_LOCAL_STOP_COMMAND_THRESHOLD=48')
        self.assertEqual(build_default_assets.make_multinet_model_info(config, "cn")["stop_threshold"], .48)


class LocalStopRuntimeTests(unittest.TestCase):
    def test_real_handlers_split_motion_stop_from_boot_interrupt_and_keep_listening(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("Host g++ is not installed")
        handlers = []
        sources = [
            ("main/application.cc", "void Application::HandleLocalStopEvent()"),
            ("main/application.cc", "void Application::HandleStopActionsEvent()"),
            ("main/application.cc", "void Application::HandleWakeWordDetectedEvent()"),
            ("main/audio/wake_words/custom_wake_word.cc", "void CustomWakeWord::FeedSamples("),
        ]
        for path, signature in sources:
            source = (ROOT / path).read_text(encoding="utf-8")
            start = source.index(signature)
            pos = source.index("{", start)
            depth = 1
            end = pos + 1
            while depth:
                depth += (source[end] == "{") - (source[end] == "}")
                end += 1
            handlers.append(source[start:end])
        # Compile the production methods verbatim, with simulated perimeter
        # interfaces; do not duplicate their behavior in a test implementation.
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            methods = temporary / "stop_methods.cc"
            methods.write_text('#include "local_stop_runtime.h"\n' + '\n'.join(handlers), encoding="utf-8")
            binary = temporary / "local-stop.exe"
            compiled = subprocess.run([compiler, "-std=c++20", "-O2", "-D_GLIBCXX_DEBUG", "-static-libstdc++", "-static-libgcc",
                                       "-I", str(ROOT / "main"), "-I", str(ROOT / "tests/host"),
                                       str(methods), str(ROOT / "tests/host/local_stop_test.cc"), "-o", str(binary)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
