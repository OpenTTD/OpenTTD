"""
Script to scan the OpenTTD source-tree for ini_key issues in WindowDesc entries.
"""

import glob
import os
import re
import sys


def scan_source_files(path, ini_keys=None):
    if ini_keys is None:
        ini_keys = set()

    errors = []

    for new_path in glob.glob(f"{path}/*.cpp"):
        if os.path.isdir(new_path):
            errors.append(scan_source_files(new_path, ini_keys))
            continue

        with open(new_path) as fp:
            output = fp.read()

        for (name, ini_key, widgets) in re.findall(r"^static WindowDesc ([a-zA-Z0-9_]*).*?, (?:\"(.*?)\")?.*?,(?:\s+.*?,){6}\s+[^\s]+\((.*?)\)", output, re.S|re.M):
            if ini_key:
                if ini_key in ini_keys:
                    errors.append(f"{new_path}: {name} ini_key is a duplicate")
                ini_keys.add(ini_key)
            widget = re.findall("static const (?:struct )?NWidgetPart " + widgets + ".*?(?:WWT_(DEFSIZE|STICKY)BOX.*?)?;", output, re.S)[0]
            if widget and not ini_key:
                errors.append(f"{new_path}: {name} has WWT_DEFSIZEBOX/WWT_STICKYBOX without ini_key")
            if ini_key and not widget:
                errors.append(f"{new_path}: {name} has ini_key without WWT_DEFSIZEBOX/WWT_STICKYBOX")

    return errors


def main():
    errors = scan_source_files("src")

    if errors:
        print("\n".join(errors))
        sys.exit(1)

    print("OK")


if __name__ == "__main__":
    main()
