"""Build the general-purpose four-servo Nailong firmware for ESP-IDF 5.5.5."""

import argparse
from urllib.parse import urlsplit

import build


NAILONG_OVERRIDES = [
    "CONFIG_WAKE_WORD_DISABLED=n",
    "CONFIG_USE_ESP_WAKE_WORD=n",
    "CONFIG_USE_AFE_WAKE_WORD=n",
    "CONFIG_USE_CUSTOM_WAKE_WORD=y",
    'CONFIG_CUSTOM_WAKE_WORD="ni hao nai long"',
    'CONFIG_CUSTOM_WAKE_WORD_DISPLAY="你好奶龙"',
    "CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=10",
    "CONFIG_SR_MN_CN_NONE=n",
    "CONFIG_SR_MN_CN_MULTINET7_QUANT=y",
    "CONFIG_FLASH_DEFAULT_ASSETS=y",
    "CONFIG_ENABLE_LOCAL_STOP_COMMAND=y",
    "CONFIG_LOCAL_STOP_COMMAND_THRESHOLD=10",
    "CONFIG_WAKE_WORD_DETECTION_IN_LISTENING=y",
    "CONFIG_USE_SERVER_AEC=n",
]


def ota_override(url):
    if any(char.isspace() or ord(char) < 32 or char in '\"\\' for char in url):
        raise ValueError("OTA URL must not contain whitespace, quotes or backslashes")
    parsed = urlsplit(url)
    if parsed.scheme not in ("http", "https") or not parsed.hostname or parsed.username or parsed.password or parsed.fragment:
        raise ValueError("OTA URL must be an http(s) address without credentials or fragment")
    parsed.port  # Validate the optional numeric port before creating sdkconfig.
    return f'CONFIG_OTA_URL="{url}"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ota-url", default="https://api.tenclass.net/xiaozhi/ota/",
                        help="Backend OTA URL; defaults to xiaozhi.me")
    args = parser.parse_args()
    try:
        overrides = NAILONG_OVERRIDES + [ota_override(args.ota_url)]
    except ValueError as error:
        parser.error(str(error))
    build.build_board(
        "otto-robot", name_filter="nailong-otto", language="zh-CN",
        idf_version=build._detect_idf_version(), sdkconfig_overrides=overrides,
    )


if __name__ == "__main__":
    main()
