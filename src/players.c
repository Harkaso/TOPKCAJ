/**
 * @file players.c
 * @brief Programme client gérant les Bots (Joueurs autonomes).
 *
 * Ce fichier contient la logique des processus enfants qui simulent des joueurs.
 * Ils se connectent à la mémoire partagée, surveillent l'état du jeu,
 * placent des paris basés sur des probabilités et gèrent leur cycle de vie.
 */

#include "shared.h"
#include <sys/wait.h>
#include <signal.h>

/**
 * @brief Flag global de contrôle d'exécution.
 * 
 * Modifié par le gestionnaire de signal (SIGINT/SIGTERM) pour permettre
 * une sortie propre de la boucle de jeu.
 */
volatile sig_atomic_t running = 1;


/**
 * @brief Enregistre un événement de mutex dans le buffer circulaire.
 * 
 * Utilitaire interne pour logger les accès au sémaphore 
 * dans la structure partagée.
 * 
 * @param shm Pointeur vers la mémoire partagée.
 * @param pid PID du processus générant l'événement.
 * @param status 1 pour Verrouillage, 0 pour Déverrouillage.
 */
static void push_mutex_event(SharedResource *shm, pid_t pid, int status) {
    if (!shm) return;
    int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
    shm->mutex_events[idx].ts = time(NULL);
    shm->mutex_events[idx].pid = pid;
    shm->mutex_events[idx].status = status;
    shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
    if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
}


/**
 * @brief Génère un pari aléatoire réaliste.
 * 
 * Utilise un générateur de nombres aléatoires et une formule mathématique
 * pour déterminer le type de pari et créer la mise.
 * 
 * `Formule de score: 100 - (ABS(N-4) * 4) + (N * 3,5)`
 * 
 * @param m Pointeur vers la structure Bet à remplir.
 * @param player_id Identifiant du joueur (utilisé pour déterminer la couleur du jeton).
 * @param bet_price Montant de la mise à jouer.
 */
void create_random_bet(Bet *m, int player_id, int bet_price) {
    m->pid = getpid();
    m->color_id = player_id;
    m->amount = bet_price;
    
    int roll = rand() % 10000;

    if (roll < 1538) {
        m->type = BET_SQUARE; m->count = 4;
        int base = 1 + rand() % 32;
        if (base % 3 == 0) base--; 
        m->numbers[0]=base;   m->numbers[1]=base+1;
        m->numbers[2]=base+3; m->numbers[3]=base+4;
    }

    else if (roll < 3063) {
        m->type = BET_DOUBLE_STREET; m->count = 6;
        int row = ((rand() % 33) / 3) * 3 + 1;
        if(row>31) row=31;
        for(int k=0;k<6;k++) m->numbers[k]=row+k;
    }

    else if (roll < 4548) {
        m->type = BET_DOZEN_1 + (rand() % 6);
        m->count = 0;
    }

    else if (roll < 5992) {
        m->type = BET_RED + (rand() % 6);
        m->count = 0;
    }

    else if (roll < 7429) {
        m->type = BET_STREET; m->count = 3;
        int row = ((rand() % 36) / 3) * 3 + 1;
        for(int k=0;k<3;k++) m->numbers[k]=row+k;
    }

    else if (roll < 8760) {
        m->type = BET_SPLIT; m->count = 2;
        if (rand() % 20 == 0) {
             m->numbers[0]=0; m->numbers[1]=37;
        } else {
            if (rand() % 2 == 0) {
                int base = 1 + rand() % 35;
                if (base % 3 == 0) base--; 
                m->numbers[0] = base; m->numbers[1] = base + 1;
            } else {
                int base = 1 + rand() % 33;
                m->numbers[0] = base; m->numbers[1] = base + 3;
            }
        }
    }

    else {
        m->type = BET_SINGLE; m->count = 1;
        m->numbers[0] = rand() % 38; 
    }
}

/**
 * @brief Gestionnaire de signal (SIGINT/SIGTERM).
 * 
 * Passe le flag `running` à 0 pour terminer proprement la boucle principale.
 * @param sig Numéro du signal reçu.
 */
void handle_sig(int sig) {
    running = 0;
}

/**
 * @brief Fonction principale d'un processus joueur.
 * 
 * Exécute le cycle de vie complet d'un joueur :
 * 1. Attachement à la mémoire partagée.
 * 2. Boucle de jeu :
 *    - Surveillance de l'état du jeu et attendre de pouvoir jouer.
 *    - Prise de décision et placement du pari.
 * 3. Nettoyage et sortie.
 * 
 * @param player_id ID interne du joueur pour la couleur du jeton.
 * @param bet_price Montant de la mise.
 */
void launch_bot(int player_id, int bet_price) {
    srand(time(NULL) ^ (getpid()<<16));
    
    // Attachement SHM
    int shmid = shmget(SHM_KEY, sizeof(SharedResource), 0666);
    if (shmid < 0) exit(1);
    SharedResource *shm = (SharedResource *)shmat(shmid, NULL, 0);

    // Gestion des signaux
    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);

    int bet_placed = 0;

    // Boucle principale
    while (running) {
        // Logique de pari
        if (shm->state == BETS_OPEN && !bet_placed) {
            usleep((rand() % 2300001) + 150000);
            
            // Placer le pari
            sem_wait(&shm->mutex);
            shm->mutex_status = 1; shm->mutex_owner = getpid();
            usleep(200000);
            push_mutex_event(shm, shm->mutex_owner, 1);

            if (shm->total_bets < MAX_BETS && shm->state == BETS_OPEN && shm->bank >= bet_price) 
            {
                shm->bank -= bet_price;
                shm->total_gains -= bet_price;
                Bet m;
                
                create_random_bet(&m, player_id, bet_price);
                
                shm->bets[shm->total_bets] = m;
                shm->total_bets++;
                bet_placed = 1;
            }

            shm->mutex_status = 0; shm->mutex_owner = 0;
            push_mutex_event(shm, getpid(), 0);
            sem_post(&shm->mutex);
        }

        // Reset pour le prochain tour
        if (shm->state == RESULTS) bet_placed = 0;
        
        usleep(100000);
    }

    // Gestion de l'arrêt
    shmdt(shm);
    exit(0);
}

/**
 * @brief Point d'entrée du Launcher de Bots.
 * 
 * Parse les arguments ligne de commande et utilise `fork()` pour lancer
 * le nombre spécifié de processus joueurs en parallèle.
 * 
 *  * Accepte les arguments en ligne de commande :
 * - `--bots <int>` : Nombre de joueurs a initialisé (de 4 à 16).
 */
int main(int argc, char *argv[]) {
    int bots_to_launch = DEFAULT_BOTS;
    int bet_price = DEFAULT_BET_PRICE;

       // Parsing des arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bots") == 0 && i+1 < argc) {
            bots_to_launch = atoi(argv[i+1]); i++;
        }
        else if (strcmp(argv[i], "--bet-price") == 0 && i+1 < argc) {
            bet_price = atoi(argv[i+1]); i++;
        }
    }

    printf("[Server] %d joueurs ont rejoint la partie.\n", bots_to_launch);

    // Lancement des bots joueurs
    for (int i = 0; i < bots_to_launch; i++) {
        if (fork() == 0) { 
            launch_bot(i % 16, bet_price); 
            exit(0); 
        }
    }
    
    while(wait(NULL) > 0);
    return 0;
}
