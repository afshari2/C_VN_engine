#ifndef MODEL_H
#define MODEL_H

#define MAX_LINES    512
#define MAX_LINE_LEN 256
#define MAX_LABELS   128
#define MAX_CHOICES  4
#define MAX_VARS     64
#define MAX_HISTORY  64

typedef enum {
    LINE_DIALOGUE,
    LINE_CHOICE,
    LINE_GOTO,
    LINE_SET,
    LINE_ADD,
    LINE_IF_GOTO
} LineType;

typedef struct {
    char name[64];
    int value;
} StoryVar;

typedef struct {
    char text[128];
    char target[64];
    int  target_index;
} ChoiceOption;

typedef struct {
    LineType type;
    char speaker[64];
    char text[MAX_LINE_LEN];
    char bg_path[MAX_LINE_LEN];
    char sprite_path[MAX_LINE_LEN];
    char sprite_pos[32];
    ChoiceOption choices[MAX_CHOICES];
    int choice_count;
    char target[64];
    int target_index;
    char var_name[64];
    char op[3];
    int value;
} StoryLine;

typedef struct {
    char name[64];
    int index;
} StoryLabel;

typedef struct {
    int current;
    StoryVar vars[MAX_VARS];
    int var_count;
} ModelState;

typedef struct {
    StoryLine lines[MAX_LINES];
    int       line_count;
    int       current;
    StoryLabel labels[MAX_LABELS];
    int       label_count;
    StoryVar  vars[MAX_VARS];
    int       var_count;
    ModelState history[MAX_HISTORY];
    int       history_count;
} Model;

// Load story from file. Returns 0 on success.
int  model_load(Model* m, const char* path);

// Returns the current resolved story line, or NULL when the story is finished.
const StoryLine* model_current_line(const Model* m);

// Advance to the next dialogue line. Returns 1 if story is finished.
int  model_next(Model* m);

// Jump to the selected choice target. Returns 1 if story is finished.
int  model_choose(Model* m, int choice_index);

// Returns 1 if story is finished.
int  model_finished(const Model* m);

// Save/load the current story position and variable state. Returns 0 on success.
int  model_save(const Model* m, const char* path);
int  model_load_save(Model* m, const char* path);

// Restore the previous visible story state. Returns 0 when a state was restored.
int  model_back(Model* m);

#endif
