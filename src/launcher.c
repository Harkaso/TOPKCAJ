// launcher.c
#include "shared.h"
#include <sys/wait.h>

pid_t pid_server, pid_gui, pid_bots;

void kill_all(int sig) {
    printf("\n[Launcher] Nettoyage de tout les modules.\n");
    
    if (pid_gui > 0) kill(pid_gui, SIGKILL);
    if (pid_bots > 0) kill(pid_bots, SIGKILL);
    
    if (pid_server > 0) {
        kill(pid_server, SIGINT);
        waitpid(pid_server, NULL, 0);
    }
    
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, kill_all);
    signal(SIGTERM, kill_all);

    printf("========================================\n");
    printf("     ETOPKCAJ - ROULETTE AMERICAINE     \n");
    printf("========================================\n");
    
    if ((pid_gui = fork()) == 0) {
        //freopen("/dev/null", "w", stderr);

        // --- GESTION DES ARGUMENTS ---
        
        // CAS 1 : Mode Debug (./launcher --debug)
        if (argc >= 2 && strcmp(argv[1], "--debug") == 0) {
            printf("[Launcher] Mode DEBUG activé.\n");
            execl("./dependencies/app", "app", "--debug", NULL);
        }
        
        // CAS 2 : Mode Bots (./launcher --bots 12)
        else if (argc >= 3 && strcmp(argv[1], "--bots") == 0) {
            if (atoi(argv[2]) <= 0) {
                printf("ERREUR: L'argument dois etre un nombre strictement positif.");
                exit(2);
            }
            else if (atoi(argv[2]) > MAX_BOTS || atoi(argv[2]) < MIN_BOTS) {
                printf("ERREUR: Le nombre de joueurs dois etre compris entre 4 et 16.");
                exit(2);
            }
            printf("[Launcher] Mode %s BOTS activé.\n", argv[2]);
            // On passe "--bots" ET le nombre (argv[2]) à l'app graphique
            execl("./dependencies/app", "app", "--bots", argv[2], NULL);
        }
        
        // CAS 3 : Lancement Normal (./launcher)
        else {
            printf("[Launcher] Mode STANDARD activé.\n");
            execl("./dependencies/app", "app", NULL);
        }

        // Si on arrive ici, c'est que execl a échoué (fichier introuvable ?)
        perror("ERREUR CRITIQUE : Impossible de lancer ./dependencies/app");
        exit(3);
    }

    // Le launcher attend que l'interface graphique (GUI) se ferme
    int status;
    waitpid(pid_gui, &status, 0);

    kill_all(0);
    return 0;
}
