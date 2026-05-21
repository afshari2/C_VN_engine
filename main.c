#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "model.h"
#include "view.h"
#include "controller.h"

static void enter_app_directory(void) {
    char* base_path = SDL_GetBasePath();
    if (!base_path) return;

#ifdef _WIN32
    _chdir(base_path);
#else
    chdir(base_path);
#endif

    SDL_free(base_path);
}

int main(int argc, char* argv[]) {

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
#ifdef IMG_INIT_AVIF
    img_flags |= IMG_INIT_AVIF;
#endif
    IMG_Init(img_flags);
    enter_app_directory();

    // ── Model ────────────────────────────────────
    Model model;
    if (model_load(&model, "assets/story.txt") != 0) return 1;

    // ── View ─────────────────────────────────────
    View view;
    if (view_init(&view, "assets/fonts/flexi.ttf", 18) != 0) return 1;

    // ── Controller ───────────────────────────────
    Controller controller;
    controller_init(&controller, &model, &view);

    // ── Loop ─────────────────────────────────────
    SDL_Event e;
    while (controller.running) {
        while (SDL_PollEvent(&e))
            controller_handle_event(&controller, &e);

        view_update(&view);   // advance typewriter timer
        view_render(&view);
        SDL_Delay(16);
    }

    // ── Cleanup ──────────────────────────────────
    view_free(&view);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
