#!/bin/bash
# This script updates the svn source and displays log changes
# This is only useful for users of CLI based SVN clients
# Written by Bjarni


# reads what version you have now
Base=`svn info | grep "Revision" | xargs -n 1 | tail -n 1`

# updates the source
svn update

# if the revision number changed
if [ "$Base" -ne "`svn info | grep "Revision" | xargs -n 1 | tail -n 1`" ]; then
# displays the log changes
svn log -r HEAD:$(($Base + 1))
fi
