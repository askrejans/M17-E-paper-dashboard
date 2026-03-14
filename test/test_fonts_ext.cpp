#include <unity.h>
#include "font_8x16.h"
#include "display/drawing.h"
#include "display/epd_display.h"
#include "data.h"
#include <stdlib.h>
#include <string.h>

extern unsigned char *fb;

// Helper: check if a glyph slot has any non-zero data
static bool glyph_exists(uint8_t slot) {
    for (int i = 0; i < 16; i++)
        if (font8x16[slot][i] != 0)
            return true;
    return false;
}

// --- Full Latvian alphabet glyph coverage ---

void test_glyph_A_macron(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x80)); // Ā
    TEST_ASSERT_TRUE(glyph_exists(0x81)); // ā
}

void test_glyph_C_caron(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x82)); // Č
    TEST_ASSERT_TRUE(glyph_exists(0x83)); // č
}

void test_glyph_E_macron(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x84)); // Ē
    TEST_ASSERT_TRUE(glyph_exists(0x85)); // ē
}

void test_glyph_G_cedilla(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x86)); // Ģ
    TEST_ASSERT_TRUE(glyph_exists(0x87)); // ģ
}

void test_glyph_I_macron(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x88)); // Ī
    TEST_ASSERT_TRUE(glyph_exists(0x89)); // ī
}

void test_glyph_K_cedilla(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x95)); // Ķ
    TEST_ASSERT_TRUE(glyph_exists(0x97)); // ķ
}

void test_glyph_L_cedilla(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x8A)); // Ļ
    TEST_ASSERT_TRUE(glyph_exists(0x8B)); // ļ
}

void test_glyph_N_cedilla(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x8C)); // Ņ
    TEST_ASSERT_TRUE(glyph_exists(0x8D)); // ņ
}

void test_glyph_S_caron(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x8E)); // Š
    TEST_ASSERT_TRUE(glyph_exists(0x8F)); // š
}

void test_glyph_U_macron(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x90)); // Ū
    TEST_ASSERT_TRUE(glyph_exists(0x91)); // ū
}

void test_glyph_Z_caron(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x92)); // Ž
    TEST_ASSERT_TRUE(glyph_exists(0x93)); // ž
}

// --- Symbol glyphs ---

void test_glyph_degree(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x94)); // °
}

void test_glyph_euro(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x96)); // €
}

// --- Graphical element glyphs ---

void test_glyph_bullet(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x98)); // ▪
}

void test_glyph_arrows(void) {
    TEST_ASSERT_TRUE(glyph_exists(0x9C)); // ↑
    TEST_ASSERT_TRUE(glyph_exists(0x9D)); // ↓
    TEST_ASSERT_TRUE(glyph_exists(0x9E)); // →
    TEST_ASSERT_TRUE(glyph_exists(0x9F)); // ←
}

void test_glyph_shapes(void) {
    TEST_ASSERT_TRUE(glyph_exists(0xA0)); // ●
    TEST_ASSERT_TRUE(glyph_exists(0xA1)); // ○
    TEST_ASSERT_TRUE(glyph_exists(0xA2)); // ■
    TEST_ASSERT_TRUE(glyph_exists(0xA3)); // ▲
    TEST_ASSERT_TRUE(glyph_exists(0xA4)); // ▼
}

void test_glyph_indicators(void) {
    TEST_ASSERT_TRUE(glyph_exists(0xA5)); // ✓
    TEST_ASSERT_TRUE(glyph_exists(0xA6)); // ✗
    TEST_ASSERT_TRUE(glyph_exists(0xA7)); // ◆
}

void test_glyph_dashboard_icons(void) {
    TEST_ASSERT_TRUE(glyph_exists(0xA8)); // ⚡
    TEST_ASSERT_TRUE(glyph_exists(0xA9)); // ☀
    TEST_ASSERT_TRUE(glyph_exists(0xAA)); // 💧
    TEST_ASSERT_TRUE(glyph_exists(0xAB)); // 🌡
    TEST_ASSERT_TRUE(glyph_exists(0xAC)); // 🏠
    TEST_ASSERT_TRUE(glyph_exists(0xAD)); // 📶
}

// --- ASCII glyphs still intact after init ---

void test_ascii_glyphs_preserved(void) {
    // Verify a few key ASCII characters survived font init
    TEST_ASSERT_TRUE(glyph_exists('A'));
    TEST_ASSERT_TRUE(glyph_exists('Z'));
    TEST_ASSERT_TRUE(glyph_exists('a'));
    TEST_ASSERT_TRUE(glyph_exists('z'));
    TEST_ASSERT_TRUE(glyph_exists('0'));
    TEST_ASSERT_TRUE(glyph_exists('9'));
    TEST_ASSERT_TRUE(glyph_exists(' ') == false); // space should be blank
}

// --- Glyph data uniqueness ---

void test_uppercase_lowercase_differ(void) {
    // Ā and ā should have different bitmaps
    TEST_ASSERT_FALSE(memcmp(font8x16[0x80], font8x16[0x81], 16) == 0);
    // Ķ and ķ should differ
    TEST_ASSERT_FALSE(memcmp(font8x16[0x95], font8x16[0x97], 16) == 0);
    // Š and š should differ
    TEST_ASSERT_FALSE(memcmp(font8x16[0x8E], font8x16[0x8F], 16) == 0);
}

// --- Text rendering to framebuffer ---

void test_text_renders_pixels(void) {
    memset(fb, 0x55, ALLSCREEN_BYTES); // fill white

    text(0, 0, "A", BLACK);

    // 'A' glyph has non-zero rows starting around row 2-3
    // Check rows 0-15 (full glyph height: 16 rows * 200 bytes/row = 3200 bytes)
    bool changed = false;
    for (int row = 0; row < 16; row++) {
        int byte_idx = row * 200; // 800 pixels / 4 pixels per byte = 200
        if (fb[byte_idx] != 0x55) { changed = true; break; }
    }
    TEST_ASSERT_TRUE(changed);
}
