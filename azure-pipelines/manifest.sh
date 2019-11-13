#!/bin/sh

set -ex

if [ -z "$1" ]; then
    echo "Usage: $0 <folder-with-bundles>"
    exit 1
fi

FOLDER=$1

if [ ! -e .version ] || [ ! -e .release_date ]; then
    echo "This script should be executed in the root of an extracted source tarball"
    exit 1
fi

# Find the name based on the version
if [ -e .is_stable ]; then
    isTesting=$(cat .version | grep "RC\|beta" || true)
    if [ -z "${isTesting}" ]; then
        NAME="stable"
    else
        NAME="testing"
    fi
else
    NAME=$(cat .version | cut -d- -f2 | cut -d- -f-2)
fi

# Convert the date to a YAML date
DATE=$(cat .release_date | tr ' ' T | sed 's/TUTC/:00-00:00/')
VERSION=$(cat .version)
BASE="openttd-${VERSION}"

echo "name: ${NAME}" > manifest.yaml
echo "date: ${DATE}" >> manifest.yaml
echo "base: ${BASE}-" >> manifest.yaml

error=""

FILES=
DEV_FILES=
for filename in $(ls ${FOLDER} | grep -v ".txt$\|.md$\|sum$" | sort); do
    case ${filename} in
        *docs*   |\
        *source* |\
        *dbg.deb |\
        *pdb.xz  )
            DEV_FILES="${DEV_FILES} ${filename}"
            ;;

        *)
            FILES="${FILES} ${filename}"
            ;;
    esac
done

# output_files key filename...
function output_files {
    if [ "$#" -lt 2 ]; then return; fi
    key=$1
    echo "${key}:" >> manifest.yaml
    shift
    while (( "$#" )); do
        filename=$1
        if [ ! -e ${FOLDER}/${filename}.md5sum ] || [ ! -e ${FOLDER}/${filename}.sha1sum ] || [ ! -e ${FOLDER}/${filename}.sha256sum ]; then
            echo "ERROR: missing checksum file for ${filename}" 1>&2
            error="y"
            continue
        fi

        echo "- id: ${filename}" >> manifest.yaml
        echo "  size: $(stat -c"%s" ${FOLDER}/${filename})" >> manifest.yaml
        echo "  md5sum: $(cat ${FOLDER}/${filename}.md5sum | cut -d\  -f1)" >> manifest.yaml
        echo "  sha1sum: $(cat ${FOLDER}/${filename}.sha1sum | cut -d\  -f1)" >> manifest.yaml
        echo "  sha256sum: $(cat ${FOLDER}/${filename}.sha256sum | cut -d\  -f1)" >> manifest.yaml
        shift
    done
}

output_files files ${FILES}
output_files dev_files ${DEV_FILES}

if [ -n "${error}" ]; then
    echo "ERROR: exiting due to earlier errors" 1>&2
    exit 1
fi
