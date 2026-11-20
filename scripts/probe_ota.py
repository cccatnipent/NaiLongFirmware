"""Read a running backend's OTA discovery response; no firmware writes or device access."""

import argparse
import json
from urllib.request import Request, urlopen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url", help="e.g. http://192.168.1.20:8003/xiaozhi/ota/")
    args = parser.parse_args()
    body = {"version": 2, "uuid": "00000000-0000-4000-8000-000000000001",
            "application": {"name": "xiaozhi", "version": "2.4.2"},
            "board": {"type": "otto-robot", "name": "nailong-otto"}}
    request = Request(args.url, data=json.dumps(body).encode(), method="POST", headers={
        "Content-Type": "application/json", "Device-Id": "02:00:00:00:00:01",
        "Client-Id": body["uuid"], "User-Agent": "nailong-course-probe/1",
    })
    with urlopen(request, timeout=10) as response:
        result = json.load(response)
    websocket = result.get("websocket", {})
    if not websocket.get("url"):
        raise SystemExit("No WebSocket URL in OTA response; check single-service backend configuration")
    print(json.dumps({"websocket_url": websocket["url"],
                      "token_present": bool(websocket.get("token")),
                      "firmware_version": result.get("firmware", {}).get("version")},
                     ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
