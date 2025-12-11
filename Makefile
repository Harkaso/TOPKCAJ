# --- CONFIGURATION ---
CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS_GUI = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# --- CIBLES (TARGETS) ---
all: folders server players app launcher

folders:
	@mkdir -p dependencies

# Compilation du Serveur
server: src/server.c
	$(CC) src/server.c -o dependencies/server $(CFLAGS)

# Compilation du Joueur
players: src/players.c
	$(CC) src/players.c -o dependencies/players $(CFLAGS)

# Compilation de l'Interface Graphique
app: src/app.c
	$(CC) src/app.c -o dependencies/app $(CFLAGS) $(LDFLAGS_GUI)

# Compilation du Launcher
launcher: src/launcher.c
	$(CC) src/launcher.c -o launcher $(CFLAGS)

# Nettoyage (commande "make clean")
clean:
	rm -f launcher dependencies/server dependencies/players dependencies/app
	rm -f *.o

# Pour tout recompiler de zéro ("make rebuild")
rebuild: clean all