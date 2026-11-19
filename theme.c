/*
 * theme.c - the colour palette and the small drawing helpers behind the look.
 *
 * Design notes:
 *   - A dark blue-grey interface: the scene and graphs are easier to read for
 *     long sessions, and the physics colours (red, blue, green, amber, purple)
 *     pop against it.
 *   - Surfaces get lighter as they come forward: background < panel < card < control.
 *   - A physical quantity keeps ONE colour everywhere (for example friction is
 *     always green: its arrow, its graph and its HUD row).
 */
#include "theme.h"
#include <math.h>

/* ----- surfaces ----- */
const Color C_BG        = {  10,  15,  28, 255 };
const Color C_PANEL     = {  15,  22,  40, 255 };
const Color C_CARD      = {  22,  32,  56, 255 };
const Color C_CARD_HI   = {  34,  48,  82, 255 };
const Color C_BORDER    = {  44,  60,  96, 255 };
const Color C_PLOT_BG   = {  11,  17,  31, 255 };
const Color C_GRID      = {  31,  44,  74, 255 };
const Color C_CHIP      = {   8,  12,  24, 225 };

/* ----- text ----- */
const Color C_TEXT       = { 232, 238, 248, 255 };
const Color C_TEXT_DIM   = { 160, 174, 200, 255 };
const Color C_TEXT_MUTED = { 112, 128, 158, 255 };

/* ----- accents and status ----- */
const Color C_ACCENT    = {  76, 141, 255, 255 };
const Color C_ACCENT_HI = { 126, 176, 255, 255 };
const Color C_GOOD      = {  52, 199, 129, 255 };
const Color C_WARN      = { 245, 165,  36, 255 };

/* ----- physics colours ----- */
const Color C_GRAV = { 255, 107, 107, 255 };
const Color C_NORM = {  90, 170, 255, 255 };
const Color C_FRIC = {  61, 220, 151, 255 };
const Color C_COMP = { 199, 146, 234, 255 };
const Color C_CRIT = { 255, 196,  87, 255 };

Color Theme_Mix(Color a, Color b, float t)
{
    Color c;
    c.r = (unsigned char)(a.r + (b.r - a.r) * t);
    c.g = (unsigned char)(a.g + (b.g - a.g) * t);
    c.b = (unsigned char)(a.b + (b.b - a.b) * t);
    c.a = (unsigned char)(a.a + (b.a - a.a) * t);
    return c;
}

void Theme_Rounded(Rectangle r, float radius, Color c)
{
    /* Raylib's "roundness" is relative to half of the shorter side (0..1). */
    float half = fminf(r.width, r.height) * 0.5f;
    float roundness = (half <= 0.0f) ? 0.0f : fminf(1.0f, radius / half);
    DrawRectangleRounded(r, roundness, 10, c);
}

void Theme_Card(Rectangle r, float radius, Color fill, Color border)
{
    Theme_Rounded(r, radius, border);
    Theme_Rounded((Rectangle){ r.x + 1, r.y + 1, r.width - 2, r.height - 2 }, radius - 1.0f, fill);
}

void Theme_Tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    DrawTriangle(a, b, c, col);   /* one of these two windings is culled, the other is drawn */
    DrawTriangle(a, c, b, col);
}
