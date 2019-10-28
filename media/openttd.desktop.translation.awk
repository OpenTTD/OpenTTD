# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

#
# Awk script to automatically generate a comment lines for
# a translated desktop shortcut. If it does not exist there
# is no output.
#

/##isocode/ { lang = $2; next }
/STR_DESKTOP_SHORTCUT_COMMENT/ { sub("^[^:]*:", "", $0); print "Comment[" lang "]=" $0; sub("_.*", "", lang); print "Comment[" lang "]=" $0; next}
