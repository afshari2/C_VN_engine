#include "vn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool vn_script_load(VN_Script* script, const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("Failed to open script: %s\n", path);
        return false;
    }

    char line[1024];
    char current_bg[256] = "";
    char current_sprite[256] = "";
    
    script->lines = NULL;
    script->count = 0;
    script->current = 0;

    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;

        if (line[0] == '#' || strlen(line) == 0) continue;

        if (strncmp(line, "BG ", 3) == 0) {
            strncpy(current_bg, line + 3, sizeof(current_bg) - 1);
        } else if (strncmp(line, "SPRITE ", 7) == 0) {
            char* sprite_path = strtok(line + 7, " ");
            if (sprite_path) {
                strncpy(current_sprite, sprite_path, sizeof(current_sprite) - 1);
            }
        } else if (strncmp(line, "SAY ", 4) == 0) {
            char* content = line + 4;
            char* speaker = strtok(content, " ");
            char* text = strtok(NULL, "");

            if (speaker && text) {
                script->lines = realloc(script->lines, sizeof(VN_Line) * (script->count + 1));
                VN_Line* vn_line = &script->lines[script->count];
                
                strncpy(vn_line->speaker, speaker, sizeof(vn_line->speaker) - 1);
                vn_line->speaker[sizeof(vn_line->speaker) - 1] = 0;
                
                strncpy(vn_line->text, text, sizeof(vn_line->text) - 1);
                vn_line->text[sizeof(vn_line->text) - 1] = 0;
                
                strncpy(vn_line->bg_path, current_bg, sizeof(vn_line->bg_path) - 1);
                vn_line->bg_path[sizeof(vn_line->bg_path) - 1] = 0;

                strncpy(vn_line->sprite_path, current_sprite, sizeof(vn_line->sprite_path) - 1);
                vn_line->sprite_path[sizeof(vn_line->sprite_path) - 1] = 0;
                
                script->count++;
            }
        }
    }

    fclose(file);
    return true;
}

void vn_script_free(VN_Script* script) {
    if (script->lines) {
        free(script->lines);
    }
    script->lines = NULL;
    script->count = 0;
}
