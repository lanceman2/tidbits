# This is a GNU make make file

SUBDIRS := pybind11 python_C


build clean cleaner:
	for dir in $(SUBDIRS) ; do\
	  if ! make -C $$dir $(@) ; then exit 1 ; fi ;\
	  done
