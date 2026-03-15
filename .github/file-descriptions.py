"""
Script to scan the OpenTTD source-tree for doxygen @file annotations.
Checks whether they exist, are recognised by doxygen and match coding style.
"""

import os
import sys

END_OF_SENTENCE = [".", "!", "?"]
SOURCE_FILE_EXTENSION = ["cpp", "c", "hpp", "h", "mm", "m", "cc"]
TEMPLATE_FILE_EXTENSION = ["preamble", "in"]
REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXCLUDED_FILES = [
    os.path.join(REPO_DIR, "src", "script", "api", "squirrel_export.sq.hpp.in"),
    os.path.join(REPO_DIR, "src", "script", "api", "script_includes.hpp.in"),
]


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
        if os.path.abspath(path) in EXCLUDED_FILES:
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
                ann = content.find("@file")
                if ann == -1:
                    errors.append(f'File "{path}" does not provide description.')
                else:
                    errors.append(f'Expected "@file" to be followed by a file name. However in file "{path}" it is followed by "{content[ann + 5 : content.find(" ", ann + 6)]}".')
                continue
            end = content.find("\n", ann)
            start = content.rfind("\n", 0, ann) + 1
            comment_end = content.find("*/", ann)
            if comment_end == -1:
                errors.append(f'Description comment of file "{path}" is not terminated.')
            if content[start : ann] == "/** ":
                if content[end - 2 : end] != "*/":
                    errors.append(f'Description of file "{path}" begins with "/** @file". Therefore expected it to be a single line ending with "*/", not "{content[end - 2 : end]}".')
                elif content[comment_end - 1] != " ":
                    errors.append(f'In description of file "{path}" "*/" should be preceded by a space.')
                elif content[comment_end - 2] not in END_OF_SENTENCE:
                    errors.append(f'Brief description of file "{path}" ends with "{content[end - 4]}". Expected period, question mark or exclamation mark.')
            elif content[start : ann] == " * ":
                if content.count("\n", ann, comment_end) < 2:
                    errors.append(f'Description of file "{path}" is multi-line while could be single line.')
                    continue
                previous_line = content.rfind("\n", 0, start - 1) + 1
                if content[previous_line : start - 1] != "/**":
                    errors.append(f'In description of file "{path}" the "@file" is preceded by " * ". Expected previous line to be "/**". Found "{content[previous_line : start - 1]}" instead.')
                elif content[end - 1] not in END_OF_SENTENCE:
                    errors.append(f'Brief description of file "{path}" ends with "{content[end - 1]}". Expected period, question mark or exclamation mark.')
                elif content[comment_end - 1] != " " or content[comment_end - 2] != "\n":
                    before_comment_end = content[content.rfind("\n", 0, comment_end) + 1 : comment_end]
                    if len(before_comment_end) == before_comment_end.count(" "):
                        errors.append(f'In description of file "{path}" "*/" should be preceded by one space. Found {len(before_comment_end)} instead.')
                    else:
                        errors.append(f'In description of file "{path}" "*/" should be preceded by one space. Found "{before_comment_end}" instead.')
            else:
                errors.append(f'In description of file "{path}" the "@file" is preceded by "{content[start : ann]}". Expected "/** " or " * ".')
    return errors


def main():
    if len(sys.argv) == 1:
        files = list_files_walk(os.path.join(REPO_DIR, "src"))
    else:
        files = read_files_list_from_file(sys.argv[1])

    errors = check_descriptions(files)

    if errors:
        print("\n".join(errors))
        sys.exit(1)

    print("OK")


if __name__ == "__main__":
    main()
