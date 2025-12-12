CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS_GUI = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: folders server players app launcher

folders:
	@mkdir -p dependencies

server: src/server.c
	$(CC) src/server.c -o dependencies/server $(CFLAGS)

players: src/players.c
	$(CC) src/players.c -o dependencies/players $(CFLAGS)

app: src/app.c
	$(CC) src/app.c -o dependencies/app $(CFLAGS) $(LDFLAGS_GUI)

launcher: src/launcher.c
	$(CC) src/launcher.c -o launcher $(CFLAGS)

clean:
	rm -f launcher dependencies/server dependencies/players dependencies/app
	rm -f *.o

rebuild: clean all

doc:
	doxygen Doxyfile

doc-pdf:
	doxygen Doxyfile
	$(MAKE) -C latex
	cp latex/refman.pdf ./Documentation_Projet.pdf