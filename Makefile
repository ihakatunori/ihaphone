#
# Makefile for IHAPHONE
#
dir_jp=/usr/share/locale/ja/LC_MESSAGES


.c.o:
	$(CC) -c -g -Wall -I./ -o $@ $< `pkg-config --cflags gtk+-3.0 `

.SUFFIX: .c .o

ihaphone: ihaphone.o iha.o
	$(CC) -g -Wall -o ihaphone $^ `pkg-config --libs gtk+-3.0 ` -lasound  -lpthread -ljpeg -lX11 -lcrypto  -lgmp -lm -lz

ihakeygen: ihakeygen.o
	$(CC) -g -Wall -o ihakeygen $^ -lcrypto -lgmp

set_locale: set_locale.o
	$(CC) -g -Wall -o set_locale $^ 

all: ihaphone ihakeygen set_locale

clean:
	-rm -f *.o
	-rm -f ihaphone ihakeygen set_locale
