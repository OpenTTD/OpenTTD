
SQUIRREL=.
MAKE=make

sq32:
	cd squirrel; $(MAKE)
	cd sqstdlib; $(MAKE)
	cd sq; $(MAKE)

sqprof:
	cd squirrel; $(MAKE) sqprof
	cd sqstdlib; $(MAKE) sqprof
	cd sq; $(MAKE) sqprof

sq64:
	cd squirrel; $(MAKE) sq64
	cd sqstdlib; $(MAKE) sq64
	cd sq; $(MAKE) sq64

clean:
	$(MAKE) -C squirrel clean
	$(MAKE) -C sqstdlib clean
	$(MAKE) -C sq clean
