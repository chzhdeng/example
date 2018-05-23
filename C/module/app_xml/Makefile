CC=gcc -Wall -g
LIBS=`xml2-config --libs`
CFLAGS=`xml2-config --cflags`

readxml: readxml.o;$(CC) -o readxml readxml.o $(LIBS)

readxml.o: readxml.c;$(CC) -c readxml.c $(CFLAGS)

clean:
	rm  readxml
	rm readxml.o
