/**
 * @file server.c
 * @brief Le Croupier : Serveur central de gestion du jeu.
 *
 * Ce fichier contient la logique principale du casino.
 * Il est responsable de :
 * - Créer et initialiser la Mémoire Partagée et le Sémaphore.
 * - Gérer les phases de jeu.
 * - Générer le numéro gagnant aléatoire.
 * - Calculer les gains et redistribuer la banque commune.
 */

 #include "shared.h"

 /**
 * @defgroup TimeSettings Paramètres de Temporisation
 * @brief Durées des différentes phases de jeu en secondes.
 * @{
 */
#define OPEN_TIME 10   /**< Durée de la phase de mise (FAITES VOS JEUX). */
#define CLOSE_TIME 2   /**< Durée de la phase de fermeture (RIEN NE VA PLUS). */
#define RESULT_TIME 13 /**< Durée de l'affichage des résultats et du paiements. */
/** @} */

/** @brief Identifiant système de la mémoire partagée (SHM ID). */
int shmid;

/** @brief Pointeur vers la structure partagée attachée à l'espace d'adressage. */
SharedResource *shm;

/**
 * @brief Gestionnaire de signal pour un arrêt propre.
 * 
 * Intercepte SIGINT (Ctrl+C) et SIGTERM.
 * - Détruit le sémaphore POSIX.
 * - Détache la mémoire partagée.
 * - Supprime le segment SHM du système.
 * 
 * @param sig Numéro du signal reçu.
 */
void cleanup(int sig) {
    printf("\n[Server] Arret du jeu...\n");
    if (shm != NULL) {
        sem_destroy(&shm->mutex);
        shmdt(shm);
    }
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

/**
 * @brief Vérifie si un numéro de roulette est Rouge.
 * 
 * Utilise un tableau codé en dur correspondant à la disposition
 * standard de la roulette.
 * 
 * @param n Le numéro à vérifier.
 * @return int 1 si le numéro est ROUGE, 0 s'il est NOIR ou VERT.
 */
int is_red(int n) {
    if (n == 0 || n == 37) return 0;
    int reds[] = {1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
    for(int i=0; i<18; i++) if(reds[i] == n) return 1;
    return 0;
}

/**
 * @brief Détermine si un type de pari est une "Mise Intérieure".
 * 
 * Les mises intérieures ont des rapports de gain plus élevés
 * mais des probabilités plus faibles.
 * 
 * @param type Le type de pari.
 * @return int 1 si c'est une mise intérieure, 0 sinon (mise extérieure).
 */
int is_inside_bet(int type) {
    return (type >= BET_SINGLE && type <= BET_DOUBLE_STREET);
}

/**
 * @brief Affiche une description textuelle d'un pari dans la console.
 * 
 * Traduit la mise (struct Bet) en une chaîne de caractères comprehensible
 * afficher dans la console.
 * 
 * @param m La structure du pari à décrire.
 */
void print_bet_desc(Bet m) {
    switch(m.type) {
        case BET_SINGLE: printf("PLEIN sur %d", m.numbers[0]); break;
        case BET_SPLIT: printf("CHEVAL sur %d-%d", m.numbers[0], m.numbers[1]); break;
        case BET_STREET: printf("TRANSVERSALE sur %d-%d-%d", m.numbers[0], m.numbers[1], m.numbers[2]); break;
        case BET_SQUARE: printf("CARRE sur %d-%d-%d-%d", m.numbers[0], m.numbers[1], m.numbers[2], m.numbers[3]); break;
        case BET_DOUBLE_STREET: printf("SIXAIN de %d a %d", m.numbers[0], m.numbers[5]); break;
        case BET_RED: printf("ROUGE"); break;
        case BET_BLACK: printf("NOIR"); break;
        case BET_EVEN: printf("PAIR"); break;
        case BET_ODD: printf("IMPAIR"); break;
        case BET_LOW: printf("1 A 18"); break;
        case BET_HIGH: printf("19 A 36"); break;
        case BET_DOZEN_1: printf("1ere 12"); break;
        case BET_DOZEN_2: printf("2eme 12"); break;
        case BET_DOZEN_3: printf("3eme 12"); break;
        case BET_COL_1: printf("COLONNE 1"); break;
        case BET_COL_2: printf("COLONNE 2"); break;
        case BET_COL_3: printf("COLONNE 3"); break;
        default: printf("Type %d", m.type); break;
    }
}

/**
 * @brief Point d'entrée du Serveur (Croupier).
 * 
 * Orchestre la boucle du jeu :
 * 1. Initialisation de l'IPC, la mémoire partager, la sémaphore POSIX, etc.
 * 2. Phase d'ouverture des mises.
 * 3. Verrouillage critique.
 * 4. Tirage aléatoire du numéro gagnant.
 * 5. Recherche des gagnants et calcul des gains.
 * 6. paimement et affichage des résultats.
 * 
 * Accepte les arguments en ligne de commande :
 * - `--bank <int>` : Montant initial de la banque commune de joueurs.
 * - `--bet-price <int>` : Prix d'une mise.
 */
int main(int argc, char *argv[]) {
    // Gestion des signaux
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Générateur aléatoire unifié par processus
    srand(time(NULL) ^ (getpid()<<16));

    int start_bank = DEFAULT_BANK;
    int bet_price = DEFAULT_BET_PRICE;

    // Parsing des arguments
    for(int i=1; i<argc; i++) {
        if(strcmp(argv[i], "--bank") == 0 && i+1 < argc) {
            start_bank = atoi(argv[i+1]); i++;
        }
        else if(strcmp(argv[i], "--bet-price") == 0 && i+1 < argc) {
            bet_price = atoi(argv[i+1]); i++;
        }
    }

    // Initialisation SHM
    shmid = shmget(SHM_KEY, sizeof(SharedResource), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }
    shm = (SharedResource *)shmat(shmid, NULL, 0);
    
    // initialisation sémaphore POSIX
    sem_init(&shm->mutex, 1, 1); 

    shm->state = BETS_OPEN;
    shm->total_bets = 0;
    shm->bank = start_bank;

    shm->player_count = 0;
    shm->mutex_status = 0;
    shm->mutex_owner = 0;
    shm->mutex_events_head = 0;
    shm->mutex_events_count = 0;

    printf("[Server] Lancement du jeu...\n");
    usleep(100000);

    // Boucle principale
    while (1) {
        // Phase 1: Mises ouvertes
        printf("\n[Croupier] FAITES VOS JEUX\n");
        shm->state = BETS_OPEN;
        sleep(OPEN_TIME);

        // Phase 2: Fin des mises
        printf("[Croupier] RIEN NE VA PLUS\n");
        sem_wait(&shm->mutex);
        shm->mutex_status = 1; 
        shm->mutex_owner = getpid();
        usleep(200000);
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = shm->mutex_owner;
            shm->mutex_events[idx].status = 1;
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        shm->state = BETS_CLOSED;   
        sleep(CLOSE_TIME);

        // Phase 3: Tirage
        int win = rand() % 38;
        shm->winning_number = win;

        // Affichage des résultats
        printf("[Croupier] RESULTAT: ");
        if (win == 37) printf("%s", "00 | VERT");
        else if (win == 0) printf("%s", "0 | VERT");
        else {
            const char* str_color = is_red(win) ? "ROUGE" : "NOIR";
            const char* str_parity = (win % 2 == 0) ? "PAIR" : "IMPAIR";
            const char* str_half = (win <= 18) ? "1 A 18" : "19 A 36";
            
            const char* str_doz;
            if (win <= 12) str_doz = "1ere 12";
            else if (win <= 24) str_doz = "2eme 12";
            else str_doz = "3eme 12";

            const char* str_col;
            if (win % 3 == 1) str_col = "COLONNE 1";
            else if (win % 3 == 2) str_col = "COLONNE 2";
            else str_col = "COLONNE 3";

            printf("%d | %s | %s | %s | %s | %s\n", 
                   win, str_color, str_parity, str_half, str_doz, str_col);
        }
        printf("\n");

        // Analyse résultats
        int is_red_res = is_red(win);
        int is_even = (win % 2 == 0 && win != 0 && win != 37);
        int is_low = (win >= 1 && win <= 18);
        int col_res = 0;
        if (win != 0 && win != 37) {
            if (win % 3 == 1) col_res = 1;
            else if (win % 3 == 2) col_res = 2;
            else col_res = 3;
        }
        int doz_res = 0;
        if (win >= 1 && win <= 12) doz_res = 1;
        else if (win >= 13 && win <= 24) doz_res = 2;
        else if (win >= 25 && win <= 36) doz_res = 3;

        // Phase 4: Paiement des gains   
        for (int i = 0; i < shm->total_bets; i++) {
            Bet m = shm->bets[i];
            int won = 0;
            int ratio = 0;

            if (is_inside_bet(m.type)) {
                for (int k = 0; k < m.count; k++) {
                    if (m.numbers[k] == win) { won = 1; break; }
                }
                // ratios des mises interieurs
                switch(m.type) {
                    case BET_SINGLE: ratio = 35; break;
                    case BET_SPLIT: ratio = 17; break;
                    case BET_STREET: ratio = 11; break;
                    case BET_SQUARE: ratio = 8; break;
                    case BET_DOUBLE_STREET: ratio = 5; break;
                    default: ratio = 1;
                }
            } else {
                // ratios des mises exterieurs
                switch(m.type) {
                    case BET_RED: if (is_red_res) { won=1; ratio=1; } break;
                    case BET_BLACK: if (!is_red_res && win!=0 && win!=37) { won=1; ratio=1; } break;
                    case BET_EVEN: if (is_even) { won=1; ratio=1; } break;
                    case BET_ODD: if (!is_even && win!=0 && win!=37) { won=1; ratio=1; } break;
                    case BET_LOW: if (is_low) { won=1; ratio=1; } break;
                    case BET_HIGH: if (!is_low && win!=0 && win!=37) { won=1; ratio=1; } break;
                    case BET_COL_1: if (col_res == 1) { won=1; ratio=2; } break;
                    case BET_COL_2: if (col_res == 2) { won=1; ratio=2; } break;
                    case BET_COL_3: if (col_res == 3) { won=1; ratio=2; } break;
                    case BET_DOZEN_1: if (doz_res == 1) { won=1; ratio=2; } break;
                    case BET_DOZEN_2: if (doz_res == 2) { won=1; ratio=2; } break;
                    case BET_DOZEN_3: if (doz_res == 3) { won=1; ratio=2; } break;
                }
            }

            if (won) {
                // calcule et paiement
                int profit = m.amount * ratio;
                int total_return = profit + m.amount;

                shm->bank += total_return;
                shm->total_gains += total_return;

                printf("  -> #%d (PID %d) A GAGNE %d$ [Pari: ", m.color_id+1, m.pid, total_return);
                print_bet_desc(m);
                printf("]\n");
            }
        }
        if(shm->total_gains == -bet_price * shm->total_bets) printf("  -> Aucun joueur ne gagne, la maison raffle %d$.\n", -shm->total_gains);
        else if(shm->total_gains < 0) printf("  -> la maison prends %d$.\n", -shm->total_gains);


        // Phases 5: Resultats et reset
        shm->state = RESULTS;
        sleep(RESULT_TIME-2); 
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = getpid();
            shm->mutex_events[idx].status = 0; 
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        
        shm->mutex_status = 0; 
        shm->mutex_owner = 0;
        sem_post(&shm->mutex);
        sleep(2); 

        // Nettoyage pour passage au tour suivant
        sem_wait(&shm->mutex);
        shm->mutex_status = 1;
        shm->mutex_owner = getpid();
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = shm->mutex_owner;
            shm->mutex_events[idx].status = 1;
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        shm->total_bets = 0;
        shm->total_gains = 0;
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = getpid();
            shm->mutex_events[idx].status = 0; 
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        
        shm->mutex_status = 0; 
        shm->mutex_owner = 0;
        sem_post(&shm->mutex);
    }
    return 0;
}
