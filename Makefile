# WEASTHER.PRG: weather gadget for the Atari ST (m68k-atari-mint-gcc)
# No -mshort: STinG calls go through src/pccall.S.
CC = m68k-atari-mint-gcc
CFLAGS = -m68000 -O2 -Wall -fomit-frame-pointer
OBJS = src/main.o src/net.o src/json.o src/weather.o src/icons.o \
       src/config.o src/texts.o src/pccall.o

WEASTHER.PRG: $(OBJS)
	$(CC) -m68000 -o $@ $(OBJS) -lgem -s

src/%.o: src/%.c src/*.h
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# host test of the JSON/weather parsing with a saved wttr.in answer
.PHONY: test clean
test: test/parsetest
	./test/parsetest test/sample_nl.json nl

test/parsetest: test/parsetest.c src/json.c src/weather.c src/*.h
	gcc -Wall -Isrc -o $@ test/parsetest.c src/json.c src/weather.c

clean:
	rm -f WEASTHER.PRG $(OBJS) test/parsetest
