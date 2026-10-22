// A fighter's joints drawn as a wire skeleton on the 3D engine: a line from each joint's parent (the
// root for the top joints) to the joint and a small cross at every joint. Positions are the game's
// world units (y down), drawn as 4.12 vertices: the caller's modelview scales them.
#ifndef T3DS_PRESENTATION_SKELETON_VIEW_H
#define T3DS_PRESENTATION_SKELETON_VIEW_H

#include "core/fighter/fighter_skeleton.h"

void skeleton_view_draw(const FighterSkeleton *skeleton);

#endif
