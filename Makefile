all:	clean icu.so icu.utf8.so icu.ustring.so test

%.so:	%.c
	gcc -shared -llua -o $@ $< `icu-config --ldflags`

clean:
	rm -f *.so

test:
	mkdir -p icu
	cp icu.utf8.so icu/utf8.so
	cp icu.ustring.so icu/ustring.so
	lua -l icu.utf8 -e 'print(icu.utf8.upper("Hello world"))'
	lua -l icu.ustring -e 'print(icu.ustring.upper(icu.ustring.decode("Hello world")))'

