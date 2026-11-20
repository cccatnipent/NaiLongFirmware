from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import build_nailong


class CourseBuildTests(unittest.TestCase):
    def test_build_passes_lan_ota_url_to_canonical_builder(self):
        url = "http://192.168.1.20:8003/xiaozhi/ota/"
        with patch.object(sys, "argv", ["build_nailong.py", "--ota-url", url]), \
             patch.object(build_nailong.build, "_detect_idf_version", return_value=(5, 5, 5)), \
             patch.object(build_nailong.build, "build_board") as builder:
            build_nailong.main()
        self.assertIn(f'CONFIG_OTA_URL="{url}"', builder.call_args.kwargs["sdkconfig_overrides"])
        self.assertEqual(builder.call_args.kwargs["name_filter"], "nailong-otto")

    def test_omitting_endpoint_uses_official_cloud(self):
        with patch.object(sys, "argv", ["build_nailong.py"]), \
             patch.object(build_nailong.build, "_detect_idf_version", return_value=(5, 5, 5)), \
             patch.object(build_nailong.build, "build_board") as builder:
            build_nailong.main()
        self.assertIn('CONFIG_OTA_URL="https://api.tenclass.net/xiaozhi/ota/"',
                      builder.call_args.kwargs["sdkconfig_overrides"])

    def test_rejects_invalid_addresses_before_writing_sdkconfig(self):
        for url in ('file:///tmp/config', 'http://host/"\nCONFIG_BAD=y', 'https://user:pass@host/',
                    'http://host:invalid/', 'http://host/a b', 'http://host/#fragment'):
            with self.subTest(url=url), self.assertRaises(ValueError):
                build_nailong.ota_override(url)


if __name__ == "__main__":
    unittest.main()
