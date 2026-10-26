#include <stdio.h>
#include <stdlib.h>
#include <time.h> 
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h> 
#include <SDL2/SDL_image.h>
#include "./constants.h"

int score = 0;
int high_score = 0;
int one_up_awarded = 0;

int WINDOW_WIDTH;
int WINDOW_HEIGHT;
int SIZE;
int left_side;
int right_side;
int top_side;

int game_is_running = FALSE;
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

// Textures
SDL_Texture* playerTexture = NULL;
SDL_Texture* bigAlienTexture = NULL; 
SDL_Texture* alienTextures[3][2]; 

// Cached Static Text Textures 
SDL_Texture* text_title = NULL;
SDL_Texture* text_paused = NULL;
SDL_Texture* text_gameover = NULL;
SDL_Texture* text_restart = NULL;
SDL_Texture* text_press_enter = NULL;
SDL_Texture* text_pts30 = NULL;
SDL_Texture* text_pts20 = NULL;
SDL_Texture* text_pts10 = NULL;

TTF_Font* font = NULL;
int imgFlags;

int alive_count = 55;
int wait_time = 5000;

int move_left = 0;
int move_right = 0;
int shoot = 0;
int shootable = 1;
int last_frame_time = 0;
int last_alien_time = 0;
int last_shoot_time = 0;
int currentFrame = 0;   

int wave_base_speed = 0; 
int direction = 1;

float left;
float right;
float bottom;

// UFO Point Popup Variables
int ufo_score_display = 0;
Uint32 ufo_score_timer = 0;
float ufo_score_x = 0;
float ufo_score_y = 0;

// Player Death Variables
Uint32 death_timer = 0;

typedef enum {STATE_MENU, STATE_PLAYING, STATE_PLAYER_DEATH, STATE_PAUSED, STATE_GAMEOVER} GameState;
GameState current_state = STATE_MENU;

struct player {
    float x;
    float y;
    float width;
    float height;
    int speed;
    int lives;
} player;

struct bunker {
    float x;
    float y;
    float width;
    float height;
    int active;
} bunkers[4][4][5];

struct alien {
    int alive;
    float x;
    float y;
    float width;
    float height; 
    int type; 
} **enemy;

struct ufo {
    float x;
    float y;
    float width;
    float height;
    int speed;
    int active;
    int direction; 
    Uint32 next_spawn_time;
} big_alien;

struct bullet {
    float x;
    float y;
    float width;
    float height;
    int speed;
    int active;
} bullet, enemyBullet[3];


// --- FILE I/O FOR HIGH SCORES ---
void load_high_score() {
    FILE* file = fopen("highscore.txt", "r");
    if (file) {
        fscanf(file, "%d", &high_score);
        fclose(file);
    }
}

void save_high_score() {
    FILE* file = fopen("highscore.txt", "w");
    if (file) {
        fprintf(file, "%d", high_score);
        fclose(file);
    }
}

// --- HELPER FUNCTIONS ---
SDL_Texture* create_text_texture(const char* text, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void check_score_milestones(int added_points) {
    score += added_points;
    if (score > high_score) {
        high_score = score;
    }
    // 1-UP Mechanic
    if (score >= 1500 && !one_up_awarded) {
        player.lives++;
        one_up_awarded = 1;
    }
}

int initialize_window(void) {
    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) return FALSE;   

    SDL_DisplayMode displayMode;
    if(SDL_GetDesktopDisplayMode(0, &displayMode) != 0) return FALSE;
    
    WINDOW_HEIGHT = displayMode.h;
    WINDOW_WIDTH = displayMode.w;

    if(WINDOW_HEIGHT > WINDOW_WIDTH) { SIZE = WINDOW_WIDTH; }
    else { SIZE = WINDOW_HEIGHT; }

    SIZE = (SIZE / 90) * 90;

    left_side = WINDOW_WIDTH / 2 - SIZE / 2;
    right_side = WINDOW_WIDTH / 2 + SIZE / 2;
    top_side = WINDOW_HEIGHT / 2 - SIZE / 2;

    if (TTF_Init() == -1) return FALSE;
    font = TTF_OpenFont("vga857.ttf", 48);
    if (font == NULL) return FALSE;

    window = SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_BORDERLESS);
    if(!window) return FALSE;

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) return FALSE;

    imgFlags = IMG_INIT_PNG;
    if(!(IMG_Init(imgFlags) & imgFlags)) return FALSE;

    srand(time(NULL)); 
    load_high_score(); 
    return TRUE;
}

void start_wave(int full_reset) {
    if (full_reset) {
        score = 0;
        player.lives = 3;
        wave_base_speed = 0; 
        one_up_awarded = 0;
        
        for (int b = 0; b < 4; ++b) {
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if ((row == 0 && (col == 0 || col == 4)) || (row == 2 && col == 2) || (row == 3 && (col >= 1 && col <= 3))) {
                        bunkers[b][row][col].active = 0; 
                    } else {
                        bunkers[b][row][col].active = 1;
                    }
                }
            }
        }
    } else {
        wave_base_speed += 30; // Noticeable speed increase per wave
    }

    direction = 1;
    player.x = WINDOW_WIDTH / 2;
    
    bullet.active = 0;
    shootable = 1;
    for(int i=0; i<3; ++i) enemyBullet[i].active = 0;

    big_alien.active = 0;
    big_alien.width = SIZE / 10;
    big_alien.height = SIZE / 20;
    big_alien.y = SIZE / 10; 
    big_alien.speed = SIZE / 3;
    big_alien.next_spawn_time = SDL_GetTicks() + 10000 + (rand() % 10000); 

    int startx = left_side + SIZE / 90;
    int starty = WINDOW_HEIGHT / 5;

    for (int j = 0; j < 5; ++j) {
        for (int i = 0; i < 11; ++i) {
            enemy[j][i].x = startx + (i * (enemy[j][i].width + 15));
            enemy[j][i].y = starty + (j * (enemy[j][i].height + 10));
            enemy[j][i].alive = 1;
        }
    }
    alive_count = 55;
}

void setup() {
    player.width = WINDOW_WIDTH / 18; 
    player.height = WINDOW_HEIGHT / 18; 
    player.speed = WINDOW_WIDTH / 3;
    
    playerTexture = IMG_LoadTexture(renderer, "images/player.png");
    bigAlienTexture = IMG_LoadTexture(renderer, "images/alien.png"); 

    bullet.height = WINDOW_HEIGHT / 50;
    bullet.width = WINDOW_WIDTH / 200;
    bullet.speed = WINDOW_HEIGHT * 2 / 3;

    alienTextures[0][0] = IMG_LoadTexture(renderer, "images/alien_3.png");
    alienTextures[0][1] = IMG_LoadTexture(renderer, "images/alien_3-2.png");
    alienTextures[1][0] = IMG_LoadTexture(renderer, "images/alien_2.png");
    alienTextures[1][1] = IMG_LoadTexture(renderer, "images/alien_2-2.png");
    alienTextures[2][0] = IMG_LoadTexture(renderer, "images/alien_1.png");
    alienTextures[2][1] = IMG_LoadTexture(renderer, "images/alien_1-2.png");

    SDL_Color yellow = {255, 255, 0, 255};
    SDL_Color red = {255, 0, 0, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color green = {0, 255, 0, 255};
    
    text_title = create_text_texture("SPACE INVADERS", green); 
    text_paused = create_text_texture("PAUSED", yellow);
    text_gameover = create_text_texture("GAME OVER", red);
    text_restart = create_text_texture("PRESS R TO RESTART", white);
    text_press_enter = create_text_texture("PRESS ENTER TO PLAY", green);
    text_pts30 = create_text_texture(" = 30 PTS", white);
    text_pts20 = create_text_texture(" = 20 PTS", white);
    text_pts10 = create_text_texture(" = 10 PTS", white);

    float bunker_width = SIZE / 8;
    float piece_w = bunker_width / 5;
    float piece_h = piece_w;
    float start_y = WINDOW_HEIGHT/2 + SIZE/4; 
    float spacing = SIZE / 4; 
    float start_x = left_side + (spacing / 2) - (bunker_width / 2);

    for (int b = 0; b < 4; ++b) {
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 5; ++col) {
                bunkers[b][row][col].width = piece_w;
                bunkers[b][row][col].height = piece_h;
                bunkers[b][row][col].x = start_x + (b * spacing) + (col * piece_w);
                bunkers[b][row][col].y = start_y + (row * piece_h);
            }
        }
    }

    for(int i=0; i<3; ++i) {
        enemyBullet[i].height = WINDOW_HEIGHT/50;
        enemyBullet[i].width = WINDOW_WIDTH/200;   
        enemyBullet[i].speed = WINDOW_HEIGHT * 1/3;
    }

    enemy = (struct alien**)calloc(5, sizeof(struct alien*));
    for (int i = 0; i < 5; ++i) {
        enemy[i] = (struct alien*)calloc(11, sizeof(struct alien));
        for (int j = 0; j < 11; ++j) {
            enemy[i][j].height = WINDOW_HEIGHT / 30;
            enemy[i][j].width = WINDOW_WIDTH / 40;
            if (i == 0) enemy[i][j].type = 0;
            else if (i == 1 || i == 2) enemy[i][j].type = 1;
            else enemy[i][j].type = 2;
        }
    }

    player.y = SIZE - player.height - 10;
    start_wave(1); 
}

void process_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { game_is_running = 0; }
        else if (event.key.keysym.sym == SDLK_ESCAPE && current_state == STATE_MENU) { game_is_running = 0;}


        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_r) {
                save_high_score();
                start_wave(1);
                current_state = STATE_MENU;
            }
            else if (event.key.keysym.sym == SDLK_RETURN && current_state == STATE_MENU) {
                start_wave(1);
                current_state = STATE_PLAYING;
            }
            else if (event.key.keysym.sym == SDLK_ESCAPE && current_state == STATE_PLAYING) { current_state = STATE_PAUSED; }
            else if (event.key.keysym.sym == SDLK_ESCAPE && current_state == STATE_PAUSED) { current_state = STATE_PLAYING; }
            
            if (current_state == STATE_PLAYING) {
                if (event.key.keysym.sym == SDLK_LEFT) move_left = 1;
                else if (event.key.keysym.sym == SDLK_RIGHT) move_right = 1;
                if (event.key.keysym.sym == SDLK_SPACE) shoot = 1;
            }
        }
        
        if (event.type == SDL_KEYUP && current_state == STATE_PLAYING) {
            if (event.key.keysym.sym == SDLK_LEFT) move_left = 0;
            if (event.key.keysym.sym == SDLK_RIGHT) move_right = 0;
        }
    }
}

void update() {
    int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - last_frame_time);
    if(time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
        SDL_Delay(time_to_wait);
    }
    float delta_time = (SDL_GetTicks() - last_frame_time) / 1000.0f;
    last_frame_time = SDL_GetTicks();

    // --- PLAYER DEATH PAUSE LOGIC ---
    if (current_state == STATE_PLAYER_DEATH) {
        if (SDL_GetTicks() - death_timer > 1500) { // Freeze for 1.5 seconds
            if (player.lives <= 0) {
                current_state = STATE_GAMEOVER;
                save_high_score();
            } else {
                bullet.active = 0;
                shootable = 1;
                for(int i=0; i<3; ++i) enemyBullet[i].active = 0;
                player.x = WINDOW_WIDTH / 2;
                current_state = STATE_PLAYING;
            }
        }
        return; // Skip all other logic while exploding
    }

    if(current_state != STATE_PLAYING) return; 

    // Player Move
    if (move_left) player.x -= player.speed * delta_time;
    if (move_right) player.x += player.speed * delta_time;
    if (player.x < left_side) player.x = left_side;
    else if (player.x > right_side - player.width) player.x = right_side - player.width;

    // Shooting
    if (shootable && shoot) {
        bullet.active = 1;
        shootable = 0;
        shoot = 0;
        bullet.x = player.x + player.width / 2;
        bullet.y = player.y - bullet.height;
    }
    if (bullet.active) bullet.y -= bullet.speed * delta_time;
    if (bullet.y + bullet.height < 0) {
        bullet.active = 0;
        shootable = 1;
    }

    // --- BIG RED UFO LOGIC ---
    if (!big_alien.active) {
        if (SDL_GetTicks() >= big_alien.next_spawn_time) {
            big_alien.active = 1;
            if (rand() % 2 == 0) {
                big_alien.direction = 1;
                big_alien.x = left_side - big_alien.width;
            } else {
                big_alien.direction = -1;
                big_alien.x = right_side;
            }
        }
    } else {
        big_alien.x += big_alien.direction * big_alien.speed * delta_time;
        if ((big_alien.direction == 1 && big_alien.x > right_side) || 
            (big_alien.direction == -1 && big_alien.x + big_alien.width < left_side)) {
            big_alien.active = 0;
            big_alien.next_spawn_time = SDL_GetTicks() + 10000 + (rand() % 15000); 
        }

        if (bullet.active) {
            SDL_Rect rect_bullet = {(int)bullet.x, (int)bullet.y, (int)bullet.width, (int)bullet.height};
            SDL_Rect rect_ufo = {(int)big_alien.x, (int)big_alien.y, (int)big_alien.width, (int)big_alien.height};
            if (SDL_HasIntersection(&rect_bullet, &rect_ufo)) {
                bullet.active = 0;
                shootable = 1;
                big_alien.active = 0;
                big_alien.next_spawn_time = SDL_GetTicks() + 10000 + (rand() % 15000);
                
                int possible_scores[] = {50, 100, 150, 300};
                int awarded = possible_scores[rand() % 4];
                check_score_milestones(awarded);

                // Setup Floating Points Visual
                ufo_score_display = awarded;
                ufo_score_timer = SDL_GetTicks() + 1000;
                ufo_score_x = big_alien.x;
                ufo_score_y = big_alien.y;
            }
        }
    }

    // Bullet vs Alien Grid Collision
    if (bullet.active) {
        SDL_Rect rect_bullet = {(int)bullet.x, (int)bullet.y, (int)bullet.width, (int)bullet.height};
        for (int j = 0; j < 5; ++j) {
            for (int i = 0; i < 11; ++i) {
                if (enemy[j][i].alive) {
                    SDL_Rect rect_alien = {(int)enemy[j][i].x, (int)enemy[j][i].y, (int)enemy[j][i].width, (int)enemy[j][i].height};
                    if (SDL_HasIntersection(&rect_bullet, &rect_alien)) {
                        if (enemy[j][i].type == 2) check_score_milestones(10);
                        else if (enemy[j][i].type == 1) check_score_milestones(20);
                        else if (enemy[j][i].type == 0) check_score_milestones(30); 
                        
                        bullet.active = 0;
                        shootable = 1;
                        enemy[j][i].alive = 0;
                        break;
                    }
                }
            }
        }
    }
    
    // Bounding Box & Alive Count
    left = WINDOW_WIDTH;
    right = 0;
    bottom = 0;
    alive_count = 0;

    for (int j = 0; j < 5; ++j) {
        for (int i = 0; i < 11; ++i) {
            if (enemy[j][i].alive) {
                alive_count++;
                if (enemy[j][i].x + enemy[j][i].width > right) right = enemy[j][i].x + enemy[j][i].width;
                if (enemy[j][i].x < left) left = enemy[j][i].x;
                if (enemy[j][i].y + enemy[j][i].height > bottom) bottom = enemy[j][i].y + enemy[j][i].height;
            }
        }
    }

    if (alive_count == 0) {
        start_wave(0); 
        return; 
    } else if (bottom >= player.y) {
        current_state = STATE_GAMEOVER;
        save_high_score();
    }

    // --- ALIEN MOVEMENT (THE SPACE INVADERS EFFECT) ---
    int hit_edge = 0;
    int step_size = SIZE / 90;
    
    // Speed Formula: Fewer aliens = shorter delay. At 1 alien, delay is ~15ms!
    int current_delay = (alive_count * 12) + 20 - wave_base_speed; 
    if (current_delay < 15) current_delay = 15; // Hard speed limit so they don't teleport

    if (SDL_GetTicks() - last_alien_time >= current_delay) {
        last_alien_time = SDL_GetTicks();
        currentFrame = !currentFrame; 

        if (direction && right + step_size >= right_side) { hit_edge = 1; }
        else if (direction == 0 && left <= left_side) { hit_edge = 1; }

        for (int j = 0; j < 5; ++j) {
            for (int i = 0; i < 11; ++i) {
                if (enemy[j][i].alive) {
                    if (hit_edge) enemy[j][i].y += 50;
                    else {
                        if (direction) enemy[j][i].x += step_size;
                        else enemy[j][i].x -= step_size;
                    }
                    
                    // --- ALIENS EATING BUNKERS ---
                    SDL_Rect rect_alien = {(int)enemy[j][i].x, (int)enemy[j][i].y, (int)enemy[j][i].width, (int)enemy[j][i].height};
                    for (int b = 0; b < 4; ++b) {
                        for (int row = 0; row < 4; ++row) {
                            for (int col = 0; col < 5; ++col) {
                                if (bunkers[b][row][col].active) {
                                    SDL_Rect rect_bunker = {(int)bunkers[b][row][col].x, (int)bunkers[b][row][col].y, (int)bunkers[b][row][col].width, (int)bunkers[b][row][col].height};
                                    if (SDL_HasIntersection(&rect_alien, &rect_bunker)) {
                                        bunkers[b][row][col].active = 0;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (hit_edge) direction = !direction;
    }

    // Enemy Shooting
    if (SDL_GetTicks() - last_shoot_time >= wait_time) {
        if (rand() % 2) {
            last_shoot_time = SDL_GetTicks();
            int bullets_to_shoot = rand() % 4;
            for (int j = 0; j < 3 && bullets_to_shoot > 0; ++j) {
                if (enemyBullet[j].active) continue;
                int randcoll = rand() % 11;
                int shooter_row = -1;
                for (int i = 4; i >= 0; --i) {
                    if (enemy[i][randcoll].alive) {
                        shooter_row = i;
                        break;
                    }
                }
                if (shooter_row != -1) {
                    enemyBullet[j].y = enemy[shooter_row][randcoll].y + enemy[shooter_row][randcoll].height;
                    enemyBullet[j].x = enemy[shooter_row][randcoll].x + enemy[shooter_row][randcoll].width/2 - enemyBullet[j].width/2;
                    enemyBullet[j].active = 1;
                    bullets_to_shoot--;
                }
            }
            
            int max_wait = (alive_count * 40) - wave_base_speed;
            if (max_wait < 500) max_wait = 500;
            wait_time = rand() % max_wait;
        }
    }

    for (int i = 0; i < 3; ++i) {
        if (enemyBullet[i].active) {
            enemyBullet[i].y += enemyBullet[i].speed * delta_time;
            
            SDL_Rect rect_enemybullet = {(int)enemyBullet[i].x, (int)enemyBullet[i].y, (int)enemyBullet[i].width, (int)enemyBullet[i].height};
            SDL_Rect rect_player = {(int)player.x, (int)player.y, (int)player.width, (int)player.height};
            
            if (bullet.active) {
                SDL_Rect rect_bullet = {(int)bullet.x, (int)bullet.y, (int)bullet.width, (int)bullet.height};
                if (SDL_HasIntersection(&rect_enemybullet, &rect_bullet)) {
                    bullet.active = 0;
                    shootable = 1;
                    enemyBullet[i].active = 0;
                    continue;
                }
            }

            // Enemy Bullet hits Player -> Trigger Death State
            if (SDL_HasIntersection(&rect_enemybullet, &rect_player)) {
                enemyBullet[i].active = 0;
                player.lives--;
                current_state = STATE_PLAYER_DEATH;
                death_timer = SDL_GetTicks();
                return; 
            }
            
            if (enemyBullet[i].y > WINDOW_HEIGHT) enemyBullet[i].active = 0;

            for (int b = 0; b < 4 && enemyBullet[i].active; ++b) {
                for (int row = 0; row < 4 && enemyBullet[i].active; ++row) {
                    for (int col = 0; col < 5 && enemyBullet[i].active; ++col) {
                        if (bunkers[b][row][col].active) {
                            SDL_Rect rect_bunker = {(int)bunkers[b][row][col].x, (int)bunkers[b][row][col].y, (int)bunkers[b][row][col].width, (int)bunkers[b][row][col].height};
                            if (SDL_HasIntersection(&rect_enemybullet, &rect_bunker)) {
                                bunkers[b][row][col].active = 0;
                                enemyBullet[i].active = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    if (bullet.active) {
        SDL_Rect rect_bullet = {(int)bullet.x, (int)bullet.y, (int)bullet.width, (int)bullet.height};
        for (int b = 0; b < 4 && bullet.active; ++b) {
            for (int row = 0; row < 4 && bullet.active; ++row) {
                for (int col = 0; col < 5 && bullet.active; ++col) {
                    if (bunkers[b][row][col].active) {
                        SDL_Rect rect_bunker = {(int)bunkers[b][row][col].x, (int)bunkers[b][row][col].y, (int)bunkers[b][row][col].width, (int)bunkers[b][row][col].height};
                        if (SDL_HasIntersection(&rect_bullet, &rect_bunker)) {
                            bunkers[b][row][col].active = 0;
                            bullet.active = 0;
                            shootable = 1;
                        }
                    }
                }
            }
        }
    }
}

void render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Rect cage_rect = {(int)(WINDOW_WIDTH/2 - SIZE/2), (int)top_side, (int)SIZE, (int)SIZE};
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderFillRect(renderer, &cage_rect);

    // Reusable text styling
    SDL_Color white = {255, 255, 255, 255}; 
    SDL_Color red = {255, 0, 0, 255}; 
    
    // Dynamic High Score String
    char hi_string[32];
    snprintf(hi_string, sizeof(hi_string), "HI-SCORE: %04d", high_score);
    SDL_Texture* hi_tex = create_text_texture(hi_string, white);

    if (current_state == STATE_MENU) {
        SDL_Rect hi_rect = {WINDOW_WIDTH/2 - SIZE/2 + 20, WINDOW_HEIGHT/100, 250, 50}; 
        SDL_RenderCopy(renderer, hi_tex, NULL, &hi_rect);

        SDL_Rect title_rect = {WINDOW_WIDTH / 2 - (SIZE / 3), WINDOW_HEIGHT / 8, (SIZE * 2) / 3, SIZE / 6};
        SDL_RenderCopy(renderer, text_title, NULL, &title_rect);
        
        int start_y = WINDOW_HEIGHT / 2 - 80;
        int spacing = WINDOW_HEIGHT / 12; 
        int center_x = WINDOW_WIDTH / 2 - 120; 

        SDL_Rect a3_rect = {center_x, start_y, 50, 40};
        SDL_Rect pts3_rect = {center_x + 70, start_y + 5, 120, 30};
        SDL_RenderCopy(renderer, alienTextures[0][0], NULL, &a3_rect);
        SDL_RenderCopy(renderer, text_pts30, NULL, &pts3_rect);

        SDL_Rect a2_rect = {center_x, start_y + spacing, 50, 40};
        SDL_Rect pts2_rect = {center_x + 70, start_y + spacing + 5, 120, 30};
        SDL_RenderCopy(renderer, alienTextures[1][0], NULL, &a2_rect);
        SDL_RenderCopy(renderer, text_pts20, NULL, &pts2_rect);

        SDL_Rect a1_rect = {center_x, start_y + spacing * 2, 50, 40};
        SDL_Rect pts1_rect = {center_x + 70, start_y + spacing * 2 + 5, 120, 30};
        SDL_RenderCopy(renderer, alienTextures[2][0], NULL, &a1_rect);
        SDL_RenderCopy(renderer, text_pts10, NULL, &pts1_rect);

        SDL_Rect play_rect = {WINDOW_WIDTH / 2 - 200, WINDOW_HEIGHT - (WINDOW_HEIGHT / 4), 400, 60};
        SDL_RenderCopy(renderer, text_press_enter, NULL, &play_rect);
    }
    else {
        // Player Ship (With Death Flicker)
        SDL_Rect player_rect = {(int)player.x, (int)player.y, (int)player.width, (int)player.height};
        if (current_state == STATE_PLAYER_DEATH) {
            // Rapidly flicker player Red/White every 100ms
            if ((SDL_GetTicks() / 100) % 2 == 0) {
                SDL_SetTextureColorMod(playerTexture, 255, 0, 0); 
                SDL_RenderCopy(renderer, playerTexture, NULL, &player_rect);
                SDL_SetTextureColorMod(playerTexture, 255, 255, 255); 
            }
        } else {
            SDL_RenderCopy(renderer, playerTexture, NULL, &player_rect);
        }

        // UFO & UFO Floating Points
        if (big_alien.active && bigAlienTexture != NULL) {
            SDL_Rect ufo_rect = {(int)big_alien.x, (int)big_alien.y, (int)big_alien.width, (int)big_alien.height};
            SDL_RenderCopy(renderer, bigAlienTexture, NULL, &ufo_rect);
        } else if (ufo_score_display > 0 && SDL_GetTicks() < ufo_score_timer) {
            char ufo_pts_str[8];
            snprintf(ufo_pts_str, sizeof(ufo_pts_str), "%d", ufo_score_display);
            SDL_Texture* pop_tex = create_text_texture(ufo_pts_str, red);
            SDL_Rect pop_rect = {(int)ufo_score_x, (int)ufo_score_y, 60, 40};
            SDL_RenderCopy(renderer, pop_tex, NULL, &pop_rect);
            SDL_DestroyTexture(pop_tex);
        }

        for (int j = 0; j < 5; ++j) {
            for (int i = 0; i < 11; ++i) {
                if (enemy[j][i].alive) {
                    SDL_Rect alien_rect = {(int)enemy[j][i].x, (int)enemy[j][i].y, (int)enemy[j][i].width, (int)enemy[j][i].height};
                    SDL_RenderCopy(renderer, alienTextures[enemy[j][i].type][currentFrame], NULL, &alien_rect);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        if (bullet.active) {
            SDL_Rect bullet_rect = {(int)bullet.x, (int)bullet.y, (int)bullet.width, (int)bullet.height};
            SDL_RenderFillRect(renderer, &bullet_rect);
        }
        for (int i = 0; i < 3; i++) {
            if (enemyBullet[i].active) {
                SDL_Rect enemyBullet_rect = {(int)enemyBullet[i].x, (int)enemyBullet[i].y, (int)enemyBullet[i].width, (int)enemyBullet[i].height};
                SDL_RenderFillRect(renderer, &enemyBullet_rect);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        for (int b = 0; b < 4; ++b) {
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (bunkers[b][row][col].active) {
                        SDL_Rect bunker_rect = {(int)bunkers[b][row][col].x, (int)bunkers[b][row][col].y, (int)bunkers[b][row][col].width, (int)bunkers[b][row][col].height};
                        SDL_RenderFillRect(renderer, &bunker_rect);
                    }
                }
            }
        }

        char lives_string[16];
        snprintf(lives_string, sizeof(lives_string), "LIVES: %d", player.lives);
        char score_string[32];
        snprintf(score_string, sizeof(score_string), "SCORE: %04d", score);

        SDL_Texture* score_tex = create_text_texture(score_string, white);
        SDL_Rect score_rect = {WINDOW_WIDTH/2 - SIZE/2 + 20, WINDOW_HEIGHT/100, 200, 50};
        SDL_RenderCopy(renderer, score_tex, NULL, &score_rect);
        SDL_DestroyTexture(score_tex);

        SDL_Texture* lives_tex = create_text_texture(lives_string, white);
        SDL_Rect lives_rect = {WINDOW_WIDTH/2 + SIZE/2 - 200, WINDOW_HEIGHT/100, 180, 50};
        SDL_RenderCopy(renderer, lives_tex, NULL, &lives_rect);
        SDL_DestroyTexture(lives_tex);
        
        // Draw the top-center Hi-Score during gameplay too
        SDL_Rect top_hi_rect = {WINDOW_WIDTH/2 - 125, WINDOW_HEIGHT/100, 250, 50};
        SDL_RenderCopy(renderer, hi_tex, NULL, &top_hi_rect);

        if (current_state == STATE_PAUSED) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); 
            SDL_RenderFillRect(renderer, NULL); 
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); 

            SDL_Rect pause_rect = {WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2 - 50, 300, 100};
            SDL_RenderCopy(renderer, text_paused, NULL, &pause_rect);
        }
        else if (current_state == STATE_GAMEOVER) {
            SDL_Rect go_rect = {WINDOW_WIDTH / 2 - 200, WINDOW_HEIGHT / 3, 400, 100};
            SDL_Rect restart_rect = {WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2 + 100, 300, 50};
            SDL_RenderCopy(renderer, text_gameover, NULL, &go_rect);
            SDL_RenderCopy(renderer, text_restart, NULL, &restart_rect);
        }
    }
    
    SDL_DestroyTexture(hi_tex); 
    SDL_RenderPresent(renderer);
}

void destroy_window() {
    save_high_score(); // Final save before closing

    for(int i = 0; i < 5; ++i) { free(enemy[i]); }
    free(enemy);
    
    SDL_DestroyTexture(playerTexture);
    if(bigAlienTexture != NULL) SDL_DestroyTexture(bigAlienTexture);

    for(int i = 0; i < 3; i++) {
        SDL_DestroyTexture(alienTextures[i][0]);
        SDL_DestroyTexture(alienTextures[i][1]);
    }

    SDL_DestroyTexture(text_title);
    SDL_DestroyTexture(text_paused);
    SDL_DestroyTexture(text_gameover);
    SDL_DestroyTexture(text_restart);
    SDL_DestroyTexture(text_press_enter);
    SDL_DestroyTexture(text_pts10);
    SDL_DestroyTexture(text_pts20);
    SDL_DestroyTexture(text_pts30);

    IMG_Quit();
    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    game_is_running = initialize_window();
    setup();

    while(game_is_running){
        process_input();
        update();
        render();
    }
    
    destroy_window();
    return 0;
}