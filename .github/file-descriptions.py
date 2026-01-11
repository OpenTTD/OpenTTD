"""
Script to scan the OpenTTD source-tree for doxygen @file annotations.
Checks whether they exist, are recognised by doxygen and match coding style.
"""

import os
import sys

END_OF_SENTENCE = [".", "!", "?"]


def list_files_walk(start_path="."):
    out = []
    for root, dirs, files in os.walk(start_path):
        for file in files:
            out.append(os.path.join(root, file))
    return out


def check_descriptions():
    errors = []
    for path in list_files_walk("./src"):
        try:
            if path.endswith(".in") or path.endswith(".ini") or path.endswith(".sh") or path.endswith(".awk") or path.endswith(".postamble"):
                continue
            if path.find("3rdparty") != -1 or path.find("lang") != -1:
                continue
            with open(path, "r") as f:
                s = f.read()
                n = path.split("/")[-1]
                if n == "CMakeLists.txt":
                    continue
                if n.endswith(".preamble"):
                    n = n[0:-9]
                ann = s.find(f"@file {n} ")
                if ann == -1:
                    if s.find("@file") == -1:
                        errors.append(f'File "{path}" does not provide description.')
                    else:
                        errors.append(f'Description of file "{path}" does not match coding style.')
                    continue
                end = s.find("\n", ann)
                start = s.rfind("\n", 0, ann) + 1
                if s[start:ann] == "/** " and s[end - 3 : end] == " */" and s[end - 4] in END_OF_SENTENCE:
                    continue
                elif s[start:ann] == " * " and s[start - 4 : start - 1] == "/**" and s[end - 1] in END_OF_SENTENCE and s[end + 1 : end + 4] != " */":
                    continue
                errors.append(f'Description of file "{path}" does not match coding style.')
        except:
            pass
    return errors


def main():
    errors = check_descriptions()

    if errors:
        print("\n".join(errors))
        sys.exit(1)

    print("OK")


if __name__ == "__main__":
    main()
