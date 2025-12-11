## Compilation du code
```bash
username@hostname:~/<path_to_project>/Etopkcaj_Project$ gcc src/server.c -o dependencies/server -pthread
username@hostname:~/<path_to_project>/Etopkcaj_Project$ gcc src/player.c -o dependencies/player -pthread
username@hostname:~/<path_to_project>/Etopkcaj_Project$ gcc src/roulette.c -o dependencies/roulette -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
username@hostname:~/<path_to_project>/Etopkcaj_Project$ gcc src/launcher.c -o launcher
```

## Lancement du jeu
```bash
username@hostname:~/<path_to_project>/Etopkcaj_Project$ ./launcher
```