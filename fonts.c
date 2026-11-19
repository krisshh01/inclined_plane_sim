/*
 * fonts.c - the embedded Inter font, loaded lazily at each requested pixel size.
 *
 * Supported characters: ASCII plus  degree, superscript 2, middle dot, x, theta,
 * mu, minus, <=, >= and approximately-equal.
 * (Keep this list in sync with UNICODES in tools/make_font_data.py, which decides
 * which glyphs are stored in font_data.h.)
 */
#include "fonts.h"
#include "font_data.h"          /* FONT_REGULAR_DATA and FONT_BOLD_DATA byte arrays */
#include <math.h>

#define MAX_FONTS 32            /* distinct (size, weight) pairs that can be cached */

/* One cached glyph atlas. */
typedef struct {
    int  size;                  /* pixel size it was rasterised at                  */
    bool bold;                  /* SemiBold (true) or Regular (false)               */
    bool owned;                 /* we loaded it, so we must unload it               */
    Font font;
} Entry;

static Entry cache[MAX_FONTS];
static int   nCache = 0;
static int   glyphs[160];       /* the codepoints we ask Raylib to rasterise        */
static int   nGlyphs = 0;

/* Bold glyph advances are rounded to whole pixels by Raylib, which packs bold
 * words together; a little extra spacing compensates. */
static float BoldSpacing(int size) { return size * 0.04f; }

void Fonts_Load(void)
{
    static const int extra[] = { 0xB0, 0xB2, 0xB7, 0xD7, 0x3B8, 0x3BC, 0x2212, 0x2264, 0x2265, 0x2248 };
    nGlyphs = 0;
    for (int c = 32; c <= 126; c++) glyphs[nGlyphs++] = c;               /* printable ASCII */
    for (int i = 0; i < (int)(sizeof extra / sizeof extra[0]); i++) glyphs[nGlyphs++] = extra[i];
    nCache = 0;
}

void Fonts_Unload(void)
{
    for (int i = 0; i < nCache; i++)
        if (cache[i].owned) UnloadFont(cache[i].font);
    nCache = 0;
}

/* Return the atlas for (size, weight), loading it the first time it is asked for. */
static Font Get(int size, bool bold)
{
    for (int i = 0; i < nCache; i++)
        if (cache[i].size == size && cache[i].bold == bold) return cache[i].font;

    if (nCache >= MAX_FONTS) return GetFontDefault();          /* cache full: fall back */

    /* On a high-DPI screen one logical pixel is several real pixels, so the atlas is
     * rasterised at the REAL size and drawn at `size`: text stays sharp, not upscaled. */
    Vector2 dpi = GetWindowScaleDPI();
    float scale = fmaxf(1.0f, fmaxf(dpi.x, dpi.y));
    int   px    = (int)ceilf((float)size * scale);

    Entry e = { size, bold, false, GetFontDefault() };         /* fallback: Raylib's built-in font */

    /* Raylib warns when a glyph (such as 'j') is taller than the nominal size.
     * That is harmless, so keep it out of the console. */
    SetTraceLogLevel(LOG_ERROR);
    Font f = bold
        ? LoadFontFromMemory(".otf", FONT_BOLD_DATA,    (int)sizeof FONT_BOLD_DATA,    px, glyphs, nGlyphs)
        : LoadFontFromMemory(".otf", FONT_REGULAR_DATA, (int)sizeof FONT_REGULAR_DATA, px, glyphs, nGlyphs);
    SetTraceLogLevel(LOG_INFO);

    if (f.texture.id != 0 && f.glyphCount > 0) {
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);  /* smooth sub-pixel positioning */
        e.font  = f;
        e.owned = true;
    }
    cache[nCache++] = e;
    return e.font;
}

Font Fonts_Get(int size) { return Get(size, false); }
Font Fonts_GetBold(int size) { return Get(size, true); }

/* ------------------------------------------------------------ regular weight */
void Text_Draw(const char *text, int x, int y, int size, Color c)
{
    DrawTextEx(Get(size, false), text, (Vector2){ (float)x, (float)y }, (float)size, 0.0f, c);
}

int Text_Width(const char *text, int size)
{
    Vector2 m = MeasureTextEx(Get(size, false), text, (float)size, 0.0f);
    return (int)ceilf(m.x);
}

void Text_DrawRight(const char *text, int xRight, int y, int size, Color c)
{
    Text_Draw(text, xRight - Text_Width(text, size), y, size, c);
}

/* ----------------------------------------------------------------- semibold */
void Text_DrawBold(const char *text, int x, int y, int size, Color c)
{
    DrawTextEx(Get(size, true), text, (Vector2){ (float)x, (float)y }, (float)size, BoldSpacing(size), c);
}

int Text_WidthBold(const char *text, int size)
{
    Vector2 m = MeasureTextEx(Get(size, true), text, (float)size, BoldSpacing(size));
    return (int)ceilf(m.x);
}

void Text_DrawBoldRight(const char *text, int xRight, int y, int size, Color c)
{
    Text_DrawBold(text, xRight - Text_WidthBold(text, size), y, size, c);
}
