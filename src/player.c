// player.c
#include "casino.h"
#include <sys/wait.h>
#include <string.h>

void create_random_bet(Bet *m, int player_id) {
    m->pid = getpid();
    m->color_id = player_id;
    m->amount = BET_PRICE;
    
    int roll = rand() % 100;
    if (roll < 65) {
        // --- MISES EXTERIEURS ---
        m->type = BET_RED + (rand() % 12); 
        m->count = 0;
    } 
    // --- B. MISES INTERIEURES (50%) ---
    else {
        // On sépare la grille standard (1-36) de la zone des Zéros
        int roll = rand() % 10; 

        // ---------------------------------------------------------
        // ZONE 1 : LA GRILLE STANDARD (90% du temps)
        // ---------------------------------------------------------
        if (roll < 9) {
            int type_in = rand() % 5; // Single, Split, Street, Square, Sixain
            
            if (type_in == 0) { // SINGLE (1-36)
                m->type = BET_SINGLE; 
                m->numbers[0] = 1 + rand() % 36; 
                m->count = 1;
            } 
            else if (type_in == 1) { // SPLIT STANDARD
                m->type = BET_SPLIT; m->count = 2;
                int dir = rand() % 2;
                if (dir == 0) { // Horizontal
                    int base = 1 + rand() % 35;
                    if (base % 3 == 0) base--; 
                    m->numbers[0] = base; m->numbers[1] = base + 1;
                } else { // Vertical
                    int base = 1 + rand() % 33;
                    m->numbers[0] = base; m->numbers[1] = base + 3;
                }
            } 
            else if (type_in == 2) { // STREET (Ligne)
                int row = ((rand() % 36) / 3) * 3 + 1;
                m->type = BET_STREET; m->count = 3;
                for(int k=0;k<3;k++) m->numbers[k]=row+k;
            } 
            else if (type_in == 3) { // SQUARE (Carré)
                int base = 1 + rand() % 32;
                if (base % 3 == 0) base--; 
                m->type = BET_SQUARE; m->count = 4;
                m->numbers[0]=base; m->numbers[1]=base+1;
                m->numbers[2]=base+3; m->numbers[3]=base+4;
            } 
            else { // SIXAIN
                int row = ((rand() % 33) / 3) * 3 + 1;
                if(row>31) row=31;
                m->type = BET_DOUBLE_STREET; m->count = 6;
                for(int k=0;k<6;k++) m->numbers[k]=row+k;
            }
        }
        // ---------------------------------------------------------
        // ZONE 2 : LA ZONE VERTE / SPECIALE (10% du temps)
        // ---------------------------------------------------------
        else {
            int special_type = rand() % 5; // 5 types de paris spéciaux

            if (special_type == 0) { 
                // SINGLE ZERO (0 ou 00)
                m->type = BET_SINGLE; m->count = 1;
                m->numbers[0] = (rand()%2 == 0) ? 0 : 37;
            }
            else if (special_type == 1) { 
                // SPLIT VERTICAL (0-00)
                m->type = BET_SPLIT; m->count = 2;
                m->numbers[0]=0; m->numbers[1]=37;
            }
            else if (special_type == 2) { 
                // SPLIT FRONTIERE (0-1 ou 00-3)
                m->type = BET_SPLIT; m->count = 2;
                if (rand() % 2 == 0) { m->numbers[0]=0; m->numbers[1]=1; } // Cheval 0-1
                else { m->numbers[0]=37; m->numbers[1]=3; }               // Cheval 00-3
            }
            else if (special_type == 3) { 
                // TRIO (0-1-2 ou 00-2-3)
                m->type = BET_TRIO; m->count = 3;
                if (rand() % 2 == 0) { // Trio Gauche
                    m->numbers[0]=0; m->numbers[1]=1; m->numbers[2]=2; 
                } else { // Trio Droite
                    m->numbers[0]=37; m->numbers[1]=2; m->numbers[2]=3; // 00-2-3
                }
            }
            else { 
                // TOP LINE (Basket : 0-00-1-2-3)
                m->type = BET_TOP_LINE; m->count = 5;
                m->numbers[0]=0; m->numbers[1]=37; 
                m->numbers[2]=1; m->numbers[3]=2; m->numbers[4]=3;
            }
        }
    }
}

int dbg_phase = 0; 
int dbg_iter = 1;

void create_debug_bet(Bet *m) {
    m->pid = getpid();
    m->color_id = 7; // Couleur Jaune/Or pour bien le voir
    m->amount = BET_PRICE;

    printf("[DEBUG-BOT] Phase %d - Iter %d\n", dbg_phase, dbg_iter);

    // Phase 0 : Test de tous les numéros pleins (0, 1..36, 00)
    if (dbg_phase == 0) {
        m->type = BET_SINGLE; m->count = 1;
        if (dbg_iter == 0) m->numbers[0] = 0;
        else if (dbg_iter == 37) m->numbers[0] = 37; // 00
        else m->numbers[0] = dbg_iter;

        dbg_iter++;
        if (dbg_iter == 37) dbg_iter = 37; // Hack pour passer au 00
        else if (dbg_iter > 37) { dbg_phase++; dbg_iter = 1; } // Fin phase
        return;
    }

    // Phase 1 : Test Chevals Horizontaux (1-2, 2-3... PAS 3-4 !)
    if (dbg_phase == 1) {
        if (dbg_iter % 3 == 0) dbg_iter++; // Si on est sur la colonne 3, on saute (pas de split avec le suivant)
        
        m->type = BET_SPLIT; m->count = 2;
        m->numbers[0] = dbg_iter; 
        m->numbers[1] = dbg_iter + 1;

        dbg_iter++;
        if (dbg_iter > 35) { dbg_phase++; dbg_iter = 1; }
        return;
    }

    // Phase 2 : Test Carrés (1-2-4-5...)
    if (dbg_phase == 2) {
        if (dbg_iter % 3 == 0) dbg_iter++; // Bord droit impossible pour carré

        m->type = BET_SQUARE; m->count = 4;
        m->numbers[0] = dbg_iter;     m->numbers[1] = dbg_iter + 1;
        m->numbers[2] = dbg_iter + 3; m->numbers[3] = dbg_iter + 4;

        dbg_iter++;
        if (dbg_iter > 32) { dbg_phase++; dbg_iter = 0; } // Fin tests grilles
        return;
    }

    // Phase 3 : Test Mises Extérieures
    if (dbg_phase == 3) {
        m->type = BET_RED + dbg_iter; // On parcourt l'enum
        m->count = 0;
        
        dbg_iter++;
        if (dbg_iter > 11) { // Il y a 12 mises extérieures
            printf("--- FIN CYCLE DEBUG ---\n");
            dbg_phase = 0; dbg_iter = 1; // On recommence tout
        }
        return;
    }
}

void lancer_bot(int player_id) {
    srand(time(NULL) ^ (getpid()<<16));
    int shmid = shmget(SHM_KEY, sizeof(GameTable), 0666);
    if (shmid < 0) exit(1);
    GameTable *shm = (GameTable *)shmat(shmid, NULL, 0);

    int bet_placed = 0;

    while (1) {
        if (shm->state == BETS_OPEN && !bet_placed) {
            usleep((rand() % 4000) * 1000);
            
            sem_wait(&shm->mutex);
            
            if (shm->total_bets < MAX_BETS && 
                shm->state == BETS_OPEN && 
                shm->bank >= BET_PRICE) 
            {
                shm->bank -= BET_PRICE;
                Bet m;
                create_random_bet(&m, player_id);
                shm->bets[shm->total_bets] = m;
                shm->total_bets++;
                bet_placed = 1;
            }
            sem_post(&shm->mutex);
            
        }
        if (shm->bank < BET_PRICE && shm->state == BETS_OPEN) {
            // TODO: si bank vide.
        }

        if (shm->state == RESULTS) bet_placed = 0;
        usleep(100000);
    }
}

int main(int argc, char *argv[]) {
    int bots_to_launch = DEFAULT_BOTS;

    if (argc >= 3) {
        if (strcmp(argv[1], "--bots") == 0) {
            bots_to_launch = atoi(argv[2]);
        }
    }

    printf("[Server] %d joueurs ont rejoint la partie.\n", bots_to_launch);

    for (int i = 0; i < bots_to_launch; i++) {
        if (fork() == 0) { 
            // On passe l'ID (i) pour la couleur
            // Si on a plus de 8 bots, on boucle les couleurs avec modulo % 8
            lancer_bot(i % 16); 
            exit(0); 
        }
    }
    
    // Le père attend indéfiniment
    while(wait(NULL) > 0);
    return 0;
}
