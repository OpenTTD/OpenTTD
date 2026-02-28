"""
Script to scan the OpenTTD source-tree for doxygen @file annotations.
Checks whether they exist, are recognised by doxygen and match coding style.
"""

import os
import sys

END_OF_SENTENCE = [".", "!", "?"]
SOURCE_FILE_EXTENSION = ["cpp", "c", "hpp", "h", "mm", "m", "cc"]
TEMPLATE_FILE_EXTENSION = ["preamble", "in"]
EXCLUDED_FILES = ["./src/script/api/squirrel_export.sq.hpp.in", "./src/script/api/script_includes.hpp.in"]


def read_files_list_from_file(file_path):
    with open(file_path, "r") as f:
        while line := f.readline():
            yield line[:-1]


def list_files_walk(start_path="."):
    for root, dirs, files in os.walk(start_path):
        for file in files:
            yield os.path.join(root, file)


def check_descriptions(files):
    errors = []
    for path in files:
        if path in EXCLUDED_FILES:
            continue
        if path.find("3rdparty") != -1 or path.find("lang") != -1:
            continue
        if not os.path.isfile(path):
            continue
        name = path[path.rfind("/") + 1 :]
        while True:
            extension = name[name.rfind('.') + 1 :]
            if extension not in TEMPLATE_FILE_EXTENSION:
                break
            name = name[0 : -len(extension) - 1]
        if extension not in SOURCE_FILE_EXTENSION:
            continue
        with open(path, "r") as f:
            content = f.read()
            ann = content.find(f"@file {name} ")
            if ann == -1:
                if content.find("@file") == -1:
                    errors.append(f'File "{path}" does not provide description.')
                else:
                    errors.append(f'Description of file "{path}" does not match coding style.')
                continue
            end = content.find("\n", ann)
            start = content.rfind("\n", 0, ann) + 1
            if content[start : ann] == "/** " and content[end - 3 : end] == " */" and content[end - 4] in END_OF_SENTENCE:
                continue
            elif content[start : ann] == " * " and content[start - 4 : start - 1] == "/**" and content[end - 1] in END_OF_SENTENCE and content[end + 1 : end + 4] != " */":
                continue
            errors.append(f'Description of file "{path}" does not match coding style.')
    return errors


def main():
    if len(sys.argv) == 1:
        files = list_files_walk("./src")
    else:
        files = read_files_list_from_file(sys.argv[1])

    errors = check_descriptions(files)

    if errors:
        print("\n".join(errors))
        sys.exit(1)

    print("OK")


if __name__ == "__main__":
    main()
