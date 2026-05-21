#include "model.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __ANDROID__
#include <SDL2/SDL.h>
#endif

typedef struct {
#ifdef __ANDROID__
    SDL_RWops* rw;
#else
    FILE* file;
#endif
} StoryFile;

static int story_file_open(StoryFile* sf, const char* path) {
#ifdef __ANDROID__
    sf->rw = SDL_RWFromFile(path, "rb");
    return sf->rw ? 0 : 1;
#else
    sf->file = fopen(path, "r");
    return sf->file ? 0 : 1;
#endif
}

static void story_file_close(StoryFile* sf) {
#ifdef __ANDROID__
    if (sf->rw) SDL_RWclose(sf->rw);
#else
    if (sf->file) fclose(sf->file);
#endif
}

static char* story_file_gets(StoryFile* sf, char* buf, size_t buf_size) {
#ifdef __ANDROID__
    if (!sf->rw || buf_size == 0) return NULL;

    size_t pos = 0;
    while (pos + 1 < buf_size) {
        char ch = '\0';
        size_t read = SDL_RWread(sf->rw, &ch, 1, 1);
        if (read != 1) break;

        buf[pos++] = ch;
        if (ch == '\n') break;
    }

    if (pos == 0) return NULL;
    buf[pos] = '\0';
    return buf;
#else
    return fgets(buf, (int)buf_size, sf->file);
#endif
}

static void copy_text(char* dst, size_t dst_size, const char* src) {
    if (dst_size == 0) return;
    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void add_label(Model* m, const char* name) {
    if (m->label_count >= MAX_LABELS || !name || !name[0]) return;
    copy_text(m->labels[m->label_count].name, sizeof(m->labels[m->label_count].name), name);
    m->labels[m->label_count].index = m->line_count;
    m->label_count++;
}

static int find_label(const Model* m, const char* name) {
    for (int i = 0; i < m->label_count; i++) {
        if (strcmp(m->labels[i].name, name) == 0)
            return m->labels[i].index;
    }
    return -1;
}

static int find_var(const Model* m, const char* name) {
    for (int i = 0; i < m->var_count; i++) {
        if (strcmp(m->vars[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int get_var(const Model* m, const char* name) {
    int index = find_var(m, name);
    return index >= 0 ? m->vars[index].value : 0;
}

static void set_var(Model* m, const char* name, int value) {
    if (!name || !name[0]) return;

    int index = find_var(m, name);
    if (index < 0) {
        if (m->var_count >= MAX_VARS) return;
        index = m->var_count++;
        copy_text(m->vars[index].name, sizeof(m->vars[index].name), name);
    }
    m->vars[index].value = value;
}

static void capture_state(const Model* m, ModelState* state) {
    state->current = m->current;
    state->var_count = m->var_count;
    for (int i = 0; i < m->var_count; i++)
        state->vars[i] = m->vars[i];
}

static void restore_state(Model* m, const ModelState* state) {
    m->current = state->current;
    m->var_count = state->var_count;
    for (int i = 0; i < state->var_count; i++)
        m->vars[i] = state->vars[i];
}

static void push_history(Model* m) {
    if (m->current >= m->line_count) return;

    if (m->history_count >= MAX_HISTORY) {
        memmove(&m->history[0], &m->history[1],
                sizeof(m->history[0]) * (MAX_HISTORY - 1));
        m->history_count = MAX_HISTORY - 1;
    }

    capture_state(m, &m->history[m->history_count++]);
}

static bool compare_int(int left, const char* op, int right) {
    if (strcmp(op, "==") == 0) return left == right;
    if (strcmp(op, "!=") == 0) return left != right;
    if (strcmp(op, ">=") == 0) return left >= right;
    if (strcmp(op, "<=") == 0) return left <= right;
    if (strcmp(op, ">") == 0) return left > right;
    if (strcmp(op, "<") == 0) return left < right;
    return false;
}

static void resolve_targets(Model* m) {
    for (int i = 0; i < m->line_count; i++) {
        StoryLine* sl = &m->lines[i];
        if (sl->type == LINE_GOTO || sl->type == LINE_IF_GOTO)
            sl->target_index = find_label(m, sl->target);
        else if (sl->type == LINE_CHOICE) {
            for (int c = 0; c < sl->choice_count; c++)
                sl->choices[c].target_index = find_label(m, sl->choices[c].target);
        }
    }
}

static void init_line(StoryLine* sl, LineType type, const char* bg,
                      const char* sprite, const char* sprite_pos) {
    memset(sl, 0, sizeof(*sl));
    sl->type = type;
    sl->target_index = -1;
    copy_text(sl->bg_path, sizeof(sl->bg_path), bg);
    copy_text(sl->sprite_path, sizeof(sl->sprite_path), sprite);
    copy_text(sl->sprite_pos, sizeof(sl->sprite_pos), sprite_pos);
    for (int i = 0; i < MAX_CHOICES; i++)
        sl->choices[i].target_index = -1;
}

static void skip_control_lines(Model* m) {
    while (m->current < m->line_count) {
        StoryLine* sl = &m->lines[m->current];
        switch (sl->type) {
            case LINE_GOTO:
                if (sl->target_index >= 0)
                    m->current = sl->target_index;
                else
                    m->current++;
                break;

            case LINE_SET:
                set_var(m, sl->var_name, sl->value);
                m->current++;
                break;

            case LINE_ADD:
                set_var(m, sl->var_name, get_var(m, sl->var_name) + sl->value);
                m->current++;
                break;

            case LINE_IF_GOTO:
                if (compare_int(get_var(m, sl->var_name), sl->op, sl->value) &&
                    sl->target_index >= 0) {
                    m->current = sl->target_index;
                } else {
                    m->current++;
                }
                break;

            default:
                return;
        }
    }
}

int model_load(Model* m, const char* path) {
    m->line_count = 0;
    m->current    = 0;
    m->label_count = 0;
    m->var_count = 0;
    m->history_count = 0;

    StoryFile story_file;
    if (story_file_open(&story_file, path) != 0) {
        fprintf(stderr, "[Model] Cannot open story file: %s\n", path);
        return 1;
    }

    char buf[MAX_LINE_LEN];
    char current_bg[MAX_LINE_LEN] = "";
    char current_sprite[MAX_LINE_LEN] = "";
    char current_sprite_pos[32] = "center";

    while (story_file_gets(&story_file, buf, sizeof(buf)) && m->line_count < MAX_LINES) {
        buf[strcspn(buf, "\r\n")] = '\0';

        // Skip blank lines and comments
        if (buf[0] == '\0' || buf[0] == '#') continue;

        if (strcmp(buf, "END") == 0) break;

        if (strncmp(buf, "LABEL ", 6) == 0) {
            add_label(m, buf + 6);
            continue;
        }

        if (strncmp(buf, "GOTO ", 5) == 0) {
            StoryLine* sl = &m->lines[m->line_count++];
            init_line(sl, LINE_GOTO, current_bg, current_sprite, current_sprite_pos);
            copy_text(sl->target, sizeof(sl->target), buf + 5);
            continue;
        }

        if (strncmp(buf, "SET ", 4) == 0) {
            char var_name[64];
            int value = 0;
            if (sscanf(buf + 4, "%63s %d", var_name, &value) == 2) {
                StoryLine* sl = &m->lines[m->line_count++];
                init_line(sl, LINE_SET, current_bg, current_sprite, current_sprite_pos);
                copy_text(sl->var_name, sizeof(sl->var_name), var_name);
                sl->value = value;
            }
            continue;
        }

        if (strncmp(buf, "ADD ", 4) == 0) {
            char var_name[64];
            int value = 0;
            if (sscanf(buf + 4, "%63s %d", var_name, &value) == 2) {
                StoryLine* sl = &m->lines[m->line_count++];
                init_line(sl, LINE_ADD, current_bg, current_sprite, current_sprite_pos);
                copy_text(sl->var_name, sizeof(sl->var_name), var_name);
                sl->value = value;
            }
            continue;
        }

        if (strncmp(buf, "IF ", 3) == 0) {
            char var_name[64];
            char op[3];
            int value = 0;
            char target[64];
            if (sscanf(buf + 3, "%63s %2s %d GOTO %63s",
                       var_name, op, &value, target) == 4) {
                StoryLine* sl = &m->lines[m->line_count++];
                init_line(sl, LINE_IF_GOTO, current_bg, current_sprite, current_sprite_pos);
                copy_text(sl->var_name, sizeof(sl->var_name), var_name);
                copy_text(sl->op, sizeof(sl->op), op);
                sl->value = value;
                copy_text(sl->target, sizeof(sl->target), target);
            }
            continue;
        }

        if (strncmp(buf, "BG ", 3) == 0) {
            copy_text(current_bg, sizeof(current_bg), buf + 3);
            continue;
        }

        if (strncmp(buf, "SPRITE ", 7) == 0) {
            char* sprite = buf + 7;
            char* pos = strchr(sprite, ' ');
            if (pos) {
                *pos++ = '\0';
                while (*pos == ' ') pos++;
                copy_text(current_sprite_pos, sizeof(current_sprite_pos),
                          *pos ? pos : "center");
            } else {
                copy_text(current_sprite_pos, sizeof(current_sprite_pos), "center");
            }
            copy_text(current_sprite, sizeof(current_sprite), sprite);
            continue;
        }

        if (strncmp(buf, "SAY ", 4) == 0) {
            char* speaker = buf + 4;
            char* text = strchr(speaker, ' ');
            if (!text) continue;

            *text++ = '\0';
            while (*text == ' ') text++;

            StoryLine* sl = &m->lines[m->line_count++];
            init_line(sl, LINE_DIALOGUE, current_bg, current_sprite, current_sprite_pos);
            copy_text(sl->speaker, sizeof(sl->speaker), speaker);
            copy_text(sl->text, sizeof(sl->text), text);
            continue;
        }

        if (strcmp(buf, "CHOICE") == 0) {
            StoryLine* sl = &m->lines[m->line_count++];
            init_line(sl, LINE_CHOICE, current_bg, current_sprite, current_sprite_pos);

            while (story_file_gets(&story_file, buf, sizeof(buf))) {
                buf[strcspn(buf, "\r\n")] = '\0';
                if (buf[0] == '\0' || buf[0] == '#') continue;
                if (strcmp(buf, "ENDCHOICE") == 0) break;
                if (strncmp(buf, "OPTION ", 7) != 0 ||
                    sl->choice_count >= MAX_CHOICES) {
                    continue;
                }

                char* option = buf + 7;
                char* arrow = strstr(option, "->");
                if (!arrow) continue;

                *arrow = '\0';
                arrow += 2;
                while (*arrow == ' ') arrow++;

                char* option_end = option + strlen(option);
                while (option_end > option && option_end[-1] == ' ')
                    *--option_end = '\0';

                ChoiceOption* choice = &sl->choices[sl->choice_count++];
                copy_text(choice->text, sizeof(choice->text), option);
                copy_text(choice->target, sizeof(choice->target), arrow);
            }
            continue;
        }

        // Backward compatibility with the original story.txt format.
        if (buf[0] == '@') {
            char path_buf[MAX_LINE_LEN];
            snprintf(path_buf, sizeof(path_buf), "assets/images/%s", buf + 1);
            copy_text(current_bg, sizeof(current_bg), path_buf);
            continue;
        }

        StoryLine* sl = &m->lines[m->line_count++];
        init_line(sl, LINE_DIALOGUE, current_bg, current_sprite, current_sprite_pos);
        copy_text(sl->speaker, sizeof(sl->speaker), "");
        copy_text(sl->text, sizeof(sl->text), buf);
    }

    story_file_close(&story_file);
    resolve_targets(m);
    skip_control_lines(m);
    return 0;
}

const StoryLine* model_current_line(const Model* m) {
    if (m->current >= m->line_count) return NULL;
    return &m->lines[m->current];
}

int model_next(Model* m) {
    if (m->current >= m->line_count) return 1;
    push_history(m);
    m->current++;
    skip_control_lines(m);
    return model_finished(m);
}

int model_choose(Model* m, int choice_index) {
    const StoryLine* sl = model_current_line(m);
    if (!sl || sl->type != LINE_CHOICE ||
        choice_index < 0 || choice_index >= sl->choice_count) {
        return model_finished(m);
    }

    push_history(m);

    int target = sl->choices[choice_index].target_index;
    if (target >= 0)
        m->current = target;
    else
        m->current++;
    skip_control_lines(m);
    return model_finished(m);
}

int model_finished(const Model* m) {
    if (m->current >= m->line_count) return 1;
    const StoryLine* sl = &m->lines[m->current];
    if (sl->type == LINE_CHOICE && sl->choice_count == 0) return 1;
    return 0;
}

int model_save(const Model* m, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[Model] Cannot write save file: %s\n", path);
        return 1;
    }

    fprintf(f, "CVN_SAVE 1\n");
    fprintf(f, "current %d\n", m->current);
    fprintf(f, "vars %d\n", m->var_count);
    for (int i = 0; i < m->var_count; i++)
        fprintf(f, "%s %d\n", m->vars[i].name, m->vars[i].value);

    fclose(f);
    return 0;
}

int model_load_save(Model* m, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[Model] Cannot open save file: %s\n", path);
        return 1;
    }

    char tag[32];
    int version = 0;
    if (fscanf(f, "%31s %d", tag, &version) != 2 ||
        strcmp(tag, "CVN_SAVE") != 0 || version != 1) {
        fprintf(stderr, "[Model] Invalid save file: %s\n", path);
        fclose(f);
        return 1;
    }

    char key[32];
    int current = 0;
    int var_count = 0;
    if (fscanf(f, "%31s %d", key, &current) != 2 ||
        strcmp(key, "current") != 0 ||
        fscanf(f, "%31s %d", key, &var_count) != 2 ||
        strcmp(key, "vars") != 0 ||
        current < 0 || current >= m->line_count ||
        var_count < 0 || var_count > MAX_VARS) {
        fprintf(stderr, "[Model] Corrupt save file: %s\n", path);
        fclose(f);
        return 1;
    }

    ModelState state;
    state.current = current;
    state.var_count = var_count;

    for (int i = 0; i < var_count; i++) {
        if (fscanf(f, "%63s %d", state.vars[i].name, &state.vars[i].value) != 2) {
            fprintf(stderr, "[Model] Corrupt save variables: %s\n", path);
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    restore_state(m, &state);
    m->history_count = 0;
    return 0;
}

int model_back(Model* m) {
    if (m->history_count <= 0)
        return 1;

    m->history_count--;
    restore_state(m, &m->history[m->history_count]);
    return 0;
}
