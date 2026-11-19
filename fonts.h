/*
 * fonts.h - shared text rendering, so the font choice lives in one place.
 *
 * The font is Inter (Regular + SemiBold, SIL OFL 1.1), embedded in font_data.h:
 * there are no font files to ship and nothing to find at run time. Each pixel
 * size you ask for gets its own glyph atlas rasterised at exactly that size, so
 * text stays crisp instead of being scaled.
 *
 * Coordinates are the top-left corner of the text, in pixels.
 */
#ifndef FONTS_H
#define FONTS_H

#include "raylib.h"

#define FONT_UI_SIZE 16          /* text size used by the Raygui controls */

void Fonts_Load(void);           /* call once after InitWindow()  */
void Fonts_Unload(void);         /* call before CloseWindow()     */

/* The regular weight at `size` px, for GuiSetFont(). */
Font Fonts_Get(int size);
Font Fonts_GetBold(int size);   /* the SemiBold weight, for emphasised buttons */

/* Regular weight. */
void Text_Draw(const char *text, int x, int y, int size, Color c);
void Text_DrawRight(const char *text, int xRight, int y, int size, Color c);  /* text ends at xRight */
int  Text_Width(const char *text, int size);

/* SemiBold weight, for titles and emphasis. */
void Text_DrawBold(const char *text, int x, int y, int size, Color c);
void Text_DrawBoldRight(const char *text, int xRight, int y, int size, Color c);
int  Text_WidthBold(const char *text, int size);

#endif /* FONTS_H */
