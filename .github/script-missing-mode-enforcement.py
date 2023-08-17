"""
Script to scan the OpenTTD's script API for functions that miss checks for the
function being called from the right mode (deity or company mode).

When a function calls either ScriptObject::Command or ScriptObject::GetCompany
then the function is considered dangerous. When one of the mode enforcement
macros from script_error.hpp, i.e. EnforceDeityMode, EnforceCompanyModeValid or
EnforceDeityOrCompanyModeValid, are called in the function, then we consider
that the function has mode enforcement.

Any dangerous function for which no enforcement is found are emitted as errors.
"""

import glob
import re
import sys


def check_mode_enforcement(path):
    errors = []
    with open(path, "r") as reader:
        mode_enforcement_found = False
        dangerous_function = False
        for line in reader:
            # Line does not start with a tab and have <word>::<word>. That looks like the begin of a function, so reset the state.
            if re.match(r"^[^\t].*\w::\w", line):
                mode_enforcement_found = False
                dangerous_function = False
                currentFunction = line
                continue

            if re.match(
                r"\t(EnforceDeityMode|EnforceCompanyModeValid|EnforceCompanyModeValid_Void|EnforceDeityOrCompanyModeValid|EnforceDeityOrCompanyModeValid_Void)\(",
                line,
            ):
                # Mode enforcement macro found
                mode_enforcement_found = True
                continue

            if re.match(r".*(ScriptObject::Command|ScriptObject::GetCompany).*", line):
                # Dangerous function found
                dangerous_function = True
                continue

            # Line with only a closing bracket. That looks like the end of a function, so check for the dangerous function without mode enforcement
            if re.match(r"^}$", line) and dangerous_function and not mode_enforcement_found:
                function_name = currentFunction.rstrip("\n").replace("/* static */ ", "")
                errors.append(f"{path}: {function_name}")

    return errors


def main():
    errors = []
    for path in sorted(glob.glob("src/script/api/*.cpp")):
        # Skip a number of files that yield only false positives
        if path.endswith(("script_object.cpp", "script_companymode.cpp", "script_controller.cpp", "script_game.cpp")):
            continue

        errors.extend(check_mode_enforcement(path))

    if errors:
        print("Mode enforcement was expected in the following files/functions:")
        print("\n".join(errors))
        sys.exit(1)

    print("OK")


if __name__ == "__main__":
    main()
