# This is a GNU make make file.

# Assuming you have the required dependencies installed:
# Run 'make && ./primes && ./test.py'


#
# Make a small python3 module and run it in a python3 script.
#
#
#
# dependencies:
# apt install python3-dev
#
# On ubuntu 18.04


CFLAGS := -O3 -Wall -Werror

LIBS := primes.so

BINS := primes

primes.o_CPPFLAGS := $(shell python3-config --includes) -DMAIN

primes.lo_CPPFLAGS := $(shell python3-config --includes)

primes.so_CFLAGS := $(shell python3-config --ldflags)



build: $(LIBS) $(BINS)


.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $($(@)_CPPFLAGS) -c -o $@ $<

%.lo: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $($(@)_CPPFLAGS) -fPIC -c -o $@ $<


$(BINS): %: %.o
	$(CC) $(CFLAGS) $($(@)_CFLAGS) -o $@ $^

$(LIBS): %.so: %.lo
	$(CC) $(CFLAGS) -shared $($(@)_CFLAGS) -o $@ $^



clean cleaner:
	rm -f $(LIBS) $(BINS) *.o *.lo
