run: libmmbs test Makefile
	./test


test: test.c libmmbs
	gcc -std=gnu89 -Wall -Werror -c -o test.o test.c 
	gcc -std=gnu89 -Wall -Werror -o test test.o mmbs.o 

libmmbs: mmbs.c mmbs.h
	gcc -std=gnu89 -Wall -Werror -c mmbs.c -o mmbs.o