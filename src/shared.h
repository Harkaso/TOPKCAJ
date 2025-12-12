// shared.h
#ifndef SHARED_H
#define SHARED_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#define SHM_KEY 0x777
#define DEFAULT_BOTS 8
#define MIN_BOTS 4
#define MAX_BOTS 16
#define DEFAULT_BANK 2000
#define DEFAULT_BET_PRICE 25
#define MAX_BETS 50
#define MUTEX_EVENT_HISTORY 32
enum GameState { BETS_OPEN = 0, BETS_CLOSED = 1, RESULTS = 2 };

enum BetType {
    // Inside Bets
    BET_SINGLE = 0, BET_SPLIT, BET_STREET, BET_SQUARE, BET_DOUBLE_STREET,
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
    pid_t pid;
    int status;       // 0 = not registered / dead, 1 = alive
    int color_id;    // color slot used by the bot
    time_t last_seen; // last heartbeat timestamp
} PlayerInfo;
// mutex event history size


typedef struct {
    time_t ts;
    pid_t pid;
    int status; // 1 = locked, 0 = unlocked
} MutexEvent;

typedef struct {
    sem_t mutex;
    int state;
    int winning_number;
    Bet bets[MAX_BETS];
    int total_bets;
    int bank;
    int total_gains;
    PlayerInfo players[MAX_BOTS];
    int player_count;
    int mutex_status;    // 0 = unlocked, 1 = locked
    pid_t mutex_owner;   // PID of process that holds the mutex (0 = none)
    // History of mutex events (lock/unlock) - circular buffer
    MutexEvent mutex_events[MUTEX_EVENT_HISTORY];
    int mutex_events_head; // next write index
    int mutex_events_count;
} SharedResource;

#endif