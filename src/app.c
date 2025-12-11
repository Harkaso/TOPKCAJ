// app.c
#include "shared.h"
#include "raylib.h"
#include <math.h>

// --- CONFIGURATION DE CALIBRATION ---
// Modifiez ces valeurs si les jetons ne tombent pas pile dans les cases !

// Dimensions de la fenêtre (doit correspondre au ratio de votre image de table)
// Keep total width under 1920 — we reserve a right-side panel for status
const int SCREEN_W = 1900;
const int SCREEN_H = 900;
const int PANEL_W = 300;

// POSITION DE LA GRILLE SUR L'IMAGE (En pixels)
// Basé sur l'image fournie, on estime le coin haut-droite de la case "3" (première case rouge en haut)
const int GRID_ORIGIN_X = 600;  // Décalage horizontal du début de la grille
const int GRID_ORIGIN_Y = 169;  // Décalage vertical de la ligne du haut (3, 6, 9...)

// TAILLE DES CASES SUR L'IMAGE
const int CELL_W = 67;  // Largeur d'une case numéro
const int CELL_H = 109;  // Hauteur d'une case numéro
const int CELL_GAP = 5;

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
    LIME,
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
    PURPLE,
    DARKBROWN,
    MAGENTA,
    BEIGE,
    WHITE,
    BLACK
};

// ORDRE DE LA ROUE (Standard Américain selon l'image)
int WHEEL_ORDER[] = {0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1, 37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2};

// TEXTURES
Texture2D tTable;
Texture2D tWheelStatic;
Texture2D tWheelSpin;
Texture2D tChip;

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
    if(n == 0)  return (Vector2){GRID_ORIGIN_X - CELL_W - CELL_GAP, GRID_ORIGIN_Y + CELL_H * 0.5f + CELL_GAP + 45};
    if(n == 37) return (Vector2){GRID_ORIGIN_X - CELL_W - CELL_GAP, GRID_ORIGIN_Y + CELL_H * 1.5f + CELL_GAP + 45};

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

    // --- LA CORRECTION EST ICI ---
    // On multiplie col par (CELL_W + CELL_GAP) au lieu de juste CELL_W
    float x = GRID_ORIGIN_X + col * (CELL_W + CELL_GAP) + (CELL_W / 2.0f);
    float y = GRID_ORIGIN_Y + row * (CELL_H + CELL_GAP) + (CELL_H / 2.0f);
    
    return (Vector2){x, y};
}

Vector2 get_bet_pos(Bet m) {
    // A. MISES SUR LES NUMEROS
    if(m.type <= BET_TOP_LINE) {
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
            res.x -= CELL_W / 2.0f;
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

int main() {
    // 1. CONNEXION MEMOIRE PARTAGEE
    int shmid = shmget(SHM_KEY, sizeof(GameTable), 0666);
    if (shmid < 0) { printf("ERREUR: Lancez ./server d'abord !\n"); return 1; }
    GameTable *shm = (GameTable *)shmat(shmid, NULL, 0);

    // 2. INIT FENETRE
    SetTraceLogLevel(LOG_NONE);
    InitWindow(SCREEN_W, SCREEN_H, "Casino - Roulette Americaine");
    SetTargetFPS(60);

    // 3. CHARGEMENT DES ASSETS
    // Assurez-vous que ces fichiers sont dans le même dossier !
    tTable = LoadTexture("./src/assets/table.png");
    tWheelStatic = LoadTexture("./src/assets/cadre.png");
    tWheelSpin = LoadTexture("./src/assets/roue.png");
    tChip = LoadTexture("./src/assets/jeton.png");
    
    // Filtrage bilinéaire pour que le redimensionnement soit joli
    SetTextureFilter(tTable, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tWheelStatic, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tWheelSpin, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(tChip, TEXTURE_FILTER_BILINEAR);

    float rotation = 0.0f;

    while (!WindowShouldClose()) {
        // Animation rotation
        if (shm->state != RESULTS) {
            rotation += 1.0f; // Vitesse normale
        } else {
            rotation += 0.3f; // Vitesse ralentie "idle" pendant les résultats
        }
        if(rotation > 360) rotation -= 360;

        BeginDrawing();
        ClearBackground(BLACK); // Fond noir si l'image de table ne couvre pas tout

        // A. Dessiner le Décor (Table + Roue)
        DrawAssets(rotation, shm->winning_number, shm->state);

        // B. Dessiner les Jetons
        DrawChips(shm);

        // C. Interface UI (Texte par dessus)
        const char* status = "";
        Color col = WHITE;
        if(shm->state == BETS_OPEN) { status="FAITES VOS JEUX"; col=GREEN; }
        else if(shm->state == BETS_CLOSED) { status="RIEN NE VA PLUS"; col=GOLD; }
        else { if (shm->winning_number == 37) {
                status = "RESULTAT: 00";
            } else {
                status = TextFormat("RESULTAT: %d", shm->winning_number);
            }
            col = RED;
        }

        // Bandeau noir semi-transparent en bas pour le texte
        DrawRectangle(0, SCREEN_H - 60, SCREEN_W, 60, (Color){0,0,0,200});
        DrawText(status, 20, SCREEN_H - 45, 30, col);
        
        const char* textBank = TextFormat("BANQUE: %d $", shm->bank);
        DrawText(textBank, 20, 20, 40, GOLD);

        // --- RIGHT STATUS PANEL ---
        int panel_x = SCREEN_W - PANEL_W;
        DrawRectangle(panel_x, 0, PANEL_W, SCREEN_H, (Color){20,20,20,220});
        // Draw right-panel fields using a vertical cursor to avoid overlapping
            time_t now = time(NULL);
            int py = 12;
            DrawText("STATUS PANEL", panel_x + 10, py, 20, RAYWHITE);
            py += 28;

            DrawText(TextFormat("Mutex locked: %s", shm->mutex_status ? "YES" : "NO"), panel_x + 10, py, 16, WHITE);
            py += 20;

            DrawText(TextFormat("Mutex owner PID: %d", (int)shm->mutex_owner), panel_x + 10, py, 16, WHITE);
            py += 20;

            // Show owner's last_seen if available
            if (shm->mutex_owner != 0) {
                int owner_last = -1;
                for (int i = 0; i < MAX_BOTS; i++) {
                    if (shm->players[i].pid == shm->mutex_owner) {
                        owner_last = (int)(now - shm->players[i].last_seen);
                        break;
                    }
                }
                if (owner_last >= 0) DrawText(TextFormat("Owner last_seen: %ds", owner_last), panel_x + 10, py, 14, WHITE);
                else DrawText("Owner last_seen: ?", panel_x + 10, py, 14, WHITE);
                py += 18;
            }

            DrawText(TextFormat("Total bets: %d", shm->total_bets), panel_x + 10, py, 14, WHITE);
            py += 18;

            DrawText(TextFormat("Bank: %d$", shm->bank), panel_x + 10, py, 16, GOLD);
            py += 22;

            // Players status list - start below the fields
            int yoff = py + 6;
            if (yoff < 140) yoff = 140;
            DrawText("Players:", panel_x + 10, yoff, 18, RAYWHITE);
        yoff += 22;
        for (int i = 0; i < MAX_BOTS && yoff < SCREEN_H - 20; i++) {
            if (shm->players[i].pid == 0) continue;
            int alive = shm->players[i].status;
            int pid = shm->players[i].pid;
            int color = shm->players[i].color_id;
            time_t last = shm->players[i].last_seen;
            int ago = (int)(now - last);
            const char *al = alive ? "ALIVE" : "DEAD";
            DrawText(TextFormat("#%d PID:%d %s (%ds) C:%d", i+1, pid, al, ago, color+1), panel_x + 10, yoff, 16, alive ? GREEN : RED);
            yoff += 20;
        }

        // --- MUTEX EVENT HISTORY (console) ---
        int hist_height = 200;
        int hist_y = SCREEN_H - hist_height - 10;
        // draw game state just above history
        const char *gstate = "?";
        if (shm->state == BETS_OPEN) gstate = "BETS_OPEN";
        else if (shm->state == BETS_CLOSED) gstate = "BETS_CLOSED";
        else if (shm->state == RESULTS) gstate = "RESULTS";
        DrawText(TextFormat("Game state: %s", gstate), panel_x + 10, hist_y - 18, 16, WHITE);

        DrawRectangle(panel_x + 6, hist_y, PANEL_W - 12, hist_height, (Color){10,10,10,200});
        DrawText("Mutex history:", panel_x + 10, hist_y + 6, 16, RAYWHITE);

        int max_lines = (hist_height - 24) / 16; // approx lines that fit
        int count = shm->mutex_events_count;
        if (count > MUTEX_EVENT_HISTORY) count = MUTEX_EVENT_HISTORY;
        if (count > max_lines) count = max_lines;

        // draw newest at bottom -> walk backwards from head-1
        int head = shm->mutex_events_head % MUTEX_EVENT_HISTORY;
        int line = 0;
        for (int i = 0; i < count; i++) {
            int idx = head - 1 - i;
            if (idx < 0) idx += MUTEX_EVENT_HISTORY;
            time_t ts = shm->mutex_events[idx].ts;
            pid_t pid = shm->mutex_events[idx].pid;
            int action = shm->mutex_events[idx].status;
            int ago = (int)(now - ts);
            const char *act = action ? "LOCK" : "UNLK";
            int y = hist_y + hist_height - 6 - (i * 16) - 14;
            DrawText(TextFormat("%3ds %s PID:%d", ago, act, (int)pid), panel_x + 10, y, 14, LIGHTGRAY);
            line++;
        }

        EndDrawing();
    }

    // Nettoyage
    UnloadTexture(tTable);
    UnloadTexture(tWheelStatic);
    UnloadTexture(tWheelSpin);
    UnloadTexture(tChip);
    shmdt(shm);
    CloseWindow();
    return 0;
}