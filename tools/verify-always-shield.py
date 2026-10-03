"""Check the private mirror fixture report, including automatic shield guards.

Run a private test-tools build in a disposable game directory with
MMVR_COMPONENT_AUDIT=mirror. Then pass its native-component-audit.json here.
This verifier does not launch a game or modify saves/settings.
"""
import argparse
import json
from itertools import product
from pathlib import Path


def valid_rows(data, key, modes, fields):
    rows = data[key]
    identities = {(left, mode) for left, mode in product((0, 1), range(modes))}
    return (len(rows) == len(identities)
            and {(r["left"], r["mode"]) for r in rows} == identities
            and all(r[field] == 1 for r in rows for field in fields))


def verify(data):
    try:
        return (data.get("fixture") is True and not data.get("unknownComponent")
                and data["alwaysShieldNullSafe"] == 1
                and valid_rows(data, "alwaysShield", 19, ("active", "mesh", "pose", "collider"))
                and valid_rows(data, "mirrorReflection", 7, ("hitPoint", "pose", "light", "boss"))
                and all(r["contact"] == (1 if r["mode"] in (0, 4, 6) else -1)
                        for r in data["mirrorReflection"]))
    except (KeyError, TypeError, ValueError):
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        passed = verify(json.loads(args.report.read_text(encoding="utf-8-sig")))
    except (OSError, ValueError):
        passed = False
    print("PASS: 38 shield guards and 14 reflection cases" if passed else "FAIL: shield fixture report")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
