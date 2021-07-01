# This is a "GNU make" make file.  It uses GNU make extensions.

PY_VER := $(shell pkg-config python3 --modversion)

# I have noticed that the "pkg-config python-3" output is somewhat broken,
# so I had to roll my own "pkg-config python-3 --libs" and other things.

PY_LIBDIR := $(shell pkg-config python-$(PY_VER) --variable=libdir)
PY_CFLAGS := $(shell pkg-config python-$(PY_VER) --cflags)
PY_LDFLAGS := -L$(PY_LIBDIR) -lpython$(PY_VER) -Wl,-rpath -Wl,$(PY_LIBDIR)
PYBIND11_CPPFLAGS := $(shell python3 -m pybind11 --includes)


CXXFLAGS := -g -Wall -Werror -std=c++11

SLIBS := mod.so
mod.lo_CPPFLAGS := $(PY_CFLAGS) $(PYBIND11_CPPFLAGS)
mod.so_LDFLAGS := $(PY_LDFLAGS)


# default target
build: $(SLIBS)



%.lo: %.cpp
	$(CXX) $(CXXFLAGS) $($@_CPPFLAGS) -fPIC -c -o $@ $<


# How to build a DSO (dynamic shared object) library (module).
$(SLIBS): %.so: %.lo
	$(CXX) $(CXXFLAGS) --shared -o $@ $< $($@_LDFLAGS)


clean cleaner:
	rm -f *.so *.lo *.o $(SLIBS)
