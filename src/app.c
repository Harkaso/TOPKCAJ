// app.c
#include "shared.h"
#include "raylib.h"
#include <math.h>
#include <sys/wait.h>

// --- CONFIGURATION DE CALIBRATION ---
// Modifiez ces valeurs si les jetons ne tombent pas pile dans les cases !

// Dimensions de la fenêtre (doit correspondre au ratio de votre image de table)
// Keep total width under 1920 — we reserve a right-side panel for status
const int SCREEN_W = 1900;
const int SCREEN_H = 900;
const int PANEL_W = 300;

// POSITION DE LA GRILLE SUR L'IMAGE (En pixels)
// Basé sur l'image fournie, on estime le coin haut-droite de la case "3" (première case rouge en haut)
const int GRID_ORIGIN_X = 601;  // Décalage horizontal du début de la grille
const int GRID_ORIGIN_Y = 169;  // Décalage vertical de la ligne du haut (3, 6, 9...)

// TAILLE DES CASES SUR L'IMAGE
const int CELL_W = 67;  // Largeur d'une case numéro
const int CELL_H = 109;  // Hauteur d'une case numéro
const float CELL_GAP = 4.5f;

const int OFFSET_Y_DOZENS = 25; 
const int HEIGHT_DOZENS = 50;   // Hauteur visuelle de la case "1st 12"

const int OFFSET_Y_CHANCES = 25;  // Ecart vertical entre le bas de "1st 12" et le haut de "Pair/Impair/Rouge..."
const int HEIGHT_CHANCES = 90;  // Hauteur visuelle de la case "Pair/Impair"

// POSITION DE LA ROUE (Dans la zone verte à gauche)
const int WHEEL_POS_X = 250;
const int WHEEL_POS_Y = 440;
const float WHEEL_SCALE = 0.45f; // Taille globale de la roue

const float BALL_SIZE = 5.5f;
const float BALL_RADIUS_OUTER = 155.0f;  // Rayon de la piste extérieure
const float BALL_RADIUS_INNER = 105.0f;  // Rayon des cases intérieures
float global_ball_angle = 0.0f;


// COULEURS DES BOTS (Teinte appliquée sur le jeton blanc)
Color bot_tints[] = { 
    DARKPURPLE,
    DARKGREEN,
    ORANGE,
    SKYBLUE,
    RED,
    DARKBLUE,
    YELLOW,
    MAROON,
    PINK,
    VIOLET,
    GOLD,
    BLUE,
    GREEN,
    MAGENTA,
    BEIGE,
    WHITE,
};

// ORDRE DE LA ROUE (Standard Américain selon l'image)
int WHEEL_ORDER[] = {0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1, 37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2};

// TEXTURES
Texture2D tTable;
Texture2D tWheelStatic;
Texture2D tWheelSpin;
Texture2D tChip;
Texture2D tMain;
Texture2D tPanel;

// --- FONCTIONS UTILITAIRES ---

int is_red_num(int n) {
    if (n==0 || n==37) return 0;
    int reds[] = {1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
    for(int i=0;i<18;i++) if(reds[i]==n) return 1;
    return 0;
}

// Calcule la position X,Y du centre d'une case numéro sur VOTRE image
Vector2 get_num_pos(int n) {
    // Cas Spéciaux 0 et 00 (A gauche)
    // On garde l'ajustement manuel car ils n'ont pas forcément le même gap
    if(n == 0)  return (Vector2){GRID_ORIGIN_X - (CELL_W / 1.75f) - (CELL_GAP * 0.5f) - 0.5f, GRID_ORIGIN_Y + (CELL_H * 2.25f) + (CELL_GAP * 0.5f) + 0.5f};
    //if (n == 0) return (Vector2){0,0};
    if(n == 37) return (Vector2){GRID_ORIGIN_X - (CELL_W / 1.75f) - (CELL_GAP * 0.5f) - 0.5f, GRID_ORIGIN_Y + (CELL_H * 0.75f) + (CELL_GAP * 0.5f) + 0.5f};
    //if (n == 37) return (Vector2){0,0};

    // Grille 1-36
    int col = (n - 1) / 3;
    
    // Calcul de la ligne (Row)
    // Row 0 (Haut, ex: 3)
    // Row 1 (Milieu, ex: 2)
    // Row 2 (Bas, ex: 1)
    int row = 0;
    if (n % 3 == 0) row = 0;      // Ligne du haut
    else if (n % 3 == 2) row = 1; // Ligne du milieu
    else row = 2;                 // Ligne du bas


    // On multiplie col par (CELL_W + CELL_GAP) au lieu de juste CELL_W
    float x = GRID_ORIGIN_X + col * (CELL_W + CELL_GAP) + (CELL_W / 2.0f);
    float y = GRID_ORIGIN_Y + row * (CELL_H + CELL_GAP) + (CELL_H / 2.0f);
    
    //if (col >= 6) x -= 2.0f; 

    return (Vector2){x, y};
}

Vector2 get_bet_pos(Bet m) {
    // A. MISES SUR LES NUMEROS
    if(m.type <= BET_DOUBLE_STREET) {
        if(m.count == 0) return (Vector2){0,0};
        
        float sx = 0, sy = 0;
        for(int i=0; i<m.count; i++) { 
            Vector2 p = get_num_pos(m.numbers[i]); 
            sx += p.x; 
            sy += p.y; 
        }
        
        Vector2 res = { sx / m.count, sy / m.count };

        // Ajustement pour les mises "à cheval" sur la ligne gauche (Street/Sixain)
        if(m.type == BET_STREET || m.type == BET_DOUBLE_STREET) {
            res.y = GRID_ORIGIN_Y;
        }

        // Jitter (Petit décalage aléatoire réduit pour être plus précis)
        res.x += (m.pid % 6) - 3;
        res.y += (m.pid % 6) - 3;
        return res;
    }

    float y_bottom_grid = get_num_pos(1).y + CELL_H/2.0f;
    
    // Centre Y pour la ligne "1st 12"
    float y_dozens = y_bottom_grid + OFFSET_Y_DOZENS + HEIGHT_DOZENS/2.0f;
    
    // Centre Y pour la ligne "Pair/Impair"
    float y_chances = y_bottom_grid + OFFSET_Y_DOZENS + HEIGHT_DOZENS + OFFSET_Y_CHANCES + HEIGHT_CHANCES/2.0f;

    Vector2 p = {0,0};

    // 2. Colonnes (Tout à droite : 2 to 1)
    // X = Bord droit du numéro 36 + Gap
    float x_col = get_num_pos(36).x + CELL_W/2.0f + CELL_GAP + CELL_W/2.0f; // On décale d'une case entière
    
    if(m.type == BET_COL_3)      p = (Vector2){x_col, get_num_pos(36).y}; // Aligné sur 36
    else if(m.type == BET_COL_2) p = (Vector2){x_col, get_num_pos(35).y}; // Aligné sur 35
    else if(m.type == BET_COL_1) p = (Vector2){x_col, get_num_pos(34).y}; // Aligné sur 34

    // 3. Douzaines (1st 12...)
    // X = Moyenne entre le premier et le dernier numéro de la zone
    else if(m.type == BET_DOZEN_1) p = (Vector2){ (get_num_pos(1).x + get_num_pos(12).x)/2.0f, y_dozens };
    else if(m.type == BET_DOZEN_2) p = (Vector2){ (get_num_pos(13).x + get_num_pos(24).x)/2.0f, y_dozens };
    else if(m.type == BET_DOZEN_3) p = (Vector2){ (get_num_pos(25).x + get_num_pos(36).x)/2.0f, y_dozens };

    // 4. Chances Simples (Rouge, Noir...)
    else if(m.type == BET_LOW)   p = (Vector2){ (get_num_pos(1).x + get_num_pos(6).x)/2.0f, y_chances };
    else if(m.type == BET_EVEN)  p = (Vector2){ (get_num_pos(7).x + get_num_pos(12).x)/2.0f, y_chances };
    else if(m.type == BET_RED)   p = (Vector2){ (get_num_pos(13).x + get_num_pos(18).x)/2.0f, y_chances };
    else if(m.type == BET_BLACK) p = (Vector2){ (get_num_pos(19).x + get_num_pos(24).x)/2.0f, y_chances };
    else if(m.type == BET_ODD)   p = (Vector2){ (get_num_pos(25).x + get_num_pos(30).x)/2.0f, y_chances };
    else if(m.type == BET_HIGH)  p = (Vector2){ (get_num_pos(31).x + get_num_pos(36).x)/2.0f, y_chances };

    // Petit jitter
    p.x += (m.pid % 8) - 4;
    p.y += (m.pid % 8) - 4;
    return p;
}

// --- DESSIN ---
// Trouve le décalage angulaire d'un numéro spécifique sur la texture de la roue
float get_slot_angle_offset(int number) {
    int index = -1;
    // On cherche la position du numéro dans le tableau ordonné
    for(int i=0; i<38; i++) {
        if (WHEEL_ORDER[i] == number) {
            index = i;
            break;
        }
    }
    
    if (index == -1) return 0.0f; // Sécurité

    // Une roue a 38 cases. 360 degrés / 38 = 9.47 degrés par case.
    float angle_per_slot = 360.0f / 38.0f;
    
    // L'angle dépend de l'index. 
    return (index * angle_per_slot + 90);
}

void DrawAssets(float wheel_rotation, int win_num, int state) {
    // 1. DESSINER LA TABLE (FOND) - Inchangé
    DrawTexturePro(tTable, 
        (Rectangle){0, 0, tTable.width, tTable.height}, 
        // Reserve right panel for realtime status
        (Rectangle){0, 0, SCREEN_W - PANEL_W, SCREEN_H}, 
        (Vector2){0,0}, 0.0f, WHITE);

    // 2. DESSINER LA ROUE - Inchangé
    
    // A. Roue Mobile (Intérieur)
    float inner_scale = WHEEL_SCALE * 0.315f;
    Rectangle sourceSpin = {0, 0, tWheelSpin.width, tWheelSpin.height};
    Rectangle destSpin = {WHEEL_POS_X, WHEEL_POS_Y, tWheelSpin.width * inner_scale, tWheelSpin.height * inner_scale};
    Vector2 originSpin = {destSpin.width/2, destSpin.height/2}; 
    DrawTexturePro(tWheelSpin, sourceSpin, destSpin, originSpin, wheel_rotation, WHITE);

    // B. Roue Fixe (Cadre Bois)
    Rectangle sourceStatic = {0, 0, tWheelStatic.width, tWheelStatic.height};
    Rectangle destStatic = {WHEEL_POS_X, WHEEL_POS_Y, tWheelStatic.width * WHEEL_SCALE, tWheelStatic.height * WHEEL_SCALE};
    Vector2 originStatic = {destStatic.width/2, destStatic.height/2};
    DrawTexturePro(tWheelStatic, sourceStatic, destStatic, originStatic, 0.0f, WHITE);

    // --- C. LOGIQUE DE LA BILLE ---
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
        
        // Calcul de la position X,Y par trigonométrie
        // bx = CentreX + cos(angle) * rayon
        // by = CentreY + sin(angle) * rayon
        float bx = WHEEL_POS_X + cosf(global_ball_angle * DEG2RAD) * current_radius;
        float by = WHEEL_POS_Y + sinf(global_ball_angle * DEG2RAD) * current_radius;
        
        // --- DESSIN DE TA BILLE REALISTE ---
        
        DrawCircle(bx + 2, by + 2, BALL_SIZE - 1.0f, (Color){0,0,0,100}); 
        DrawCircle(bx, by, BALL_SIZE, WHITE); 
        DrawCircleGradient(bx, by, BALL_SIZE, WHITE, GRAY);
    }
}

void DrawChips(GameTable *shm) {
    for (int i = 0; i < shm->total_bets; i++) {
        Vector2 pos = get_bet_pos(shm->bets[i]);
        
        // Si la position est valide
        if (pos.x > 10) {
            // Taille du jeton (Ajustez 0.15f selon la résolution de votre image jeton)
            float scale = 0.052f; 
            
            // Ombre portée
            DrawTexturePro(tChip, 
                (Rectangle){0,0,tChip.width,tChip.height}, 
                (Rectangle){pos.x+3, pos.y+3, tChip.width*scale, tChip.height*scale}, 
                (Vector2){(tChip.width*scale)/2, (tChip.height*scale)/2}, 
                0.0f, (Color){0,0,0,100});

            // Le Jeton Teinté
            Color tint = bot_tints[shm->bets[i].color_id];
            DrawTexturePro(tChip, 
                (Rectangle){0,0,tChip.width,tChip.height}, 
                (Rectangle){pos.x, pos.y, tChip.width*scale, tChip.height*scale}, 
                (Vector2){(tChip.width*scale)/2, (tChip.height*scale)/2}, 
                0.0f, tint);
                
            // Petit texte ID du bot au centre du jeton
            DrawText(TextFormat("%d", shm->bets[i].color_id+1), pos.x-4, pos.y-6, 10, BLACK);
        }
    }
}

// 1. SHM pointer will be obtained when the game starts (server launched by GUI)
int shmid = -1;
GameTable *shm = NULL;
pid_t pid_server = 0;
pid_t pid_bots = 0;

// cleanup handler to stop server/players if started
void gui_cleanup_int(int sig) {
    // Try graceful shutdown of child process groups, then force if necessary
    if (pid_bots > 0) {
        // Send SIGTERM to the players process group
        kill(-pid_bots, SIGTERM);
        // Wait up to 2 seconds for group to exit
        for (int i = 0; i < 20; i++) {
            if (kill(pid_bots, 0) == -1) break; // no such process
            usleep(100000);
        }
        // If still status, force kill the group
        kill(-pid_bots, SIGKILL);
        waitpid(pid_bots, NULL, 0);
        pid_bots = 0;
    }
    if (pid_server > 0) {
        // Ask server to exit gracefully
        kill(-pid_server, SIGINT);
        for (int i = 0; i < 20; i++) {
            if (kill(pid_server, 0) == -1) break;
            usleep(100000);
        }
        // Force kill if still present
        kill(-pid_server, SIGKILL);
        waitpid(pid_server, NULL, 0);
        pid_server = 0;
    }
    if (shm != NULL) {
        shmdt(shm);
        shm = NULL;
    }
    CloseWindow();
    // If called from a signal handler, exit immediately
    _exit(0);
}

// atexit-friendly wrapper
void gui_cleanup_atexit(void) { gui_cleanup_int(0); }

int main(int argc, char *argv[]) {

    signal(SIGINT, gui_cleanup_int);
    signal(SIGTERM, gui_cleanup_int);
    signal(SIGHUP, gui_cleanup_int);
    atexit(gui_cleanup_atexit);

    char str_bots[10] = "8"; // Défaut
    int is_debug = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            is_debug = 1;
        }
        else if (strcmp(argv[i], "--bots") == 0 && i+1 < argc) {
            strncpy(str_bots, argv[i+1], 9);
            i++;
        }
    }

    // 2. INIT FENETRE
    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_W, SCREEN_H, "ETOPKCEJ - Roulette Americaine");
    SetTargetFPS(60);

    // 3. CHARGEMENT DES ASSETS
    // Assurez-vous que ces fichiers sont dans le même dossier !
    tTable = LoadTexture("./src/assets/table.png");
    tWheelStatic = LoadTexture("./src/assets/cadre.png");
    tWheelSpin = LoadTexture("./src/assets/roue.png");
    tChip = LoadTexture("./src/assets/jeton.png");
    // Main menu background (full window)
    tMain = LoadTexture("./src/assets/Main.png");
    
    // Filtrage bilinéaire pour que le redimensionnement soit joli
    SetTextureFilter(tTable, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tWheelStatic, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tWheelSpin, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tChip, TEXTURE_FILTER_BILINEAR);

    // --- AUDIO INITIALIZATION ---
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

    while (!WindowShouldClose()) {
        // Update animation rotation only when inside the game (not menu)
        if (!in_menu) {
            if (shm->state != RESULTS) {
                rotation += 1.0f; // Vitesse normale
            } else {
                rotation += 0.3f; // Vitesse ralentie "idle" pendant les résultats
            }
            if(rotation > 360) rotation -= 360;
        }

        // Initialize previous state/bank when shared memory is first attached
        if (shm != NULL && prev_state == -1) {
            prev_state = shm->state;
            prev_bank = shm->bank;
        }

        BeginDrawing();
        ClearBackground(BLACK); // Fond noir si l'image de table ne couvre pas tout

        if (in_menu) {
            // Draw main menu full-screen
            DrawTexturePro(tMain,
                (Rectangle){0, 0, tMain.width, tMain.height},
                (Rectangle){0, 0, SCREEN_W, SCREEN_H},
                (Vector2){0,0}, 0.0f, WHITE);

            // Invisible button area (1000,400) size 440x140
            Vector2 mp = GetMousePosition();
            Rectangle btn = {1000, 400, 440, 140};
            bool hover = (mp.x >= btn.x && mp.x <= btn.x + btn.width && mp.y >= btn.y && mp.y <= btn.y + btn.height);
            if (hover) {
                // subtle hover outline to aid the user
                DrawRectangleLinesEx(btn, 2, (Color){255,255,255,80});
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hover) {
                // Spawn server and players only after user explicitly starts the game
                if (pid_server == 0) {
                    pid_server = fork();
                    if (pid_server == 0) {
                        // child: create a new process group so we can kill the whole group later
                        setpgid(0,0);
                        // child: replace with server binary
                        execl("./dependencies/server", "./dependencies/server", (char*)NULL);
                        // if exec fails
                        perror("execl server");
                        _exit(127);
                    }
                }
                if (pid_bots == 0) {
                    pid_bots = fork();
                    if (pid_bots == 0) {
                        // child: create a new process group so we can kill the whole group later
                        setpgid(0,0);
                            
                        if (is_debug) {
                            execl("./dependencies/players", "players", "--debug", NULL);
                        } else {
                            execl("./dependencies/players", "players", "--bots", str_bots, NULL);
                        }
                        
                        perror("execl player");
                        _exit(127);
                        }
                }

                // Wait for shared memory segment to be created by the server
                int tries = 0;
                while (tries < 30) { // wait up to ~30 seconds
                    shmid = shmget(SHM_KEY, sizeof(GameTable), 0666);
                    if (shmid != -1) break;
                    tries++;
                    sleep(1);
                }
                if (shmid == -1) {
                    // Failed to find SHM in time — kill children and remain in menu
                    if (pid_bots > 0) { kill(pid_bots, SIGKILL); waitpid(pid_bots, NULL, 0); pid_bots = 0; }
                    if (pid_server > 0) { kill(pid_server, SIGINT); waitpid(pid_server, NULL, 0); pid_server = 0; }
                    // Inform via trace log and stay in menu
                    TraceLog(LOG_WARNING, "Server did not create shared memory — aborting start");
                } else {
                    // Attach to shared memory and enter game
                    shm = (GameTable*)shmat(shmid, NULL, 0);
                    if (shm == (void*)-1) {
                        shm = NULL;
                        TraceLog(LOG_WARNING, "Failed to attach shared memory in GUI");
                    } else {
                        in_menu = false; // successfully attached, enter game
                    }
                }
            }
        } else {
            // A. Dessiner le Décor (Table + Roue)
            DrawAssets(rotation, shm->winning_number, shm->state);

            // B. Dessiner les Jetons
            DrawChips(shm);
        }

        // Debug key: press 'K' to force bank to 0 (show game over panel)
        if (!in_menu && IsKeyPressed(KEY_K) && shm != NULL) {
            if (sem_wait(&shm->mutex) == 0) {
                shm->bank = 0;
                shm->total_bets = 0;
                sem_post(&shm->mutex);
                TraceLog(LOG_INFO, "[GUI-DEBUG] bank set to 0 via KEY_K");
            }
        }

        // Draw UI and status panel only when inside the game
        if (!in_menu) {
            if (shm->state == RESULTS) {
                int win = shm->winning_number;
                
                // 1. Déterminer la couleur
                Color resColor = DARKGREEN; // Par défaut pour 0 et 00
                if (is_red_num(win)) resColor = RED;
                else if (win != 0 && win != 37) resColor = BLACK;

                // 2. Position (Sous la roue centrée)
                // Roue centrée en X=250, Y=440. Rayon env. 180.
                // On place la boite vers Y=660
                int boxW = 100;
                int boxH = 90;
                int boxX = WHEEL_POS_X - boxW/2; 
                int boxY = WHEEL_POS_Y + 260; 

                // 3. Dessin de la boite
                DrawRectangle(boxX, boxY, boxW, boxH, resColor);       // Fond Couleur
                DrawRectangleLinesEx((Rectangle){boxX, boxY, boxW, boxH}, 3, GOLD); // Bordure Or
                
                // Ombre portée légère
                DrawRectangleLinesEx((Rectangle){boxX+4, boxY+4, boxW, boxH}, 3, (Color){0,0,0,50});

                // 4. Texte du Numéro
                const char* txtNum = (win == 37) ? "00" : TextFormat("%d", win);
                int txtSize = 50;
                int txtW = MeasureText(txtNum, txtSize);
                
                // Centrage du texte
                DrawText(txtNum, boxX + (boxW/2) - (txtW/2), boxY + (boxH/2) - (txtSize/2), txtSize, WHITE);
            }

            const char* textBank = TextFormat("BANQUE: %d $", shm->bank);
            DrawText(textBank, 20, 20, 40, GOLD);

            // --- RIGHT STATUS PANEL (Dashboard Final) ---
            int panel_x = SCREEN_W - PANEL_W;
            time_t now = time(NULL);
            
            // 1. FOND ET TITRE
            DrawRectangle(panel_x, 0, PANEL_W, SCREEN_H, (Color){15, 15, 20, 245}); // Fond bleu nuit
            DrawLine(panel_x, 0, panel_x, SCREEN_H, (Color){255, 215, 0, 100}); // Ligne Or
            
            DrawText("ETOPKCAJ", panel_x + 20, 15, 24, GOLD);
            DrawRectangle(panel_x + 20, 45, PANEL_W - 40, 2, (Color){255, 215, 0, 50});

            // 2. BLOC BANQUE & MISES
            int y_stats = 60;
            DrawRectangle(panel_x + 10, y_stats, PANEL_W - 20, 80, (Color){255, 255, 255, 10});
            DrawRectangleLines(panel_x + 10, y_stats, PANEL_W - 20, 80, (Color){255, 255, 255, 30});
            
            DrawText("BANQUE COMMUNE", panel_x + 20, y_stats + 10, 10, LIGHTGRAY);
            DrawText(TextFormat("%d$", shm->bank), panel_x + 20, y_stats + 25, 28, (shm->bank > 0) ? GOLD : RED);
            
            DrawText("NBR DE MISES", panel_x + 160, y_stats + 10, 10, LIGHTGRAY);
            DrawText(TextFormat("%d", shm->total_bets), panel_x + 160, y_stats + 25, 28, SKYBLUE);

            // 3. BLOC ETAT DU JEU
            int y_state = 150;
            Color stateColor = GRAY;
            const char* stateText = "INCONNU";
            
            if (shm->state == BETS_OPEN) { stateColor = LIME; stateText = "OUVERT - PARIS EN COURS"; }
            else if (shm->state == BETS_CLOSED) { stateColor = ORANGE; stateText = "FERME - RIEN NE VA PLUS"; }
            else if (shm->state == RESULTS) { stateColor = RED; stateText = "RESULTATS & PAIEMENT"; }

            DrawRectangle(panel_x + 10, y_state, PANEL_W - 20, 30, Fade(stateColor, 0.2f));
            DrawRectangleLines(panel_x + 10, y_state, PANEL_W - 20, 30, stateColor);
            DrawText(stateText, panel_x + 20, y_state + 8, 12, stateColor);

            // 4. BLOC TECHNIQUE (MUTEX)
            int y_tech = 205;
            DrawText("STATUS MUTEX", panel_x + 20, y_tech, 12, GRAY);
            
            Color ledColor = shm->mutex_status ? RED : GREEN;
            DrawCircle(panel_x + 25, y_tech + 25, 6, ledColor);
            DrawText(shm->mutex_status ? "VERROUILLE" : "LIBRE", panel_x + 40, y_tech + 18, 14, ledColor);
            
            if (shm->mutex_status) {
                DrawText(TextFormat("PAR PID: %d", shm->mutex_owner), panel_x + 140, y_tech + 18, 14, WHITE);
            }

            // 5. LISTE DES JOUEURS (Tableau Détaillé)
            int y_list = 265;
            DrawText(TextFormat("JOUEURS (%d)", shm->player_count), panel_x + 20, y_list, 14, GOLD);
            DrawRectangle(panel_x + 20, y_list + 18, PANEL_W - 40, 1, GRAY);
            
            int col_py = y_list + 25;
            // En-têtes : ID | PID | PARI | C. | PING
            DrawText("ID",   panel_x + 15, col_py, 10, DARKGRAY);
            DrawText("PID",  panel_x + 40, col_py, 10, DARKGRAY);
            DrawText("PARI", panel_x + 92, col_py, 10, DARKGRAY);
            DrawText("COUL.",   panel_x + 210, col_py, 10, DARKGRAY);
            DrawText("PING", panel_x + 250, col_py, 10, DARKGRAY);
            
            int row_y = col_py + 15;
            int log_h = 180;
            
            for (int i = 0; i < MAX_BOTS; i++) {
                if (shm->players[i].pid == 0) continue; 
                if (row_y > SCREEN_H - log_h - 20) break; 

                int pid = shm->players[i].pid;
                
                // --- CALCUL DU "VRAI" PING (Dernière action Mutex) ---
                time_t last_action_ts = 0;
                // On scanne l'historique pour trouver la dernière trace de ce PID
                for(int k=0; k < MUTEX_EVENT_HISTORY; k++) {
                    if (shm->mutex_events[k].pid == pid) {
                        if (shm->mutex_events[k].ts > last_action_ts) {
                            last_action_ts = shm->mutex_events[k].ts;
                        }
                    }
                }

                int ago = (last_action_ts > 0) ? (int)(now - last_action_ts) : -1;

                Color rowCol = WHITE; 
                
                // Si on est en phase de RESULTATS, on calcule si le joueur a gagné
                if (shm->state == RESULTS) {
                    int win = shm->winning_number;
                    int won = 0;
                    
                    // On cherche le pari du joueur
                    for(int b=0; b < shm->total_bets; b++) {
                        if (shm->bets[b].pid == pid) {
                            Bet m = shm->bets[b];
                            // Vérification rapide de victoire
                            if (m.type <= BET_DOUBLE_STREET) { // Mises Intérieures
                                for(int k=0; k<m.count; k++) if(m.numbers[k] == win) won=1;
                            } else { // Mises Extérieures
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
                    
                    if (won) rowCol = GREEN;        // GAGNANT EN VERT
                    else rowCol = Fade(GRAY, 0.4f); // PERDANT EN GRIS SOMBRE
                }
                
                // 1. ID
                DrawText(TextFormat("%d", i+1), panel_x + 15, row_y, 12, rowCol);

                // 2. PID
                DrawText(TextFormat("%d", pid), panel_x + 40, row_y, 12, rowCol);
                
                // 3. PARI (Recherche inchangée)
                const char* betStr = "-";
                Color betCol = GRAY;
                for(int b=0; b < shm->total_bets; b++) {
                    if (shm->bets[b].pid == pid) {
                        betCol = SKYBLUE;
                        Bet m = shm->bets[b];
                        switch(m.type) {
                            case BET_SINGLE: betStr = TextFormat("Plein %d", m.numbers[0]); if(m.numbers[0]==37) betStr="Plein 00"; break;
                            case BET_SPLIT:  
                                if(m.numbers[1]==37) betStr = "Cheval 0-00"; 
                                else betStr = TextFormat("Cheval %d-%d", m.numbers[0], m.numbers[1]); 
                                break;
                            case BET_STREET: betStr = TextFormat("Traversale %d : %d", m.numbers[0], m.numbers[2]); break;
                            case BET_SQUARE: betStr = TextFormat("Carre %d-%d-%d-%d", m.numbers[0], m.numbers[1], m.numbers[2], m.numbers[3]); break;
                            case BET_DOUBLE_STREET: betStr = TextFormat("Sixain %d : %d", m.numbers[0], m.numbers[5]); break;
                            case BET_RED:   betStr = "ROUGE"; betCol=RED; break;
                            case BET_BLACK: betStr = "NOIR"; betCol=GRAY; break;
                            case BET_EVEN:  betStr = "PAIR"; break;
                            case BET_ODD:   betStr = "IMPAIR"; break;
                            case BET_LOW:   betStr = "1 a 18"; break;
                            case BET_HIGH:  betStr = "19 a 36"; break;
                            case BET_DOZEN_1: betStr = "1ere 12"; break;
                            case BET_DOZEN_2: betStr = "2eme 12"; break;
                            case BET_DOZEN_3: betStr = "3eme 12"; break;
                            case BET_COL_1: betStr = "COL 1"; break;
                            case BET_COL_2: betStr = "COL 2"; break;
                            case BET_COL_3: betStr = "COL 3"; break;
                            default: betStr = "?"; break;
                        }
                        break; 
                    }
                }
                DrawText(betStr, panel_x + 92, row_y, 10, betCol);

                // 4. COULEUR
                DrawCircle(panel_x + 220, row_y + 6, 5, bot_tints[shm->players[i].color_id]);
                DrawCircleLines(panel_x + 220, row_y + 6, 6, WHITE); 

                // 5. PING (Basé sur le Mutex cette fois)
                if (ago != -1) {
                    Color pingCol = (ago < 4) ? GREEN : (ago < 10 ? YELLOW : RED);
                    DrawText(TextFormat("%ds", ago), panel_x + 250, row_y, 12, pingCol);
                } else {
                    DrawText("-", panel_x + 250, row_y, 12, DARKGRAY);
                }

                row_y += 20;
            }

            // 6. INFO SERVEUR ET HISTORIQUE (En bas)
            int log_y = SCREEN_H - log_h - 10;

            // --- INFO SERVEUR ---
            // Calcul Ping Serveur (scan logs)
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

            // --- SYSLOG ---
            DrawRectangle(panel_x + 10, log_y, PANEL_W - 20, log_h, (Color){0, 0, 0, 180});
            DrawRectangleLines(panel_x + 10, log_y, PANEL_W - 20, log_h, DARKGRAY);
            DrawText("> SYSLOG (HISTORY)", panel_x + 15, log_y + 5, 10, GREEN);

            int max_lines = (log_h - 20) / 14;
            int count = shm->mutex_events_count;
            if (count > MUTEX_EVENT_HISTORY) count = MUTEX_EVENT_HISTORY;
            if (count > max_lines) count = max_lines;

            int head = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
            
            for (int i = 0; i < count; i++) {
                int idx = head - 1 - i;
                if (idx < 0) idx += MUTEX_EVENT_HISTORY;
                
                time_t ts = shm->mutex_events[idx].ts;
                // Formatage Heure Absolue (HH:MM:SS) pour éviter le clignotement
                struct tm *tm_info = localtime(&ts);
                char timeBuffer[9];
                strftime(timeBuffer, 9, "%H:%M:%S", tm_info);
                
                int status = shm->mutex_events[idx].status;
                Color logCol = status ? RED : DARKGREEN;
                const char* act = status ? "LOCK" : "FREE";
                
                int line_y = log_y + log_h - 18 - (i * 14);
                // Affiche [HH:MM:SS] au lieu de [02s]
                DrawText(TextFormat("[%s] %s PID:%d", timeBuffer, act, shm->mutex_events[idx].pid), 
                         panel_x + 15, line_y, 10, logCol);
            }

            // If this round had no winners, show the 'PERDU' panel in the middle
            if (shm->state == RESULTS && show_lost_panel) {
                // Darken background
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0,0,0,160});
                float px = (SCREEN_W / 2.0f) - (tPanel.width / 2.0f);
                float py = (SCREEN_H / 2.0f) - (tPanel.height / 2.0f);
                DrawTexturePro(tPanel,
                    (Rectangle){0,0,tPanel.width, tPanel.height},
                    (Rectangle){px, py, tPanel.width, tPanel.height},
                    (Vector2){0,0}, 0.0f, WHITE);
                // Overlay text "PERDU" and summary
                DrawText("PERDU", (int)(px + tPanel.width/2 - MeasureText("PERDU", 48)/2), (int)(py + 30), 48, RED);
                const char *summary = "La banque est vide. Vous avez perdu.";
                DrawText(summary, (int)(px + tPanel.width/2 - MeasureText(summary, 18)/2), (int)(py + 100), 18, LIGHTGRAY);
                DrawText(TextFormat("Banque: %d$", shm->bank), (int)(px + tPanel.width/2 - MeasureText("Banque: 0000$", 16)/2), (int)(py + 140), 16, GOLD);

                // Rejouer button
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
                    // Reset bank and bets under semaphore protection
                    if (shm != NULL) {
                        if (sem_wait(&shm->mutex) == 0) {
                            shm->bank = START_BANK;
                            shm->total_bets = 0;
                            sem_post(&shm->mutex);
                        }
                    }
                    show_lost_panel = false;
                }
            }
        }

        // If shared memory exists, detect transitions to RESULTS to play sounds
        if (shm != NULL) {
            if (shm->state != prev_state) {
                if (shm->state == RESULTS) {
                    // If bank increased compared to previous value => at least one player won
                    if (prev_bank >= 0 && shm->bank > prev_bank) {
                        if (audio_ready) PlaySound(s_win);
                    }
                    // leaving RESULTS doesn't clear the bank-based lost panel here
                }
                prev_state = shm->state;
            }
            // Bank dropped to zero -> play empty sound and show lost panel
            if (prev_bank != -1 && shm->bank == 0 && prev_bank != 0) {
                if (audio_ready) PlaySound(s_empty);
            }
            // show lost panel whenever bank is zero
            show_lost_panel = (shm->bank == 0);
            prev_bank = shm->bank;
        }

        // Update music stream each frame when playing ambient
        if (audio_ready && ambient_playing) UpdateMusicStream(ambient);

        EndDrawing();
    }

    // If server/players were started, terminate them (kill their process groups)
    if (pid_bots > 0) {
        // kill entire group
        kill(-pid_bots, SIGKILL);
        waitpid(pid_bots, NULL, 0);
        pid_bots = 0;
    }
    if (pid_server > 0) {
        // ask server to exit gracefully
        kill(-pid_server, SIGINT);
        waitpid(pid_server, NULL, 0);
        pid_server = 0;
    }

    // Nettoyage
    UnloadTexture(tTable);
    UnloadTexture(tWheelStatic);
    UnloadTexture(tWheelSpin);
    UnloadTexture(tChip);
    UnloadTexture(tMain);
    if (shm != NULL) shmdt(shm);
    // Unload panel texture if loaded
    UnloadTexture(tPanel);
    CloseWindow();
    return 0;
}