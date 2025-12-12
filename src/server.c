// server.c
#include "shared.h"

#define OPEN_TIME 10
#define CLOSE_TIME 2
#define RESULT_TIME 13
// Watchdog: if a process holds the mutex but its last_seen is older than this many seconds,
// the server will try to clear the lock to avoid permanent deadlock.
#define MUTEX_WATCHDOG_SECONDS 6
int shmid;
SharedResource *shm;

void cleanup(int sig) {
    printf("\n[Server] Arret du jeu...\n");
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
    return (type >= BET_SINGLE && type <= BET_DOUBLE_STREET);
}

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

int main(int argc, char *argv[]) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    srand(time(NULL) ^ (getpid()<<16));

    int start_bank = DEFAULT_BANK;
    int bet_price = DEFAULT_BET_PRICE;

    for(int i=1; i<argc; i++) {
        if(strcmp(argv[i], "--bank") == 0 && i+1 < argc) {
            start_bank = atoi(argv[i+1]); i++;
        }
        else if(strcmp(argv[i], "--bet-price") == 0 && i+1 < argc) {
            bet_price = atoi(argv[i+1]); i++;
        }
    }

    // Création SHM
    shmid = shmget(SHM_KEY, sizeof(SharedResource), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }
    shm = (SharedResource *)shmat(shmid, NULL, 0);
    
    // --- INITIALISATION SEMAPHORE POSIX ---
    // Param 1: pointeur vers le sem
    // Param 2: 1 = partagé entre processus (C'est ce qui remplace IPC System V) [cite: 644]
    // Param 3: 1 = valeur initiale (déverrouillé) [cite: 646]
    sem_init(&shm->mutex, 1, 1); 

    shm->state = BETS_OPEN;
    shm->total_bets = 0;
    shm->bank = start_bank;

    // Initialize player registry
    for (int i = 0; i < MAX_BOTS; i++) {
        shm->players[i].pid = 0;
        shm->players[i].status = 0;
        shm->players[i].color_id = 0;
        shm->players[i].last_seen = 0;
    }
    shm->player_count = 0;
    shm->mutex_status = 0;
    shm->mutex_owner = 0;
    shm->mutex_events_head = 0;
    shm->mutex_events_count = 0;

    printf("[Server] Lancement du jeu...\n");
    usleep(100000);

    while (1) {
        // --- WATCHDOG: detect stale mutex owner and try to recover ---
        if (shm->mutex_status && shm->mutex_owner != 0) {
            // find owner in players and check last_seen
            time_t now = time(NULL);
            int owner_idx = -1;
            for (int i = 0; i < MAX_BOTS; i++) {
                if (shm->players[i].pid == shm->mutex_owner) { owner_idx = i; break; }
            }

            int stale = 0;
            if (owner_idx >= 0) {
                if (now - shm->players[owner_idx].last_seen > MUTEX_WATCHDOG_SECONDS) stale = 1;
            } else {
                // owner not found in players list -> consider stale
                stale = 1;
            }

            if (stale) {
                int sval = 0;
                sem_getvalue(&shm->mutex, &sval);
                if (sval == 0) {
                    // semaphore appears locked; try to post and clear flags
                    printf("[Server-Watchdog] Detected stale mutex owner PID %d, releasing semaphore...\n", (int)shm->mutex_owner);
                    sem_post(&shm->mutex); // try to release
                } else {
                    printf("[Server-Watchdog] Detected stale mutex owner PID %d but semaphore value=%d; clearing flags\n", (int)shm->mutex_owner, sval);
                }
                // record unlock event
                int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
                shm->mutex_events[idx].ts = time(NULL);
                shm->mutex_events[idx].pid = shm->mutex_owner;
                shm->mutex_events[idx].status = 0;
                shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
                if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;

                shm->mutex_status = 0;
                shm->mutex_owner = 0;
            }
        }
        // --- PHASE 1: MISES OUVERTES ---
        printf("\n[Croupier] FAITES VOS JEUX\n");
        shm->state = BETS_OPEN;
        sleep(OPEN_TIME);

        // --- PHASE 2: RIEN NE VA PLUS ---
        printf("[Croupier] RIEN NE VA PLUS\n");
        sem_wait(&shm->mutex);
        shm->mutex_status = 1; 
        shm->mutex_owner = getpid();
        usleep(200000);
        // record lock event
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = shm->mutex_owner;
            shm->mutex_events[idx].status = 1; // LOCK
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        shm->state = BETS_CLOSED;
        /*
        // record unlock event
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = getpid();
            shm->mutex_events[idx].status = 0;
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        shm->mutex_status = 0; shm->mutex_owner = 0;
        sem_post(&shm->mutex);  
        */      
        sleep(CLOSE_TIME);

        // --- PHASE 3: TIRAGE ---
        int win = rand() % 38;
        shm->winning_number = win;

        printf("[Croupier] RESULTAT: ");
        if (win == 37) printf("%s", "00 | VERT");
        else if (win == 0) printf("%s", "0 | VERT");
        else {
            // --- Calcul des propriétés pour l'affichage ---
            const char* s_color = is_red(win) ? "ROUGE" : "NOIR";
            const char* s_parity = (win % 2 == 0) ? "PAIR" : "IMPAIR";
            const char* s_half = (win <= 18) ? "1 A 18" : "19 A 36";
            
            // Douzaine (D1, D2, D3)
            const char* s_doz;
            if (win <= 12) s_doz = "1ere 12";
            else if (win <= 24) s_doz = "2eme 12";
            else s_doz = "3eme 12";

            // Colonne (C1, C2, C3)
            const char* s_col;
            if (win % 3 == 1) s_col = "COLONNE 1";
            else if (win % 3 == 2) s_col = "COLONNE 2";
            else s_col = "COLONNE 3";

            // Affichage complet : Num - Coul - Parité - Moitié - Douz - Col
            printf("%d | %s | %s | %s | %s | %s\n", 
                   win, s_color, s_parity, s_half, s_doz, s_col);
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

        // --- PHASE 4: PAIEMENT ---      
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
                shm->total_gains += total_return;

                printf("  -> #%d (PID %d) A GAGNE %d$ [Pari: ", m.color_id+1, m.pid, total_return);
                print_bet_desc(m);
                printf("]\n");
            }
        }
        if(shm->total_gains == -bet_price * shm->total_bets) printf("  -> Aucun joueur ne gagne, la maison raffle %d$.\n", -shm->total_gains);
        else if(shm->total_gains < 0) printf("  -> la maison prends %d$.\n", -shm->total_gains);


        // --- PHASE 5: AFFICHAGE ---
        shm->state = RESULTS;
        sleep(RESULT_TIME-2); 
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = getpid();
            shm->mutex_events[idx].status = 0; // FREE
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        
        shm->mutex_status = 0; 
        shm->mutex_owner = 0;
        sem_post(&shm->mutex);
        sleep(2); 

        // --- RESET ---
        sem_wait(&shm->mutex);
        shm->mutex_status = 1;
        shm->mutex_owner = getpid();
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = shm->mutex_owner;
            shm->mutex_events[idx].status = 1; // LOCK
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        shm->total_bets = 0;
        shm->total_gains = 0;
        // record unlock event
        {
            int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            shm->mutex_events[idx].ts = time(NULL);
            shm->mutex_events[idx].pid = getpid();
            shm->mutex_events[idx].status = 0; // FREE
            shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
            if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
        }
        
        shm->mutex_status = 0; 
        shm->mutex_owner = 0;
        sem_post(&shm->mutex);
    }
    return 0;
}
