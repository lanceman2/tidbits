# This is a GNU make file that uses GNU make make extensions


# Where to install selected files in this directory.  We must use = and
# not := because PREFIX is set later when common.make is included.  It's
# (= and/or :=) a GNU make thing.
INSTALL_DIR = $(PREFIX)/conf


mime.types:
	cp ../../httpd-$$(../config HTTPD_TAG)/docs/conf/mime.types $@


httpd.conf: httpd.conf.rl

nginx_stage.yml: nginx_stage.ymlin


BUILD := mime.types

# public_cert.pem is automatically added to cleanfiles
# because it's built with a script with it's name
# public_cert.pem with .bl added.
#
CLEANFILES := private_key.pem

INSTALLED := private_key.pem public_cert.pem httpd.conf mime.types

include ../quickbuild.make
