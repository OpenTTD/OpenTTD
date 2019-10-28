# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

#
# Awk script to extract translations for baseset descriptions
# from lang files for insertion into .obg/obs/obm files.
# If there is no translation, there is no output.
#
# The input file is scanned for the pattern
#  !! <ini-key> <STR_id>
#
# The lang files (passed as variable 'langfiles') are scanned for <STR_id> and
# the translations are added to the output file:
#  <ini-key>.<iso-code> = <translation>
#

# Simple insertion sort since not all AWKs have a sort implementation
function isort(A) {
	n = 0
	for (val in A) {
		n++;
	}

	for (i = 2; i <= n; i++) {
		j = i;
		hold = A[j]
		while (A[j - 1] > hold) {
			j--;
			A[j + 1] = A[j]
		}
		A[j] = hold
	}

	return n
}

/^!!/ {
	ini_key = $2;
	str_id = $3;

	file = langfiles
	while ((getline < file) > 0) {
		if (match($0, "##isocode") > 0) {
			lang = $2;
		} else if (match($0, "^" str_id " *:") > 0) {
			sub("^[^:]*:", "", $0)
			i++;
			if (lang == "en_GB") {
				texts[i] = ini_key "       = "$0;
			} else {
				texts[i] = ini_key "." lang " = "$0;
			}
		}
	}
	close(file);

	count = isort(texts);
	for (i = 1; i <= count; i++) {
		print texts[i]
	}

	next
}

{ print }
