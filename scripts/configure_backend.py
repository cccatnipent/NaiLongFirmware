"""Generate private backend settings for one student's LAN, without touching firmware identity."""

import argparse
import copy
import ipaddress
import os
from pathlib import Path
import re
import secrets

import yaml

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "xiaozhi-esp32-server/main/xiaozhi-server/data/.config.yaml"


def validate_host(host):
    if not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?", host):
        raise ValueError("Use a LAN IPv4 address or hostname, without protocol, port or path")
    if host.lower() in {"localhost", "0.0.0.0"}:
        raise ValueError("Use the computer's LAN address that the robot can reach")
    try:
        address = ipaddress.ip_address(host)
    except ValueError:
        if all(c.isdigit() or c == "." for c in host):
            raise ValueError("Invalid IPv4 address")
    else:
        if address.is_loopback or address.is_unspecified or address.is_multicast:
            raise ValueError("Use the computer's LAN unicast address")
    return host


def make_config(template, host, prompt, api_key=None, base_url=None, model=None):
    host = validate_host(host)
    config = copy.deepcopy(template)
    config["server"].update({
        "websocket": f"ws://{host}:8000/xiaozhi/v1/",
        "vision_explain": f"http://{host}:8003/mcp/vision/explain",
        "auth_key": secrets.token_hex(32),
    })
    config["prompt"] = prompt
    llm = config["LLM"]["CourseLLM"]
    for name, value in (("api_key", api_key), ("base_url", base_url), ("model_name", model)):
        if value:
            llm[name] = value
    return config


def write_config(path, config):
    path.parent.mkdir(parents=True, exist_ok=True)
    # Fail rather than replacing a student's existing keys/settings.
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        yaml.safe_dump(config, stream, allow_unicode=True, sort_keys=False)
    try:
        path.chmod(0o600)
    except OSError:
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Computer LAN IPv4 address or hostname")
    parser.add_argument("--base-url", help="Your model service's compatible API base URL")
    parser.add_argument("--model", help="A model supporting streaming tool/function calls")
    parser.add_argument("--api-key-env", default="NAILONG_LLM_API_KEY")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    template = yaml.safe_load((ROOT / "config/backend.example.yaml").read_text(encoding="utf-8"))
    try:
        config = make_config(template, args.host, (ROOT / "persona.txt").read_text(encoding="utf-8"),
                             os.environ.get(args.api_key_env), args.base_url, args.model)
        write_config(args.output, config)
    except (ValueError, FileExistsError) as error:
        parser.error(str(error))
    print(f"Created {args.output}; keys are not printed.")
    print(f"Firmware OTA URL: http://{args.host}:8003/xiaozhi/ota/")
    if any("YOUR_" in str(value) for value in config["LLM"]["CourseLLM"].values()):
        print("Fill YOUR_* model settings in the generated file before starting the backend.")


if __name__ == "__main__":
    main()
