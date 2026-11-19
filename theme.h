/*
 * theme.h - the visual identity of the program, shared by every drawing module.
 *
 * One place defines the colours, so changing the look of the whole application
 * means editing the palette in theme.c. The helpers below draw the rounded
 * "cards" and pills the interface is built from.
 *
 * Raylib's own rounded-rectangle outline function changed its signature between
 * versions, so cards are drawn as a filled border colour with a slightly smaller
 * filled rectangle on top. That works on every Raylib 5.x release.
 */
#ifndef THEME_H
#define THEME_H

#include "raylib.h"

/* ----- surfaces ----- */
extern const Color C_BG;          /* window background                         */
extern const Color C_PANEL;       /* left panel and graph column               */
extern const Color C_CARD;        /* a card on a panel                         */
extern const Color C_CARD_HI;     /* raised elements (buttons, inputs)         */
extern const Color C_BORDER;      /* card and control outlines                 */
extern const Color C_PLOT_BG;     /* inside of a graph                         */
extern const Color C_GRID;        /* graph grid lines                          */
extern const Color C_CHIP;        /* translucent label background in the scene */

/* ----- text ----- */
extern const Color C_TEXT;        /* primary text                              */
extern const Color C_TEXT_DIM;    /* secondary text                            */
extern const Color C_TEXT_MUTED;  /* hints, units, axis labels                 */

/* ----- accents and status ----- */
extern const Color C_ACCENT;      /* primary blue: sliders, selected toggles   */
extern const Color C_ACCENT_HI;   /* lighter blue: hover, angle label          */
extern const Color C_GOOD;        /* green: Start, "sticks", at rest           */
extern const Color C_WARN;        /* amber: Pause, "slides", sliding           */

/* ----- physics colours (the same colour means the same thing everywhere) ----- */
extern const Color C_GRAV;        /* gravity / weight                          */
extern const Color C_NORM;        /* normal force, and the v graph             */
extern const Color C_FRIC;        /* friction, and the friction graph          */
extern const Color C_COMP;        /* gravity components                        */
extern const Color C_CRIT;        /* critical angle                            */

/* Linear blend of two colours: t = 0 gives a, t = 1 gives b. */
Color Theme_Mix(Color a, Color b, float t);

/* Filled rounded rectangle with a corner radius given in PIXELS. */
void Theme_Rounded(Rectangle r, float radius, Color c);

/* Rounded card: a 1 px border colour with a filled interior. Use an OPAQUE
 * fill (a translucent one would let the border show through). */
void Theme_Card(Rectangle r, float radius, Color fill, Color border);

/* A triangle that is visible whichever way its corners are ordered
 * (Raylib culls clockwise triangles, which is a common source of invisible shapes). */
void Theme_Tri(Vector2 a, Vector2 b, Vector2 c, Color col);

#endif /* THEME_H */
