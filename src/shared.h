// casino.h
#ifndef CASINO_H
#define CASINO_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define SHM_KEY 0x777
#define DEFAULT_BOTS 8
#define MIN_BOTS 4
#define MAX_BOTS 20
#define START_BANK 2000
#define BET_PRICE 20
#define MAX_BETS 50


enum GameState { BETS_OPEN = 0, BETS_CLOSED = 1, RESULTS = 2 };

enum BetType {
    // Inside Bets
    BET_SINGLE = 0, BET_SPLIT, BET_STREET, BET_SQUARE, BET_DOUBLE_STREET, BET_TRIO, BET_TOP_LINE,
    // Outside Bets
    BET_RED, BET_BLACK, BET_EVEN, BET_ODD, BET_LOW, BET_HIGH,
    BET_DOZEN_1, BET_DOZEN_2, BET_DOZEN_3,
    BET_COL_1, BET_COL_2, BET_COL_3
};

typedef struct {
    pid_t pid;
    int type;
    int numbers[6];
    int count;
    int amount;
    int color_id;
} Bet;

typedef struct {
    sem_t mutex;
    int state;
    int winning_number;
    Bet bets[MAX_BETS];
    int total_bets;
    int bank;
} GameTable;

#endif
