CFLAGS=`pkg-config --cflags gtk+-3.0 libvlc`
LIBS=`pkg-config --libs gtk+-3.0 libvlc`

all:
	gcc -o cli1 cli.c -Wall -lpthread ${LIBS} ${CFLAGS}
	gcc -o ser ser.c -w -lpthread
copy:
		cp cli1 ./cli2
		cp cli1 ./cli3
clean:
	rm -f cli1 cli2 cli3 ser
