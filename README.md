# ETOPKCAJ - Roulette Américaine

## Contexte du Projet

Ce projet a été réalisé dans le cadre du module **Système d'exploitation avancé**.
L'objectif est l'étude et la mise en œuvre des mécanismes permettant à des processus distincts de communiquer et de se synchroniser *(Communication Inter-Processus IPC)*.

> **Intitulé :** Communication inter-processus (IPC) : mémoire partagée et échange de messages
> - Étude des mécanismes d’IPC permettant aux processus de communiquer et se synchroniser.
> - réaliser une application utilisant mémoire partagée synchronisée (mutex, sémaphores) ou communication par messages/threads.

## Architecture Technique

L'application simule une table de roulette américaine où plusieurs processus interagissent en temps réel autour d'une ressource commune.

### 1. Mémoire Partagée (Shared Memory)
Nous utilisons `<sys/shm.h>` pour créer un segment mémoire accessible par tous les processus.
*   **Données partagées :** État du jeu (Mises ouvertes/fermé), Banque commune, Tableau des paris, Historique des événements.

### 2. Synchronisation (Sémaphores)
L'accès concurrent à la mémoire partagée est protégé par un **Sémaphore POSIX** (`<semaphore.h>`) utilisé en tant que **Mutex** (Exclusion Mutuelle).
*   **Problème résolu :** Évite les corruptions de données (Race Condition) lorsque plusieurs bots tentent simultanément de modifier la banque ou d'ajouter un pari sur la table.
*   **Implémentation :** Pattern `sem_wait()` (Lock) avant écriture et `sem_post()` (Unlock) après écriture.

### 3. Gestion des Processus
Le système est multi-processus orchestré via `fork()`:
*   **Le Serveur (Croupier) :** Processus maître. Il gère le cycle de vie (Mises -> Tirage -> Paiement) et écrit l'état du jeu.
*   **Les Bots (Joueurs) :** Processus enfants multiples (4 à 16). Ils lisent l'état et écrivent des paris de manière autonome.
*   **L'Interface (GUI) :** Processus observateur. Affiche l'état de la mémoire via **Raylib**.

---

## Structure des Fichiers

*   `shared.h` : Définition de la structure `SharedResource` et du protocole d'échange.
*   `server.c` : Logique du Croupier, création de la SHM et du Sémaphore.
*   `players.c` : Logique des Bots (Joueurs).
*   `app.c` : Interface graphique et Launcher principal.
*   `launcher.c` : Wrapper pour lancer l'application avec des arguments.

---

## Installation et Compilation

### Prérequis
*   GCC, Make
*   Librairie `Raylib`
*   Environnement Linux/Unix.
```bash
sudo apt update

sudo apt install build-essential git

sudo apt install libraylib-dev

# Si erreur ou version trop ancienne, installer raylib manuellement
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
git clone https://github.com/raysan5/raylib.git raylib
cd raylib/src/
make PLATFORM=PLATFORM_DESKTOP
sudo make install
```

### Compilation

À la racine du projet, exécutez simplement :

#### Compiler simplement
```bash
make
```

#### Nettoyer les fichiers executables
```bash
make clean
```

#### Recompiler de zéro
```bash
make rebuild
```

#### Compiler la documentation
```bash
make doc
make doc-pdf
```

---

## Utilisation

Lancez simplement le launcher à la racine du projet (aprés avoir compilé):

```bash
./launcher
```

### Options disponibles
Vous pouvez surcharger la configuration par défaut :

```bash
./launcher --bots [int] --bank [int] --bet-price [int]
```
*   `--bots` : Nombre de joueurs (dois être entre 4 et 16).
*   `--bank` : Montant de la banque commune des joueurs.
*   `--bet-price` : Montant d'une mise.

---

## Cycle de Jeu 
Phases complètes du jeu:
*   Phase "Faites vos jeux" (Accès écriture autorisé pour les bots).
*   Phase "Rien ne va plus" (Verrouillage par le serveur).
*   Phase "Résultats" (Distribution des gains).

---

## Auteurs
Projet réalisé par **Oussam SALAH** et **Mouhcine ABDELMOUMENE**, étudiants en **4ème année d'ingénieur** spécialité **Sécurité Informatique** à l'**Université des Sciences et des Technologies d'Oran**.