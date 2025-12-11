> ⚠️ **Ce programme fonctionne sur `Linux` uniquement.**

-----

## Compilation du code
```bash
# Pour tout compiler
username@hostname:~/<path_to_project>/Etopkcaj$ make # ou make all

# Pour tout effacer (retirer tout les executables)
username@hostname:~/<path_to_project>/Etopkcaj$ make clean

# Pour recompiler (si buggé ou corrompu) equivaut a make clean + make all
username@hostname:~/<path_to_project>/Etopkcaj$ make rebuild

# pour compiler un fichier specifique
username@hostname:~/<path_to_project>/Etopkcaj$ make server # compile seulement src/server.c
username@hostname:~/<path_to_project>/Etopkcaj$ make players # compile seulement src/players.c
username@hostname:~/<path_to_project>/Etopkcaj$ make app # compile seulement src/app.c (l'interface graphique)
username@hostname:~/<path_to_project>/Etopkcaj$ make launcher # compile seulement src/launcher.c

```

## Lancement du jeu
```bash
# Pour lancer l'application
username@hostname:~/<path_to_project>/Etopkcaj$ ./launcher
```