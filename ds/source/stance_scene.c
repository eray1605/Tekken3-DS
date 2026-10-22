#include "stance_scene.h"

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>

#include "content/pose_tables_file.h"
#include "content/rig_file.h"
#include "content/spline_file.h"
#include "pad.h"
#include "platform/files.h"
#include "presentation/skeleton_view.h"

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
    free(*data);
    *data = load(scene, path, &size);
    if (!*data)
        return false;
    if (!motion_bank_parse(*data, size, bank)) {
        scene->error = path;
        return false;
    }
    return true;
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

// The next slot from `slot` in `step` direction (wrapping) that has an animation.
static void play_next_slot(StanceScene *scene, int step)
{
    int slot = scene->slot;
    for (int i = 0; i < MOTION_MOVE_SLOTS; i++) {
        slot = (slot + step + MOTION_MOVE_SLOTS) % MOTION_MOVE_SLOTS;
        if (play_slot(scene, slot))
            return;
    }
}

bool stance_scene_init(StanceScene *scene, int key)
{
    scene->error = NULL;
    scene->bank_type = -1;
    scene->own_data = NULL;
    scene->orbit = GAME_TURN / 8;
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
    scene->common_data = NULL;
    if (!load_bank(scene, COMMON_BANK, &scene->common_data, &scene->common))
        return false;
    return stance_scene_load_key(scene, key);
}

bool stance_scene_load_key(StanceScene *scene, int key)
{
    if (key < 0 || key >= scene->characters.count || scene->characters.keys[key].costume_slot < 0)
        return false;
    const CharacterEntry *entry = &scene->characters.keys[key];
    if (entry->bank_type != scene->bank_type) {
        scene->bank_type = -1;
        if (!load_bank(scene, entry->bank_type, &scene->own_data, &scene->own))
            return false;
        scene->bank_type = entry->bank_type;
    }
    static char path[32];
    snprintf(path, sizeof(path), RIG_FILE_FORMAT, entry->costume_slot);
    size_t size = 0;
    uint8_t *data = load(scene, path, &size);
    if (!data)
        return false;
    bool ok = rig_parse(data, size, &scene->rig);
    free(data);
    if (!ok) {
        scene->error = path;
        return false;
    }
    scene->key = key;
    scene->set = (MotionSet){&scene->own, &scene->common};
    fighter_skeleton_init(&scene->skeleton, &scene->tables, &scene->rig);
    scene->slot = STANCE_SLOT;
    if (!play_slot(scene, STANCE_SLOT))
        play_next_slot(scene, 1);
    return true;
}

// The next costume key with a model in `step` direction.
static void load_next_key(StanceScene *scene, int step)
{
    int count = scene->characters.count;
    int key = scene->key;
    for (int i = 0; i < count; i++) {
        key = (key + step + count) % count;
        if (scene->characters.keys[key].costume_slot >= 0) {
            stance_scene_load_key(scene, key);
            return;
        }
    }
}

void stance_scene_step(StanceScene *scene, uint16_t held, uint16_t pressed)
{
    if (held & PAD_LEFT)
        scene->orbit = (scene->orbit - ORBIT_PER_FRAME) & (GAME_TURN - 1);
    if (held & PAD_RIGHT)
        scene->orbit = (scene->orbit + ORBIT_PER_FRAME) & (GAME_TURN - 1);
    if (pressed & PAD_UP)
        load_next_key(scene, 1);
    if (pressed & PAD_DOWN)
        load_next_key(scene, -1);
    if (pressed & PAD_CIRCLE)
        play_next_slot(scene, 1);
    if (pressed & PAD_CROSS)
        play_next_slot(scene, -1);
    if (pressed & PAD_START)
        play_slot(scene, STANCE_SLOT);

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
}

static void draw_floor(void)
{
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

void stance_scene_draw(const StanceScene *scene)
{
    int orbit = ds_angle(scene->orbit);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(mulf32(sinLerp(orbit), CAMERA_DISTANCE), CAMERA_HEIGHT, mulf32(cosLerp(orbit), CAMERA_DISTANCE),
                 0, floattof32(0.45), 0, 0, inttof32(1), 0);
    glScalef32(WORLD_SCALE, -WORLD_SCALE, WORLD_SCALE);   // game units, y down
    draw_floor();
    skeleton_view_draw(&scene->skeleton);
}
