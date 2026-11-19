/*
 * scene.h - Module 3: the scene renderer (ramp, block, force vectors, camera).
 *
 * Scene_Draw() paints everything inside `sceneRect` and nothing outside it.
 * Force labels are drawn in screen space after the camera is closed, so text
 * keeps the same size when the user zooms.
 */
#ifndef SCENE_MODULE_H
#define SCENE_MODULE_H

#include "raylib.h"
#include "types.h"

/* p   : physical parameters (angle, mass, friction coefficients, ...)
 * cur : the block's state at the time being shown
 * v   : display options, zoom and pan
 * sceneRect : the screen rectangle the scene may draw in */
void Scene_Draw(const Params *p, Sample cur, const ViewOptions *v, Rectangle sceneRect);

#endif /* SCENE_MODULE_H */
