#include "view.h"
#include <stdio.h>
#include <string.h>

// ── Editable UI palette ──────────────────────
// Change these values to recolor the PC-98 style menu, choices, and command buttons.
static const SDL_Color UI_PANEL_FILL      = {244, 232, 180, 255};
static const SDL_Color UI_BUTTON_FILL     = {76, 13, 22, 242};
static const SDL_Color UI_BUTTON_SELECTED = {122, 24, 34, 244};
static const SDL_Color UI_BORDER          = {174, 68, 62, 255};
static const SDL_Color UI_TEXT            = {255, 235, 218, 255};
static const SDL_Color UI_TEXT_HOT        = {255, 210, 118, 255};
static const SDL_Color UI_STATUS_TEXT     = {255, 226, 190, 255};
static const SDL_Color UI_FALLBACK_BG     = {30, 24, 58, 255};
static const SDL_Color UI_PATREON_FILL    = {255, 103, 30, 250};
static const SDL_Color UI_PATREON_HOT     = {255, 128, 48, 255};
static const SDL_Color UI_PATREON_BORDER  = {255, 224, 182, 255};
static const SDL_Color UI_PATREON_TEXT    = {255, 255, 255, 255};

// ── Helpers ──────────────────────────────────

static void copy_text(char* dst, size_t dst_size, const char* src) {
    if (dst_size == 0) return;
    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static int blind_max_h(void) {
    return (SCENE_H + BLIND_COUNT - 1) / BLIND_COUNT;
}

static void set_draw_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

// ── Asset loaders ────────────────────────────

static SDL_Texture* load_bmp(SDL_Renderer* r, const char* path) {
    SDL_Surface* s = SDL_LoadBMP(path);
    if (!s) { fprintf(stderr, "[View] Cannot load BMP: %s\n", path); return NULL; }
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    return t;
}

static SDL_Texture* load_image(SDL_Renderer* r, const char* path) {
    SDL_Texture* t = IMG_LoadTexture(r, path);
    if (!t) fprintf(stderr, "[View] Cannot load image: %s (%s)\n", path, IMG_GetError());
    return t;
}

static bool is_bmp_path(const char* path) {
    const char* ext = strrchr(path, '.');
    return ext && strcmp(ext, ".bmp") == 0;
}

static SDL_Texture* load_texture(SDL_Renderer* r, const char* path) {
    SDL_Texture* t = IMG_LoadTexture(r, path);
    if (t) return t;

    if (is_bmp_path(path))
        return load_bmp(r, path);

    fprintf(stderr, "[View] Cannot load image: %s (%s)\n", path, IMG_GetError());
    return NULL;
}

// ── Init ─────────────────────────────────────

int view_init(View* v, const char* font_path, int font_size) {
    v->window = SDL_CreateWindow(
        "A cat in girls dormitory",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_RESIZABLE
    );
    if (!v->window) { fprintf(stderr, "[View] Window: %s\n", SDL_GetError()); return 1; }

    v->renderer = SDL_CreateRenderer(v->window, -1, SDL_RENDERER_ACCELERATED);
    if (!v->renderer)
        v->renderer = SDL_CreateRenderer(v->window, -1, SDL_RENDERER_SOFTWARE);
    if (!v->renderer) { fprintf(stderr, "[View] Renderer: %s\n", SDL_GetError()); return 1; }

    SDL_SetWindowMinimumSize(v->window, WIN_W / 2, WIN_H / 2);
    SDL_RenderSetIntegerScale(v->renderer, SDL_FALSE);
    SDL_RenderSetLogicalSize(v->renderer, WIN_W, WIN_H);

    v->tex_windowbg = load_texture(v->renderer, "assets/windowbg.avif");
    v->tex_box      = load_bmp(v->renderer, "assets/textbox.bmp");
    v->tex_heart    = load_image(v->renderer, "assets/heart.png");
    v->tex_menubg   = load_image(v->renderer, "assets/black.avif");
    v->tex_bg       = NULL;
    v->tex_sprite   = NULL;
    v->tex_next_sprite = NULL;

    v->font = TTF_OpenFont(font_path, font_size);
    if (!v->font) { fprintf(stderr, "[View] Font: %s\n", TTF_GetError()); return 1; }

    v->text_color    = (SDL_Color){30, 30, 30, 255};
    v->speaker[0]    = '\0';
    v->full_text[0]  = '\0';
    v->visible_chars = 0;
    v->typing_done   = true;
    v->choices_visible = false;
    v->choice_count = 0;
    v->choice_selected = 0;

    v->blind_state      = BLIND_NONE;
    v->blind_h          = 0;
    v->blind_last_step  = 0;
    v->current_bg[0]    = '\0';
    v->next_bg[0]       = '\0';
    v->current_sprite[0] = '\0';
    v->next_sprite[0]   = '\0';
    v->pending_sprite[0] = '\0';
    v->sprite_blind_state = SPRITE_BLIND_NONE;
    v->sprite_blind_h = 0;
    v->sprite_blind_last_step = 0;
    v->status_text[0]   = '\0';
    v->status_until     = 0;
    v->screen           = VIEW_SCREEN_MENU;
    v->menu_selected    = 0;
    v->audio_enabled    = true;
    v->fullscreen_enabled = false;
    copy_text(v->sprite_pos, sizeof(v->sprite_pos), "center");
    copy_text(v->next_sprite_pos, sizeof(v->next_sprite_pos), "center");
    copy_text(v->pending_sprite_pos, sizeof(v->pending_sprite_pos), "center");

    return 0;
}

// ── Visuals ──────────────────────────────────

static int sprite_bar_max_h(void) {
    return (SCENE_H + BLIND_COUNT - 1) / BLIND_COUNT;
}

static void clear_next_sprite(View* v) {
    if (v->tex_next_sprite) {
        SDL_DestroyTexture(v->tex_next_sprite);
        v->tex_next_sprite = NULL;
    }
}

static void apply_sprite_immediate(View* v, const char* sprite_path,
                                   const char* sprite_pos) {
    if (v->tex_sprite) {
        SDL_DestroyTexture(v->tex_sprite);
        v->tex_sprite = NULL;
    }

    copy_text(v->current_sprite, sizeof(v->current_sprite), sprite_path);
    copy_text(v->sprite_pos, sizeof(v->sprite_pos),
              sprite_pos && sprite_pos[0] ? sprite_pos : "center");

    if (v->current_sprite[0])
        v->tex_sprite = load_texture(v->renderer, v->current_sprite);
}

static void begin_sprite_reveal(View* v, const char* sprite_path,
                                const char* sprite_pos) {
    apply_sprite_immediate(v, sprite_path, sprite_pos);
    if (!v->tex_sprite) {
        v->sprite_blind_state = SPRITE_BLIND_NONE;
        return;
    }

    v->sprite_blind_state = SPRITE_BLIND_REVEAL;
    v->sprite_blind_h = 0;
    v->sprite_blind_last_step = SDL_GetTicks();
}

static void request_sprite_change(View* v, const char* sprite_path,
                                  const char* sprite_pos) {
    const char* safe_sprite = sprite_path ? sprite_path : "";
    const char* safe_pos = (sprite_pos && sprite_pos[0]) ? sprite_pos : "center";

    if (v->sprite_blind_state == SPRITE_BLIND_NONE &&
        strcmp(v->current_sprite, safe_sprite) == 0 &&
        strcmp(v->sprite_pos, safe_pos) == 0) {
        return;
    }

    clear_next_sprite(v);
    copy_text(v->pending_sprite, sizeof(v->pending_sprite), safe_sprite);
    copy_text(v->pending_sprite_pos, sizeof(v->pending_sprite_pos), safe_pos);

    if (v->tex_sprite) {
        v->sprite_blind_state = SPRITE_BLIND_HIDE;
        v->sprite_blind_h = 0;
        v->sprite_blind_last_step = SDL_GetTicks();
    } else if (safe_sprite[0]) {
        begin_sprite_reveal(v, safe_sprite, safe_pos);
    } else {
        apply_sprite_immediate(v, "", safe_pos);
        v->sprite_blind_state = SPRITE_BLIND_NONE;
    }
}

void view_set_visuals(View* v, const char* bg_path, const char* sprite_path,
                      const char* sprite_pos) {
    const char* safe_bg = bg_path ? bg_path : "";
    const char* safe_sprite = sprite_path ? sprite_path : "";
    const char* safe_pos = (sprite_pos && sprite_pos[0]) ? sprite_pos : "center";

    if (!v->tex_bg) {
        copy_text(v->current_bg, sizeof(v->current_bg), safe_bg);
        if (v->current_bg[0])
            v->tex_bg = load_texture(v->renderer, v->current_bg);
        request_sprite_change(v, safe_sprite, safe_pos);
        return;
    }

    if (strcmp(v->current_bg, safe_bg) != 0 && safe_bg[0]) {
        copy_text(v->next_bg, sizeof(v->next_bg), safe_bg);
        copy_text(v->next_sprite, sizeof(v->next_sprite), safe_sprite);
        copy_text(v->next_sprite_pos, sizeof(v->next_sprite_pos), safe_pos);
        v->blind_state     = BLIND_ODD_CLOSE;
        v->blind_h         = 0;
        v->blind_last_step = SDL_GetTicks();
        return;
    }

    request_sprite_change(v, safe_sprite, safe_pos);
}

// ── Typewriter ───────────────────────────────

void view_set_text(View* v, const char* speaker, const char* text) {
    if (!text || text[0] == '\0') return;
    view_clear_choices(v);
    copy_text(v->speaker, sizeof(v->speaker), speaker);
    if (speaker && speaker[0])
        snprintf(v->full_text, sizeof(v->full_text), "[%s] %s", speaker, text);
    else
        copy_text(v->full_text, sizeof(v->full_text), text);
    v->visible_chars  = 0;
    v->last_char_time = SDL_GetTicks();
    v->typing_done    = false;
}

void view_set_choices(View* v, const StoryLine* line, int selected) {
    if (!line || line->type != LINE_CHOICE || line->choice_count <= 0) {
        view_clear_choices(v);
        return;
    }

    v->choices_visible = true;
    v->choice_count = line->choice_count;
    v->choice_selected = selected;
    if (v->choice_selected < 0) v->choice_selected = 0;
    if (v->choice_selected >= v->choice_count)
        v->choice_selected = v->choice_count - 1;

    v->speaker[0] = '\0';
    v->full_text[0] = '\0';
    v->visible_chars = 0;
    v->typing_done = true;

    for (int i = 0; i < v->choice_count; i++)
        copy_text(v->choice_text[i], sizeof(v->choice_text[i]), line->choices[i].text);
}

void view_clear_choices(View* v) {
    v->choices_visible = false;
    v->choice_count = 0;
    v->choice_selected = 0;
}

void view_skip_typewriter(View* v) {
    v->visible_chars = (int)strlen(v->full_text);
    v->typing_done   = true;
}

bool view_is_typing(const View* v) {
    return !v->typing_done;
}

bool view_is_transitioning(const View* v) {
    return v->blind_state != BLIND_NONE ||
           v->sprite_blind_state != SPRITE_BLIND_NONE;
}

void view_set_status(View* v, const char* text) {
    copy_text(v->status_text, sizeof(v->status_text), text);
    v->status_until = SDL_GetTicks() + 1600;
}

ViewCommand view_command_at(int x, int y) {
    const int button_y = 452;
    const int button_w = 90;
    const int button_h = 20;
    const int gap = 8;
    const int first_x = 50;

    if (y < button_y || y >= button_y + button_h)
        return VIEW_COMMAND_NONE;

    if (x >= first_x && x < first_x + button_w)
        return VIEW_COMMAND_SAVE;
    if (x >= first_x + button_w + gap &&
        x < first_x + button_w * 2 + gap)
        return VIEW_COMMAND_LOAD;
    if (x >= first_x + button_w * 2 + gap * 2 &&
        x < first_x + button_w * 3 + gap * 2)
        return VIEW_COMMAND_BACK;
    if (x >= first_x + button_w * 3 + gap * 3 &&
        x < first_x + button_w * 4 + gap * 3)
        return VIEW_COMMAND_PATREON;

    return VIEW_COMMAND_NONE;
}

void view_set_screen(View* v, ViewScreen screen) {
    v->screen = screen;
}

void view_set_menu_state(View* v, int selected, bool audio_enabled,
                         bool fullscreen_enabled) {
    v->menu_selected = selected;
    v->audio_enabled = audio_enabled;
    v->fullscreen_enabled = fullscreen_enabled;
}

ViewMenuAction view_menu_action_at(const View* v, int x, int y) {
    int count = v->screen == VIEW_SCREEN_SETTINGS ? 3 : 5;
    int box_x = 190;
    int box_y = v->screen == VIEW_SCREEN_SETTINGS ? 176 : 160;
    int box_w = 260;
    int row_h = 34;

    if (x < box_x || x >= box_x + box_w ||
        y < box_y || y >= box_y + count * row_h) {
        return VIEW_MENU_NONE;
    }

    int row = (y - box_y) / row_h;
    if (v->screen == VIEW_SCREEN_SETTINGS) {
        if (row == 0) return VIEW_MENU_AUDIO;
        if (row == 1) return VIEW_MENU_FULLSCREEN;
        return VIEW_MENU_BACK;
    }

    switch (row) {
        case 0: return VIEW_MENU_NEW_GAME;
        case 1: return VIEW_MENU_LOAD;
        case 2: return VIEW_MENU_SETTINGS;
        case 3: return VIEW_MENU_PATREON;
        case 4: return VIEW_MENU_QUIT;
        default: return VIEW_MENU_NONE;
    }
}

int view_set_fullscreen(View* v, bool enabled) {
    Uint32 mode = enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(v->window, mode) != 0) {
        fprintf(stderr, "[View] Fullscreen: %s\n", SDL_GetError());
        return 1;
    }

    v->fullscreen_enabled = enabled;
    SDL_RenderSetLogicalSize(v->renderer, WIN_W, WIN_H);
    return 0;
}

// ── Update ───────────────────────────────────

void view_update(View* v) {

    // Typewriter — only runs when no transition is happening
    if (!v->typing_done && v->blind_state == BLIND_NONE &&
        v->sprite_blind_state == SPRITE_BLIND_NONE) {
        int total  = (int)strlen(v->full_text);
        Uint32 now = SDL_GetTicks();
        while (v->visible_chars < total &&
               now - v->last_char_time >= TYPEWRITER_MS) {
            v->visible_chars++;
            v->last_char_time += TYPEWRITER_MS;
        }
        if (v->visible_chars >= total)
            v->typing_done = true;
    }

    if (v->blind_state == BLIND_NONE &&
        v->sprite_blind_state != SPRITE_BLIND_NONE) {
        Uint32 now = SDL_GetTicks();
        if (now - v->sprite_blind_last_step < BLIND_STEP_MS) return;
        v->sprite_blind_last_step += BLIND_STEP_MS;

        int sprite_bar_h = sprite_bar_max_h();
        v->sprite_blind_h += BLIND_SPEED;
        if (v->sprite_blind_h >= sprite_bar_h) {
            v->sprite_blind_h = sprite_bar_h;

            if (v->sprite_blind_state == SPRITE_BLIND_HIDE) {
                if (v->pending_sprite[0]) {
                    begin_sprite_reveal(v, v->pending_sprite, v->pending_sprite_pos);
                } else {
                    apply_sprite_immediate(v, "", v->pending_sprite_pos);
                    v->sprite_blind_state = SPRITE_BLIND_NONE;
                    v->last_char_time = SDL_GetTicks();
                }
            } else {
                v->sprite_blind_state = SPRITE_BLIND_NONE;
                v->sprite_blind_h = 0;
                v->last_char_time = SDL_GetTicks();
            }
        }
        return;
    }

    // Venetian blind — only step every BLIND_STEP_MS milliseconds
    if (v->blind_state != BLIND_NONE) {
        Uint32 now = SDL_GetTicks();
        if (now - v->blind_last_step < BLIND_STEP_MS) return;
        v->blind_last_step += BLIND_STEP_MS;  // accumulate, don't snap to now
    }

    int bar_h = blind_max_h();

    if (v->blind_state == BLIND_ODD_CLOSE) {
        // Odd bars (0,2,4...) grow downward
        v->blind_h += BLIND_SPEED;
        if (v->blind_h >= bar_h) {
            v->blind_h     = bar_h;       // snap to full bar height
            v->blind_state = BLIND_EVEN_CLOSE;
            v->blind_h     = 0;           // reset counter for even bars
        }

    } else if (v->blind_state == BLIND_EVEN_CLOSE) {
        // Even bars (1,3,5...) grow to fill the gaps
        v->blind_h += BLIND_SPEED;
        if (v->blind_h >= bar_h) {
            v->blind_h = bar_h;

            // Fully closed — swap textures now
            if (v->tex_bg) {
                SDL_DestroyTexture(v->tex_bg);
                v->tex_bg = NULL;
            }
            copy_text(v->current_bg, sizeof(v->current_bg), v->next_bg);
            v->tex_bg = load_texture(v->renderer, v->current_bg);
            apply_sprite_immediate(v, "", v->next_sprite_pos);

            // Start opening
            v->blind_state = BLIND_ODD_OPEN;
            v->blind_h     = bar_h;       // start fully closed, shrink to 0
        }

    } else if (v->blind_state == BLIND_ODD_OPEN) {
        // Odd bars shrink upward to reveal new scene
        v->blind_h -= BLIND_SPEED;
        if (v->blind_h <= 0) {
            v->blind_h     = 0;
            v->blind_state = BLIND_EVEN_OPEN;
            v->blind_h     = bar_h;       // even bars still full, start shrinking
        }

    } else if (v->blind_state == BLIND_EVEN_OPEN) {
        // Even bars shrink to finish the reveal
        v->blind_h -= BLIND_SPEED;
        if (v->blind_h <= 0) {
            v->blind_h     = 0;
            v->blind_state = BLIND_NONE;  // all done

            request_sprite_change(v, v->next_sprite, v->next_sprite_pos);
            if (v->sprite_blind_state == SPRITE_BLIND_NONE)
                v->last_char_time = SDL_GetTicks();
        }
    }
}

// ── Render ───────────────────────────────────

// Draw the venetian blind bars over the scene area.
static void render_blinds(View* v, int group_a_h, int group_b_h) {
    SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(v->renderer, 0, 0, 0, 255);

    for (int i = 0; i < BLIND_COUNT; i++) {

        /*  Distribute remainder pixels naturally across all bars.
            No last-bar special case needed — each bar is derived
            independently so they tile perfectly with zero gaps.      */
        int bar_top    = SCENE_Y + (i       * SCENE_H) / BLIND_COUNT;
        int bar_bottom = SCENE_Y + ((i + 1) * SCENE_H) / BLIND_COUNT;
        int bar_height = bar_bottom - bar_top;

        bool group_a = (i % 2 == 0);   /* even index  → group A */
        /* group A closes top-down, group B closes bottom-up          */

        int current_h = group_a ? group_a_h : group_b_h;
        int draw_h = current_h < bar_height ? current_h : bar_height;

        if (draw_h <= 0) continue;

        /*  Alternate growth direction — the key to the venetian look.
            Group A grows downward from bar_top.
            Group B grows upward from bar_bottom.                     */
        int draw_y = group_a
                   ? bar_top                   /* top-down  */
                   : (bar_bottom - draw_h);    /* bottom-up */

        SDL_Rect bar = { SCENE_X, draw_y, SCENE_W, draw_h };
        SDL_RenderFillRect(v->renderer, &bar);
    }
}

static SDL_Rect sprite_rect_for(View* v, SDL_Texture* texture) {
    SDL_Rect sprite_rect = {SCENE_X, SCENE_Y, 0, 0};
    int tex_w = 0;
    int tex_h = 0;
    SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h);

    if (tex_w <= 0 || tex_h <= 0)
        return sprite_rect;

    int sprite_h = SCENE_H;
    int sprite_w = tex_w * sprite_h / tex_h;
    if (sprite_w > SCENE_W) {
        sprite_w = SCENE_W;
        sprite_h = tex_h * sprite_w / tex_w;
    }

    int sprite_x = SCENE_X + (SCENE_W - sprite_w) / 2;
    if (strcmp(v->sprite_pos, "left") == 0)
        sprite_x = SCENE_X + 28;
    else if (strcmp(v->sprite_pos, "right") == 0)
        sprite_x = SCENE_X + SCENE_W - sprite_w - 28;

    sprite_rect.x = sprite_x;
    sprite_rect.y = SCENE_Y + SCENE_H - sprite_h;
    sprite_rect.w = sprite_w;
    sprite_rect.h = sprite_h;
    return sprite_rect;
}

static void render_sprite_segment(View* v, SDL_Texture* texture,
                                  SDL_Rect dst, int seg_y, int seg_h) {
    int tex_w = 0;
    int tex_h = 0;
    SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h);
    if (tex_w <= 0 || tex_h <= 0 || dst.h <= 0 || seg_h <= 0)
        return;

    SDL_Rect src = {
        0,
        (seg_y - dst.y) * tex_h / dst.h,
        tex_w,
        seg_h * tex_h / dst.h
    };
    if (src.h <= 0) src.h = 1;

    SDL_Rect seg_dst = {dst.x, seg_y, dst.w, seg_h};
    SDL_RenderCopy(v->renderer, texture, &src, &seg_dst);
}

static void render_sprite_blinds(View* v, SDL_Texture* texture,
                                 SDL_Rect dst, bool reveal) {
    if (!texture || dst.w <= 0 || dst.h <= 0)
        return;

    for (int i = 0; i < BLIND_COUNT; i++) {
        int bar_top = dst.y + (i * dst.h) / BLIND_COUNT;
        int bar_bottom = dst.y + ((i + 1) * dst.h) / BLIND_COUNT;
        int bar_height = bar_bottom - bar_top;
        int blind_h = v->sprite_blind_h < bar_height
                    ? v->sprite_blind_h
                    : bar_height;
        bool group_a = (i % 2 == 0);

        if (reveal) {
            if (blind_h <= 0) continue;
            int draw_y = group_a ? bar_top : (bar_bottom - blind_h);
            render_sprite_segment(v, texture, dst, draw_y, blind_h);
        } else {
            int visible_h = bar_height - blind_h;
            if (visible_h <= 0) continue;
            int draw_y = group_a ? (bar_top + blind_h) : bar_top;
            render_sprite_segment(v, texture, dst, draw_y, visible_h);
        }
    }
}

static void render_sprite(View* v) {
    if (!v->tex_sprite)
        return;

    SDL_Rect sprite_rect = sprite_rect_for(v, v->tex_sprite);
    if (sprite_rect.w <= 0 || sprite_rect.h <= 0)
        return;

    if (v->sprite_blind_state == SPRITE_BLIND_REVEAL) {
        render_sprite_blinds(v, v->tex_sprite, sprite_rect, true);
    } else if (v->sprite_blind_state == SPRITE_BLIND_HIDE) {
        render_sprite_blinds(v, v->tex_sprite, sprite_rect, false);
    } else {
        SDL_RenderCopy(v->renderer, v->tex_sprite, NULL, &sprite_rect);
    }
}

static void render_choice_box(View* v) {
    if (!v->choices_visible || v->choice_count <= 0 || view_is_transitioning(v))
        return;

    int row_h = 24;
    int box_w = 430;
    int box_h = 20 + row_h * v->choice_count;
    SDL_Rect outer = {
        SCENE_X + (SCENE_W - box_w) / 2,
        SCENE_Y + SCENE_H - box_h - 18,
        box_w,
        box_h
    };
    SDL_Rect inner = {outer.x + 3, outer.y + 3, outer.w - 6, outer.h - 6};

    SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_BLEND);
    set_draw_color(v->renderer, UI_PANEL_FILL);
    SDL_RenderFillRect(v->renderer, &outer);
    set_draw_color(v->renderer, UI_BUTTON_FILL);
    SDL_RenderFillRect(v->renderer, &inner);
    set_draw_color(v->renderer, UI_BORDER);
    SDL_RenderDrawRect(v->renderer, &outer);

    for (int i = 0; i < v->choice_count; i++) {
        int y = inner.y + 9 + i * row_h;
        SDL_Color color = i == v->choice_selected ? UI_TEXT_HOT : UI_TEXT;

        if (i == v->choice_selected) {
            SDL_Rect marker = {inner.x + 14, y + 5, 8, 8};
            set_draw_color(v->renderer, UI_TEXT_HOT);
            SDL_RenderFillRect(v->renderer, &marker);
        }

        SDL_Surface* s = TTF_RenderText_Blended(v->font, v->choice_text[i], color);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(v->renderer, s);
            SDL_Rect r = {inner.x + 34, y, s->w, s->h};
            SDL_RenderCopy(v->renderer, t, NULL, &r);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_NONE);
}

static void render_text_at(View* v, const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* s = TTF_RenderText_Blended(v->font, text, color);
    if (!s) return;

    SDL_Texture* t = SDL_CreateTextureFromSurface(v->renderer, s);
    if (t) {
        SDL_Rect r = {x, y, s->w, s->h};
        SDL_RenderCopy(v->renderer, t, NULL, &r);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

static void render_command_button_style(View* v, int x, int y, const char* label,
                                        SDL_Color fill, SDL_Color border,
                                        SDL_Color text) {
    SDL_Rect outer = {x, y, 90, 20};
    SDL_Rect inner = {x + 2, y + 2, 86, 16};

    SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_BLEND);
    set_draw_color(v->renderer, UI_PANEL_FILL);
    SDL_RenderFillRect(v->renderer, &outer);
    set_draw_color(v->renderer, fill);
    SDL_RenderFillRect(v->renderer, &inner);
    set_draw_color(v->renderer, border);
    SDL_RenderDrawRect(v->renderer, &outer);

    render_text_at(v, label, x + 8, y + 1, text);
    SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_NONE);
}

static void render_command_button(View* v, int x, int y, const char* label) {
    render_command_button_style(v, x, y, label, UI_BUTTON_FILL, UI_BORDER,
                                UI_STATUS_TEXT);
}

static void render_patreon_command_button(View* v, int x, int y) {
    render_command_button_style(v, x, y, "P PATREON", UI_PATREON_FILL,
                                UI_PATREON_BORDER, UI_PATREON_TEXT);
}

static void render_command_strip(View* v) {
    const int y = 452;

    render_command_button(v, 50, y, "F5 SAVE");
    render_command_button(v, 148, y, "F9 LOAD");
    render_command_button(v, 246, y, "BS BACK");
    render_patreon_command_button(v, 344, y);

    if (v->status_text[0] && SDL_GetTicks() < v->status_until) {
        render_text_at(v, v->status_text, 454, y + 1, UI_STATUS_TEXT);
    }
}

static void render_menu_row_style(View* v, int x, int y, int w, const char* label,
                                  bool selected, bool patreon) {
    SDL_Rect outer = {x, y, w, 28};
    SDL_Rect inner = {x + 3, y + 3, w - 6, 22};
    SDL_Color text = patreon ? UI_PATREON_TEXT : (selected ? UI_TEXT_HOT : UI_TEXT);
    SDL_Color fill = patreon ?
        (selected ? UI_PATREON_HOT : UI_PATREON_FILL) :
        (selected ? UI_BUTTON_SELECTED : UI_BUTTON_FILL);
    SDL_Color border = patreon ? UI_PATREON_BORDER : UI_BORDER;

    SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_BLEND);
    set_draw_color(v->renderer, UI_PANEL_FILL);
    SDL_RenderFillRect(v->renderer, &outer);
    set_draw_color(v->renderer, fill);
    SDL_RenderFillRect(v->renderer, &inner);
    set_draw_color(v->renderer, border);
    SDL_RenderDrawRect(v->renderer, &outer);

    if (selected) {
        SDL_Rect marker = {x + 13, y + 10, 8, 8};
        set_draw_color(v->renderer, patreon ? UI_PATREON_TEXT : UI_TEXT_HOT);
        SDL_RenderFillRect(v->renderer, &marker);
    }

    render_text_at(v, label, x + 32, y + 4, text);
    SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_NONE);
}

static void render_menu_row(View* v, int x, int y, int w, const char* label,
                            bool selected) {
    render_menu_row_style(v, x, y, w, label, selected, false);
}

static void render_patreon_menu_row(View* v, int x, int y, int w,
                                    bool selected) {
    render_menu_row_style(v, x, y, w, "PATREON", selected, true);
}

static void render_menu_screen(View* v) {
    SDL_RenderCopy(v->renderer, v->tex_windowbg, NULL, NULL);

    SDL_Rect scene = {SCENE_X, SCENE_Y, SCENE_W, SCENE_H};
    if (v->tex_menubg) {
        SDL_RenderCopy(v->renderer, v->tex_menubg, NULL, &scene);
    } else {
        set_draw_color(v->renderer, UI_FALLBACK_BG);
        SDL_RenderFillRect(v->renderer, &scene);
    }
    set_draw_color(v->renderer, UI_PANEL_FILL);
    SDL_RenderDrawRect(v->renderer, &scene);

    render_text_at(v, "A CAT IN GIRLS DORMITORY", 148, 82, UI_TEXT_HOT);
    render_text_at(v, "C VISUAL NOVEL ENGINE", 205, 112, UI_TEXT);

    int x = 190;
    int y = 160;
    int w = 260;
    const char* labels[] = {"NEW GAME", "LOAD", "SETTINGS", "PATREON", "QUIT"};
    for (int i = 0; i < 5; i++) {
        if (i == 3)
            render_patreon_menu_row(v, x, y + i * 34, w, v->menu_selected == i);
        else
            render_menu_row(v, x, y + i * 34, w, labels[i], v->menu_selected == i);
    }

    if (v->status_text[0] && SDL_GetTicks() < v->status_until) {
        render_text_at(v, v->status_text, 238, 334, UI_STATUS_TEXT);
    }

    SDL_Rect box_rect = {BOX_X, BOX_Y, BOX_W, BOX_H};
    SDL_RenderCopy(v->renderer, v->tex_box, NULL, &box_rect);
    render_text_at(v, "ARROWS/W/S SELECT    ENTER CONFIRM", BOX_X + 22, BOX_Y + 18, v->text_color);
    render_text_at(v, "PC-98 STYLE MODE", BOX_X + 22, BOX_Y + 44, v->text_color);
}

static void render_settings_screen(View* v) {
    SDL_RenderCopy(v->renderer, v->tex_windowbg, NULL, NULL);

    SDL_Rect scene = {SCENE_X, SCENE_Y, SCENE_W, SCENE_H};
    if (v->tex_menubg) {
        SDL_RenderCopy(v->renderer, v->tex_menubg, NULL, &scene);
    } else {
        set_draw_color(v->renderer, UI_FALLBACK_BG);
        SDL_RenderFillRect(v->renderer, &scene);
    }
    set_draw_color(v->renderer, UI_PANEL_FILL);
    SDL_RenderDrawRect(v->renderer, &scene);

    render_text_at(v, "SETTINGS", 276, 110, UI_TEXT_HOT);
    render_text_at(v, "DISPLAY / AUDIO", 238, 144, UI_TEXT);

    char audio_label[64];
    snprintf(audio_label, sizeof(audio_label), "AUDIO: %s",
             v->audio_enabled ? "ON" : "OFF");
    char fullscreen_label[64];
    snprintf(fullscreen_label, sizeof(fullscreen_label), "FULLSCREEN: %s",
             v->fullscreen_enabled ? "ON" : "OFF");

    render_menu_row(v, 190, 176, 260, audio_label, v->menu_selected == 0);
    render_menu_row(v, 190, 210, 260, fullscreen_label, v->menu_selected == 1);
    render_menu_row(v, 190, 244, 260, "BACK", v->menu_selected == 2);

    if (v->status_text[0] && SDL_GetTicks() < v->status_until) {
        render_text_at(v, v->status_text, 246, 286, UI_STATUS_TEXT);
    }

    SDL_Rect box_rect = {BOX_X, BOX_Y, BOX_W, BOX_H};
    SDL_RenderCopy(v->renderer, v->tex_box, NULL, &box_rect);
    render_text_at(v, "ENTER TO TOGGLE    ESC/BACKSPACE BACK", BOX_X + 22, BOX_Y + 28, v->text_color);
}

void view_render(View* v) {
    SDL_RenderClear(v->renderer);

    if (v->screen == VIEW_SCREEN_MENU) {
        render_menu_screen(v);
        SDL_RenderPresent(v->renderer);
        return;
    }

    if (v->screen == VIEW_SCREEN_SETTINGS) {
        render_settings_screen(v);
        SDL_RenderPresent(v->renderer);
        return;
    }

    SDL_Rect scene_rect = {SCENE_X, SCENE_Y, SCENE_W, SCENE_H};
    SDL_Rect box_rect   = {BOX_X,   BOX_Y,   BOX_W,   BOX_H};

    // 1. Window background
    SDL_RenderCopy(v->renderer, v->tex_windowbg, NULL, NULL);

    // 2. Scene image
    if (v->tex_bg)
        SDL_RenderCopy(v->renderer, v->tex_bg, NULL, &scene_rect);

    render_sprite(v);

    // 3. Venetian blind overlay
    switch (v->blind_state) {
        case BLIND_ODD_CLOSE:
            render_blinds(v, v->blind_h, 0);
            break;
        case BLIND_EVEN_CLOSE:
            render_blinds(v, blind_max_h(), v->blind_h);
            break;
        case BLIND_ODD_OPEN:
            render_blinds(v, v->blind_h, blind_max_h());
            break;
        case BLIND_EVEN_OPEN:
            render_blinds(v, 0, v->blind_h);
            break;
        case BLIND_NONE:
        default:
            break;
    }

    // 4. Text box
    SDL_RenderCopy(v->renderer, v->tex_box, NULL, &box_rect);

    // 5. Typewriter text — word-wrapped, only visible_chars revealed
    if (v->full_text[0] != '\0' && v->visible_chars > 0
        && !view_is_transitioning(v)) {

        char visible[512];
        int len  = (int)strlen(v->full_text);
        int copy = v->visible_chars < len ? v->visible_chars : len;
        strncpy(visible, v->full_text, copy);
        visible[copy] = '\0';

        int max_w  = BOX_W - TEXT_PAD_X * 2;
        int line_h = 0;
        TTF_SizeText(v->font, "A", NULL, &line_h);

        int draw_x = BOX_X + TEXT_PAD_X;
        int draw_y = BOX_Y + TEXT_PAD_Y;

        char line_buf[512] = {0};
        char word[128];
        const char* p = visible;

        while (*p) {
            int wi = 0;
            while (*p && *p != ' ') word[wi++] = *p++;
            word[wi] = '\0';
            if (*p == ' ') p++;

            char test[512];
            if (line_buf[0]) snprintf(test, sizeof(test), "%s %s", line_buf, word);
            else             snprintf(test, sizeof(test), "%s",     word);

            int tw = 0;
            TTF_SizeText(v->font, test, &tw, NULL);

            if (tw > max_w && line_buf[0]) {
                SDL_Surface* s = TTF_RenderText_Blended(v->font, line_buf, v->text_color);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(v->renderer, s);
                    SDL_Rect r = {draw_x, draw_y, s->w, s->h};
                    SDL_RenderCopy(v->renderer, t, NULL, &r);
                    SDL_DestroyTexture(t);
                    SDL_FreeSurface(s);
                }
                draw_y += line_h + 2;
                snprintf(line_buf, sizeof(line_buf), "%s", word);
            } else {
                snprintf(line_buf, sizeof(line_buf), "%s", test);
            }
        }

        if (line_buf[0]) {
            SDL_Surface* s = TTF_RenderText_Blended(v->font, line_buf, v->text_color);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(v->renderer, s);
                SDL_Rect r = {draw_x, draw_y, s->w, s->h};
                SDL_RenderCopy(v->renderer, t, NULL, &r);
                SDL_DestroyTexture(t);
                SDL_FreeSurface(s);
            }
        }
    }

    // 6. Heart — only when typing done and no transition
    if (v->typing_done && !view_is_transitioning(v) && v->tex_heart) {
        SDL_Rect heart_rect = {
            BOX_X + BOX_W - HEART_SIZE - HEART_MARGIN,
            BOX_Y + BOX_H - HEART_SIZE - HEART_MARGIN,
            HEART_SIZE,
            HEART_SIZE
        };
        SDL_RenderCopy(v->renderer, v->tex_heart, NULL, &heart_rect);
    }

    render_choice_box(v);
    render_command_strip(v);

    SDL_RenderPresent(v->renderer);
}

// ── Cleanup ──────────────────────────────────

void view_free(View* v) {
    if (v->tex_heart)    SDL_DestroyTexture(v->tex_heart);
    if (v->tex_menubg)   SDL_DestroyTexture(v->tex_menubg);
    if (v->tex_next_sprite) SDL_DestroyTexture(v->tex_next_sprite);
    if (v->tex_sprite)   SDL_DestroyTexture(v->tex_sprite);
    if (v->tex_bg)       SDL_DestroyTexture(v->tex_bg);
    if (v->tex_box)      SDL_DestroyTexture(v->tex_box);
    if (v->tex_windowbg) SDL_DestroyTexture(v->tex_windowbg);
    if (v->font)         TTF_CloseFont(v->font);
    if (v->renderer)     SDL_DestroyRenderer(v->renderer);
    if (v->window)       SDL_DestroyWindow(v->window);
}
