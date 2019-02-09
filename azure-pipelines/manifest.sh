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

echo "name: ${NAME}" >> manifest.yaml
echo "date: ${DATE}" >> manifest.yaml
echo "base: ${BASE}-" >> manifest.yaml
echo "files:" >> manifest.yaml

error=""
for i in $(ls ${FOLDER} | grep -v ".txt$\|.md$\|sum$" | sort); do
    if [ -n "$(echo $i | grep pdb.xz)" ]; then continue; fi
    if [ -n "$(echo $i | grep dbg.deb)" ]; then continue; fi

    if [ ! -e ${FOLDER}/$i.md5sum ] || [ ! -e ${FOLDER}/$i.sha1sum ] || [ ! -e ${FOLDER}/$i.sha256sum ]; then
        echo "ERROR: missing checksum file for ${i}" 1>&2
        error="y"
        continue
    fi

    echo "- id: $i" >> manifest.yaml
    echo "  size: $(stat -c"%s" ${FOLDER}/$i)" >> manifest.yaml
    echo "  md5sum: $(cat ${FOLDER}/$i.md5sum | cut -d\  -f1)" >> manifest.yaml
    echo "  sha1sum: $(cat ${FOLDER}/$i.sha1sum | cut -d\  -f1)" >> manifest.yaml
    echo "  sha256sum: $(cat ${FOLDER}/$i.sha256sum | cut -d\  -f1)" >> manifest.yaml
done

if [ -n "${error}" ]; then
    echo "ERROR: exiting due to earlier errors" 1>&2
    exit 1
fi
