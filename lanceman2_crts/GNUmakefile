
# We wish to have things accessed/built in this order:
SUBDIRS := liquid-dsp include lib share bin var


crts_mkTUN = $(PREFIX)/bin/crts_mkTUN

# The program crts_mkTUN need a special installation so it can startup as
# effective user root and create a TUN device and route it and then it
# goes back to being a regular user.  We could have added this as the last
# part of the "install" target but we wanted to keep the basic install as
# a non-root thing, with no root access.
sudo_install:
	sudo chown root:root $(crts_mkTUN)
	sudo chmod u+s $(crts_mkTUN)

# if you do not have sudo install you can run 'make root_install' as root
root_install:
	chown root:root $(crts_mkTUN)
	chmod u+s $(crts_mkTUN)




include ./quickbuild.make
