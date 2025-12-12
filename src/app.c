/**
 * @file app.c
 * @brief Interface Graphique (Raylib) et Lanceur Principal.
 *
 * Ce module remplit deux rôles majeurs :
 * 1. **Frontend :** Affiche la table, la roue, les jetons et le tableau de bord en temps réel
 *    en lisant l'état de la mémoire partagée.
 * 2. **Launcher :** Fork le processus Serveur et les processus Bots au démarrage,
 *    et assure leur nettoyage à la fermeture.
 */

#include "shared.h"
#include "raylib.h"
#include <math.h>
#include <sys/wait.h>

/**
 * @defgroup Calibration Paramètres de Calibrage Graphique
 * @brief Constantes d'affichages.
 */
/**
 * @brief Taille et découpage de l'écran.
 * @{
 */
const int SCREEN_W = 1900;
const int SCREEN_H = 900;
const int PANEL_W = 300;
/** @} */

/**
 * @brief Repères de la table de jeu.
 * @{
 */
const int GRID_ORIGIN_X = 601;
const int GRID_ORIGIN_Y = 169;

const int CELL_W = 67;
const int CELL_H = 109;
const float CELL_GAP = 4.5f;

const int OFFSET_Y_DOZENS = 25; 
const int HEIGHT_DOZENS = 50;

const int OFFSET_Y_CHANCES = 25;
const int HEIGHT_CHANCES = 90;
/** @} */

/**
 * @brief Position et échelle de la roue.
 * @{
 */
const int WHEEL_POS_X = 250;
const int WHEEL_POS_Y = 365;
const float WHEEL_SCALE = 0.45f;
/** @} */

/**
 * @brief Propriétés de la bille de jeu.
 * @{
 */
const float BALL_SIZE = 5.5f;
const float BALL_RADIUS_OUTER = 155.0f;
const float BALL_RADIUS_INNER = 105.0f;
float global_ball_angle = 0.0f;
/** @} */

/**
 * @brief Palette de couleurs pour identifier les joueurs.
 * 
 * Correspond au champ `color_id` dans les structures.
 */
Color bot_tints[] = { 
    DARKPURPLE,
    DARKGREEN,
    ORANGE,
    SKYBLUE,
    RED,
    DARKBLUE,
    VIOLET,
    YELLOW,
    MAGENTA,
    GOLD,
    BLUE,
    GREEN,
    PINK,
    BEIGE,
    MAROON,
    WHITE,
};

/**
 * @brief Ordre officiel des numéros sur une Roulette Américaine.
 * 
 * Nécessaire pour repèrer les positions de la roue.
 */
int WHEEL_ORDER[] = {0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1, 37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2};

/**
 * @brief Textures globales.
 * 
 * Conteneur des images et textures pour utilisation.
 */
Texture2D tTable;
Texture2D tWheelStatic;
Texture2D tWheelSpin;
Texture2D tChip;
Texture2D tMain;
Texture2D tPanel;

/**
 * @brief Vérifie si un numéro de roulette est Rouge.
 * 
 * Utilise un tableau codé en dur correspondant à la disposition
 * standard de la roulette.
 * 
 * @param n Le numéro à vérifier.
 * @return int 1 si le numéro est ROUGE, 0 s'il est NOIR ou VERT.
 */
int is_red_num(int n) {
    if (n==0 || n==37) return 0;
    int reds[] = {1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
    for(int i=0;i<18;i++) if(reds[i]==n) return 1;
    return 0;
}

/**
 * @brief Calcule les coordonnées pixels (X,Y) du centre d'une case numéro.
 * 
 * Transforme un numéro logique en coordonnées basées
 *  sur la grille calibrée (`GRID_ORIGIN`).
 * 
 * @param n Numéro cible.
 * @return Vector2 Position (x, y).
 */
Vector2 get_num_pos(int n) {
    if(n == 0)  return (Vector2){GRID_ORIGIN_X - (CELL_W / 1.75f) - (CELL_GAP * 0.5f) - 0.5f, 
        GRID_ORIGIN_Y + (CELL_H * 2.25f) + (CELL_GAP * 0.5f) + 0.5f};
    if(n == 37) return (Vector2){GRID_ORIGIN_X - (CELL_W / 1.75f) - (CELL_GAP * 0.5f) - 0.5f, 
        GRID_ORIGIN_Y + (CELL_H * 0.75f) + (CELL_GAP * 0.5f) + 0.5f};

    int col = (n - 1) / 3;
 
    int row = 0;
    if (n % 3 == 0) row = 0;
    else if (n % 3 == 2) row = 1;
    else row = 2;     


    float x = GRID_ORIGIN_X + col * (CELL_W + CELL_GAP) + (CELL_W / 2.0f);
    float y = GRID_ORIGIN_Y + row * (CELL_H + CELL_GAP) + (CELL_H / 2.0f);
    
    return (Vector2){x, y};
}

/**
 * @brief Calcule la position d'affichage d'un jeton sur la table.
 * 
 * Logique complexe qui détermine où dessiner le jeton en fonction du type de pari :
 * Ajoute un léger "jitter" (décalage aléatoire) basé sur le PID pour éviter
 * l'empilement parfait des jetons.
 * 
 * @param m Structure du pari.
 * @return Vector2 Position du jeton.
 */
Vector2 get_bet_pos(Bet m) {
    if(m.type <= BET_DOUBLE_STREET) {
        if(m.count == 0) return (Vector2){0,0};
        
        float sx = 0, sy = 0;
        for(int i=0; i<m.count; i++) { 
            Vector2 p = get_num_pos(m.numbers[i]); 
            sx += p.x; 
            sy += p.y; 
        }
        
        Vector2 res = { sx / m.count, sy / m.count };

        if(m.type == BET_STREET || m.type == BET_DOUBLE_STREET) {
            res.y = GRID_ORIGIN_Y;
        }

        // Jitter
        res.x += (m.pid % 8) - 4;
        res.y += (m.pid % 8) - 4;
        return res;
    }

    float y_bottom_grid = get_num_pos(1).y + CELL_H/2.0f;
    
    float y_dozens = y_bottom_grid + OFFSET_Y_DOZENS + HEIGHT_DOZENS/2.0f;
    
    float y_chances = y_bottom_grid + OFFSET_Y_DOZENS + HEIGHT_DOZENS + OFFSET_Y_CHANCES + HEIGHT_CHANCES/2.0f;

    Vector2 p = {0,0};

    float x_col = get_num_pos(36).x + CELL_W/2.0f + CELL_GAP + CELL_W/2.0f;
    
    if(m.type == BET_COL_3)      p = (Vector2){x_col, get_num_pos(36).y};
    else if(m.type == BET_COL_2) p = (Vector2){x_col, get_num_pos(35).y};
    else if(m.type == BET_COL_1) p = (Vector2){x_col, get_num_pos(34).y};

    else if(m.type == BET_DOZEN_1) p = (Vector2){ (get_num_pos(1).x + get_num_pos(12).x)/2.0f, y_dozens };
    else if(m.type == BET_DOZEN_2) p = (Vector2){ (get_num_pos(13).x + get_num_pos(24).x)/2.0f, y_dozens };
    else if(m.type == BET_DOZEN_3) p = (Vector2){ (get_num_pos(25).x + get_num_pos(36).x)/2.0f, y_dozens };

    else if(m.type == BET_LOW)   p = (Vector2){ (get_num_pos(1).x + get_num_pos(6).x)/2.0f, y_chances };
    else if(m.type == BET_EVEN)  p = (Vector2){ (get_num_pos(7).x + get_num_pos(12).x)/2.0f, y_chances };
    else if(m.type == BET_RED)   p = (Vector2){ (get_num_pos(13).x + get_num_pos(18).x)/2.0f, y_chances };
    else if(m.type == BET_BLACK) p = (Vector2){ (get_num_pos(19).x + get_num_pos(24).x)/2.0f, y_chances };
    else if(m.type == BET_ODD)   p = (Vector2){ (get_num_pos(25).x + get_num_pos(30).x)/2.0f, y_chances };
    else if(m.type == BET_HIGH)  p = (Vector2){ (get_num_pos(31).x + get_num_pos(36).x)/2.0f, y_chances };

    // Jitter
    p.x += (m.pid % 8) - 4;
    p.y += (m.pid % 8) - 4;
    return p;
}

/**
 * @brief Calcule l'angle de rotation d'un numéro de la roue.
 * 
 * Donne la position du numéro gagnant en calculant son angle
 * pour donner l'endroit exacte ou dois tomber la bille
 * 
 * @param number Numéro gagnant.
 * @return float Angle en degrés.
 */
float get_slot_angle_offset(int number) {
    int index = -1;
    for(int i=0; i<38; i++) {
        if (WHEEL_ORDER[i] == number) {
            index = i;
            break;
        }
    }
    
    if (index == -1) return 0.0f;

    float angle_per_slot = 360.0f / 38.0f;
    
    return (index * angle_per_slot + 90);
}

/**
 * @brief Dessine les éléments statiques et animés du jeu (Table, Roue, Bille).
 * 
 * Gère la machine à états visuelle :
 * - BETS_OPEN : La roue et la bille tournent vite.
 * - BETS_CLOSE: La roue et la bille ralentissent et tournent doucement.
 * - RESULTS : La bille tombe sur le numéro gagnant.
 * 
 * @param wheel_rotation Angle actuel de la roue.
 * @param win_num Numéro gagnant.
 * @param state État du jeu.
 */
void DrawAssets(float wheel_rotation, int win_num, int state) {
    // Dessin de la table
    DrawTexturePro(tTable, 
        (Rectangle){0, 0, tTable.width, tTable.height}, 
        // Reserve right panel for realtime status
        (Rectangle){0, 0, SCREEN_W - PANEL_W, SCREEN_H}, 
        (Vector2){0,0}, 0.0f, WHITE);

    // Dessin de la roue
    
    // Roue intérieur
    float inner_scale = WHEEL_SCALE * 0.315f;
    Rectangle sourceSpin = {0, 0, tWheelSpin.width, tWheelSpin.height};
    Rectangle destSpin = {WHEEL_POS_X, WHEEL_POS_Y, tWheelSpin.width * inner_scale, tWheelSpin.height * inner_scale};
    Vector2 originSpin = {destSpin.width/2, destSpin.height/2}; 
    DrawTexturePro(tWheelSpin, sourceSpin, destSpin, originSpin, wheel_rotation, WHITE);

    // Roue extérieur
    Rectangle sourceStatic = {0, 0, tWheelStatic.width, tWheelStatic.height};
    Rectangle destStatic = {WHEEL_POS_X, WHEEL_POS_Y, tWheelStatic.width * WHEEL_SCALE, tWheelStatic.height * WHEEL_SCALE};
    Vector2 originStatic = {destStatic.width/2, destStatic.height/2};
    DrawTexturePro(tWheelStatic, sourceStatic, destStatic, originStatic, 0.0f, WHITE);

    // Dessin de la bille
        if (state != 99) {         
        float current_radius = 0.0f;

        if (state == RESULTS) {
            float num_offset = get_slot_angle_offset(win_num);
            
            global_ball_angle = wheel_rotation + num_offset;
            current_radius = BALL_RADIUS_INNER; 
        } else {
            current_radius = BALL_RADIUS_OUTER;
            float speed = 0.0f;
            
            if (state == BETS_OPEN) {
                speed = -6.5f;
            } else {
                speed = -2.5f;
            }
            
            global_ball_angle += speed; 
        }
        

        float bx = WHEEL_POS_X + cosf(global_ball_angle * DEG2RAD) * current_radius;
        float by = WHEEL_POS_Y + sinf(global_ball_angle * DEG2RAD) * current_radius;
        
        
        DrawCircle(bx + 2, by + 2, BALL_SIZE - 1.0f, (Color){0,0,0,100}); 
        DrawCircle(bx, by, BALL_SIZE, WHITE); 
        DrawCircleGradient(bx, by, BALL_SIZE, WHITE, GRAY);
    }
}

/**
 * @brief Dessine tous les jetons des mises sur ce tour.
 * 
 * Parcourt les mises faites et affiche un jeton pour chaque pari,
 * teinté avec la couleur du joueur.
 * 
 * @param shm Pointeur vers la ressource partagée.
 */
void DrawChips(SharedResource *shm) {
    for (int i = 0; i < shm->total_bets; i++) {
        Vector2 pos = get_bet_pos(shm->bets[i]);
        
        if (pos.x > 10) {
            float scale = 0.052f; 
            
            DrawTexturePro(tChip, 
                (Rectangle){0,0,tChip.width,tChip.height}, 
                (Rectangle){pos.x+3, pos.y+3, tChip.width*scale, tChip.height*scale}, 
                (Vector2){(tChip.width*scale)/2, (tChip.height*scale)/2}, 
                0.0f, (Color){0,0,0,100});

            Color tint = bot_tints[shm->bets[i].color_id];
            DrawTexturePro(tChip, 
                (Rectangle){0,0,tChip.width,tChip.height}, 
                (Rectangle){pos.x, pos.y, tChip.width*scale, tChip.height*scale}, 
                (Vector2){(tChip.width*scale)/2, (tChip.height*scale)/2}, 
                0.0f, tint);
                
            DrawText(TextFormat("%d", shm->bets[i].color_id+1), pos.x-4, pos.y-6, 10, BLACK);
        }
    }
}

// Variables globales de gestion de processusint 
shmid = -1;
SharedResource *shm = NULL;
pid_t pid_server = 0;
pid_t pid_bots = 0;

/**
 * @brief Gestionnaire de nettoyage et de fermeture.
 * 
 * Appelé sur SIGINT ou fermeture fenêtre.
 * 1. Envoie SIGTERM puis SIGKILL aux groupes de processus enfants (Serveur + Bots).
 * 2. Attend la fin des processus (waitpid) pour éviter les zombies.
 * 3. Détache la mémoire partagée.
 * 4. Ferme la fenêtre Raylib.
 * 
 * @param sig Signal reçu (ou 0 si appel manuel).
 */
void gui_cleanup_int(int sig) {
    if (pid_bots > 0) {
        kill(-pid_bots, SIGTERM);
        for (int i = 0; i < 20; i++) {
            if (kill(pid_bots, 0) == -1) break;
            usleep(100000);
        }
        kill(-pid_bots, SIGKILL);
        waitpid(pid_bots, NULL, 0);
        pid_bots = 0;
    }
    if (pid_server > 0) {
        kill(-pid_server, SIGINT);
        for (int i = 0; i < 20; i++) {
            if (kill(pid_server, 0) == -1) break;
            usleep(100000);
        }
        kill(-pid_server, SIGKILL);
        waitpid(pid_server, NULL, 0);
        pid_server = 0;
    }
    if (shm != NULL) {
        shmdt(shm);
        shm = NULL;
    }
    CloseWindow();
    _exit(0);
}

/** 
 * @brief Wrapper pour atexit(). 
 */
void gui_cleanup_atexit(void) { gui_cleanup_int(0); }

/**
 * @brief Point d'entrée de l'application graphique.
 * 
 * Cycle de vie :
 * 1. Initialisation de la fenêtre et des assets.
 * 2. Boucle de Menu : Attend que l'utilisateur clique sur "JOUER".
 * 3. Lancement du Serveur et des Bots.
 * 4. Attachement à la SHM créée par le serveur.
 * 5. Boucle de Jeu :
 *    - Lecture non-bloquante de la SHM.
 *    - Calcul des animations.
 *    - Dessin de la scène.
 *    - Dessin du Panneau Latéral en temps réel.
 * 6. Nettoyage final.
 */
int main(int argc, char *argv[]) {

    // Gestion des signaux
    signal(SIGINT, gui_cleanup_int);
    signal(SIGTERM, gui_cleanup_int);
    signal(SIGHUP, gui_cleanup_int);
    atexit(gui_cleanup_atexit);

    char str_bots[10]; sprintf(str_bots, "%d", DEFAULT_BOTS);
    char str_bank[10]; sprintf(str_bank, "%d", DEFAULT_BANK);
    char str_price[10]; sprintf(str_price, "%d", DEFAULT_BET_PRICE);
    int start_bank = DEFAULT_BANK;

    // Parsing des arguments
    for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--bots") == 0 && i+1 < argc) {
                strncpy(str_bots, argv[i+1], 9); i++;
            }
            else if (strcmp(argv[i], "--bank") == 0 && i+1 < argc) {
                strncpy(str_bank, argv[i+1], 9); 
                start_bank = atoi(argv[i+1]);
                i++;
            }
            else if (strcmp(argv[i], "--bet-price") == 0 && i+1 < argc) {
                strncpy(str_price, argv[i+1], 9); i++;
            }
        }

    // Initialisation de la fenêtre
    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_W, SCREEN_H, "ETOPKCEJ - Roulette Americaine");
    SetTargetFPS(60);

    // Chargement des textures
    tTable = LoadTexture("./src/assets/table.png");
    tWheelStatic = LoadTexture("./src/assets/cadre.png");
    tWheelSpin = LoadTexture("./src/assets/roue.png");
    tChip = LoadTexture("./src/assets/jeton.png");
    tMain = LoadTexture("./src/assets/Main.png");
    
    SetTextureFilter(tTable, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tWheelStatic, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tWheelSpin, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tChip, TEXTURE_FILTER_BILINEAR);

    // Chargement des audios
    InitAudioDevice();
    bool audio_ready = IsAudioDeviceReady();
    Music ambient = {0};
    Sound s_win = {0};
    Sound s_empty = {0};
    bool ambient_playing = false;
    if (audio_ready) {
        ambient = LoadMusicStream("./src/assets/ambient.mp3");
        s_win = LoadSound("./src/assets/win.mp3");
        s_empty = LoadSound("./src/assets/empty.mp3");
        PlayMusicStream(ambient);
        ambient_playing = true;
    }

    float rotation = 0.0f;
    bool in_menu = true;
    int prev_state = -1;
    int prev_bank = -1;
    bool show_lost_panel = false;
    tPanel = LoadTexture("./src/assets/panel_clean_640x240.png");
    SetTextureFilter(tPanel, TEXTURE_FILTER_BILINEAR);

    // Boucle principale 
    while (!WindowShouldClose()) {
        if (!in_menu) {
            if (shm->state != RESULTS) {
                rotation += 1.0f;
            } else {
                rotation += 0.3f;
            }
            if(rotation > 360) rotation -= 360;
        }

        if (shm != NULL && prev_state == -1) {
            prev_state = shm->state;
            prev_bank = shm->bank;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        if (in_menu) {
            DrawTexturePro(tMain,
                (Rectangle){0, 0, tMain.width, tMain.height},
                (Rectangle){0, 0, SCREEN_W, SCREEN_H},
                (Vector2){0,0}, 0.0f, WHITE);

            Vector2 mp = GetMousePosition();
            Rectangle btn = {1000, 400, 440, 140};
            bool hover = (mp.x >= btn.x && mp.x <= btn.x + btn.width && mp.y >= btn.y && mp.y <= btn.y + btn.height);
            if (hover) {
                DrawRectangleLinesEx(btn, 2, (Color){255,255,255,80});
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hover) {
                // Lancement du serveur
                if (pid_server == 0) {
                    pid_server = fork();
                    if (pid_server == 0) {
                        setpgid(0,0);
                        execl("./dependencies/server", "./dependencies/server", 
                              "--bank", str_bank, 
                              "--bet-price", str_price, 
                              NULL);
                        _exit(0);
                    }
                }

                // Lancement des joueurs
                if (pid_bots == 0) {
                    pid_bots = fork();
                    if (pid_bots == 0) {
                        setpgid(0,0);
                            
                        execl("./dependencies/players", "players", 
                              "--bots", str_bots, 
                              "--bet-price", str_price, 
                              NULL);
                        _exit(0);
                        }
                }
                
                // Attachement a la mémoire partager et lancement du jeu
                int tries = 0;
                while (tries < 30) {
                    shmid = shmget(SHM_KEY, sizeof(SharedResource), 0666);
                    if (shmid != -1) break;
                    tries++;
                    sleep(1);
                }
                if (shmid == -1) {
                    if (pid_bots > 0) { kill(pid_bots, SIGKILL); waitpid(pid_bots, NULL, 0); pid_bots = 0; }
                    if (pid_server > 0) { kill(pid_server, SIGINT); waitpid(pid_server, NULL, 0); pid_server = 0; }
                    TraceLog(LOG_WARNING, "Server did not create shared memory — aborting start");
                } else {
                    shm = (SharedResource*)shmat(shmid, NULL, 0);
                    if (shm == (void*)-1) {
                        shm = NULL;
                        TraceLog(LOG_WARNING, "Failed to attach shared memory in GUI");
                    } else {
                        in_menu = false;
                    }
                }
            }
        } else {
            // Dessiner le décor
            DrawAssets(rotation, shm->winning_number, shm->state);

            // Dessiner les jetons
            DrawChips(shm);
        }

        if (!in_menu) {
            if (shm->state == RESULTS) {
                int win = shm->winning_number;
                
                Color resColor = DARKGREEN;
                if (is_red_num(win)) resColor = RED;
                else if (win != 0 && win != 37) resColor = BLACK;

                int boxW = 100;
                int boxH = 90;
                int boxX = WHEEL_POS_X - boxW/2; 
                int boxY = WHEEL_POS_Y + 260; 

                DrawRectangle(boxX, boxY, boxW, boxH, resColor);
                DrawRectangleLinesEx((Rectangle){boxX, boxY, boxW, boxH}, 3, GOLD);
                
                DrawRectangleLinesEx((Rectangle){boxX+4, boxY+4, boxW, boxH}, 3, (Color){0,0,0,50});

                const char* txtNum = (win == 37) ? "00" : TextFormat("%d", win);
                int txtSize = 50;
                int txtW = MeasureText(txtNum, txtSize);
                
                DrawText(txtNum, boxX + (boxW/2) - (txtW/2), boxY + (boxH/2) - (txtSize/2), txtSize, WHITE);
            }

            // Panneau d'informations
            int panel_x = SCREEN_W - PANEL_W;
            time_t now = time(NULL);
            
            DrawRectangle(panel_x, 0, PANEL_W, SCREEN_H, (Color){15, 15, 20, 245});
            DrawLine(panel_x, 0, panel_x, SCREEN_H, (Color){255, 215, 0, 100});
            
            DrawText("ETOPKCAJ", panel_x + 20, 15, 24, GOLD);
            DrawRectangle(panel_x + 20, 45, PANEL_W - 40, 2, (Color){255, 215, 0, 50});

            int y_stats = 60;
            DrawRectangle(panel_x + 10, y_stats, PANEL_W - 20, 80, (Color){255, 255, 255, 10});
            DrawRectangleLines(panel_x + 10, y_stats, PANEL_W - 20, 80, (Color){255, 255, 255, 30});
            
            DrawText("BANQUE COMMUNE", panel_x + 20, y_stats + 10, 10, LIGHTGRAY);
            DrawText(TextFormat("%d$", shm->bank), panel_x + 20, y_stats + 25, 28, (shm->bank > 0) ? GOLD : RED);
            
            DrawText("NBR DE MISES", panel_x + 160, y_stats + 10, 10, LIGHTGRAY);
            DrawText(TextFormat("%d", shm->total_bets), panel_x + 160, y_stats + 25, 28, SKYBLUE);

            int y_state = 150;
            Color stateColor = GRAY;
            const char* stateText = "INCONNU";
            const char* extraText = ""; 
            Color extraColor = WHITE;

            if (shm->state == BETS_OPEN) { stateColor = LIME; stateText = "OUVERT - FAITES VOS JEUX"; }
            else if (shm->state == BETS_CLOSED) { stateColor = ORANGE; stateText = "FERME - RIEN NE VA PLUS"; }
            else if (shm->state == RESULTS) { 
                stateColor = RED;
                stateText = "RESULTATS & PAIEMENT"; 

                int net = shm->total_gains;
                if (net >= 0) {
                    extraColor = GREEN; 
                    extraText = TextFormat("(Gain: +%d$)", net);
                } else {
                    extraColor = LIGHTGRAY;
                    extraText = TextFormat("(Perte: %d$)", net);
                }
            }

            DrawRectangle(panel_x + 10, y_state, PANEL_W - 20, 30, Fade(stateColor, 0.2f));
            DrawRectangleLines(panel_x + 10, y_state, PANEL_W - 20, 30, stateColor);
            DrawText(stateText, panel_x + 20, y_state + 8, 12, stateColor);

            if (shm->state == RESULTS) {
                int widthTitre = MeasureText(stateText, 12);
                DrawText(extraText, panel_x + 20 + widthTitre + 15, y_state + 8, 12, extraColor);
            }

            // Status Mutex
            int y_tech = 205;
            DrawText("STATUS MUTEX", panel_x + 20, y_tech, 12, GRAY);
            
            Color ledColor = shm->mutex_status ? RED : GREEN;
            DrawCircle(panel_x + 25, y_tech + 25, 6, ledColor);
            DrawText(shm->mutex_status ? "VERROUILLE" : "LIBRE", panel_x + 40, y_tech + 18, 14, ledColor);
            
            if (shm->mutex_status) {
                DrawText(TextFormat("PAR PID: %d", shm->mutex_owner), panel_x + 140, y_tech + 18, 14, WHITE);
            }

            // Liste des joueurs
            int y_list = 265;
            DrawText(TextFormat("JOUEURS (%d)", shm->player_count), panel_x + 20, y_list, 14, GOLD);
            DrawRectangle(panel_x + 20, y_list + 18, PANEL_W - 40, 1, GRAY);
            
            int col_py = y_list + 25;
            DrawText("ID",   panel_x + 15, col_py, 10, DARKGRAY);
            DrawText("PID",  panel_x + 40, col_py, 10, DARKGRAY);
            DrawText("PARI", panel_x + 92, col_py, 10, DARKGRAY);
            DrawText("COUL.",   panel_x + 210, col_py, 10, DARKGRAY);
            DrawText("PING", panel_x + 250, col_py, 10, DARKGRAY);
            
            int row_y = col_py + 15;
            int log_h = 180;
            
            for (int i = 0; i < MAX_BOTS; i++) {
                if (shm->bets[i].pid == 0) continue; 
                if (row_y > SCREEN_H - log_h - 20) break; 

                int pid = shm->bets[i].pid;
                time_t last_action_ts = 0;

                for(int k=0; k < MUTEX_EVENT_HISTORY; k++) {
                    if (shm->mutex_events[k].pid == pid) {
                        if (shm->mutex_events[k].ts > last_action_ts) {
                            last_action_ts = shm->mutex_events[k].ts;
                        }
                    }
                }

                int ago = (last_action_ts > 0) ? (int)(now - last_action_ts) : -1;
                Color rowCol = WHITE; 
                
                // Gains
                if (shm->state == RESULTS) {
                    int win = shm->winning_number;
                    int won = 0;
                    
                    for(int b=0; b < shm->total_bets; b++) {
                        if (shm->bets[b].pid == pid) {
                            Bet m = shm->bets[b];
                            if (m.type <= BET_DOUBLE_STREET) { 
                                // Mises Intérieures
                                for(int k=0; k<m.count; k++) if(m.numbers[k] == win) won=1;
                            } else { 
                                // Mises Extérieures
                                int is_red = is_red_num(win);
                                int is_zero = (win == 0 || win == 37);
                                switch(m.type) {
                                    case BET_RED: if(is_red) won=1; break;
                                    case BET_BLACK: if(!is_red && !is_zero) won=1; break;
                                    case BET_EVEN: if(win%2==0 && !is_zero) won=1; break;
                                    case BET_ODD: if(win%2!=0 && !is_zero) won=1; break;
                                    case BET_LOW: if(win>=1 && win<=18) won=1; break;
                                    case BET_HIGH: if(win>=19 && win<=36) won=1; break;
                                    case BET_COL_1: if(win!=0 && win!=37 && win%3==1) won=1; break;
                                    case BET_COL_2: if(win!=0 && win!=37 && win%3==2) won=1; break;
                                    case BET_COL_3: if(win!=0 && win!=37 && win%3==0) won=1; break;
                                    case BET_DOZEN_1: if(win>=1 && win<=12) won=1; break;
                                    case BET_DOZEN_2: if(win>=13 && win<=24) won=1; break;
                                    case BET_DOZEN_3: if(win>=25 && win<=36) won=1; break;
                                }
                            }
                            break;
                        }
                    }
                    
                    if (won) rowCol = GREEN;
                    else rowCol = Fade(GRAY, 0.4f);
                }
                
                DrawText(TextFormat("%d", i+1), panel_x + 15, row_y, 12, rowCol);
                DrawText(TextFormat("%d", pid), panel_x + 40, row_y, 12, rowCol);
                
                const char* betStr = "-";
                Color betCol = GRAY;
                for(int b=0; b < shm->total_bets; b++) {
                    if (shm->bets[b].pid == pid) {
                        betCol = SKYBLUE;
                        Bet m = shm->bets[b];
                        switch(m.type) {
                            case BET_SINGLE: betStr = TextFormat("PLEIN %d", m.numbers[0]); if(m.numbers[0]==37) betStr="Plein 00"; break;
                            case BET_SPLIT:  
                                if(m.numbers[1]==37) betStr = "CHEVAL 0-00"; 
                                else betStr = TextFormat("CHEVAL %d-%d", m.numbers[0], m.numbers[1]); 
                                break;
                            case BET_STREET: betStr = TextFormat("TRANSVER. %d : %d", m.numbers[0], m.numbers[2]); break;
                            case BET_SQUARE: betStr = TextFormat("CARRE %d-%d-%d-%d", m.numbers[0], m.numbers[1], m.numbers[2], m.numbers[3]); break;
                            case BET_DOUBLE_STREET: betStr = TextFormat("SIXAIN %d : %d", m.numbers[0], m.numbers[5]); break;
                            case BET_RED:   betStr = "ROUGE"; betCol=RED; break;
                            case BET_BLACK: betStr = "NOIR"; betCol=GRAY; break;
                            case BET_EVEN:  betStr = "PAIR"; break;
                            case BET_ODD:   betStr = "IMPAIR"; break;
                            case BET_LOW:   betStr = "1 A 18"; break;
                            case BET_HIGH:  betStr = "19 A 36"; break;
                            case BET_DOZEN_1: betStr = "1ere 12"; break;
                            case BET_DOZEN_2: betStr = "2eme 12"; break;
                            case BET_DOZEN_3: betStr = "3eme 12"; break;
                            case BET_COL_1: betStr = "COL. 1"; break;
                            case BET_COL_2: betStr = "COL. 2"; break;
                            case BET_COL_3: betStr = "COL. 3"; break;
                            default: betStr = "?"; break;
                        }
                        break; 
                    }
                }
                DrawText(betStr, panel_x + 92, row_y, 10, betCol);

                DrawCircle(panel_x + 220, row_y + 6, 5, bot_tints[shm->bets[i].color_id]);
                DrawCircleLines(panel_x + 220, row_y + 6, 6, WHITE); 

                if (ago != -1) {
                    Color pingCol = (ago < 4) ? GREEN : (ago < 10 ? YELLOW : RED);
                    DrawText(TextFormat("%ds", ago), panel_x + 250, row_y, 12, pingCol);
                } else {
                    DrawText("-", panel_x + 250, row_y, 12, DARKGRAY);
                }

                row_y += 20;
            }

            // Historique Mutex
            int log_y = SCREEN_H - log_h - 10;

            time_t srv_last = 0;
            for(int k=0; k < MUTEX_EVENT_HISTORY; k++) {
                if (shm->mutex_events[k].pid == pid_server && shm->mutex_events[k].ts > srv_last) 
                    srv_last = shm->mutex_events[k].ts;
            }
            int srv_ago = (srv_last > 0) ? (int)(now - srv_last) : -1;
            
            int y_srv_info = log_y - 25;
            DrawText("SERVER", panel_x + 15, y_srv_info, 14, GOLD);
            DrawText(TextFormat("PID: %d", pid_server), panel_x + 90, y_srv_info, 11, LIGHTGRAY);
            DrawText("PING:", panel_x + 210, y_srv_info, 11, LIGHTGRAY);
            if (srv_ago != -1) {
                Color sCol = (srv_ago < 4) ? GREEN : (srv_ago < 10 ? YELLOW : RED);
                DrawText(TextFormat("%ds", srv_ago), panel_x + 250, y_srv_info, 12, sCol);
            } else {
                DrawText("-", panel_x + 250, y_srv_info, 12, DARKGRAY);
            }

            DrawRectangle(panel_x + 10, log_y, PANEL_W - 20, log_h, (Color){0, 0, 0, 180});
            DrawRectangleLines(panel_x + 10, log_y, PANEL_W - 20, log_h, DARKGRAY);
            DrawText("> SYSLOG (MUTEX HISTORY)", panel_x + 15, log_y + 5, 10, GREEN);

            int max_lines = (log_h - 20) / 14;
            int count = shm->mutex_events_count;
            if (count > MUTEX_EVENT_HISTORY) count = MUTEX_EVENT_HISTORY;
            if (count > max_lines) count = max_lines;

            int head = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            
            for (int i = 0; i < count; i++) {
                int idx = head - 1 - i;
                if (idx < 0) idx += MUTEX_EVENT_HISTORY;
                
                time_t ts = shm->mutex_events[idx].ts;
                struct tm *tm_info = localtime(&ts);
                char timeBuffer[9];
                strftime(timeBuffer, 9, "%H:%M:%S", tm_info);
                
                int status = shm->mutex_events[idx].status;
                Color logCol = status ? RED : DARKGREEN;
                const char* act = status ? "LOCK" : "FREE";
                
                int line_y = log_y + log_h - 18 - (i * 14);
                DrawText(TextFormat("[%s] %s PID:%d", timeBuffer, act, shm->mutex_events[idx].pid), 
                         panel_x + 15, line_y, 10, logCol);
            }

            // Gestion de défaite (banque à 0)
            if (shm->state == RESULTS && show_lost_panel) {
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0,0,0,160});
                float px = (SCREEN_W / 2.0f) - (tPanel.width / 2.0f);
                float py = (SCREEN_H / 2.0f) - (tPanel.height / 2.0f);
                DrawTexturePro(tPanel,
                    (Rectangle){0,0,tPanel.width, tPanel.height},
                    (Rectangle){px, py, tPanel.width, tPanel.height},
                    (Vector2){0,0}, 0.0f, WHITE);
                DrawText("PERDU", (int)(px + tPanel.width/2 - MeasureText("PERDU", 48)/2), (int)(py + 30), 48, RED);
                const char *summary = "La banque est vide. Vous avez perdu.";
                DrawText(summary, (int)(px + tPanel.width/2 - MeasureText(summary, 18)/2), (int)(py + 100), 18, LIGHTGRAY);
                DrawText(TextFormat("Banque: %d$", shm->bank), (int)(px + tPanel.width/2 - MeasureText("Banque: 0000$", 16)/2), (int)(py + 140), 16, GOLD);

                int btn_w = 180; int btn_h = 42;
                float bx = px + (tPanel.width/2) - (btn_w/2);
                float by = py + tPanel.height - 70;
                Rectangle rbtn = { bx, by, btn_w, btn_h };
                Vector2 mp = GetMousePosition();
                bool hoverbtn = (mp.x >= rbtn.x && mp.x <= rbtn.x + rbtn.width && mp.y >= rbtn.y && mp.y <= rbtn.y + rbtn.height);
                DrawRectangleRec(rbtn, hoverbtn ? (Color){100,180,100,255} : (Color){80,160,80,255});
                DrawRectangleLinesEx(rbtn, 2, BLACK);
                DrawText("REJOUER", (int)(bx + btn_w/2 - MeasureText("REJOUER", 20)/2), (int)(by + btn_h/2 - 10), 20, BLACK);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverbtn) {
                    if (shm != NULL) {
                        if (sem_wait(&shm->mutex) == 0) {
                            shm->bank = start_bank;
                            shm->total_bets = 0;
                            sem_post(&shm->mutex);
                        }
                    }
                    show_lost_panel = false;
                }
            }
        }

        // Gestion audio
        if (shm != NULL) {
            if (shm->state != prev_state) {
                if (shm->state == RESULTS) {
                    if (prev_bank >= 0 && shm->bank > prev_bank) {
                        if (audio_ready) PlaySound(s_win);
                    }
                }
                prev_state = shm->state;
            }
            if (prev_bank != -1 && shm->bank == 0 && prev_bank != 0) {
                if (audio_ready) PlaySound(s_empty);
            }
            show_lost_panel = (shm->bank == 0);
            prev_bank = shm->bank;
        }

        if (audio_ready && ambient_playing) UpdateMusicStream(ambient);

        EndDrawing();
    }

    // Gestion d'arrêt et nettoyage
    if (pid_bots > 0) {
        kill(-pid_bots, SIGKILL);
        waitpid(pid_bots, NULL, 0);
        pid_bots = 0;
    }
    if (pid_server > 0) {
        kill(-pid_server, SIGINT);
        waitpid(pid_server, NULL, 0);
        pid_server = 0;
    }

    UnloadTexture(tTable);
    UnloadTexture(tWheelStatic);
    UnloadTexture(tWheelSpin);
    UnloadTexture(tChip);
    UnloadTexture(tMain);
    if (shm != NULL) shmdt(shm);
    UnloadTexture(tPanel);
    CloseWindow();
    return 0;
}