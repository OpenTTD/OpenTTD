"""
Script to scan the OpenTTD source-tree for STR_ entries that are defined but
no longer used.

This is not completely trivial, as OpenTTD references a lot of strings in
relation to another string. The most obvious example of this is a list. OpenTTD
only references the first entry in the list, and does "+ <var>" to get to the
correct string.

There are other ways OpenTTD does use relative values. This script tries to
account for all of them, to give the best approximation we have for "this
string is unused".
"""

import glob
import os
import re
import subprocess
import sys

from enum import Enum

LENGTH_NAME_LOOKUP = {
    "VEHICLE_TYPES": 4,
}


class SkipType(Enum):
    NONE = 1
    LENGTH = 2
    EXTERNAL = 3
    ZERO_IS_SPECIAL = 4
    EXPECT_NEWLINE = 5


def read_language_file(filename, strings_found, errors):
    strings_defined = []

    skip = SkipType.NONE
    length = 0
    common_prefix = ""
    last_tiny_string = ""

    with open(filename) as fp:
        for line in fp.readlines():
            if not line.strip():
                if skip == SkipType.EXPECT_NEWLINE:
                    skip = SkipType.NONE
                continue

            line = line.strip()

            if skip == SkipType.EXPECT_NEWLINE:
                # The only thing allowed after a list, is this next marker, or a newline.
                if line == "###next-name-looks-similar":
                    # "###next-name-looks-similar"
                    # Indicates the common prefix of the last list has a very
                    # similar name to the next entry, but isn't part of the
                    # list. So do not emit a warning about them looking very
                    # similar.

                    if length != 0:
                        errors.append(f"ERROR: list around {name} is shorted than indicated by ###length")

                    common_prefix = ""
                else:
                    errors.append(f"ERROR: expected a newline after a list, but didn't find any around {name}. Did you add an entry to the list without increasing the length?")

                skip = SkipType.NONE

            if line[0] == "#":
                if line.startswith("###length "):
                    # "###length <count>"
                    # Indicates the next few entries are part of a list. Only
                    # the first entry is possibly referenced, and the rest are
                    # indirectly.

                    if length != 0:
                        errors.append(f"ERROR: list around {name} is shorted than indicated by ###length")

                    length = line.split(" ")[1].strip()

                    if length.isnumeric():
                        length = int(length)
                    else:
                        length = LENGTH_NAME_LOOKUP[length]

                    skip = SkipType.LENGTH
                elif line.startswith("###external "):
                    # "###external <count>"
                    # Indicates the next few entries are used outside the
                    # source and will not be referenced.

                    if length != 0:
                        errors.append(f"ERROR: list around {name} is shorted than indicated by ###length")

                    length = line.split(" ")[1].strip()
                    length = int(length)

                    skip = SkipType.EXTERNAL
                elif line.startswith("###setting-zero-is-special"):
                    # "###setting-zero-is-special"
                    # Indicates the next entry is part of the "zero is special"
                    # flag of settings. These entries are not referenced
                    # directly in the code.

                    if length != 0:
                        errors.append(f"ERROR: list around {name} is shorted than indicated by ###length")

                    skip = SkipType.ZERO_IS_SPECIAL

                continue

            name = line.split(":")[0].strip()
            strings_defined.append(name)

            # If a string ends on _TINY or _SMALL, it can be the {TINY} variant.
            # Check for this by some fuzzy matching.
            if name.endswith(("_SMALL", "_TINY")):
                last_tiny_string = name
            elif last_tiny_string:
                matching_name = "_".join(last_tiny_string.split("_")[:-1])
                if name == matching_name:
                    strings_found.add(last_tiny_string)
            else:
                last_tiny_string = ""

            if skip == SkipType.EXTERNAL:
                strings_found.add(name)
                skip = SkipType.LENGTH

            if skip == SkipType.LENGTH:
                skip = SkipType.NONE
                length -= 1
                common_prefix = name
            elif skip == SkipType.ZERO_IS_SPECIAL:
                strings_found.add(name)
            elif length > 0:
                strings_found.add(name)
                length -= 1

                # Find the common prefix of these strings
                for i in range(len(common_prefix)):
                    if common_prefix[0 : i + 1] != name[0 : i + 1]:
                        common_prefix = common_prefix[0:i]
                        break

                if length == 0:
                    skip = SkipType.EXPECT_NEWLINE

                    if len(common_prefix) < 6:
                        errors.append(f"ERROR: common prefix of block including {name} was reduced to {common_prefix}. This means the names in the list are not consistent.")
            elif common_prefix:
                if name.startswith(common_prefix):
                    errors.append(f"ERROR: {name} looks a lot like block above with prefix {common_prefix}. This mostly means that the list length was too short. Use '###next-name-looks-similar' if it is not.")
                common_prefix = ""

    return strings_defined


def scan_source_files(path, strings_found):
    for new_path in glob.glob(f"{path}/*"):
        if os.path.isdir(new_path):
            scan_source_files(new_path, strings_found)
            continue

        if not new_path.endswith((".c", ".h", ".cpp", ".hpp", ".ini")):
            continue

        # Most files we can just open, but some use magic, that requires the
        # G++ preprocessor before we can make sense out of it.
        if new_path == "src/table/cargo_const.h":
            p = subprocess.run(["g++", "-E", new_path], stdout=subprocess.PIPE)
            output = p.stdout.decode()
        else:
            with open(new_path) as fp:
                output = fp.read()

        # Find all the string references.
        matches = re.findall(r"[^A-Z_](STR_[A-Z0-9_]*)", output)
        strings_found.update(matches)


def main():
    strings_found = set()
    errors = []

    scan_source_files("src", strings_found)
    strings_defined = read_language_file("src/lang/english.txt", strings_found, errors)

    # STR_LAST_STRINGID is special, and not really a string.
    strings_found.remove("STR_LAST_STRINGID")
    # These are mentioned in comments, not really a string.
    strings_found.remove("STR_XXX")
    strings_found.remove("STR_NEWS")
    strings_found.remove("STR_CONTENT_TYPE_")

    # This string is added for completion, but never used.
    strings_defined.remove("STR_JUST_DATE_SHORT")

    strings_defined = sorted(strings_defined)
    strings_found = sorted(list(strings_found))

    for string in strings_found:
        if string not in strings_defined:
            errors.append(f"ERROR: {string} found but never defined.")

    for string in strings_defined:
        if string not in strings_found:
            errors.append(f"ERROR: {string} is (possibly) no longer needed.")

    if errors:
        print("\n".join(errors))
        sys.exit(1)

    print("OK")


if __name__ == "__main__":
    main()
