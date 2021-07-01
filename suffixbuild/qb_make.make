#####################################################################
# This file is part of the quickbuild package build system
#
# This is a GNU make file which uses GNU make extensions

#####################################################################
# Quickbuild Generic VARIABLE DEFINITIONS
#####################################################################


# The following may be defined before including this
# file.
#
#
#   CLEAN_FILES
#
#       is other files to remove with `make clean'
#
#   DIST_CLEAN_FILES
#
#       is other files to remove with 'make distclean'
#
#



# VPATH makes it so you can't have a target that have the same name in
# different directories.
VPATH = $(srcdir)

ifneq ($(strip $(srcdir)),.)
vpath %.pc . $(srcdir)
vpath %.c . $(srcdir)
vpath %.h . $(srcdir)
vpath %.md . $(srcdir)
endif


# Users optional override of subdirs
# This way the user may keep a particular order in which
# make is run in subdirectories.
ifdef SUBDIRS
subdirs := $(strip $(filter-out .,$(SUBDIRS)))
endif


current_qb_srcs := $(sort $(wildcard $(srcdir)/qb.make))


# Rerun quickbuild if some quickbuild source files are added or
# removed
ifneq ($(sort $(qb_srcs)),$(strip $(current_qb_srcs)))
need_qb := need_qb
need_qb_why := quickbuild files added or removed in the top source directory:\\n\
 $(shell echo "    ") $(top_srcdir)\\n\
 $(shell echo "      ") $(sort $(qb_srcs)) != $(current_qb_srcs)
endif



built_html = $(sort \
 $(patsubst $(srcdir)/%.md, %.html, $(wildcard $(srcdir)/*.md))\
 $(patsubst $(srcdir)/%.md.in, %.html, $(wildcard $(srcdir)/*.md.in))\
 $(patsubst $(srcdir)/%.html.in, %.html, $(wildcard $(srcdir)/*.html.in))\
 $(patsubst $(srcdir)/%.phtml.in, %.html, $(wildcard $(srcdir)/*.phtml.in))\
 $(patsubst $(srcdir)/%.phtml, %.html, $(wildcard $(srcdir)/*.phtml)))

html = $(sort $(built_html) $(wildcard $(srcdir)/*.html))


built_pkgconfig = $(sort $(patsubst $(srcdir)/%.pc.in, %.pc, $(wildcard $(srcdir)/*.pc.in)))

pkgconfig = $(sort $(built_pkgconfig) $(patsubst $(srcdir)/%.pc, %.pc, $(wildcard $(srcdir)/*.pc)))


built_c = $(sort\
  $(patsubst $(srcdir)/%.c.in, %.c, $(wildcard $(srcdir)/*.c.in))\
  $(patsubst $(srcdir)/%.h.in, %.h, $(wildcard $(srcdir)/*.h.in))\
)


#####################################################################
# END BASE VARIABLE DEFINITIONS
#####################################################################


#####################################################################
# Now the hard part, re-compose targets and dependences
#####################################################################



lib := $(sort\
 $(shell echo "$(.VARIABLES)" | tr ' ' '\n' |\
 egrep '\.so_SOURCES$$' | sed -e 's/\.so_SOURCES$$/\.so/g'))

bin := $(sort\
 $(shell echo "$(.VARIABLES)" | tr ' ' '\n' |\
 egrep -v '(\.so_SOURCES$$|\.h_SOURCES$$|^BUILT_SOURCES$$)' | egrep '_SOURCES$$' |\
 sed -e 's/_SOURCES$$//g'))

include_cat := $(sort\
 $(shell echo "$(.VARIABLES)" | tr ' ' '\n' |\
 egrep '\.h_SOURCES$$' | sed -e 's/\.h_SOURCES$$/\.h/g'))

include := $(sort $(HEADERS) $(include_cat))


define SET_vpath =
  #
  # example
  #
  #  1 = name.c or name.c.in
  #  2 = ./  or src/../
  #

  ifeq ($$(strip $(2)),./)
    ifndef top_build_is_top_src
      # in a seperate build dir tree
      # We handled this case above
      #vpath $(1) $$(srcdir)
    endif
  else
    vpath $(1) $$(srcdir)/$(2)
  endif
endef



define GET_include_cat_dep =
  # 
  # foo.h: foo1.h foo2.h ...
  $(1): $$(patsubst %.in,%,$$($(1)_SOURCES))
  src_in := $$(sort\
 $$(patsubst $$(srcdir)/%,%,$$(wildcard $$(addprefix $$(srcdir)/,$$(addsuffix .in,$$($(1)_SOURCES)))))\
 $$(filter %.in,$$($(1)_SOURCES)))
  # set vpath for finding .in files
  $$(foreach f,$$(src_in),$$(eval $$(call SET_vpath,$$(notdir $$(f)),$$(dir $$(f)))))
  undefine src_in
endef

$(foreach h,$(include_cat),$(eval $(call GET_include_cat_dep,$(h))))

undefine GET_include_cat_dep



ifeq ($(sort $(lib) $(bin)),)
ifndef BINS
# the default quickbuild thing to do with *.c
BINS := .
endif
endif

ifeq ($(strip $(BINS)),NONE)
  undefine BINS
endif

debug_var :=
depend_files :=
built_files :=
clean_files :=
.lib :=
.bin :=


ifdef BINS
ifeq ($(sort $(BINS)),.)
  add_bins := $(sort\
 $(patsubst %.c,%,$(notdir $(wildcard $(srcdir)/*.c)))\
 $(patsubst %.c.in,%,$(notdir $(wildcard $(srcdir)/*.c.in))))
else
  add_bins := $(sort\
 $(patsubst %.c,%,$(BINS))\
 $(patsubst %.c.in,%,$(BINS)))
endif
endif


define ADD_bins =
ifndef $(1)_SOURCES
  $(1)_SOURCES = $(1).c
endif
  bin += $(1)
endef

$(foreach b,$(add_bins),$(eval $(call ADD_bins,$(b))))

# remove duplicates
bin := $(sort $(bin))


undefine add_bins
undefine ADD_bins


# spaghetti anyone?
# GNU make at it's best.  I can't believe this works.

define SET_pkg_opts =
  #
  # 1 = hello_ or libhello.so_ or ''
  # 2 = mod  like gtk+-3.0
  #
  $(1)cflags := $$($(1)cflags) $$(shell pkg-config --cflags $(2))
  $(1)ldopts := $$($(1)ldopts) $$(shell pkg-config --libs $(2))
endef

$(foreach pkg,$(ADD_PKG),$(eval $(call SET_pkg_opts,,$(pkg))))


define GET_dep_in_srcdir =
  #
  ifeq ($$(sort $$(dir $(2))),./)
    $(1): $(2)
  endif
  #
endef


define GET_obj_DEPS =
  #
  # for example
  #
  # 1 = foo # program name or shared library name like libfoo.so
  # 2 = main # code main.c
  # 3 = o # or lo as in *.lo or *.o
  #

  # we append compiler preprocessor *CPPFLAGS flags to the compiler flags
  # *CFLAGS calling it prefix_thingy_CFLAGS
  #
  # objects are made with CPPFLAGS and CFLAGS
  # libs and programs are linked with just the CFLAGS
  #
  # Users that write qb.make files can set many flavors of *_CPPFLAGS and
  # *_CFLAGS.
  #
  # cflags was set from the environment at configure time and CFLAGS is
  # set in user qb.make files.  CFLAGS gets added to all compiler lines
  # in the whole directory.  foo-main.o_CFLAGS is a compiler flag for
  # just compiling the main.c file into foo-main.o for the foo program.
  # This is much more flexable than GNU automake (autotools), where you
  # don't get that kind of compiler options flag fine tooth control.
  #
  ifndef top_build_is_top_src
    # separate build tree look in srcdir for source header files
    # We can't tell it there are *.h.in files making *.h files
    # that we need to include.
    icppflags := -iquote . -iquote $$(srcdir)
  endif

  ccflags := $$(cppflags) $$($(1)-$(2).$(3)_CPPFLAGS) $$($(2).$(3)_CPPFLAGS)\
    $$($(1)_CPPFLAGS)  $$(icppflags) $$(CPPFLAGS)\
    $$($(1)-$(2).$(3)_CFLAGS) $$($(2).$(3)_CFLAGS) $$($(1)_cflags)
  # explicitly state the first prerequisite
  $(1)-$(2).$(3): $(2).c
  depend_files += .deps_$(1)-$(2).Po
  .deps_$(1)-$(2).Po_target := $(1)-$(2).$(3)
  .deps_$(1)-$(2).Po_cflags := $$(ccflags)
  .deps_$(1)-$(2).Po: $(2).c
  $(1)-$(2).$(3)_cflags := $$(ccflags)
  undefine ccflags
  undefine icppflags
endef


define MK_path_list =
  # makes a path like $(2) := "/path1:/path2/foo:/path3/bar"
  ifdef $(2)
    $(2) := $$(strip $$($(2))):$$(strip $(1))
  else
    $(2) := $(1)
  endif
endef


define GET_bin_or_lib =
  #
  # example
  #
  # 1 = program_name # or libfoo.so
  # 2 = 'o' # or 'lo'  as in *.o or *.lo object files
  # 3 = lib or bin

  ifeq ($$(sort $$($(1)_SOURCES)),.)
    $(1)_SOURCES := $$(sort\
 $$(patsubst $$(srcdir)/%,%,$$(wildcard $$(srcdir)/*.c $$(srcdir)/*.h))\
 $$(patsubst $$(srcdir)/%.in,%,$$(wildcard $$(srcdir)/*.c.in $$(srcdir)/*.h.in)))
  else
    $(1)_SOURCES := $$(sort $$(patsubst %.in,%,$$($(1)_SOURCES)))
  endif
  ifdef $(1)_NOTSOURCES
    $(1)_SOURCES := $$(filter-out $$($(1)_NOTSOURCES), $$($(1)_SOURCES))
  endif

  src_in := $$(sort\
 $$(patsubst $(srcdir)/%,%,$$(wildcard $$(addprefix $$(srcdir)/,$$(addsuffix .in,$$($(1)_SOURCES)))))\
 $$(filter %.in,$$($(1)_SOURCES)))
  # set vpath for finding .in files
  $$(foreach f,$$(src_in),$$(eval $$(call SET_vpath,$$(notdir $$(f)),$$(dir $$(f)))))

  # set the vpath for finding .c files
  # this foreach cannot be on multiple lines
  $$(foreach f,$$(filter %.c, $$($(1)_SOURCES)),$$(eval $$(call SET_vpath,$$(notdir $$(f)),$$(dir $$(f)))))
 
  $(1)_OBJS := $$(patsubst %.c,$(1)-%.$(2), $$(filter %.c, $$(notdir $$($(1)_SOURCES))))
  $(1): $$($(1)_OBJS)
  clean_files += $$($(1)_OBJS)
  $$(foreach pkg,$$($(1)_ADD_PKG),$$(eval $$(call SET_pkg_opts,$(1)_,$$(pkg))))

  $(1)_cflags := $$(cflags) $$($(1)_CFLAGS) $$($(1)_cflags) # no *CPPFLAGS in there
  $$(foreach obj,$$($(1)_OBJS),$$(eval $$(call GET_obj_DEPS,$(1),$$(patsubst $(1)-%.$(2),%,$$(obj)),$(2))))

  $(1)_LIBADD := $$(strip $$(filter %.so, $$($(1)_LIBADD) $$(LIBADD))) # ADD libs from this package like libfoo.so
  $(1)_ldadd  := $$(foreach l,$$($(1)_LIBADD), -L$$(patsubst %/,%,$$(dir $$(l))) $$(patsubst lib%.so,-l%,$$(notdir $$(l))))
  $(1)_ldopts := $$($(1)_LDFLAGS) $$(LDFLAGS) $$($(1)_ldopts) $$(ldopts)

  ifneq ($$(strip $$($(1)_LIBADD)),)
    # We assuming that $(1)_LIBADD is only set for linking with
    # this package shared object library files.
    undefine path

    # this foreach monster creates for example  path := /path1:/path2/foo:/path3/bar
    $$(foreach d,$$(realpath $$(patsubst %/,%,$$(dir $$($(1)_LIBADD)))),\
      $$(eval $$(call MK_path_list,$$(d),path)))

    $(1)_rpath := -Wl,-rpath -Wl,$$(path)
    .$(3) += $(1) # .bin or .lib
    # We must have the thing being built depend on the LIDADD libfoo.so
    # files just so they build the libfoo.so before $(1)
    # We must have the thing being built depend on the LIDADD libfoo.so
    # files (but not ../libfoo.so) just so they build the libfoo.so before it
    $$(foreach l,$$($(1)_LIBADD),$$(eval $$(call GET_dep_in_srcdir,$(1),$$(l))))
    undefine path
  endif

  built_files += $(1)
  undefine src_in
endef


$(foreach b,$(bin),$(eval $(call GET_bin_or_lib,$(b),o,bin)))

$(foreach l,$(lib),$(eval $(call GET_bin_or_lib,$(l),lo,lib)))


# done with functions, so remove this butt ugly GNU make function shit
undefine GET_sharedlibs
undefine GET_bin_or_lib
undefine MK_path_list
undefine SET_vpath

##################################################################
# figure out what gets installed based on user defined INSTALL
# or NO_INSTALL
#



ifdef NO_INSTALL
ifdef INSTALL
$(error "NO_INSTALL and INSTALL are both defined in $(srcdir)/qb.make")
endif
endif

define GET_liba_cflags_from_libso
#1 = libfoo
#2 = foo # as in foo.c source to libfoo.a-foo.o
  .deps_$(1).a-$(2).Po_cflags := $$(.deps_$(1).so-$(2).Po_cflags)
  $(1).a-$(2).o_cflags := $$($(1).so-$(2).lo_cflags)
endef


define GET_liba_from_lib =
# 1 = libfoo
  objs := $$(patsubst $(1).so-%.lo,$(1).a-%.o,$$($(1).so_OBJS))
  $(1).a: $$(objs)
  clean_files += $$(objs)
  $(1)_cflags := $$(cflags) $$($(1)_CFLAGS) $$($(1)_cflags) # no *CPPFLAGS in there
  $$(foreach o,$$(objs),$$(eval $$(call GET_obj_DEPS,$(1).a,$$(patsubst $(1).a-%.o,%,$$(o)),o)))
  $$(foreach o,$$(objs),$$(eval $$(call GET_liba_cflags_from_libso,$(1),$$(patsubst $(1).a-%.o,%,$$(o)))))
  undefine objs
endef

# We get lib*.a from the lib*.so so users will have an
# static archive option too.
liba = $(patsubst lib%.so, lib%.a, $(lib))
$(foreach l,$(liba),$(eval $(call GET_liba_from_lib,$(patsubst %.a,%,$(l)))))
built_files += $(liba)
libadir := $(libdir)
undefine GET_liba_from_lib
undefine SET_liba_obj_dep
undefine GET_obj_DEPS
undefine GET_liba_cflags_from_libso

install_var_types := bin lib liba pkgconfig html include


# by default we install everything that we can
installed := $(strip $(foreach f, $(install_var_types), $($(f))) $(SCRIPTS))

# in qb_make:
# The user can set INSTALL to select files to install,
# or the user can set NO_INSTALL to select file to not install.

ifdef NO_INSTALL
ifeq ($(sort $(NO_INSTALL)),.)
installed :=
else
installed := $(strip $(filter-out $(NO_INSTALL),$(installed)))
endif
endif

ifdef INSTALL
ifneq ($(sort $(INSTALL)),.)
installed := $(strip $(filter $(INSTALL), $(installed)))
endif
endif

# all things installed much have a corresponding phony target,
# so that running 'make install' always copies files.
install :=

define SET_install_from_to =
  #
  # example:
  #
  #  1 = bin or lib
  #  2 = foo or libfoo.so
  #
  # install += install_bin_foo
  # install_bin_foo_from := foo
  # install_bin_foo_to := $(bindir)/foo
  # for some files we are just installing from source
  # like for some *.h or *.html
  ifeq ($$(strip $$(wildcard $$(srcdir)/$(2))),$$(strip $$(srcdir)/$(2)))
    # it exists in the source directory
    install_$(1)_$(2)_from := $$(srcdir)/$(2)
  else
    # it will be built in the build dir which
    # may be the srcdir or not.
    install_$(1)_$(2)_from := $(2)
  endif
  install_$(1)_$(2)_to := $$($(1)dir)/$(2)
  install += install_$(1)_$(2)
endef

define GET_install =
  # example:  1 = bin
  ifneq ($$(strip $$($(1))),)
    install_$(1) := $$(strip $$(filter $$($(1)),$$(installed)))
    # example  install_html = foo.html bar.html
    ifneq ($$(strip $$(install_$(1))),)
      $$(foreach f,$$(install_$(1)),$$(eval $$(call SET_install_from_to,$(1),$$(f))))
    else
      undefine install_$(1)
    endif
  endif
endef


$(foreach t,$(install_var_types),$(eval $(call GET_install,$(t))))



undefine GET_install
undefine install_var_types
undefine SET_install_from_to

########################################################################
# Now add to the binaries and shared libraries a version
# to install that has the correct LD LIBRARY PATH linked in.
# If there are any _LIBADD shared libraries to link we make two
# binaries and shared libraries, one that runs in the build directory
# and one that runs from the installed shared libraries.  They
# have different linker options.
#
# GNU autotools works around this problem by making a shell wrapper and
# the binaries in a .lib/ sub-directory.  They do it with libtool.
#

define GET_rpath_installs =
  #
  # example:
  #
  #
  # 1 = foo or libfoo.so
  # 2 = bin or lib
  ifneq ($$(strip $$(filter $(1),$$(installed))),)
    # We make a different binary the for installation so that
    # dynamic linking still works.
    .$(2)_$(1)_rpath := -Wl,-rpath -Wl,$$(libdir)
    .$(2)_$(1)_cflags := $$($(1)_cflags)
    .$(2)_$(1)_ldadd  := $$(foreach l,$$($(1)_LIBADD), -L$$(patsubst %/,%,$$(dir $$(l))) $$(patsubst lib%.so,-l%,$$(notdir $$(l))))
    .$(2)_$(1)_ldopts := $$($(1)_ldopts)
    # We must have the thing being built depend on the LIDADD libfoo.so
    # files (but not ../libfoo.so) just so they build the libfoo.so before it
    $$(foreach l,$$($(1)_LIBADD),$$(eval $$(call GET_dep_in_srcdir,.$(2)_$(1),$$(l))))
    # What objects we make it with.
    .$(2)_$(1): $$($(1)_OBJS)
    # we build an addition binary which has different linking opts
    built_files += .$(2)_$(1)
    # We need a rule to make it for $(bin): below
    $(2) += .$(2)_$(1)
    # this is why we waited to do this now,
    # we change the _from thingy.
    # override example: install_bin_foo_from := .bin_foo
    # inplace of install_bin_foo_from := foo
    install_$(2)_$(1)_from := .$(2)_$(1)
    # so in effect we change what is installed.
  endif
endef

$(foreach lb,$(.lib),$(eval $(call GET_rpath_installs,$(lb),lib)))
$(foreach lb,$(.bin),$(eval $(call GET_rpath_installs,$(lb),bin)))

undefine .lib
undefine .bin
undefine GET_rpath_installs


define SCRIPTS_set_install =
  # 1 = foo
  #
  ifneq ($$(strip $$(wildcard $(srcdir)/$(1).in)),)
    $(1): $(srcdir)/$(1).in
    built_files += $(1)
    install_bin_$(1)_from := $(1)
  else
    install_bin_$(1)_from := $$(srcdir)/$(1)
  endif
  ifneq ($$(strip $$(filter $(1),$$(installed))),)
    install_bin_$(1)_to := $$(bindir)/$(1)
    install += install_bin_$(1)
    ifndef install_bin
      install_bin := $(1)
    endif
  endif
endef

$(foreach s,$(SCRIPTS),$(eval $(call SCRIPTS_set_install,$(s))))
 

undefine installed
undefine SCRIPTS_set_install

####################################################################

built_files := $(strip $(built_files) $(built_html) $(built_pkgconfig) $(BUILT_FILES))


ifeq ($(strip $(built_files)),)
undefine built_files
endif

# add the users BUILT_SOURCES CLEAN_FILES DIST_CLEAN_FILES to the internal
# versions of them.

# Just C code files that may be generated
built_sources := $(sort $(BUILT_SOURCES) $(built_c) $(include))


old_depend_files := $(sort $(filter-out $(depend_files), $(wildcard .deps_*)))

clean_files := $(sort $(clean_files) $(built_files) $(built_sources) $(CLEAN_FILES))

dist_clean_files := $(sort $(dist_clean_files) $(DIST_CLEAN_FILES) $(clean_files))



ifndef top_build_is_top_src
build_top_srcdir_test = build_top_srcdir_test
endif

ifdef subdirs
build_rec = build_rec
clean_rec = clean_rec
_debug_rec = _debug_rec
distclean_rec = distclean_rec
install_rec = install_rec
endif


ifeq ($(strip $(install)),)
undefine install
endif




#####################################################################
# END           compose a list of things to build
#####################################################################



.DEFAULT_GOAL = build

#####################################################################
# END VARIABLE DEFINITIONS
#####################################################################

#####################################################################
# BEGIN RULES    How we do stuff.  There are some rules above :(
#####################################################################

.PHONY: $(build_top_srcdir_test)\
 $(build_rec) $(clean_rec) $(_debug_rec) $(distclean_rec) $(install_rec)\
 build clean _debug distclean install mkdirs\
 _debug_norec reqb need_qb quickbuild qb buildGNUmakefile_test all\
 $(install)

.SUFFIXES:
.SUFFIXES: .md .html .pc .o .lo .c .h .so .Po .phtml .ph\
 .pc.in .c.in .h.in .html.in .md.in .phtml.in .ph.in


.INTERMEDIATE: .o .lo

.SECONDARY:


# remove some GNU make implicit rules
% : s.%
% : RCS/%,v
% : SCCS/s.%
% : %,v
% : RCS/%
% : %.o


# do not make depend files when doing clean distclean and dist.
ifeq ($(sort $(filter clean distclean dist _debug,$(MAKECMDGOALS))),)
ifneq ($(strip $(depend_files)),)

# using include and not -include is more robust but prints a warning.
include $(depend_files)

$(depend_files): $(built_sources)

# We remove the include warning and error with -include, so
# now we much give the user feedback if making the depend file
# fails.
%.Po:
	$(strip $(CC) -MM $($(@)_cflags) $< -MF $@ -MT $($(@)_target))

endif
endif


# generic suffix rules

# *.phtml are PHP files that get made into HTML files.
%.html: %.phtml
	$(top_builddir)/qb_php_compile . $< $@

%.html: %.md
	marked $< > $@

%.pc: %.pc.in
	$(top_builddir)/qb_config $< $@ "# this is a generated file"

%.html: %.html.in
	$(top_builddir)/qb_config $< $@ "<!-- this is a generated file -->"

%.phtml: %.phtml.in
	$(top_builddir)/qb_config $< $@ "<?php /* this is a generated file */ ?>"

%.ph: %.ph.in
	$(top_builddir)/qb_config $< $@ "<?php /* this is a generated file */ ?>"

%.md: %.md.in
	$(top_builddir)/qb_config $< $@ "<!-- this is a generated file -->"

%.h: %.h.in
	$(top_builddir)/qb_config $< $@ "/* this is a generated file */"

%.c: %.c.in
	$(top_builddir)/qb_config $< $@ "/* this is a generated file */"



$(SCRIPTS):
	$(top_builddir)/qb_config $< $@ $($(@)_HEADER)
	chmod 755 $@


# Hate all the extra white space in compile command lines?
# Not with quickbuild; we strip it out.  It's so pretty.
#
# The *_CPPFLAGS have been concatenated to the $($(@)_CFLAGS in the .o and .lo cases
# All the *_CFLAGS and CFLAGS have been concatenated in $($(@)_cflags for
# .so and bin

%.o:
	$(strip $(CC) $($(@)_cflags) -c $< -o $@)

%.lo:
	$(strip $(CC) -fPIC $($(@)_cflags) -c $< -o $@)

%.a:
	ar -rcs $@ $^ $($(@)_ARADD)

%.so:
	$(strip $(CC) $($(@)_cflags) -shared\
	  $(filter %.lo,$^) -o $@ $($(@)_ldadd) $($(@)_ldopts) $($(@)_rpath))

$(bin):
	$(strip $(CC) $($(@)_cflags)\
	  $(filter %.o,$^) -o $@ $($(@)_ldadd) $($(@)_ldopts) $($(@)_rpath))

$(include_cat):
	echo "/* this is a generated file */" | cat - $^ > $@



clean: $(clean_rec)
ifneq ($(strip $(depend_files) $(old_depend_files)),)
	rm -f .deps_*.Po
endif
ifneq ($(strip $(clean_files)),)
	rm -f $(clean_files)
endif

distclean: $(distclean_rec)
ifneq ($(strip $(depend_files) $(old_depend_files)),)
	rm -f .deps_*.Po
endif
	rm -f $(dist_clean_files)
ifeq ($(strip $(top_builddir)),.)
	rm -f qb_config
endif


_debug: _debug_norec $(_debug_rec)
$(_debug_rec): _debug_norec
_debug_norec:
	@echo "subdirs=$(subdirs)"
	@echo "src_subdirs=$(src_subdirs)"
	@echo "built_files=$(built_files)"
	@echo "qb_rebuild_command=$(qb_rebuild_command)"
	@echo "qb_core_files=$(qb_core_files)"
	@echo "qb_srcs=$(qb_srcs)"
	@echo "lib=$(lib)"
	@echo "bin=$(bin)"
	@echo "debug_var=$(debug_var)"
	@echo "depend_files=$(depend_files)"
	@echo "built_sources=$(built_sources)"
	@echo "install=$(install)"

# Check that GNUmakefile is not setting need_qb in this particular
# make run
# This stops 
buildGNUmakefile_test:
ifdef need_qb
	@echo "###################################################################"
	@echo "#                                                                 #"
	@echo "#  quickbuild failed to make a consistent/usable build system     #"
	@echo "#  This make keeps trying to remake GNUmakefile again and again   #"
	@echo "#                                                                 #"
ifdef need_qb_why
	@echo "#  $(need_qb_why)"
else
	@echo "#      You need to figure this out before you can use 'make'      #"
endif
	@echo "#                                                                 #"
	@echo "###################################################################"
	exit 1
endif
	@echo "Succeeded in remaking GNUmakefile(s) that does not need rebuilding"


# GNU make automatically remakes make files before
# all other targets.  It's analogous to rerunning
# GNU autotools configure.  GNUmakefile is just
# one of the files generated with this rule.
# We remove this target and run a test after remaking GNUmakefile.
ifneq ($(strip $(MAKECMDGOALS)),buildGNUmakefile_test)
GNUmakefile: $(sort $(qb_core_files) $(current_qb_srcs)) $(need_qb)
	@echo "Rerunning quickbuild due to source changes"
ifdef need_qb_why
	@echo "  $(need_qb_why)"
else
	@echo "  the following quickbuild file(s) changed:"
	@echo "      $?"
endif
	$(qb_rebuild_command)
	cd $(top_builddir) && $(MAKE) buildGNUmakefile_test
endif


quickbuild qb:
	$(qb_rebuild_command)

# Force the order in which things are built
$(build_rec): $(build_top_srcdir_test) $(built_sources) $(built_files)


build all: $(build_top_srcdir_test) $(built_sources) $(built_files) $(build_rec)
ifneq ($(strip $(old_depend_files)),)
	rm -f $(old_depend_files)
endif


ifdef install
$(install_rec): $(install)
install: $(install) $(install_rec)
$(install): mkdirs
	cp $($(@)_from) $(DESTDIR)$($(@)_to)
else
install: $(install_rec)
	@echo "Nothing to install"
endif

mkdirs: $(build_top_srcdir_test) $(built_sources) $(built_files)
ifdef install_lib
	@install -v -d $(DESTDIR)$(libdir)
endif
ifdef install_bin
	@install -v -d $(DESTDIR)$(bindir)
endif
ifdef install_pkgconfig
	@install -v -d $(DESTDIR)$(pkgconfigdir)
endif
ifdef install_html
	@install -v -d $(DESTDIR)$(htmldir)
endif
ifdef install_include
	@install -v -d $(DESTDIR)$(includedir)
endif


ifdef build_top_srcdir_test
build_top_srcdir_test:
	@if [ -f "$(top_srcdir)/GNUmakefile" ]\
	    || [ -f "$(top_srcdir)/Makefile" ] ; then\
	    echo "You cannot build in the source tree at the same time as here" ;\
	    echo "Top source path: $(top_srcdir)" ;\
	    echo "Try running:" ;\
	    echo ;\
	    echo "    cd $(top_srcdir) && make distclean" ;\
	    echo ;\
	    exit 1 ; fi
endif


ifeq ($(strip $(top_builddir)),.)
dist:
	mkdir $(NAME).$(VERSION)
	cp -a $(wildcard * .dep*) $(NAME).$(VERSION)
	$(MAKE) -C $(NAME).$(VERSION) distclean
	cd $(NAME).$(VERSION) && rm -rf $(NAME).*.tar.* $(no_dist_files)
ifdef DIST_CLEAN
	cd $(NAME).$(VERSION) && $(DIST_CLEAN)
endif
	tar -cJvf $(NAME).$(VERSION).tar.xz $(NAME).$(VERSION)
	rm -rf $(NAME).$(VERSION)
endif

# We check for sub directory make file removal as
# we recurse
ifdef subdirs
build_rec clean_rec _debug_rec distclean_rec install_rec:
	@for d in $(subdirs); do\
          target="$(patsubst %_rec,%,$@)" ;\
          cd $$d || exit 1 ;\
	  if [ ! -f GNUmakefile ] ; then\
	    echo 'Missing GNUmakefile in ${PWD}' ;\
	    echo "  Run: 'make qb' to remake it" ;\
	    echo "  and then run this make command again." ;\
	    exit 1 ;\
	  fi ;\
          $(MAKE) $$target || exit 1 ;\
          cd .. || exit 1 ;\
          done
endif

