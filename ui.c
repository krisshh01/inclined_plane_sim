/*
 * ui.c - Module 4: the control panel. This is the ONLY file that defines
 * RAYGUI_IMPLEMENTATION, so Raygui's code is compiled exactly once.
 *
 * STRUCTURE
 * ---------
 *   ApplyTheme()        colours every Raygui control to match theme.c
 *   DrawLeftPanel()     header, PARAMETERS, CRITICAL ANGLE, VECTOR LENGTH, SHOW
 *   DrawTransportBar()  Start/Pause/Resume, Reset, step, time box, zoom
 *   HandleShortcuts()   Space, R, Left/Right
 *   UI_Draw()           calls the three above and reports parameter changes
 *
 * Raygui is an immediate-mode GUI: every control is a function call made every
 * frame that draws itself AND returns whether it was used. There is no widget
 * tree and no callbacks, so "state" is just the variables passed in by pointer.
 */
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "ui.h"
#include "fonts.h"
#include "theme.h"
#include "physics.h"      /* Physics_CriticalAngle(): a pure function, no drawing */
#include "sim.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZOOM_MIN 0.3f
#define ZOOM_MAX 4.0f

/* ------------------------------------------------- state kept between frames */
static const float stepSizes[3] = { 0.01f, 0.1f, 1.0f };   /* choices for the step buttons (s) */
static int   stepSel  = 1;                                 /* index into stepSizes (default 0.1 s) */
static char  timeText[32] = "0.000";                       /* text shown in the time box        */
static bool  timeEdit = false;                             /* true while the user is typing     */

/* ===================================================================== */
/*  Theme                                                                */
/* ===================================================================== */

/* Raygui stores colours as 0xRRGGBBAA integers. */
static int Hex(Color c) { return ColorToInt(c); }

/*
 * Colour every Raygui control. Raygui keeps a "style" table; a property set on
 * DEFAULT is copied to ALL controls, so those calls come first and the
 * control-specific overrides after them.
 *
 * Each control has colours for four states, named *_NORMAL, *_FOCUSED (mouse
 * over), *_PRESSED (being clicked, or "on" for toggles) and *_DISABLED.
 */
static void ApplyTheme(void)
{
    /* ---- every control ---- */
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,      Hex(C_PANEL));
    GuiSetStyle(DEFAULT, LINE_COLOR,            Hex(C_BORDER));

    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,   Hex(C_BORDER));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,     Hex(C_CARD_HI));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,     Hex(C_TEXT));

    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED,  Hex(C_ACCENT_HI));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,    Hex((Color){ 48, 66, 108, 255 }));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,    Hex(WHITE));

    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED,  Hex(C_ACCENT));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,    Hex(C_ACCENT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,    Hex(WHITE));

    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, Hex((Color){ 36, 48, 76, 255 }));
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED,   Hex((Color){ 22, 31, 52, 255 }));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,   Hex(C_TEXT_MUTED));

    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, TEXT_SIZE, FONT_UI_SIZE);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 0);

    /* ---- slider: dark track, blue fill ----
     * Raygui draws the FILL with BASE_COLOR_PRESSED normally, TEXT_COLOR_FOCUSED
     * on hover and TEXT_COLOR_PRESSED while dragging, so those are set to blues
     * (otherwise the default white would flash on hover). */
    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL,   Hex((Color){ 12, 18, 34, 255 }));
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED,  Hex(C_ACCENT));
    GuiSetStyle(SLIDER, TEXT_COLOR_FOCUSED,  Hex(C_ACCENT_HI));
    GuiSetStyle(SLIDER, TEXT_COLOR_PRESSED,  Hex(C_ACCENT_HI));
    GuiSetStyle(SLIDER, SLIDER_PADDING, 2);

    /* ---- check box: the tick is drawn in the TEXT colour of its state ---- */
    GuiSetStyle(CHECKBOX, BORDER_COLOR_NORMAL, Hex((Color){ 96, 114, 158, 255 }));
    GuiSetStyle(CHECKBOX, TEXT_COLOR_NORMAL,   Hex(C_ACCENT));
    GuiSetStyle(CHECKBOX, TEXT_COLOR_FOCUSED,  Hex(C_ACCENT_HI));
    GuiSetStyle(CHECKBOX, TEXT_COLOR_PRESSED,  Hex(C_ACCENT));
    GuiSetStyle(CHECKBOX, CHECK_PADDING, 3);

    /* ---- text box (the time field): while typing it turns deep blue with a blue outline ---- */
    GuiSetStyle(TEXTBOX, BASE_COLOR_PRESSED,   Hex((Color){ 22, 44, 96, 255 }));
    GuiSetStyle(TEXTBOX, BORDER_COLOR_PRESSED, Hex(C_ACCENT));

    /* ---- toggle group: a little air between the segments ---- */
    GuiSetStyle(TOGGLE, GROUP_PADDING, 4);

    /* ---- rounded look ----
     * Raygui draws square boxes. For buttons, toggles, the text box, sliders and
     * check boxes, its boxes are made fully transparent and ui.c draws rounded
     * surfaces itself (Surface(), DrawSlider(), CheckRow()). Raygui still does
     * the input handling and draws the text on top. */
    const int clear = Hex(BLANK);
    const int boxProps[8] = { BORDER_COLOR_NORMAL, BASE_COLOR_NORMAL, BORDER_COLOR_FOCUSED, BASE_COLOR_FOCUSED,
                              BORDER_COLOR_PRESSED, BASE_COLOR_PRESSED, BORDER_COLOR_DISABLED, BASE_COLOR_DISABLED };
    for (int i = 0; i < 8; i++) {
        GuiSetStyle(BUTTON, boxProps[i], clear);
        GuiSetStyle(TOGGLE, boxProps[i], clear);
        GuiSetStyle(SLIDER, boxProps[i], clear);
        GuiSetStyle(CHECKBOX, boxProps[i], clear);
    }
    GuiSetStyle(SLIDER,   TEXT_COLOR_FOCUSED,  clear);     /* the slider fill */
    GuiSetStyle(SLIDER,   TEXT_COLOR_PRESSED,  clear);
    GuiSetStyle(SLIDER,   TEXT_COLOR_DISABLED, clear);
    GuiSetStyle(CHECKBOX, TEXT_COLOR_NORMAL,   clear);     /* the tick */
    GuiSetStyle(CHECKBOX, TEXT_COLOR_FOCUSED,  clear);
    GuiSetStyle(CHECKBOX, TEXT_COLOR_PRESSED,  clear);
    GuiSetStyle(CHECKBOX, TEXT_PADDING, 10);
    GuiSetStyle(TEXTBOX,  BORDER_WIDTH, 0);                /* the text cursor keeps BORDER_COLOR_PRESSED */
    GuiSetStyle(TEXTBOX,  BASE_COLOR_PRESSED, clear);
    GuiSetStyle(TEXTBOX,  TEXT_PADDING, 10);
    GuiSetStyle(LABEL,    TEXT_COLOR_NORMAL,  Hex(C_TEXT_DIM));   /* check box labels */
    GuiSetStyle(LABEL,    TEXT_COLOR_FOCUSED, Hex(C_TEXT));
    GuiSetStyle(LABEL,    TEXT_COLOR_PRESSED, Hex(C_TEXT));
}

void UI_Init(void)
{
    GuiSetFont(Fonts_Get(FONT_UI_SIZE));     /* every control uses the embedded font */
    ApplyTheme();
}

/* ===================================================================== */
/*  Small building blocks                                                */
/* ===================================================================== */

static float ClampZoom(float z) { return z < ZOOM_MIN ? ZOOM_MIN : (z > ZOOM_MAX ? ZOOM_MAX : z); }

static const Color C_FIELD = { 12, 18, 34, 255 };   /* sunken background of tracks and input fields */

/* Mouse interaction with a rectangle, used to shade the surfaces drawn below. */
typedef enum { HOT_NONE, HOT_HOVER, HOT_DOWN } Hot;
static Hot HotState(Rectangle r)
{
    if (!CheckCollisionPointRec(GetMousePosition(), r)) return HOT_NONE;
    return IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? HOT_DOWN : HOT_HOVER;
}

/* A rounded control surface: lighter on hover, darker while held, with a faint
 * highlight along the top edge so it reads as raised. */
static void Surface(Rectangle r, Color fill, Color border, Hot h)
{
    if (h == HOT_HOVER)     { fill = Theme_Mix(fill, WHITE, 0.10f); border = Theme_Mix(border, WHITE, 0.25f); }
    else if (h == HOT_DOWN) { fill = Theme_Mix(fill, BLACK, 0.18f); }
    Theme_Card(r, 8, fill, border);
    DrawRectangle((int)r.x + 8, (int)r.y + 1, (int)r.width - 16, 1, Fade(WHITE, 0.07f));
}

/* A regular button: rounded surface underneath, Raygui on top for the click and the text. */
static bool Button(Rectangle r, const char *text)
{
    Surface(r, C_CARD_HI, C_BORDER, HotState(r));
    return GuiButton(r, text) != 0;
}

/*
 * A button with its own colours (used for the Start/Pause/Resume button).
 * Raygui has no per-call colour argument, so the text style is saved, changed,
 * the button drawn, and the style restored.
 */
static bool ColoredButton(Rectangle r, const char *text, Color base, Color textCol)
{
    static const int props[3] = { TEXT_COLOR_NORMAL, TEXT_COLOR_FOCUSED, TEXT_COLOR_PRESSED };
    int saved[3];
    for (int i = 0; i < 3; i++) saved[i] = GuiGetStyle(BUTTON, props[i]);

    Hot h = HotState(r);
    Color fill = h == HOT_HOVER ? Theme_Mix(base, WHITE, 0.15f) : h == HOT_DOWN ? Theme_Mix(base, BLACK, 0.18f) : base;
    Theme_Rounded((Rectangle){ r.x, r.y + 2, r.width, r.height }, 8, Fade(base, 0.25f));   /* soft drop glow */
    Theme_Rounded(r, 8, fill);
    DrawRectangle((int)r.x + 8, (int)r.y + 1, (int)r.width - 16, 1, Fade(WHITE, 0.30f));

    for (int i = 0; i < 3; i++) GuiSetStyle(BUTTON, props[i], Hex(textCol));
    GuiSetFont(Fonts_GetBold(FONT_UI_SIZE));
    bool clicked = GuiButton(r, text) != 0;
    GuiSetFont(Fonts_Get(FONT_UI_SIZE));
    for (int i = 0; i < 3; i++) GuiSetStyle(BUTTON, props[i], saved[i]);
    return clicked;
}

/* Rounded slider: a thin sunken track, an accent fill and a round knob.
 * The knob centre matches where Raygui maps the value across `bar`. */
static float SliderX(Rectangle bar, float val, float lo, float hi)
{
    float t = (val - lo) / (hi - lo);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float pad = (float)GuiGetStyle(SLIDER, SLIDER_PADDING) + 1.0f;
    return bar.x + pad + t * (bar.width - 2.0f * pad);
}

static void DrawSlider(Rectangle bar, float val, float lo, float hi, Hot h)
{
    float cy = bar.y + bar.height * 0.5f;
    float kx = SliderX(bar, val, lo, hi);
    Theme_Rounded((Rectangle){ bar.x, cy - 3.0f, bar.width, 6.0f }, 3.0f, C_FIELD);
    Theme_Rounded((Rectangle){ bar.x, cy - 3.0f, fmaxf(kx - bar.x, 6.0f), 6.0f }, 3.0f,
                  h == HOT_NONE ? C_ACCENT : C_ACCENT_HI);
    if (h != HOT_NONE) DrawCircleV((Vector2){ kx, cy }, 12.0f, Fade(C_ACCENT, 0.25f));   /* hover halo */
    DrawCircleV((Vector2){ kx, cy + 1.0f }, 8.0f, Fade(BLACK, 0.35f));                   /* shadow     */
    DrawCircleV((Vector2){ kx, cy }, 8.0f, C_TEXT);
    DrawCircleV((Vector2){ kx, cy }, 3.0f, h == HOT_NONE ? C_ACCENT : C_ACCENT_HI);
}

/* A rounded check box with a drawn tick; Raygui handles the click and the label. */
static void CheckRow(float x, float y, const char *text, bool *on)
{
    Rectangle box = { x, y, 18, 18 };
    Rectangle hit = { x, y, 18.0f + GuiGetStyle(CHECKBOX, TEXT_PADDING) + GuiGetTextWidth(text), 18 };
    Hot h = HotState(hit);
    GuiCheckBox(box, text, on);

    if (*on) {
        Theme_Rounded(box, 5, h == HOT_NONE ? C_ACCENT : C_ACCENT_HI);
        DrawLineEx((Vector2){ x + 4.5f, y + 9.5f },  (Vector2){ x + 7.8f, y + 12.8f }, 2.2f, WHITE);
        DrawLineEx((Vector2){ x + 7.0f, y + 12.8f }, (Vector2){ x + 13.8f, y + 5.5f }, 2.2f, WHITE);
    } else {
        Theme_Card(box, 5, C_FIELD, h == HOT_NONE ? (Color){ 74, 92, 134, 255 } : C_ACCENT_HI);
    }
}

/* A card with a small upper-case section title in its top-left corner. */
static void DrawCard(Rectangle card, const char *title)
{
    Theme_Card(card, 12, C_CARD, C_BORDER);
    DrawRectangle((int)card.x + 12, (int)card.y + 1, (int)card.width - 24, 1, Fade(WHITE, 0.05f));   /* top sheen */
    Text_DrawBold(title, (int)card.x + 14, (int)card.y + 12, 11, C_TEXT_MUTED);
}

/* ===================================================================== */
/*  Typed values                                                         */
/* ===================================================================== */
/*
 * The value next to every slider is also a text field: click it, type a number,
 * press Enter (or click anywhere else) to apply it, or Esc to cancel. Only one
 * field is edited at a time, so its state lives here rather than in each row.
 * The value being edited is identified by its address (`editVal`).
 */
static float *editVal  = NULL;                 /* the value being typed into, or NULL        */
static float  editLo, editHi, editStep;        /* its range and precision                    */
static char   editBuf[24];                     /* the text typed so far                      */
static float *flashVal = NULL;                 /* field to outline in amber for a moment ... */
static double flashUntil = 0.0;                /* ... (typed value was clamped or not a number) */
static float *justCommitted = NULL;            /* value set by typing in this frame, if any  */

static void Flash(float *v) { flashVal = v; flashUntil = GetTime() + 0.9; }

/* Snap a value to `step` and keep it in [lo, hi]. */
static float SnapClamp(float v, float lo, float hi, float step)
{
    v = roundf(v / step) * step;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v == 0.0f ? 0.0f : v;                /* turn -0 into +0 */
}

/* Apply the typed text to the value being edited, and leave edit mode.
 * Empty text keeps the old value; text that is not a number is ignored (flash);
 * an out-of-range number is clamped to the nearest limit (flash). */
static void CommitEdit(void)
{
    if (editVal == NULL) return;
    for (char *c = editBuf; *c; c++) if (*c == ',') *c = '.';   /* accept a decimal comma */
    const char *s = editBuf;
    while (*s == ' ') s++;
    if (*s != '\0') {
        char *end;
        double d = strtod(s, &end);
        if (end == s || !isfinite(d)) {
            Flash(editVal);                                       /* not a number: keep the old value */
        } else {
            if (d < editLo || d > editHi) Flash(editVal);         /* out of range: clamped below */
            *editVal = SnapClamp((float)d, editLo, editHi, editStep);
            justCommitted = editVal;
        }
    }
    editVal = NULL;
}

/* Number of decimals a step needs (0.1 -> 1, 0.01 -> 2). */
static int StepDecimals(float step)
{
    int d = (int)ceilf(-log10f(step) - 1e-4f);
    return d < 0 ? 0 : d;
}

/*
 * The value field of a slider row. Idle, it shows the formatted value with its
 * unit on a sunken field; clicked, it becomes a text box for a bare number.
 */
static void ValueField(Rectangle field, float *val, float lo, float hi, float step, const char *fmt)
{
    if (editVal == val) {
        if (IsKeyPressed(KEY_ESCAPE)) { editVal = NULL; return; }     /* cancel: draw it idle next frame */
        Theme_Card(field, 7, (Color){ 22, 44, 96, 255 }, C_ACCENT);
        if (editBuf[0] == '\0')                                       /* faint hint: the current value */
            Text_Draw(TextFormat("%.*f", StepDecimals(step), *val), (int)field.x + 10,
                      (int)(field.y + field.height / 2) - FONT_UI_SIZE / 2, FONT_UI_SIZE, C_TEXT_MUTED);
        if (GuiTextBox(field, editBuf, (int)sizeof editBuf, true) ||   /* Enter or click-away */
            IsKeyPressed(KEY_KP_ENTER)) CommitEdit();                  /* keypad Enter too    */
        return;
    }

    Hot   h     = HotState(field);
    bool  flash = flashVal == val && GetTime() < flashUntil;
    Color border = flash ? C_WARN : (h != HOT_NONE ? C_ACCENT_HI : (Color){ 36, 50, 82, 255 });
    Theme_Card(field, 7, h != HOT_NONE ? Theme_Mix(C_FIELD, WHITE, 0.05f) : C_FIELD, border);

    char text[32];
    snprintf(text, sizeof text, fmt, *val);
    Text_DrawBoldRight(text, (int)(field.x + field.width) - 9, (int)(field.y + field.height / 2) - 8, 15, C_TEXT);

    if (h != HOT_NONE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        CommitEdit();                          /* another field may still be open: apply it first */
        editVal  = val;
        editLo   = lo; editHi = hi; editStep = step;
        editBuf[0] = '\0';                     /* start empty: just type the new number */
    }
}

/*
 * One slider row inside a card:  label | slider | value field.
 * `marker` (if it lies inside [lo, hi]) draws an amber notch on the slider track;
 * the angle slider uses it to show the critical angle, v0 to show zero.
 */
#define NO_MARKER NAN    /* "no notch": NaN is never inside [lo, hi] (-1 was, for v0) */
#define FIELD_W   88.0f  /* width of the value field */

static void SliderRow(Rectangle card, float y, const char *label, float *val,
                      float lo, float hi, float step, const char *fmt, float marker)
{
    const float lx = card.x + 14.0f;
    const Rectangle field = { card.x + card.width - 14.0f - FIELD_W, y - 2.0f, FIELD_W, 24.0f };
    const Rectangle bar   = { lx + 62.0f, y, field.x - 12.0f - (lx + 62.0f), 20.0f };

    Text_Draw(label, (int)lx, (int)y + 2, 15, C_TEXT_DIM);

    /* The field goes first: a click on this row's slider while its field is open
     * then applies the typed value BEFORE the slider moves, so the click wins. */
    ValueField(field, val, lo, hi, step, fmt);

    Hot h = HotState(bar);
    GuiSliderBar(bar, NULL, NULL, val, lo, hi);               /* input only: Raygui draws nothing visible here */

    /* Snap to the precision the value is SHOWN with, so the simulation uses exactly
     * the number on screen. (Without this, v0 = -0.04 is shown as "-0.0 m/s" but the
     * block starts moving, and below the critical angle kinetic friction may never stop it.) */
    *val = SnapClamp(*val, lo, hi, step);

    /* the critical-angle notch sits on the track, under the knob */
    if (marker >= lo && marker <= hi) {
        float mx = SliderX(bar, marker, lo, hi), cy = bar.y + bar.height * 0.5f;
        Theme_Rounded((Rectangle){ mx - 1.5f, cy - 8.0f, 3.0f, 16.0f }, 1.5f, C_CRIT);
        Theme_Tri((Vector2){ mx - 4.5f, cy - 13.0f }, (Vector2){ mx + 4.5f, cy - 13.0f },
                  (Vector2){ mx, cy - 7.0f }, C_CRIT);
    }
    DrawSlider(bar, *val, lo, hi, h);
}

/* ===================================================================== */
/*  Left panel                                                           */
/* ===================================================================== */
static void DrawLeftPanel(Params *p, ViewOptions *v)
{
    DrawRectangle(0, 0, LEFT_W, SCREEN_H, C_PANEL);
    DrawRectangle(LEFT_W - 1, 0, 1, SCREEN_H, C_BORDER);

    /* ---- header: a little ramp logo, the title and a subtitle ---- */
    Theme_Rounded((Rectangle){ 14, 14, 36, 36 }, 9, C_ACCENT);
    Theme_Tri((Vector2){ 20, 43 }, (Vector2){ 44, 43 }, (Vector2){ 44, 24 }, WHITE);
    Text_DrawBold("Block on an Incline", 60, 13, 19, C_TEXT);
    Text_Draw("Static & kinetic friction", 60, 36, 13, C_TEXT_MUTED);

    const float cx = 12.0f, cw = LEFT_W - 24.0f;
    float y = 64.0f;

    /* ---- PARAMETERS ---- */
    Rectangle card = { cx, y, cw, 34 + 6 * 26 + 8 };
    DrawCard(card, "PARAMETERS");
    float ry = card.y + 34;
    const double thc = Physics_CriticalAngle(p->mu_s);        /* the critical angle, needed for the slider notch */
    SliderRow(card, ry, "Angle",   &p->angleDeg, 0.0f, 90.0f, 0.1f,  "%.1f\xC2\xB0",     (float)thc); ry += 26;
    SliderRow(card, ry, "Mass",    &p->mass,     0.1f, 20.0f, 0.1f,  "%.1f kg",           NO_MARKER);      ry += 26;
    SliderRow(card, ry, "Gravity", &p->g,        0.5f, 25.0f, 0.01f, "%.2f m/s\xC2\xB2",  NO_MARKER);      ry += 26;
    SliderRow(card, ry, "v0",      &p->v0,      -8.0f,  8.0f, 0.1f,  "%+.1f m/s",         0.0f);        ry += 26;
    SliderRow(card, ry, "\xCE\xBCs", &p->mu_s,   0.0f,  2.0f, 0.01f, "%.2f",              NO_MARKER);      ry += 26;
    SliderRow(card, ry, "\xCE\xBCk", &p->mu_k,   0.0f,  2.0f, 0.01f, "%.2f",              NO_MARKER);
    if (p->mu_k > p->mu_s) {                                   /* kinetic friction never exceeds static */
        if (justCommitted == &p->mu_k) Flash(&p->mu_k);        /* a typed mu_k was capped: show it */
        p->mu_k = p->mu_s;
    }
    y += card.height + 8;

    /* ---- CRITICAL ANGLE ----
     * theta_c = atan(mu_s) is the steepest angle at which a block at rest still sticks.
     * It follows the friction sliders, and the card says what it means for the current angle. */
    card = (Rectangle){ cx, y, cw, 110 };
    DrawCard(card, "CRITICAL ANGLE");
    const double thc2 = Physics_CriticalAngle(p->mu_s);                       /* static: when sliding starts   */
    const double thk  = Physics_CriticalAngle(fminf(p->mu_k, p->mu_s));       /* kinetic: constant-speed angle */
    const int lx = (int)card.x + 14, rx = (int)(card.x + card.width) - 14;
    Text_Draw("\xCE\xB8""c = atan(\xCE\xBCs)", lx, (int)card.y + 36, 15, C_TEXT_DIM);
    Text_DrawBoldRight(TextFormat("%.1f\xC2\xB0", thc2), rx, (int)card.y + 30, 24, C_CRIT);
    Text_Draw("\xCE\xB8k = atan(\xCE\xBCk)", lx, (int)card.y + 62, 13, C_TEXT_MUTED);
    Text_DrawRight(TextFormat("%.1f\xC2\xB0", thk), rx, (int)card.y + 62, 13, C_TEXT_DIM);

    /* a status chip: what happens if the block is released from rest at this angle */
    const double diff = (double)p->angleDeg - thc2;
    const char *msg;
    Color mc;
    if      (diff >  0.05) { msg = "\xCE\xB8 > \xCE\xB8""c : released from rest, it slides"; mc = C_WARN;  }
    else if (diff < -0.05) { msg = "\xCE\xB8 < \xCE\xB8""c : released from rest, it sticks"; mc = C_GOOD;  }
    else                   { msg = "\xCE\xB8 = \xCE\xB8""c : on the verge of sliding";       mc = C_CRIT;  }
    Rectangle chip = { card.x + 14, card.y + 82, card.width - 28, 20 };
    Theme_Rounded(chip, 10, Fade(mc, 0.18f));
    Text_Draw(msg, (int)(chip.x + chip.width / 2) - Text_Width(msg, 13) / 2, (int)chip.y + 3, 13, mc);
    y += card.height + 8;

    /* ---- VECTOR LENGTH (drawing only: never touches the physics) ---- */
    card = (Rectangle){ cx, y, cw, 34 + 3 * 26 + 8 };
    DrawCard(card, "VECTOR LENGTH");
    ry = card.y + 34;
    SliderRow(card, ry, "Gravity",  &v->scaleGravity,  0.25f, 3.0f, 0.01f, "x%.2f", NO_MARKER); ry += 26;
    SliderRow(card, ry, "Normal",   &v->scaleNormal,   0.25f, 3.0f, 0.01f, "x%.2f", NO_MARKER); ry += 26;
    SliderRow(card, ry, "Friction", &v->scaleFriction, 0.25f, 3.0f, 0.01f, "x%.2f", NO_MARKER);
    y += card.height + 8;

    /* ---- SHOW ---- */
    card = (Rectangle){ cx, y, cw, 34 + 7 * 24 + 6 };
    DrawCard(card, "SHOW");
    ry = card.y + 34;
    const float bx = card.x + 14;
    CheckRow(bx, ry, "Gravity components",   &v->showParallelPerp); ry += 24;
    CheckRow(bx, ry, "x / y components",     &v->showXY);           ry += 24;
    CheckRow(bx, ry, "Critical angle line",  &v->showCritical);     ry += 24;
    CheckRow(bx, ry, "Time",                 &v->showTime);         ry += 24;
    CheckRow(bx, ry, "Graph: v vs t",        &v->showGraphV);       ry += 24;
    CheckRow(bx, ry, "Graph: a vs t",        &v->showGraphA);       ry += 24;
    CheckRow(bx, ry, "Graph: friction vs t", &v->showGraphF);
    y += card.height + 10;

    /* ---- keyboard hints: each key in a small key-cap ---- */
    static const char *keys[5][2] = { { "Space", "start / pause" }, { "R", "reset" },
                                      { "Left / Right", "step" }, { "Wheel", "zoom" },
                                      { "Drag", "pan the scene" } };
    float kx = cx + 2, ky = y;
    for (int i = 0; i < 5; i++) {
        int kw = Text_WidthBold(keys[i][0], 11) + 12;
        int dw = Text_Width(keys[i][1], 12);
        if (kx + kw + 6 + dw > cx + cw) { kx = cx + 2; ky += 22; }   /* wrap to the next line */
        Theme_Card((Rectangle){ kx, ky, (float)kw, 18 }, 4, C_CARD_HI, C_BORDER);
        Text_DrawBold(keys[i][0], (int)kx + 6, (int)ky + 2, 11, C_TEXT_DIM);
        Text_Draw(keys[i][1], (int)kx + kw + 6, (int)ky + 2, 12, C_TEXT_MUTED);
        kx += kw + 6 + dw + 14;
    }
}

/* ===================================================================== */
/*  Transport bar                                                        */
/* ===================================================================== */
static void DrawTransportBar(ViewOptions *v)
{
    const SimMode mode = Sim_Mode();
    const Rectangle bar = { SCENE_X + 10, SCREEN_H - BAR_H + 8, SCENE_W - 20, BAR_H - 16 };
    Theme_Card(bar, 14, C_CARD, C_BORDER);

    const float x0 = bar.x + 14, y1 = bar.y + 12;      /* first row  */
    const float y2 = y1 + 34 + 8;                      /* second row */

    /* ---- ONE button for Start, Pause and Resume ----
     * Its label and colour follow the clock:  STOPPED -> "Start" (green),
     * RUNNING -> "Pause" (amber), PAUSED -> "Resume" (green). */
    const char *label = mode == SIM_RUNNING ? "Pause" : (mode == SIM_PAUSED ? "Resume" : "Start");
    const Color dark  = { 10, 18, 30, 255 };           /* dark text reads well on green and amber */
    if (ColoredButton((Rectangle){ x0, y1, 120, 34 }, label,
                      mode == SIM_RUNNING ? C_WARN : C_GOOD, dark)) {
        if (mode == SIM_RUNNING) Sim_Pause(); else Sim_Start();
    }

    if (Button((Rectangle){ x0 + 128, y1, 76, 34 }, "Reset"))  Sim_Reset();
    if (Button((Rectangle){ x0 + 212, y1, 70, 34 }, "Step -")) Sim_StepBy(-stepSizes[stepSel]);
    if (Button((Rectangle){ x0 + 290, y1, 70, 34 }, "Step +")) Sim_StepBy( stepSizes[stepSel]);

    /* ---- time box: click, type a time, press Enter to jump there ---- */
    Text_Draw("Time", (int)x0 + 378, (int)y1 + 9, 14, C_TEXT_DIM);
    Rectangle tb = { x0 + 418, y1, 92, 34 };
    Theme_Card(tb, 8, timeEdit ? (Color){ 22, 44, 96, 255 } : C_FIELD,          /* sunken field behind the text */
               timeEdit ? C_ACCENT : (HotState(tb) != HOT_NONE ? C_ACCENT_HI : C_BORDER));
    if (!timeEdit) snprintf(timeText, sizeof timeText, "%.3f", Sim_Time());   /* mirror the clock */
    /* GuiTextBox exists in every raygui version. It returns true when editing should
     * toggle: a click on the box (start) or Enter / a click elsewhere (finish). */
    if (timeEdit && IsKeyPressed(KEY_ESCAPE)) timeEdit = false;              /* Esc cancels */
    if (GuiTextBox(tb, timeText, (int)sizeof timeText, timeEdit)) {
        if (!timeEdit) {                       /* clicked: start typing a new time */
            timeEdit = true;
            timeText[0] = '\0';
        } else {                               /* Enter or click-away: jump there */
            timeEdit = false;
            if (timeText[0] != '\0') Sim_JumpTo(atof(timeText));
        }
    }

    /* ---- second row: step size, zoom ---- */
    Text_Draw("Step size", (int)x0, (int)y2 + 6, 14, C_TEXT_DIM);
    for (int i = 0; i < 3; i++) {                      /* segment surfaces; the chosen one is accent blue */
        Rectangle seg = { x0 + 70 + i * (44 + 4), y2, 44, 28 };
        if (i == stepSel) Theme_Rounded(seg, 8, C_ACCENT);
        else              Surface(seg, C_CARD_HI, C_BORDER, HotState(seg));
    }
    GuiToggleGroup((Rectangle){ x0 + 70, y2, 44, 28 }, "0.01;0.1;1.0", &stepSel);

    if (Button((Rectangle){ x0 + 232, y2, 70,  28 }, "Zoom -"))     v->zoom = ClampZoom(v->zoom / 1.2f);
    if (Button((Rectangle){ x0 + 308, y2, 70,  28 }, "Zoom +"))     v->zoom = ClampZoom(v->zoom * 1.2f);
    if (Button((Rectangle){ x0 + 384, y2, 100, 28 }, "Reset view")) { v->zoom = 1.0f; v->panX = v->panY = 0.0f; }
}

/* ===================================================================== */
/*  Keyboard                                                             */
/* ===================================================================== */
static void HandleShortcuts(void)
{
    if (timeEdit || editVal) return;           /* the user is typing a number: leave the keys alone */
    if (IsKeyPressed(KEY_SPACE)) { if (Sim_Mode() == SIM_RUNNING) Sim_Pause(); else Sim_Start(); }
    if (IsKeyPressed(KEY_R))     Sim_Reset();
    if (IsKeyPressed(KEY_RIGHT)) Sim_StepBy( stepSizes[stepSel]);
    if (IsKeyPressed(KEY_LEFT))  Sim_StepBy(-stepSizes[stepSel]);
}

/* ===================================================================== */
/*  Public functions                                                     */
/* ===================================================================== */
bool UI_Draw(Params *p, ViewOptions *v)
{
    const Params before = *p;                  /* to detect a physical change below */

    justCommitted = NULL;
    HandleShortcuts();
    DrawLeftPanel(p, v);
    DrawTransportBar(v);

    /* Esc normally closes the window. While a number is being typed it cancels
     * the edit instead, so the exit key is switched off for that time. */
    SetExitKey((timeEdit || editVal) ? KEY_NULL : KEY_ESCAPE);

    return memcmp(&before, p, sizeof(Params)) != 0;
}

void UI_HandleScene(ViewOptions *v, Rectangle sceneRect)
{
    static bool dragging = false;              /* true while the left button drags the scene */
    Vector2 m = GetMousePosition();
    bool inside = CheckCollisionPointRec(m, sceneRect);

    /* wheel: zoom by 10 % per notch */
    float wheel = GetMouseWheelMove();
    if (inside && wheel != 0.0f) v->zoom = ClampZoom(v->zoom * powf(1.1f, wheel));

    /* drag: pan. The drag only starts inside the scene, but continues until the button is released. */
    if (inside && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dragging = true;
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))          dragging = false;
    if (dragging) {
        Vector2 d = GetMouseDelta();
        v->panX += d.x;
        v->panY += d.y;
    }
}
