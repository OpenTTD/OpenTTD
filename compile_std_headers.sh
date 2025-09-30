copy_gcm_cache () {
	cd $1
	rm -r gcm.cache
	for d in */; do
    	if [ "$d" == "*/" ] ; then
			continue
		fi
		copy_gcm_cache "$d" ../"$2"
	done
	ln -fs $2 gcm.cache
	cd ..
}

if test -f "gcm.cache/std.gcm"; then
    echo "std module already exists"
else
	echo "compiling std module"
	gcc -c -std=c++20 -fmodules -fsearch-include-path bits/std.cc
	echo "done"
fi

if test -f "std.cxx"; then
	rm std.cxx
fi

> std.cxx

#copy_gcm_cache src/ ../gcm.cache

