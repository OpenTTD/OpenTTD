#!/bin/sh

# $Id$

if ! [ -f ai/regression/completeness.sh ]; then
	echo "Make sure you are in the root of OpenTTD before starting this script."
	exit 1
fi

cat ai/regression/tst_*/main.nut | tr ';' '\n' | awk '
/^function/ {
	for (local in locals) {
		delete locals[local]
	}
	if (match($0, "function Regression::Start") || match($0, "function Regression::Stop")) next
	locals["this"] = "AIControllerSquirrel"
}

/local/ {
	gsub(".*local", "local")
	if (match($4, "^AI")) {
		sub("\\(.*", "", $4)
		locals[$2] = $4
	}
}

/Valuate/ {
	gsub(".*Valuate\\(", "")
	gsub("\\).*", "")
	gsub(",.*", "")
	gsub("\\.", "::")
	print $0
}

/\./ {
	for (local in locals) {
		if (match($0, local ".")) {
			fname = substr($0, index($0, local "."))
			sub("\\(.*", "", fname)
			sub("\\.", "::", fname)
			sub(local, locals[local], fname)
			print fname
			if (match(locals[local], "List")) {
				sub(locals[local], "AIAbstractList", fname)
				print fname
			}
		}
	}
	# We want to remove everything before the FIRST occurence of AI.
	# If we do not remove any other occurences of AI from the string
	# we will remove everything before the LAST occurence of AI, so
	# do some little magic to make it work the way we want.
	sub("AI", "AXXXXY")
	gsub("AI", "AXXXXX")
	sub(".*AXXXXY", "AI")
	if (match($0, "^AI") && match($0, ".")) {
		sub("\\(.*", "", $0)
		sub("\\.", "::", $0)
		print $0
	}
}
' | sed 's/	//g' | sort | uniq > tmp.in_regression

grep 'DefSQ.*Method' ../src/script/api/ai/*.hpp.sq | grep -v 'AIError::' | grep -v 'AIAbstractList::Valuate' | grep -v '::GetClassName' | sed 's/^[^,]*, &//g;s/,[^,]*//g' | sort > tmp.in_api

diff -u tmp.in_regression tmp.in_api | grep -v '^+++' | grep '^+' | sed 's/^+//'

rm -f tmp.in_regression tmp.in_api

