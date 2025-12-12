/**
 * @file launcher.c
 * @brief Point d'entrée unique et lanceur de l'application.
 *
 * Ce programme sert de "wrapper". Son rôle est de :
 * 1. Accueillir l'utilisateur.
 * 2. Parser les arguments de la ligne de commande (--bots, --bank...).
 * 3. Lancer l'interface graphique.
 * 4. Assurer que tout se ferme si l'utilisateur fait Ctrl+C.
 */
#include "shared.h"
#include <sys/wait.h>

/** 
 * @brief PID de l'interface graphique pour le
*/
pid_t pid_gui;

/**
 * @brief Gestionnaire d'arrêt d'urgence et de nettoyage.
 * 
 * Appelé lors d'un signal (SIGINT/SIGTERM) ou à la fin normale du programme.
 * 
 * @param sig Numéro du signal reçu (ou 0 pour un appel manuel).
 */
void kill_all(int sig) {
    printf("[Launcher] Nettoyage de tout les modules...\n");
    if (pid_gui > 0) kill(pid_gui, SIGKILL);
    exit(0);
}

/**
 * @brief Point d'entrée du Lanceur.
 * 
 * Analyse les arguments passés au programmeet les transmet
 * à l'exécutable de l'application graphique via `execl`.
 * Le launcher reste ensuite en attente jusqu'à la fermeture 
 * ou l'arrêt du programme.
 */
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

        // Parsing des arguments
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--bots") == 0 && i+1 < argc) {
                strncpy(str_bots, argv[i+1], 9); i++;
            }
            else if (strcmp(argv[i], "--bank") == 0 && i+1 < argc) {
                strncpy(str_bank, argv[i+1], 9); i++;
            }
            else if (strcmp(argv[i], "--bet-price") == 0 && i+1 < argc) {
                strncpy(str_price, argv[i+1], 9); i++;
            }
        }
        
        printf("[Launcher] Config: %s Bots | Bank: %s$ | Bet: %s$\n", str_bots, str_bank, str_price);

        // Lancement de l'application
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
