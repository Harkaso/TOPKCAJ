// launcher.c
#include "shared.h"
#include <sys/wait.h>
#include <string.h>

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
    
    /*
    int num_bots = -99;

    if (argc >= 3 && strcmp(argv[1], "--bots") == 0) {
        num_bots = atoi(argv[2]);
        if (num_bots <= 0) {
            perror("Erreur: Le nombre de joueurs doit etre une nombre strictement positif.");
            exit(2);            
        }
        if (num_bots < 4 && num_bots > 20) {
            perror("Erreur: Le nombre de joueurs doit etre compris entre 4 et 20.");
            exit(2);
        }
    }
    */

    if ((pid_server = fork()) == 0) {
        // Do not start server here. GUI will start server/players when user clicks "Play".
        exit(0);
    }
    sleep(1); 
    // Start only the GUI. The GUI will launch server and players when the user starts the game.
    if ((pid_gui = fork()) == 0) {
        freopen("/dev/null", "w", stderr);
        execl("./dependencies/app", "app", NULL);
        perror("Erreur: lancement de l'interface graphique impossible."); exit(3);
    }

    int status;
    waitpid(pid_gui, &status, 0);

    kill_all(0);
    return 0;
}
