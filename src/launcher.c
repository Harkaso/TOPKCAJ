// launcher.c
#include "shared.h"
#include <sys/wait.h>

pid_t pid_gui;

void kill_all(int sig) {
    printf("[Launcher] Nettoyage de tout les modules...\n");
    
    if (pid_gui > 0) kill(pid_gui, SIGKILL);
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, kill_all);
    signal(SIGTERM, kill_all);

    printf("========================================\n");
    printf("     ETOPKCAJ - ROULETTE AMERICAINE     \n");
    printf("========================================\n");
    
    if ((pid_gui = fork()) == 0) {
        freopen("/dev/null", "w", stderr);

        char str_bots[10]; sprintf(str_bots, "%d", DEFAULT_BOTS);
        char str_bank[10]; sprintf(str_bank, "%d", DEFAULT_BANK);
        char str_price[10]; sprintf(str_price, "%d", DEFAULT_BET_PRICE);

        // --- GESTION DES ARGUMENTS ---
        
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--bots") == 0 && i+1 < argc) {
                strncpy(str_bots, argv[i+1], 9); i++;
            }
            else if (strcmp(argv[i], "--bank") == 0 && i+1 < argc) {
                strncpy(str_bank, argv[i+1], 9); i++; // On remplace le défaut par l'argument
            }
            else if (strcmp(argv[i], "--bet-price") == 0 && i+1 < argc) {
                strncpy(str_price, argv[i+1], 9); i++;
            }
        }
        
        printf("[Launcher] Config: %s Bots | Bank: %s$ | Bet: %s$\n", str_bots, str_bank, str_price);

        execl("./dependencies/app", "app", 
              "--bots", str_bots, 
              "--bank", str_bank, 
              "--bet-price", str_price, 
              NULL);
    }

    int status;
    waitpid(pid_gui, &status, 0);

    kill_all(0);
    return 0;
}
