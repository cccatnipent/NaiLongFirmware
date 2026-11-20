import ast
from collections.abc import Mapping
import importlib.util
from pathlib import Path
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("configure_backend", ROOT / "scripts/configure_backend.py")
configure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(configure)


class CourseConfigTests(unittest.TestCase):
    def test_generated_overlay_merges_with_actual_backend_defaults(self):
        backend = ROOT / "xiaozhi-esp32-server/main/xiaozhi-server"
        tree = ast.parse((backend / "config/config_loader.py").read_text(encoding="utf-8"))
        method = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == "merge_configs")
        namespace = {"Mapping": Mapping}
        exec(compile(ast.Module(body=[method], type_ignores=[]), "config_loader.py", "exec"), namespace)
        defaults = yaml.safe_load((backend / "config.yaml").read_text(encoding="utf-8"))
        template = yaml.safe_load((ROOT / "config/backend.example.yaml").read_text(encoding="utf-8"))
        overlay = configure.make_config(template, "192.168.1.20", "persona")
        merged = namespace["merge_configs"](defaults, overlay)
        self.assertEqual(merged["server"]["websocket"], "ws://192.168.1.20:8000/xiaozhi/v1/")
        self.assertEqual(merged["ASR"]["FunASR"]["model_dir"], "models/SenseVoiceSmall")
        self.assertEqual(merged["ASR"]["FunASR"]["language"], "zh")
        self.assertEqual(merged["LLM"][merged["selected_module"]["LLM"]]["type"], "openai")
        self.assertEqual(merged["Intent"]["function_call"]["functions"], [])
        self.assertTrue(merged["server"]["auth"]["enabled"])
        self.assertEqual(merged["server"]["auth"]["allowed_devices"], [])
        self.assertFalse(merged["manager-api"]["url"])

    def test_config_routes_devices_to_student_host_and_sets_tools_and_prompt(self):
        template = yaml.safe_load((ROOT / "config/backend.example.yaml").read_text(encoding="utf-8"))
        config = configure.make_config(template, "192.168.1.20", "course persona", "test-key",
                                       "https://example.com/v1", "test-model")
        self.assertEqual(config["server"]["websocket"], "ws://192.168.1.20:8000/xiaozhi/v1/")
        self.assertEqual(config["prompt"], "course persona")
        self.assertEqual(config["selected_module"]["Intent"], "function_call")
        self.assertEqual(config["LLM"]["CourseLLM"]["api_key"], "test-key")
        self.assertNotIn("auth_key", template["server"])
        self.assertNotEqual(config["server"]["auth_key"], configure.make_config(template, "192.168.1.20", "x")["server"]["auth_key"])

    def test_host_rejects_inaccessible_addresses_and_config_injection(self):
        for value in ("localhost", "0.0.0.0", "127.0.0.1", "224.0.0.1", "192.168.1.999",
                      "http://192.168.1.20", "host:8003", "host\nadmin: true", "host/path"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                configure.validate_host(value)

    def test_existing_student_config_is_never_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "data/.config.yaml"
            configure.write_config(path, {"secret": "one"})
            original = path.read_bytes()
            with self.assertRaises(FileExistsError):
                configure.write_config(path, {"secret": "two"})
            self.assertEqual(path.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
