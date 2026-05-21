#include "controller.h"
#include <stdio.h>
#include <string.h>

#define STORY_PATH "assets/story.txt"
#define PATREON_URL "https://patreon.com/c10ud"

static int menu_item_count(const Controller* c) {
    return c->screen == CONTROLLER_SETTINGS ? 3 : 5;
}

static void open_patreon(Controller* c) {
    if (SDL_OpenURL(PATREON_URL) == 0)
        view_set_status(c->view, "OPENED PATREON");
    else
        view_set_status(c->view, "PATREON OPEN FAILED");
}

static void set_screen(Controller* c, ControllerScreen screen) {
    c->screen = screen;
    c->menu_index = 0;

    if (screen == CONTROLLER_GAME)
        view_set_screen(c->view, VIEW_SCREEN_GAME);
    else if (screen == CONTROLLER_SETTINGS)
        view_set_screen(c->view, VIEW_SCREEN_SETTINGS);
    else
        view_set_screen(c->view, VIEW_SCREEN_MENU);

    view_set_menu_state(c->view, c->menu_index, c->audio_enabled,
                        c->fullscreen_enabled);
}

static void save_settings(const Controller* c) {
    FILE* f = fopen(c->settings_path, "w");
    if (!f) return;
    fprintf(f, "audio %d\n", c->audio_enabled ? 1 : 0);
    fprintf(f, "fullscreen %d\n", c->fullscreen_enabled ? 1 : 0);
    fclose(f);
}

static void load_settings(Controller* c) {
    c->audio_enabled = true;
    c->fullscreen_enabled = false;

    FILE* f = fopen(c->settings_path, "r");
    if (!f) return;

    char key[32];
    int value = 1;
    while (fscanf(f, "%31s %d", key, &value) == 2) {
        if (strcmp(key, "audio") == 0)
            c->audio_enabled = value != 0;
        else if (strcmp(key, "fullscreen") == 0)
            c->fullscreen_enabled = value != 0;
    }

    fclose(f);
}

static void sync_view(Controller* c) {
    const StoryLine* line = model_current_line(c->model);
    if (!line) return;
    view_set_visuals(c->view, line->bg_path, line->sprite_path, line->sprite_pos);
    if (line->type == LINE_CHOICE) {
        c->choice_index = 0;
        view_set_choices(c->view, line, c->choice_index);
    } else {
        view_clear_choices(c->view);
        view_set_text(c->view, line->speaker, line->text);
    }
}

static void new_game(Controller* c) {
    if (model_load(c->model, c->story_path) != 0) {
        view_set_status(c->view, "NEW GAME FAILED");
        return;
    }

    set_screen(c, CONTROLLER_GAME);
    sync_view(c);
}

static void load_game(Controller* c) {
    if (model_load_save(c->model, c->save_path) == 0) {
        set_screen(c, CONTROLLER_GAME);
        sync_view(c);
        view_set_status(c->view, "LOADED");
    } else {
        view_set_status(c->view, "NO SAVE DATA");
    }
}

static void controller_command(Controller* c, ViewCommand command) {
    if (command == VIEW_COMMAND_NONE || view_is_transitioning(c->view))
        return;

    switch (command) {
        case VIEW_COMMAND_SAVE:
            if (model_save(c->model, c->save_path) == 0)
                view_set_status(c->view, "SAVED TO DISK");
            else
                view_set_status(c->view, "SAVE FAILED");
            break;

        case VIEW_COMMAND_LOAD:
            load_game(c);
            break;

        case VIEW_COMMAND_BACK:
            if (model_back(c->model) == 0) {
                sync_view(c);
                view_set_status(c->view, "BACK");
            } else {
                view_set_status(c->view, "NO HISTORY");
            }
            break;

        case VIEW_COMMAND_PATREON:
            open_patreon(c);
            break;

        case VIEW_COMMAND_NONE:
        default:
            break;
    }
}

void controller_init(Controller* c, Model* m, View* v) {
    c->model   = m;
    c->view    = v;
    c->running = 1;
    c->choice_index = 0;
    c->menu_index = 0;
    c->screen = CONTROLLER_MENU;
    c->audio_enabled = true;
    c->fullscreen_enabled = false;
    snprintf(c->story_path, sizeof(c->story_path), "%s", STORY_PATH);

    char* pref_path = SDL_GetPrefPath("C_VN_Engine", "A_Cat_In_Girls_Dormitory");
    if (pref_path) {
        snprintf(c->save_path, sizeof(c->save_path), "%ssave.dat", pref_path);
        snprintf(c->settings_path, sizeof(c->settings_path), "%ssettings.cfg", pref_path);
        SDL_free(pref_path);
    } else {
        snprintf(c->save_path, sizeof(c->save_path), "save.dat");
        snprintf(c->settings_path, sizeof(c->settings_path), "settings.cfg");
    }

    load_settings(c);
    if (view_set_fullscreen(c->view, c->fullscreen_enabled) != 0)
        c->fullscreen_enabled = false;
    set_screen(c, CONTROLLER_MENU);
}

static void choose_menu_action(Controller* c, ViewMenuAction action) {
    if (action == VIEW_MENU_NONE)
        return;

    if (c->screen == CONTROLLER_SETTINGS) {
        if (action == VIEW_MENU_AUDIO) {
            c->audio_enabled = !c->audio_enabled;
            save_settings(c);
            view_set_menu_state(c->view, c->menu_index, c->audio_enabled,
                                c->fullscreen_enabled);
            view_set_status(c->view, c->audio_enabled ? "AUDIO ON" : "AUDIO OFF");
        } else if (action == VIEW_MENU_FULLSCREEN) {
            bool next = !c->fullscreen_enabled;
            if (view_set_fullscreen(c->view, next) == 0) {
                c->fullscreen_enabled = next;
                save_settings(c);
                view_set_menu_state(c->view, c->menu_index, c->audio_enabled,
                                    c->fullscreen_enabled);
                view_set_status(c->view, next ? "FULLSCREEN ON" : "FULLSCREEN OFF");
            } else {
                view_set_status(c->view, "FULLSCREEN FAILED");
            }
        } else if (action == VIEW_MENU_BACK) {
            set_screen(c, CONTROLLER_MENU);
        }
        return;
    }

    switch (action) {
        case VIEW_MENU_NEW_GAME:
            new_game(c);
            break;
        case VIEW_MENU_LOAD:
            load_game(c);
            break;
        case VIEW_MENU_SETTINGS:
            set_screen(c, CONTROLLER_SETTINGS);
            break;
        case VIEW_MENU_PATREON:
            open_patreon(c);
            break;
        case VIEW_MENU_QUIT:
            c->running = 0;
            break;
        default:
            break;
    }
}

static void choose_selected_menu_item(Controller* c) {
    if (c->screen == CONTROLLER_SETTINGS) {
        if (c->menu_index == 0)
            choose_menu_action(c, VIEW_MENU_AUDIO);
        else if (c->menu_index == 1)
            choose_menu_action(c, VIEW_MENU_FULLSCREEN);
        else
            choose_menu_action(c, VIEW_MENU_BACK);
        return;
    }

    switch (c->menu_index) {
        case 0: choose_menu_action(c, VIEW_MENU_NEW_GAME); break;
        case 1: choose_menu_action(c, VIEW_MENU_LOAD); break;
        case 2: choose_menu_action(c, VIEW_MENU_SETTINGS); break;
        case 3: choose_menu_action(c, VIEW_MENU_PATREON); break;
        case 4: choose_menu_action(c, VIEW_MENU_QUIT); break;
        default: break;
    }
}

static void move_menu_selection(Controller* c, int delta) {
    int count = menu_item_count(c);
    c->menu_index = (c->menu_index + delta + count) % count;
    view_set_menu_state(c->view, c->menu_index, c->audio_enabled,
                        c->fullscreen_enabled);
}

static void handle_menu_event(Controller* c, SDL_Event* e) {
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        float logical_x = 0.0f;
        float logical_y = 0.0f;
        SDL_RenderWindowToLogical(c->view->renderer,
                                  e->button.x, e->button.y,
                                  &logical_x, &logical_y);
        choose_menu_action(c, view_menu_action_at(c->view, (int)logical_x, (int)logical_y));
        return;
    }

    if (e->type != SDL_KEYDOWN)
        return;

    switch (e->key.keysym.sym) {
        case SDLK_ESCAPE:
            if (c->screen == CONTROLLER_SETTINGS)
                set_screen(c, CONTROLLER_MENU);
            else
                c->running = 0;
            break;

        case SDLK_BACKSPACE:
            if (c->screen == CONTROLLER_SETTINGS)
                set_screen(c, CONTROLLER_MENU);
            break;

        case SDLK_UP:
        case SDLK_w:
            move_menu_selection(c, -1);
            break;

        case SDLK_DOWN:
        case SDLK_s:
            move_menu_selection(c, 1);
            break;

        case SDLK_SPACE:
        case SDLK_RETURN:
            choose_selected_menu_item(c);
            break;

        case SDLK_p:
            choose_menu_action(c, VIEW_MENU_PATREON);
            break;
    }
}

void controller_handle_event(Controller* c, SDL_Event* e) {
    if (e->type == SDL_QUIT) { c->running = 0; return; }

    if (c->screen != CONTROLLER_GAME) {
        handle_menu_event(c, e);
        return;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        float logical_x = 0.0f;
        float logical_y = 0.0f;
        SDL_RenderWindowToLogical(c->view->renderer,
                                  e->button.x, e->button.y,
                                  &logical_x, &logical_y);
        controller_command(c, view_command_at((int)logical_x, (int)logical_y));
        return;
    }

    if (e->type == SDL_KEYDOWN) {
        const StoryLine* line = model_current_line(c->model);

        switch (e->key.keysym.sym) {

            case SDLK_ESCAPE:
                c->running = 0;
                break;

            case SDLK_F5:
                controller_command(c, VIEW_COMMAND_SAVE);
                break;

            case SDLK_F9:
                controller_command(c, VIEW_COMMAND_LOAD);
                break;

            case SDLK_BACKSPACE:
                controller_command(c, VIEW_COMMAND_BACK);
                break;

            case SDLK_p:
                controller_command(c, VIEW_COMMAND_PATREON);
                break;

            case SDLK_SPACE:
            case SDLK_RETURN:
                // Block all input while the blind transition is playing
                if (view_is_transitioning(c->view)) break;

                if (line && line->type == LINE_CHOICE) {
                    if (model_choose(c->model, c->choice_index))
                        c->running = 0;
                    else
                        sync_view(c);
                    break;
                }

                // Still typing — skip to end first
                if (view_is_typing(c->view)) {
                    view_skip_typewriter(c->view);
                } else {
                    // Advance to next line
                    model_next(c->model);
                    if (model_finished(c->model))
                        c->running = 0;
                    else
                        sync_view(c);
                }
                break;

            case SDLK_UP:
            case SDLK_w:
                if (line && line->type == LINE_CHOICE && line->choice_count > 0) {
                    c->choice_index--;
                    if (c->choice_index < 0)
                        c->choice_index = line->choice_count - 1;
                    view_set_choices(c->view, line, c->choice_index);
                }
                break;

            case SDLK_DOWN:
            case SDLK_s:
                if (line && line->type == LINE_CHOICE && line->choice_count > 0) {
                    c->choice_index = (c->choice_index + 1) % line->choice_count;
                    view_set_choices(c->view, line, c->choice_index);
                }
                break;
        }
    }
}
