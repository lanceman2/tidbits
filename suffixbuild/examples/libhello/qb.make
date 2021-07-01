libhello.so_SOURCES = hello.c
hello_SOURCES := main.c
hello_LIBADD  := libhello.so
hello.h_SOURCES := _hello.h.in
