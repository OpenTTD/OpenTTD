# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

BEGIN {
	# Very basic variant function; barely any error checking.
	# Just use the first argument as the file to start from when assembling everything
	path = ARGV[1];
	gsub("[^/\\\\]*$", "", path);
	assemble(ARGV[1]);
}

# Recursive function for assembling by means of resolving the #includes.
function assemble(filename) {
	while ((getline < filename) > 0) {
		if (NF == 2 && $1 == "#include" ) {
			# Remove the quotes.
			gsub("[\"'<>]", "", $2);
			assemble(path $2);
		} else {
			print $0;
		}
	}

	if (close(filename) < 0) {
		print "Could not open " filename > "/dev/stderr";
		exit -1;
	}
}
