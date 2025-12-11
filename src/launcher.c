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
        execl("./dependencies/server", "server", NULL);
        perror("Erreur: lancement du serveur impossible."); exit(1);
    }
    sleep(1); 

    if ((pid_bots = fork()) == 0) {
        /*
        if (num_bots > 1) {
            execl("./dependencies/players", "players", "--bots", num_bots, NULL);
        } 
        else {
            execl("./dependencies/players", "players", NULL);
        }
        */
        execl("./dependencies/players", "players", NULL);
        perror("Erreur: lancement des joueurs impossible."); exit(2);
    }

    if ((pid_gui = fork()) == 0) {
        freopen("/dev/null", "w", stderr);
        execl("./dependencies/app", "app", NULL);
        perror("Erreur: lancement lancement de l'interface graphique impossible."); exit(3);
    }

    int status;
    waitpid(pid_gui, &status, 0);

    kill_all(0);
    return 0;
}
