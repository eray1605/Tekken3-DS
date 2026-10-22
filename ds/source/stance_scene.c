#include "stance_scene.h"

#include <nds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "content/model_file.h"
#include "content/pose_tables_file.h"
#include "content/rig_file.h"
#include "content/spline_file.h"
#include "platform/files.h"
#include "presentation/skeleton_view.h"
#include "stance_input.h"

#define GAME_TURN 4096
#define ORBIT_PER_FRAME 24
#define TIMER 0
#define COMMON_BANK 99
// One GL unit is 1024 game units (the game's y points down): a fighter stands about 1.7 units tall.
#define WORLD_SCALE inttof32(4)
#define CAMERA_DISTANCE floattof32(3.2)
#define CAMERA_HEIGHT floattof32(0.6)
#define FLOOR_TILES 8       // per side
#define FLOOR_TILE 1024     // game units

static inline int ds_angle(int32_t game_angle)
{
    return game_angle << 3;
}

static uint8_t *load(StanceScene *scene, const char *path, size_t *size)
{
    uint8_t *data = files_load(path, size);
    if (!data)
        scene->error = path;
    return data;
}

static bool load_bank(StanceScene *scene, int bank_type, uint8_t **data, MotionBank *bank)
{
    static char path[32];
    snprintf(path, sizeof(path), "motion/divmot%02d.bin", bank_type);
    size_t size = 0;
    *data = load(scene, path, &size);
    if (!*data)
        return false;
    if (!motion_bank_parse(*data, size, bank)) {
        scene->error = path;
        return false;
    }
    return true;
}

static void log_command(StanceScene *scene, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

// Appends a line to the command log (the oldest scrolls out), prefixed with the frame.
static void log_command(StanceScene *scene, const char *fmt, ...)
{
    for (int i = 0; i + 1 < STANCE_LOG_LINES; i++)
        memcpy(scene->log[i], scene->log[i + 1], STANCE_LOG_CHARS);
    char *line = scene->log[STANCE_LOG_LINES - 1];
    int used = snprintf(line, STANCE_LOG_CHARS, "%5lu ", (unsigned long)scene->steps);
    va_list args;
    va_start(args, fmt);
    vsnprintf(line + used, STANCE_LOG_CHARS - (size_t)used, fmt, args);
    va_end(args);
}

// The animation of a move slot that has one; false when the slot is empty in both banks.
static bool play_slot(StanceScene *scene, int slot)
{
    AnimStream stream;
    if (!motion_set_anim_for_slot(&scene->set, slot, &stream))
        return false;
    scene->slot = slot;
    scene->stream = stream;
    scene->frame = 0;
    scene->core_worst_us = 0;
    return true;
}

// The next slot from the current one in `step` direction (wrapping) that has an animation.
static void play_next_slot(StanceScene *scene, int step)
{
    int slot = scene->slot;
    for (int i = 0; i < MOTION_MOVE_SLOTS; i++) {
        slot = (slot + step + MOTION_MOVE_SLOTS) % MOTION_MOVE_SLOTS;
        if (play_slot(scene, slot))
            return;
    }
}

static void release_fighter(StanceFighter *fighter)
{
    fighter_view_release(&fighter->view);
    free(fighter->bank_data);
    free(fighter->model_data);
    fighter->bank_data = NULL;
    fighter->model_data = NULL;
    fighter->key = -1;
}

bool stance_scene_init(StanceScene *scene, int key)
{
    scene->error = NULL;
    scene->show_skeleton = false;
    scene->orbit = GAME_TURN / 8;
    scene->steps = 0;
    scene->shown = 0;
    memset(scene->log, 0, sizeof(scene->log));
    for (int f = 0; f < 2; f++) {
        scene->fighters[f] = (StanceFighter){.key = -1};
        scene->fighters[f].view.model = NULL;
    }
    size_t size = 0;
    uint8_t *data = load(scene, POSE_TABLES_FILE, &size);
    if (!data)
        return false;
    bool ok = pose_tables_parse(data, size, &scene->tables);
    free(data);
    if (!ok) {
        scene->error = POSE_TABLES_FILE;
        return false;
    }
    scene->spline_file = load(scene, SPLINE_TABLES_FILE, &size);
    if (!scene->spline_file)
        return false;
    if (!spline_tables_parse(scene->spline_file, size, &scene->spline)) {
        scene->error = SPLINE_TABLES_FILE;
        return false;
    }
    data = load(scene, CHARACTERS_FILE, &size);
    if (!data)
        return false;
    ok = characters_parse(data, size, &scene->characters);
    free(data);
    if (!ok) {
        scene->error = CHARACTERS_FILE;
        return false;
    }
    if (!load_bank(scene, COMMON_BANK, &scene->common_data, &scene->common))
        return false;
    return stance_scene_load_key(scene, key);
}

const CharacterEntry *stance_scene_character(const StanceScene *scene)
{
    return &scene->characters.keys[scene->fighters[scene->shown].key];
}

// A costume key that has a fighter: a model and a name (the table's last keys are empty records).
static bool playable(const StanceScene *scene, int key)
{
    return key >= 0 && key < scene->characters.count && scene->characters.keys[key].costume_slot >= 0 &&
           scene->characters.keys[key].name[0] != '\0';
}

// Loads everything of `key` into `fighter`; false (`error` set) when a file is missing or bad.
static bool load_fighter(StanceScene *scene, StanceFighter *fighter, int key)
{
    const CharacterEntry *entry = &scene->characters.keys[key];
    if (!load_bank(scene, entry->bank_type, &fighter->bank_data, &fighter->bank))
        return false;
    static char path[32];
    snprintf(path, sizeof(path), RIG_FILE_FORMAT, entry->costume_slot);
    size_t size = 0;
    uint8_t *data = load(scene, path, &size);
    if (!data)
        return false;
    bool ok = rig_parse(data, size, &fighter->rig);
    free(data);
    if (!ok) {
        scene->error = path;
        return false;
    }
    snprintf(path, sizeof(path), MODEL_FILE_FORMAT, entry->costume_slot);
    fighter->model_data = load(scene, path, &size);
    if (!fighter->model_data)
        return false;
    if (!model_parse(fighter->model_data, size, &fighter->model) || !fighter_view_init(&fighter->view, &fighter->model)) {
        scene->error = path;
        return false;
    }
    fighter->key = key;
    return true;
}

bool stance_scene_load_key(StanceScene *scene, int key)
{
    if (!playable(scene, key))
        return false;
    int spare = scene->fighters[scene->shown].key < 0 ? scene->shown : 1 - scene->shown;
    StanceFighter *fighter = &scene->fighters[spare];
    release_fighter(fighter);
    if (!load_fighter(scene, fighter, key)) {
        release_fighter(fighter);
        return false;
    }
    if (spare != scene->shown)
        release_fighter(&scene->fighters[scene->shown]);
    scene->shown = spare;
    scene->error = NULL;
    scene->set = (MotionSet){&fighter->bank, &scene->common};
    fighter_skeleton_init(&scene->skeleton, &scene->tables, &fighter->rig);
    scene->slot = STANCE_SLOT;
    if (!play_slot(scene, STANCE_SLOT))
        play_next_slot(scene, 1);
    return true;
}

// The next playable costume key in `step` direction; false when it would not load.
static bool load_next_key(StanceScene *scene, int step)
{
    int count = scene->characters.count;
    int key = scene->fighters[scene->shown].key;
    for (int i = 0; i < count; i++) {
        key = (key + step + count) % count;
        if (playable(scene, key))
            return stance_scene_load_key(scene, key);
    }
    return false;
}

void stance_scene_step(StanceScene *scene, const PadState *pad)
{
    StanceInput in = stance_input_from_pad(pad);
    scene->orbit = (scene->orbit + in.orbit * ORBIT_PER_FRAME) & (GAME_TURN - 1);
    if (in.key_step) {
        if (load_next_key(scene, in.key_step))
            log_command(scene, "%s key %d %s", in.key_step > 0 ? "Up" : "Dn", scene->fighters[scene->shown].key,
                        stance_scene_character(scene)->name);
        else
            log_command(scene, "key failed: %.18s", scene->error ? scene->error : "?");
    }
    if (in.slot_step) {
        play_next_slot(scene, in.slot_step);
        log_command(scene, "%s slot %d (%d frames)", in.slot_step > 0 ? "A" : "B", scene->slot, scene->stream.frames);
    }
    if (in.stance) {
        play_slot(scene, STANCE_SLOT);
        log_command(scene, "Start stance, slot %d", scene->slot);
    }
    if (in.toggle_view) {
        scene->show_skeleton = !scene->show_skeleton;
        log_command(scene, "Select %s", scene->show_skeleton ? "skeleton" : "model");
    }

    // The core's share: sample the frame, pose the skeleton in place.
    static const Vec3 ANCHOR = {{0, 0, 0}};
    cpuStartTiming(TIMER);
    int32_t pose[ANIM_CHANNELS];
    anim_sample_pose(&scene->stream, scene->frame, &scene->spline, pose);
    uint32_t sampled = cpuGetTiming();
    fighter_skeleton_update(&scene->skeleton, pose, &ANCHOR, 0, 0);
    uint32_t total = cpuEndTiming();
    scene->sample_us = timerTicks2usec(sampled);
    scene->core_us = timerTicks2usec(total);
    if (scene->core_us > scene->core_worst_us)
        scene->core_worst_us = scene->core_us;
    scene->frame = (scene->frame + 1) % scene->stream.frames;
    scene->steps++;
}

static void draw_floor(void)
{
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glBegin(GL_QUADS);
    for (int z = -FLOOR_TILES / 2; z < FLOOR_TILES / 2; z++) {
        for (int x = -FLOOR_TILES / 2; x < FLOOR_TILES / 2; x++) {
            if ((x + z) & 1)
                glColor3b(50, 50, 70);
            else
                glColor3b(90, 90, 115);
            glVertex3v16((v16)(x * FLOOR_TILE), 0, (v16)(z * FLOOR_TILE));
            glVertex3v16((v16)(x * FLOOR_TILE), 0, (v16)((z + 1) * FLOOR_TILE));
            glVertex3v16((v16)((x + 1) * FLOOR_TILE), 0, (v16)((z + 1) * FLOOR_TILE));
            glVertex3v16((v16)((x + 1) * FLOOR_TILE), 0, (v16)(z * FLOOR_TILE));
        }
    }
    glEnd();
}

void stance_scene_draw(StanceScene *scene)
{
    int orbit = ds_angle(scene->orbit);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(mulf32(sinLerp(orbit), CAMERA_DISTANCE), CAMERA_HEIGHT, mulf32(cosLerp(orbit), CAMERA_DISTANCE),
                 0, floattof32(0.45), 0, 0, inttof32(1), 0);
    glScalef32(WORLD_SCALE, -WORLD_SCALE, WORLD_SCALE);   // game units, y down
    draw_floor();
    cpuStartTiming(TIMER);
    FighterView *view = &scene->fighters[scene->shown].view;
    if (scene->show_skeleton || !view->model)
        skeleton_view_draw(&scene->skeleton);
    else
        fighter_view_draw(view, &scene->skeleton);
    scene->draw_us = timerTicks2usec(cpuEndTiming());
}
