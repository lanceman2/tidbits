
PREPREFIX := $(shell $(dir $(lastword $(MAKEFILE_LIST)))/config PREPREFIX)

PREFIX = $(PREPREFIX)/ood_example_01

