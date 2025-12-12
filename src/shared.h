/**
 * @file shared.h
 * @brief Définitions des structures IPC et constantes partagés par les fichiers.
 *
 * Ce fichier contient toutes les définitions nécessaires au partage de ressouces
 * entre le serveur (croupier), les bots (joueurs) et l'interface graphique.
 * Il inclut la configuration du sémaphore POSIX et les états du jeu.
 */

#ifndef SHARED_H
#define SHARED_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>

/**
 * @defgroup Config Constants de Configuration
 * @brief Paramètres globaux du jeu.
 * @{
 */
#define SHM_KEY 0x777
#define DEFAULT_BOTS 8
#define MIN_BOTS 4
#define MAX_BOTS 16
#define DEFAULT_BANK 2000
#define DEFAULT_BET_PRICE 25
#define MAX_BETS 50
#define MUTEX_EVENT_HISTORY 32
/** @} */

/**
 * @enum GameState
 * @brief Les différentes phases du cycle de jeu de la roulette.
 */
enum GameState { BETS_OPEN = 0, BETS_CLOSED = 1, RESULTS = 2 };

/**
 * @enum BetType
 * @brief Types de mises supportés.
 */
enum BetType {
    // mises interieurs
    BET_SINGLE = 0, BET_SPLIT, BET_STREET, BET_SQUARE, BET_DOUBLE_STREET,
    //mises exterieurs
    BET_RED, BET_BLACK, BET_EVEN, BET_ODD, BET_LOW, BET_HIGH,
    BET_DOZEN_1, BET_DOZEN_2, BET_DOZEN_3, BET_COL_1, BET_COL_2, BET_COL_3
};

/**
 * @struct Bet
 * @brief Représente un pari unique placé par un joueur.
 * 
 * Contient toutes les informations d'une mise faite par un joueur.
 */
typedef struct {
    pid_t pid;       /**< PID du joueur propriétaire du pari. */
    int type;        /**< Type de pari. */
    int numbers[6];  /**< Tableau des numéros couverts par ce pari. */
    int count;       /**< Nombre de numéros couverts (Taille du tableau `numbers`). */
    int amount;      /**< Montant de la mise. */
    int color_id;    /**< ID de la couleur prise par ce joueur. */
} Bet;

/**
 * @struct MutexEvent
 * @brief Entrée de log pour l'historique d'utilisation du sémaphore.
 * 
 * Permet de garder et tracer l'activité recente du mutex.
 */
typedef struct {
    time_t ts;   /**< Timestamp de l'événement. */
    pid_t pid;   /**< PID du processus ayant accédé au mutex. */
    int status;  /**< Type d'action : 1 = Verrouillage, 0 = Déverrouillage. */
} MutexEvent;

/**
 * @struct SharedResource
 * @brief La structure principale stockée en Mémoire Partagée (SHM).
 * 
 * C'est l'objet unique partagé entre tous les processus. Il contient l'état complet
 * du jeu, la banque commune, les joueurs et les mécanismes de synchronisation.
 */
typedef struct {
    sem_t mutex;           /**< Sémaphore POSIX pour l'exclusion mutuelle. */
    int state;             /**< État actuel du jeu. */
    int winning_number;    /**< Le numéro gagnant du tour actuel. */
    Bet bets[MAX_BETS];    /**< Tableau stockant tous les paris du tour. */
    int total_bets;        /**< Nombre de paris actuellement dans le tableau. */
    int bank;              /**< Montant actuel de la banque commune des joueurs. */
    int total_gains;       /**< Bilan net du tour (Gains - Pertes). */
    int player_count;      /**< Nombre de joueurs actifs. */
    
    // Monitoring et Debugging
    int mutex_status;      /**< État observable du mutex (0=Libre, 1=Occupé). */
    pid_t mutex_owner;     /**< PID du processus détenant actuellement le mutex. */
    
    // Historique circulaire
    MutexEvent mutex_events[MUTEX_EVENT_HISTORY]; /**< Buffer circulaire des logs mutex. */
    int mutex_events_head;  /**< Index de la prochaine écriture dans le buffer. */
    int mutex_events_count; /**< Nombre total d'événements enregistrés. */
} SharedResource;

#endif