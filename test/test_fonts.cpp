#include <unity.h>
#include "font_8x16.h"
#include "display/drawing.h"
#include "display/epd_display.h"
#include <stdlib.h>

extern unsigned char *fb;

void test_font_init_nonzero(void) {
    font8x16_init();
    bool any = false;
    for (int i = 0; i < 16; i++) if (font8x16['A'][i] != 0) { any = true; break; }
    TEST_ASSERT_TRUE(any);
}

void test_text_utf8_latvian(void) {
    // Verify Latvian glyph `Ā` is present in font table (index 0x80)
    font8x16_init();
    bool any = false;
    for (int i = 0; i < 16; i++) if (font8x16[0x80][i] != 0) { any = true; break; }
    TEST_ASSERT_TRUE(any);
}

void test_text_degree_symbol(void) {
    // Verify degree glyph (index 0x94) is present in font table
    font8x16_init();
    bool any = false;
    for (int i = 0; i < 16; i++) if (font8x16[0x94][i] != 0) { any = true; break; }
    TEST_ASSERT_TRUE(any);
}
