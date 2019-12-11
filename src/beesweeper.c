#include<SDL2/SDL.h>
#include<SDL2/SDL2_gfxPrimitives.h>
#include<SDL2/SDL_ttf.h>
#include<SDL2/SDL_image.h>
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<stdbool.h>
#include<time.h>

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 620

#define BEE_COUNT 14

#define RES_PATH "res/"
#define FONT RES_PATH "FreeSans.ttf"
#define BEE_IMG RES_PATH "bee.png"

/* TODO: this may be changed dynamically when the screen is resized */
#define HEXAGON_RADIUS 40
#define HEXAGON_EDGE_SIZE 4
#define HEXAGON_XDISTANCE (HEXAGON_RADIUS*cos(M_PI/6) - HEXAGON_EDGE_SIZE/2)
#define HEXAGON_YDISTANCE ((3*HEXAGON_RADIUS)/2-HEXAGON_EDGE_SIZE/2)
#define HEXAGON_BOARD_X_OFFSET 75 /* pixels from the screen edge */
#define HEXAGON_BOARD_Y_OFFSET 100
#define HEXAGON_X_TOTAL_NUM 12 /* total number of hexagons along x & y axis */
#define HEXAGON_Y_TOTAL_NUM 8

SDL_Surface *beeimage;
SDL_Texture *beetexture;

TTF_Font *font;

typedef struct {
        int x;
        int y;
} Point;

typedef struct {
        Point coord;
        int   radius;
        bool  show;
        bool  hovered;
        bool  bee;
        int   neighbours;
} Hexagon;

typedef struct {
        Hexagon **honeycomb;
        int cells_shown;
        bool found_bee;
} Game;

void draw_hexagon(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
        Sint16 vx[6];
        Sint16 vy[6];
        double angle = M_PI / 2;
        int i;

        for (i=0; i<6; i++) {
                angle += M_PI/3;
                vx[i] = (Sint16) cx + radius * cos(angle);
                vy[i] = (Sint16) cy + radius * sin(angle);
        }

        filledPolygonRGBA(renderer, vx, vy, 6, color.r, color.g, color.b, color.a);
}

void draw_bee_count(SDL_Renderer *renderer, Hexagon *hexagon) {
        char msg[4];
        snprintf(msg, 4, "%d", hexagon->neighbours);
        SDL_Color TextColor = {0, 0, 0};
        SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, msg, TextColor);
        SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(Message, NULL, NULL, &texW, &texH);
        SDL_Rect Message_rect = { hexagon->coord.x - 6, hexagon->coord.y - 10, texW , texH };

        SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
        SDL_DestroyTexture(Message);
        SDL_FreeSurface(surfaceMessage);
}

void draw_honeycomb_cell(SDL_Renderer *renderer, Hexagon *hexagon) {
        SDL_Color edge_color = {0x85, 0x54, 0x08, 0xFF};
        SDL_Color inner_color_normal = {0xFF, 0xB9, 0x14, 0xFF};
        SDL_Color inner_color_hovered = {0xFF, 0xAF, 0x44, 0xFF};
        SDL_Color show_color = {0xFF, 0xCD, 0x58, 0xFF};

        draw_hexagon(renderer, hexagon->coord.x, hexagon->coord.y, hexagon->radius, edge_color);

        if (hexagon->show) {
                draw_hexagon(renderer, hexagon->coord.x, hexagon->coord.y, hexagon->radius - HEXAGON_EDGE_SIZE, show_color);
                if (hexagon->bee) {
                        SDL_Rect Message_rect = { hexagon->coord.x - HEXAGON_RADIUS/2, hexagon->coord.y - HEXAGON_RADIUS/2, HEXAGON_RADIUS, HEXAGON_RADIUS};
                        SDL_RenderCopy(renderer, beetexture, NULL, &Message_rect);
                }
                else if (hexagon->neighbours > 0) {
                        draw_bee_count(renderer, hexagon);
                }
        }
        else if (hexagon->hovered) {
                draw_hexagon(renderer, hexagon->coord.x, hexagon->coord.y, hexagon->radius - HEXAGON_EDGE_SIZE, inner_color_hovered);
        }
        else {
                draw_hexagon(renderer, hexagon->coord.x, hexagon->coord.y, hexagon->radius - HEXAGON_EDGE_SIZE, inner_color_normal);
        }
}

void draw_hexagon_board(SDL_Renderer *renderer, Hexagon **hexagon_board) {
        SDL_SetRenderDrawColor(renderer, 250, 222, 125, 255);
        SDL_RenderClear(renderer);

        int x, y;

        for (x = 0; x < HEXAGON_X_TOTAL_NUM; x++) {
                for (y = 0; y < HEXAGON_Y_TOTAL_NUM; y++) {
                        draw_honeycomb_cell(renderer, &hexagon_board[x][y]);
                }
        }
}

bool point_is_in_hexagon(Point point, Hexagon hexagon) {
    /* move point to 1st quadrant relative to the hegagon's origo */
    point.x = abs(point.x - hexagon.coord.x);
    point.y = abs(point.y - hexagon.coord.y);
    /* check if point is inside the hexagon's region */
    return point.x <= hexagon.radius*sqrt(3)/2 && point.y <= -1/sqrt(3)*point.x + hexagon.radius;
}

void update_hover_status (int px, int py, Hexagon **hexagons) {
        Point point = (Point) {px, py};
        int x, y;

        for (x = 0; x < HEXAGON_X_TOTAL_NUM; x++) {
                for (y = 0; y < HEXAGON_Y_TOTAL_NUM; y++) {
                        hexagons[x][y].hovered = point_is_in_hexagon(point, hexagons[x][y]);
                }
        }
}

bool is_in_bounds(Point coord) {
        return coord.x >= 0 && coord.x < HEXAGON_X_TOTAL_NUM && coord.y >= 0 && coord.y < HEXAGON_Y_TOTAL_NUM;
}

/* Hexagons need 3d coordinates, but since we are in a 2d plane only 2 axis are needed.
   The first will be for the x coordinate, the second a slanted y coordinate pointing SW. */
Point transform_xy_to_hexcoord(Point coord) {
        coord.x += (coord.y+1)/2;
        return coord;
}

Point transform_hexcoord_to_xy(Point coord) {
        coord.x -= (coord.y+1)/2;
        return coord;
}


Hexagon **create_hexagon_board(int width, int height) {
        int x, y;
        Hexagon **hexagon_board;


        hexagon_board = (Hexagon **) malloc(sizeof(Hexagon *) * width);
        for (x = 0; x < width; x++) {
                hexagon_board[x] = (Hexagon *) malloc(height * sizeof(Hexagon));
        }

        /* initialise honeycomb cells */
        for (x = 0; x < width; x++) {
                for (y = 0; y < height; y++) {
                        /* coordinates for where to draw the hexagons */
                        hexagon_board[x][y].coord.x = HEXAGON_BOARD_X_OFFSET + HEXAGON_XDISTANCE * 2 * x + HEXAGON_XDISTANCE * (y%2);
                        hexagon_board[x][y].coord.y = HEXAGON_BOARD_Y_OFFSET + HEXAGON_YDISTANCE * y; 
                        hexagon_board[x][y].radius = HEXAGON_RADIUS;
                        hexagon_board[x][y].neighbours = 0;
                        hexagon_board[x][y].show = false;
                        hexagon_board[x][y].hovered = false;
                        hexagon_board[x][y].bee = false;
                }
        }

        /* populate bees */
        for (int i = 0; i < BEE_COUNT; i++) {
                x = rand() % HEXAGON_X_TOTAL_NUM;
                y = rand() % HEXAGON_Y_TOTAL_NUM;
                if (hexagon_board[x][y].bee) {
                        i--;
                        continue;
                }
                hexagon_board[x][y].bee = true;
        }

        /* neighbour directions specified in hex coordinations */
        Point directions[6] = {
                { -1, -1 }, { 0, -1 },
                { -1, 0 }, { 1, 0 },
                { 0, 1 }, { 1, 1 } };

        /* update neighbouring bee count */
        for (x = 0; x < width; x++) {
                for (y = 0; y < height; y++) {
                        for (int i = 0; i < 6; i++) {
                                Point coord = { x, y };
                                coord = transform_xy_to_hexcoord(coord);
                                coord.x += directions[i].x;
                                coord.y += directions[i].y;
                                coord = transform_hexcoord_to_xy(coord);
                                if (is_in_bounds(coord)) {
                                        hexagon_board[x][y].neighbours += hexagon_board[coord.x][coord.y].bee;
                                }
                        }
                }
        }

        return hexagon_board;
}

void sweep (int startx, int starty, Game *game) {
        /* neighbour directions specified in hex coordinations */
        Point directions[6] = {
                { -1, -1 }, { 0, -1 },
                { -1, 0 }, { 1, 0 },
                { 0, 1 }, { 1, 1 } };

        if (!game->honeycomb[startx][starty].show) {
                game->honeycomb[startx][starty].show = true;
                game->cells_shown++;
        }

        if (game->honeycomb[startx][starty].neighbours > 0) { return; }

        for (int i = 0; i < 6; i++) {
                Point coord = { startx, starty };
                coord = transform_xy_to_hexcoord(coord);
                coord.x += directions[i].x;
                coord.y += directions[i].y;
                coord = transform_hexcoord_to_xy(coord);
                if (is_in_bounds(coord) && game->honeycomb[coord.x][coord.y].bee == false && game->honeycomb[coord.x][coord.y].show == false) {
                        game->honeycomb[coord.x][coord.y].show = true;
                        game->cells_shown++;
                        if (game->honeycomb[coord.x][coord.y].neighbours == 0) {
                                sweep(coord.x, coord.y, game);
                        }
                }
        }
}

void reveal_all(Hexagon **hexagon_board) {
        int x, y;
        for (x = 0; x < HEXAGON_X_TOTAL_NUM; x++) {
                for (y = 0; y < HEXAGON_Y_TOTAL_NUM; y++) {
                        hexagon_board[x][y].show = true;
                }
        }
}

void handle_click(int x, int y, Game *game) { // Hexagon **hexagon_board) {
        Point point = (Point) {x, y};
        int i, j;

        for (i = 0; i < HEXAGON_X_TOTAL_NUM; i++) {
                for (j = 0; j < HEXAGON_Y_TOTAL_NUM; j++) {
                        if (point_is_in_hexagon(point, game->honeycomb[i][j])) {
                                if (game->honeycomb[i][j].bee) {
                                        game->honeycomb[i][j].show = true;
                                        game->found_bee = true;
                                }
                                else
                                        sweep(i, j, game);
                                return;
                        }
                }
        }
}

void display_msg (SDL_Renderer *renderer, TTF_Font *font, char *msg) {
        SDL_Color TextColor = {0, 0, 0};
        SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, msg, TextColor);
        SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(Message, NULL, NULL, &texW, &texH);
        SDL_Rect Message_rect = { 20, 20, texW, texH };

        SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
        SDL_DestroyTexture(Message);
        SDL_FreeSurface(surfaceMessage);
}


void free_hexagon_board(Hexagon **hexagon_board) {
        int x;
        for (x = 0; x < HEXAGON_X_TOTAL_NUM; x++) {
                free(hexagon_board[x]);
        }
        free(hexagon_board);
}

void new_game(Game *game) {
        if (game->honeycomb) {
                free_hexagon_board(game->honeycomb);
        }
        game->honeycomb = create_hexagon_board(HEXAGON_X_TOTAL_NUM, HEXAGON_Y_TOTAL_NUM);
        game->cells_shown = 0;
        game->found_bee = false;
}

void game_loop (SDL_Renderer *renderer) {
        SDL_Event event;
        bool quit = false;
        int x, y;
        Game game = {0};

        new_game(&game);

        font = TTF_OpenFont(FONT, 24);
        if (!font) {
                fprintf(stderr, "Error: could not open " FONT "\n");
        }

        beeimage = IMG_Load(BEE_IMG);
        beetexture = SDL_CreateTextureFromSurface(renderer, beeimage);

        while(!quit){
            while (SDL_PollEvent(&event)){
                        if (game.found_bee) {
                                draw_hexagon_board(renderer, game.honeycomb);
                                display_msg(renderer, font, "You got stung! Press Enter to try a new honeycomb.");
                        }
                        else if (game.cells_shown + BEE_COUNT == HEXAGON_X_TOTAL_NUM * HEXAGON_Y_TOTAL_NUM) {
                                reveal_all(game.honeycomb);
                                draw_hexagon_board(renderer, game.honeycomb);
                                display_msg(renderer, font, "You sweeped 'em all! Press Enter to try a new honeycomb.");
                        }
                        else 
                                draw_hexagon_board(renderer, game.honeycomb);

                        SDL_RenderPresent(renderer);

                        switch(event.type) {
                                case SDL_QUIT:
                                        quit = true;
                                        break;
                                case SDL_KEYDOWN:
                                        switch( event.key.keysym.sym ){
                                                case SDLK_q:
                                                        puts("bye!");
                                                        quit = true;
                                                        break;
                                                case SDLK_RETURN:
                                                        new_game(&game);
                                                default:
                                                        break;
                                        }
                                        break;
                                case SDL_MOUSEBUTTONDOWN:
                                        SDL_GetMouseState(&x, &y);
                                        if (!game.found_bee)
                                                handle_click(x, y, &game);
                                        break;
                                case SDL_MOUSEMOTION:
                                        SDL_GetMouseState(&x, &y);
                                        update_hover_status(x, y, game.honeycomb);
                                        break;
                        }
                }
        }
        free_hexagon_board(game.honeycomb);
        TTF_CloseFont(font);
}


int main (int argc, char **agrv) {
        SDL_Window *window = NULL;
        SDL_Renderer *renderer = NULL;

        /* Setup SDL */
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                printf("Could not init: %s\n", SDL_GetError());
                return 1;
        }
        window = SDL_CreateWindow("Beesweeper",
                                    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (window == NULL) {
                printf("Window could not be created: %s\n", SDL_GetError());
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == NULL) {
                printf("Renderer error: %s\n", SDL_GetError());
        }
        TTF_Init();
        IMG_Init(IMG_INIT_PNG);

        srand(time(NULL));

        game_loop(renderer);

        TTF_Quit();
        IMG_Quit();
        SDL_DestroyWindow(window);
        SDL_DestroyRenderer(renderer);
        SDL_Quit();

        return 0;
}
