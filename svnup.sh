#!/bin/sh

# $Id$

# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

# This script updates the svn source and displays log changes
# This is only useful for users of CLI based SVN clients
# Written by Bjarni

[ "$PAGER" ] || PAGER=less


(

# reads what version you have now
Base=`svn info | grep "Revision" | xargs -n 1 | tail -n 1`

# updates the source
svn update > svn.log
cat svn.log

# if the revision number changed
if [ "$Base" -ne "`svn info | grep "Revision" | xargs -n 1 | tail -n 1`" ]; then
# displays the log changes
svn log -v -r HEAD:$(($Base + 1))
fi

# displays merged files
cat svn.log|grep "^G"
cat svn.log|grep "^C"

) | $PAGER
