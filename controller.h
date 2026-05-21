#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "model.h"
#include "view.h"

typedef enum {
    CONTROLLER_MENU,
    CONTROLLER_GAME,
    CONTROLLER_SETTINGS
} ControllerScreen;

typedef struct {
    Model* model;
    View*  view;
    int    running;
    int    choice_index;
    int    menu_index;
    ControllerScreen screen;
    bool   audio_enabled;
    bool   fullscreen_enabled;
    char   story_path[256];
    char   save_path[512];
    char   settings_path[512];
} Controller;

// Wire model and view together, show the first line.
void controller_init(Controller* c, Model* m, View* v);

// Process one SDL event. Updates model and view as needed.
void controller_handle_event(Controller* c, SDL_Event* e);

#endif
