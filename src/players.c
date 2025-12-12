// players.c
#include "shared.h"
#include <sys/wait.h>
#include <signal.h>

volatile sig_atomic_t running = 1;


// helper to append mutex events into SHM circular buffer (caller should hold shm->mutex)
static void push_mutex_event(SharedResource *shm, pid_t pid, int status) {
    if (!shm) return;
    int idx = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
    shm->mutex_events[idx].ts = time(NULL);
    shm->mutex_events[idx].pid = pid;
    shm->mutex_events[idx].status = status;
    shm->mutex_events_head = (idx + 1) % MUTEX_EVENT_HISTORY;
    if (shm->mutex_events_count < MUTEX_EVENT_HISTORY) shm->mutex_events_count++;
}

/*
score formula = 100 - (ABS(N-4) * 4) + (N * 3,5)

+-----------+-----+-------+---------+-------+
| name      | num | score | perc (%)| seuil |
|-----------|-----|-------|---------|-------|
| carre     | 4   | 114.0 | 15.38 % | 1538  |
| sixain    | 6   | 113.0 | 15.25 % | 3063  |
| col/doz   | 12  | 110.0 | 14.84 % | 4548  |
| simple    | 18  | 107.0 | 14.44 % | 5992  |
| transver. | 3   | 106.5 | 14.37 % | 7429  |
| cheval    | 2   | 99.0  | 13.36 % | 8760  |
| plein     | 1   | 91.5  | 12.35 % | -     |
+-----------+-----+-------+---------+-------+
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
        // 5% de chance de tenter un Split spécial avec 0
        if (rand() % 20 == 0) {
             m->numbers[0]=0; m->numbers[1]=37; // 0-00
        } else {
            if (rand() % 2 == 0) { // Horizontal
                int base = 1 + rand() % 35;
                if (base % 3 == 0) base--; 
                m->numbers[0] = base; m->numbers[1] = base + 1;
            } else { // Vertical
                int base = 1 + rand() % 33;
                m->numbers[0] = base; m->numbers[1] = base + 3;
            }
        }
    }

    else {
        m->type = BET_SINGLE; m->count = 1;
        // 0 à 37 (inclus 00 représenté par 37)
        m->numbers[0] = rand() % 38; 
    }
}



// register the player in the shared table

// helper: register slot
int register_player(SharedResource *shm_local, int pid_color) {
    int idx = -1;
    sem_wait(&shm_local->mutex);

    // Log lock
    shm_local->mutex_status = 1; shm_local->mutex_owner = getpid();
    push_mutex_event(shm_local, shm_local->mutex_owner, 1);

    // Recherche slot vide
    for (int i = 0; i < MAX_BOTS; i++) {
        if (shm_local->players[i].status == 0) {
            shm_local->players[i].pid = getpid();
            shm_local->players[i].status = 1;
            shm_local->players[i].color_id = pid_color;
            shm_local->players[i].last_seen = time(NULL);
            shm_local->player_count++;
            idx = i;
            break;
        }
    }

    // Log unlock
    shm_local->mutex_status = 0; shm_local->mutex_owner = 0;
    push_mutex_event(shm_local, getpid(), 0);
    sem_post(&shm_local->mutex);
    return idx;
}

void deregister_player(SharedResource *shm_local, int slot) {
    if (slot < 0) return;
    sem_wait(&shm_local->mutex);
    shm_local->mutex_status = 1; shm_local->mutex_owner = getpid();
    push_mutex_event(shm_local, shm_local->mutex_owner, 1);
    shm_local->players[slot].status = 0;
    shm_local->players[slot].pid = 0;
    shm_local->players[slot].color_id = 0;
    shm_local->players[slot].last_seen = 0;
    if (shm_local->player_count > 0) shm_local->player_count--;
    shm_local->mutex_status = 0; shm_local->mutex_owner = 0;
    push_mutex_event(shm_local, getpid(), 0);
    sem_post(&shm_local->mutex);
}

// signal handler to try cleanup on graceful termination
void handle_sig(int sig) {
    running = 0;
}

void lancer_bot(int player_id, int bet_price) {
    srand(time(NULL) ^ (getpid()<<16));
    
    // Attachement SHM
    int shmid = shmget(SHM_KEY, sizeof(SharedResource), 0666);
    if (shmid < 0) exit(1);
    SharedResource *shm = (SharedResource *)shmat(shmid, NULL, 0);

    // Signaux
    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);

    // Enregistrement
    int reg_slot = register_player(shm, player_id);
    int bet_placed = 0;

    // Boucle principale
    while (running) {
        // Heartbeat (battement de coeur pour le GUI)
        if (reg_slot >= 0) {
            sem_wait(&shm->mutex);
            // On évite les logs trop fréquents pour le heartbeat sinon ça flood l'historique
            shm->players[reg_slot].last_seen = time(NULL);
            sem_post(&shm->mutex);
        }

        // Logique de pari
        if (shm->state == BETS_OPEN && !bet_placed) {
            usleep((rand() % 2300001) + 150000);
            
            // Section Critique : Placer le pari
            sem_wait(&shm->mutex);
            shm->mutex_status = 1; shm->mutex_owner = getpid();
            usleep(200000);
            push_mutex_event(shm, shm->mutex_owner, 1);

            if (shm->total_bets < MAX_BETS && shm->state == BETS_OPEN && shm->bank >= bet_price) 
            {
                shm->bank -= bet_price;
                shm->total_gains -= bet_price;
                Bet m;
                
                create_random_bet(&m, player_id, bet_price); // Mode jeu
                
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
        
        usleep(100000); // 0.1s de pause processeur
    }

    // --- NETTOYAGE PROPRE ---
    if (reg_slot >= 0) deregister_player(shm, reg_slot);
    shmdt(shm);
    exit(0);
}

int main(int argc, char *argv[]) {
    int bots_to_launch = DEFAULT_BOTS;
    int bet_price = DEFAULT_BET_PRICE;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bots") == 0 && i+1 < argc) {
            bots_to_launch = atoi(argv[i+1]); i++;
        }
        else if (strcmp(argv[i], "--bet-price") == 0 && i+1 < argc) {
            bet_price = atoi(argv[i+1]); i++; // On récupère le prix
        }
    }

    printf("[Server] %d joueurs ont rejoint la partie.\n", bots_to_launch);

    for (int i = 0; i < bots_to_launch; i++) {
        if (fork() == 0) { 
            // On passe l'ID (i) pour la couleur
            lancer_bot(i % 16, bet_price); 
            exit(0); 
        }
    }
    
    // Le père attend indéfiniment
    while(wait(NULL) > 0);
    return 0;
}
