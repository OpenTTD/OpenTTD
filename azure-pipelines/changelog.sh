#!/bin/sh

tag=$(git describe --tags 2>/dev/null)

# If we are a tag, show the part of the changelog that matches the tag.
# In case of a stable, also show all betas and RCs.
if [ -n "$tag" ]; then
    grep="."
    if [ "$(echo $tag | grep '-')" = "" ]; then
            grep='^[0-9]\.[0-9]\.[0-9][^-]'
    fi
    next=$(cat changelog.txt | grep '^[0-9]' | awk 'BEGIN { show="false" } // { if (show=="true") print $0; if ($1=="'$tag'") show="true"} ' | grep "$grep" | head -n1 | sed 's/ .*//')
    cat changelog.txt | awk 'BEGIN { show="false" } /^[0-9].[0-9].[0-9]/ { if ($1=="'$next'") show="false"; if ($1=="'$tag'") show="true";} // { if (show=="true") print $0 }'
    exit 0
fi

# In all other cases, show the git log of the last 7 days
revdate=$(git log -1 --pretty=format:"%ci")
last_week=$(date -u -d "$revdate -7days" +"%Y-%m-%d %H:%M")
git log --after="${last_week}" --pretty=fuller
