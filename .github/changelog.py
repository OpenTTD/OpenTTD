import sys

"""
This script assumes changelogs use the following format:
    ## <major>.x (eg. "## 15.x") to indicate a major version series
    ### <major>.<minor>[-<suffix>] <date etc> (eg. "## 15.0 (2025-04-01)", "### 15.1-beta1 (2024-12-25)") to indicate an individual version
"""

def main():
    current_version = sys.argv[1]
    stable_version = current_version.split("-")[0]
    major_version = current_version.split(".")[0]
    # set when current version is found
    current_found = False

    with open("changelog.md", "r") as file:
        for line in file:
            if line.startswith("### "):
                if not line.startswith(f"### {current_version} ") and not current_found:
                    # First version in changelog should be the current one
                    sys.stderr.write(f"Changelog doesn't start with current version ({current_version})\n")
                    sys.exit(1)
                if not line.startswith(f"### {stable_version}"):
                    # Reached a previous stable version
                    break
                if line.startswith(f"### {current_version} "):
                    current_found = True
            elif line.startswith("## "):
                if not line.startswith(f"## {major_version}.x"):
                    # Reached a previous major version
                    break

            print(line.rstrip())

if __name__ == '__main__':
    main()
