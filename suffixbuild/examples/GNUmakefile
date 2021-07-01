# This make file will make everything in this package: quickbuild in ..,
# and all the examples which are example packages.

SHELL = /bin/bash

subdirs = $(filter-out GNUmakefile %_BUILD,$(wildcard *))
rm_dirs = $(strip $(wildcard $(addsuffix /installed/,$(subdirs))) $(wildcard *_BUILD))
made_dirs = $(patsubst %/GNUmakefile,%,$(wildcard $(addsuffix /GNUmakefile,$(subdirs))))

build: clean
	$(MAKE) -C ..
	@for d in $(subdirs) ; do\
	    echo ;\
	    echo "=========== building package $$d in $${d}_BUILD ============" ;\
	    echo ;\
	    mkdir -p $${d}_BUILD ;\
	    cd $${d}_BUILD || exit 1 ;\
	    ../../quickbuild --src-prefix ../$$d || exit 1 ;\
	    $(MAKE) install || exit 1 ;\
	    if [ -f install_test ] ; then ./install_test || exit 1 ; fi ;\
	    cd .. || exit 1 ;\
	    echo ;\
	    echo "=========== building package $$d in the source ============" ;\
	    echo ;\
	    cd $$d || exit 1 ;\
	    ../../quickbuild || exit 1 ;\
	    $(MAKE) install || exit 1 ;\
            if [ -f install_test ] ; then ./install_test || exit 1 ; fi ;\
	    cd .. || exit 1 ;\
	    done || exit 1

	@echo
	@echo "Built all example packages SUCCESSFULLY in both src and separate build dir"
	@echo "or so it appears"

# There is nothing to distinguish between clean and distclean files.
clean distclean:
ifneq ($(rm_dirs),)
	rm -rf $(rm_dirs)
endif
ifneq ($(made_dirs),)
	for d in $(made_dirs) ; do make -C $$d distclean ; done
endif
ifeq ($(strip $(rm_dirs) $(made_dirs)),)
	@echo "Nothing to clean."
endif
