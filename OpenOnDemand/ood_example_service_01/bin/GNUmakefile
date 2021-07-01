# This is a GNU make file that uses GNU make make extensions

# Where to install selected files in this directory.  We must use = and
# not := because PREFIX is set later when common.make is included.  It's
# (= and/or :=) a GNU make thing.
INSTALL_DIR = $(PREFIX)/bin

HTTPD_PREFIX := $(shell  ../config HTTPD_PREFIX)

IN_VARS := HTTPD_PREFIX

# LaunchService is built from LaunchService.in from quickbuild built-in
# rules.
INSTALLED := LaunchService

# Makes LaunchService executable
LaunchService_MODE := 0755


include ../quickbuild.make
