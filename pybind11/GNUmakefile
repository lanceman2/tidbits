# This is a GNU make make file.

# Assuming you have the required dependencies installed:
# Run 'make && ./primes && ./test.py'


# I copied stuff from: 2015: Jagged\Verge:
# http://www.jaggedverge.com/2016/09/generating-python-bindings-from-c11-with-pybind11/
#
#
# Make a small python3 module with pybind11 and run it in a python3 script.
#
#
# The big pain was finding out the hard way that the name of the DSO
# module file and source file must be the same as the python module
# name.  I had primes_py3.cpp as a symlink to primes.cpp and that broke
# this.  I didn't know that the CPP macro processor could resolve symlinks
# in the user macro code.  I doubt that it can, but whatever, I'll just
# "make it work".
#
# dependencies:
# apt install python3-pybind11 python3-dev
#
# On ubuntu 18.04


CFLAGS := -O3 -Wall -Werror

primes.o_CPPFLAGS := $(shell python3-config --includes) -DMAIN

BINS := primes

LIBS := primes_py3.so

primes_py3.lo_CPPFLAGS := $(shell python3-config --includes)


primes_py3.so_CFLAGS := $(shell python3-config --ldflags)

build: $(BINS) $(LIBS)


.cpp.o:
	$(CXX) $(CFLAGS) $(CPPFLAGS) $($(@)_CPPFLAGS) -c -o $@ $<

primes_py3.lo: %.lo: %.cpp
	$(CXX) $(CFLAGS) $(CPPFLAGS) $($(@)_CPPFLAGS) -fPIC -c -o $@ $<

primes_py3.cpp: primes.cpp
	cp $< $@


$(BINS): %: %.o
	$(CXX) $(CFLAGS) -o $@ $^

$(LIBS): %.so: %.lo
	$(CXX) $(CFLAGS) -shared $($(@)_CFLAGS) -o $@ $^


clean cleaner:
	rm -f $(BINS) $(LIBS) primes_py3.cpp *.o *.lo
