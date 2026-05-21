#ifndef VIEW_H
#define VIEW_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include "model.h"

#define WIN_W            640
#define WIN_H            480
#define SCENE_X          30
#define SCENE_Y          30
#define SCENE_W          580
#define SCENE_H          326
#define BOX_X            50
#define BOX_Y            366
#define BOX_W            540
#define BOX_H            80
#define TEXT_PAD_X       10
#define TEXT_PAD_Y       10
#define HEART_SIZE       30
#define HEART_MARGIN     8
#define TYPEWRITER_MS    40   // ms per character

// Venetian blind settings
#define BLIND_COUNT      48    // number of horizontal bars
#define BLIND_SPEED      2     // pixels per step each bar grows
#define BLIND_STEP_MS    24    // ms between each step (higher = slower)

typedef enum {
    BLIND_NONE,        // no transition
    BLIND_ODD_CLOSE,   // step 1: odd bars growing to cover scene
    BLIND_EVEN_CLOSE,  // step 2: even bars fill the gaps
    BLIND_ODD_OPEN,    // step 3: odd bars shrinking to reveal new scene
    BLIND_EVEN_OPEN    // step 4: even bars finish the reveal
} BlindState;

typedef enum {
    VIEW_COMMAND_NONE,
    VIEW_COMMAND_SAVE,
    VIEW_COMMAND_LOAD,
    VIEW_COMMAND_BACK
} ViewCommand;

typedef enum {
    VIEW_SCREEN_MENU,
    VIEW_SCREEN_GAME,
    VIEW_SCREEN_SETTINGS
} ViewScreen;

typedef enum {
    VIEW_MENU_NONE,
    VIEW_MENU_NEW_GAME,
    VIEW_MENU_LOAD,
    VIEW_MENU_SETTINGS,
    VIEW_MENU_QUIT,
    VIEW_MENU_AUDIO,
    VIEW_MENU_BACK
} ViewMenuAction;

typedef struct {
    SDL_Window*   window;
    SDL_Renderer* renderer;

    SDL_Texture*  tex_windowbg;
    SDL_Texture*  tex_bg;
    SDL_Texture*  tex_sprite;
    SDL_Texture*  tex_box;
    SDL_Texture*  tex_heart;

    TTF_Font*     font;
    SDL_Color     text_color;

    // Typewriter
    char          speaker[64];
    char          full_text[512];
    int           visible_chars;
    Uint32        last_char_time;
    bool          typing_done;

    bool          choices_visible;
    char          choice_text[MAX_CHOICES][128];
    int           choice_count;
    int           choice_selected;

    // Venetian blind transition
    BlindState    blind_state;
    int           blind_h;           // current height of each bar (0 to bar_total_h)
    Uint32        blind_last_step;   // timestamp of last blind step
    char          current_bg[256];
    char          next_bg[256];      // queued bg, loaded when fully closed
    char          current_sprite[256];
    char          next_sprite[256];
    char          sprite_pos[32];
    char          next_sprite_pos[32];

    char          status_text[64];
    Uint32        status_until;

    ViewScreen    screen;
    int           menu_selected;
    bool          audio_enabled;
} View;

int  view_init(View* v, const char* font_path, int font_size);
void view_set_visuals(View* v, const char* bg_path, const char* sprite_path,
                      const char* sprite_pos);
void view_set_text(View* v, const char* speaker, const char* text);
void view_set_choices(View* v, const StoryLine* line, int selected);
void view_clear_choices(View* v);
void view_skip_typewriter(View* v);
bool view_is_typing(const View* v);
bool view_is_transitioning(const View* v);  // true while blind is animating
void view_set_status(View* v, const char* text);
ViewCommand view_command_at(int x, int y);
void view_set_screen(View* v, ViewScreen screen);
void view_set_menu_state(View* v, int selected, bool audio_enabled);
ViewMenuAction view_menu_action_at(const View* v, int x, int y);
void view_update(View* v);
void view_render(View* v);
void view_free(View* v);

#endif
