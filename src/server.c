// server.c
#include "casino.h"

#define OPEN_TIME 7
#define CLOSE_TIME 3
#define RESULT_TIME 10
int shmid;
GameTable *shm;

void cleanup(int sig) {
    printf("\n[Server] Nettoyage IPC et arrêt...\n");
    if (shm != NULL) {
        sem_destroy(&shm->mutex);
        shmdt(shm);
    }
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

int is_red(int n) {
    if (n == 0 || n == 37) return 0;
    int reds[] = {1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
    for(int i=0; i<18; i++) if(reds[i] == n) return 1;
    return 0;
}

int is_inside_bet(int type) {
    return (type >= BET_SINGLE && type <= BET_TOP_LINE);
}

void print_bet_desc(Bet m) {
    switch(m.type) {
        case BET_SINGLE: printf("PLEIN sur %d", m.numbers[0]); break;
        case BET_SPLIT: printf("CHEVAL sur %d-%d", m.numbers[0], m.numbers[1]); break;
        case BET_STREET: printf("TRANSVERSALE sur %d-%d-%d", m.numbers[0], m.numbers[1], m.numbers[2]); break;
        case BET_SQUARE: printf("CARRE sur %d-%d-%d-%d", m.numbers[0], m.numbers[1], m.numbers[2], m.numbers[3]); break;
        case BET_DOUBLE_STREET: printf("SIXAIN de %d a %d", m.numbers[0], m.numbers[5]); break;
        case BET_TRIO: printf("TRIO sur %d-%d-%d", m.numbers[0], m.numbers[1], m.numbers[2]); break;
        case BET_TOP_LINE: printf("TOP LINE (0-00-1-2-3)"); break;
        case BET_RED: printf("ROUGE"); break;
        case BET_BLACK: printf("NOIR"); break;
        case BET_EVEN: printf("PAIR"); break;
        case BET_ODD: printf("IMPAIR"); break;
        case BET_LOW: printf("MANQUE (1-18)"); break;
        case BET_HIGH: printf("PASSE (19-36)"); break;
        case BET_DOZEN_1: printf("1ere DOUZAINE"); break;
        case BET_DOZEN_2: printf("2eme DOUZAINE"); break;
        case BET_DOZEN_3: printf("3eme DOUZAINE"); break;
        case BET_COL_1: printf("COLONNE 1"); break;
        case BET_COL_2: printf("COLONNE 2"); break;
        case BET_COL_3: printf("COLONNE 3"); break;
        default: printf("Type %d", m.type); break;
    }
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    srand(time(NULL));

    // Création SHM
    shmid = shmget(SHM_KEY, sizeof(GameTable), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }
    shm = (GameTable *)shmat(shmid, NULL, 0);
    
    // --- INITIALISATION SEMAPHORE POSIX ---
    // Param 1: pointeur vers le sem
    // Param 2: 1 = partagé entre processus (C'est ce qui remplace IPC System V) [cite: 644]
    // Param 3: 1 = valeur initiale (déverrouillé) [cite: 646]
    sem_init(&shm->mutex, 1, 1); 

    shm->state = BETS_OPEN;
    shm->total_bets = 0;
    shm->bank = START_BANK;

    printf("[Croupier] Casino Ouvert. Attente des joueurs...\n");

    while (1) {
        // --- PHASE 1: MISES OUVERTES ---
        printf("\n[Croupier] FAITES VOS JEUX\n");
        shm->state = BETS_OPEN;
        sleep(OPEN_TIME);

        // --- PHASE 2: RIEN NE VA PLUS ---
        printf("[Croupier] RIEN NE VA PLUS\n");
        sem_wait(&shm->mutex);
        shm->state = BETS_CLOSED;
        sem_post(&shm->mutex);        
        sleep(CLOSE_TIME);

        // --- PHASE 3: TIRAGE ---
        int win = rand() % 38;
        shm->winning_number = win;

        printf("[Croupier] RESULTAT: %s\n", (win == 37) ? "00" : (win == 0) ? "0" : "Nombre");
        if (win != 0 && win != 37) printf("[Croupier] Le numero est %d\n", win);

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

        // --- PHASE 4: PAIEMENT ---
        sem_wait(&shm->mutex);
        
        int total_gains = 0;
        for (int i = 0; i < shm->total_bets; i++) {
            Bet m = shm->bets[i];
            int won = 0;
            int ratio = 0;

            if (is_inside_bet(m.type)) {
                for (int k = 0; k < m.count; k++) {
                    if (m.numbers[k] == win) { won = 1; break; }
                }
                switch(m.type) {
                    case BET_SINGLE: ratio = 35; break;
                    case BET_SPLIT: ratio = 17; break;
                    case BET_STREET: ratio = 11; break;
                    case BET_SQUARE: ratio = 8; break;
                    case BET_DOUBLE_STREET: ratio = 5; break;
                    case BET_TRIO: ratio=11; break;
                    case BET_TOP_LINE: ratio=6; break;
                    default: ratio = 1;
                }
            } else {
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
                int profit = m.amount * ratio;
                int total_return = profit + m.amount;

                shm->bank += total_return;
                total_gains += profit;

                printf("  -> Joueur %d (PID %d) A GAGNE %d$ ! [Bet: ", m.color_id+1, m.pid, total_return);
                print_bet_desc(m);
                printf("]\n");
            }
        }
        sem_post(&shm->mutex);
        if(total_gains == 0) printf("[Croupier] Aucun joueur ne gagne, la maison gagne tout.\n");

        // --- PHASE 5: AFFICHAGE ---
        shm->state = RESULTS;
        sleep(RESULT_TIME); 

        // --- RESET ---
        sem_wait(&shm->mutex);
        shm->total_bets = 0;
        sem_post(&shm->mutex);
    }
    return 0;
}
