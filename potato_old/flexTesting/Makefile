#! GNU make file


flex_test_LFLAGS := -lfl

.SECONDARY:


bin := flex_test

flex_c := $(patsubst %.flex,%.c,$(wildcard *.flex))

%.c: %.flex
	flex --outfile=$@ $<
%.o: %.c
	$(CC) $($(@)_CPPFLAGS) -Wall -c $< -o $@


all: $(bin)

flex_test: flexParser.o main.o




$(bin):
	$(CC) -Wall $(filter %.o,$^) -o $@ $($(@)_LFLAGS)

clean:
	rm -f $(bin) *.o $(flex_c)
