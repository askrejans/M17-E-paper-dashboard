#include <unity.h>
#include "display/drawing.h"
#include "display/epd_display.h"
#include "data.h"
#include <stdlib.h>
#include <string.h>

extern unsigned char *fb;

// --- fillRectDither tests ---

void test_fillRectDither_checkerboard(void) {
    memset(fb, 0x55, ALLSCREEN_BYTES); // fill white

    // Fill a 4x2 area with black/yellow dither
    fillRectDither(0, 0, 4, 2, BLACK, YELLOW);

    // Row 0: pixel 0 = YELLOW(2), pixel 1 = BLACK(0), pixel 2 = YELLOW(2), pixel 3 = BLACK(0)
    // (0+0)&1=0 -> c2=YELLOW, (1+0)&1=1 -> c1=BLACK, (2+0)&1=0 -> c2=YELLOW, (3+0)&1=1 -> c1=BLACK
    // Packed: YELLOW<<6 | BLACK<<4 | YELLOW<<2 | BLACK = 0x80 | 0x00 | 0x08 | 0x00 = 0x88
    TEST_ASSERT_EQUAL_HEX8(0x88, fb[0]);

    // Row 1 (byte at index 200): opposite pattern
    // (0+1)&1=1 -> c1=BLACK, (1+1)&1=0 -> c2=YELLOW, etc.
    // Packed: BLACK<<6 | YELLOW<<4 | BLACK<<2 | YELLOW = 0x00 | 0x20 | 0x00 | 0x02 = 0x22
    TEST_ASSERT_EQUAL_HEX8(0x22, fb[200]);
}

void test_fillRectDither_same_color(void) {
    memset(fb, 0, ALLSCREEN_BYTES);

    // Dither with same color should be solid fill
    fillRectDither(0, 0, 4, 1, WHITE, WHITE);
    TEST_ASSERT_EQUAL_HEX8(0x55, fb[0]);
}

// --- fillRectDitherDense tests ---

void test_fillRectDitherDense_pattern(void) {
    memset(fb, 0, ALLSCREEN_BYTES);

    // Dense dither: 75% c1 / 25% c2
    fillRectDitherDense(0, 0, 2, 2, BLACK, YELLOW);

    // (0,0): mx=0, my=0 -> c2=YELLOW
    // (1,0): mx=1, my=0 -> c1=BLACK
    // Packed byte: YELLOW<<6 | BLACK<<4 | ... rest 0
    uint8_t expected_row0 = (YELLOW << 6) | (BLACK << 4);
    TEST_ASSERT_EQUAL_HEX8(expected_row0, fb[0] & 0xF0);
}

// --- hline tests ---

void test_hline_draws_line(void) {
    memset(fb, 0x55, ALLSCREEN_BYTES); // white fill

    hline(0, 0, 4, BLACK);

    // 4 black pixels at row 0 -> byte 0 should be all-black
    TEST_ASSERT_EQUAL_HEX8(0x00, fb[0]);
}

void test_hline_yellow(void) {
    memset(fb, 0, ALLSCREEN_BYTES); // black fill

    hline(0, 0, 4, YELLOW);

    // 4 yellow pixels: 10 10 10 10 = 0xAA
    TEST_ASSERT_EQUAL_HEX8(0xAA, fb[0]);
}

// --- vline tests ---

void test_vline_draws_line(void) {
    memset(fb, 0x55, ALLSCREEN_BYTES); // white fill

    vline(0, 0, 3, BLACK);

    // Pixel (0,0): bits 7-6 of fb[0] should be 00 (BLACK)
    TEST_ASSERT_EQUAL_HEX8(0x00, fb[0] & 0xC0);

    // Pixel (0,1): byte at y=1, x=0 -> index 200
    TEST_ASSERT_EQUAL_HEX8(0x00, fb[200] & 0xC0);

    // Pixel (0,2): byte at y=2, x=0 -> index 400
    TEST_ASSERT_EQUAL_HEX8(0x00, fb[400] & 0xC0);
}

// --- rectColored tests ---

void test_rectColored_yellow_border(void) {
    memset(fb, 0, ALLSCREEN_BYTES);

    rectColored(0, 0, 4, 3, YELLOW);

    // Top-left pixel should be YELLOW
    // Pixel (0,0) is bits 7-6 of fb[0]
    TEST_ASSERT_EQUAL_HEX8(0x80, fb[0] & 0xC0); // YELLOW=2 -> 10 in bits 7-6
}

// --- Layout constants tests ---

void test_layout_columns_equal_width(void) {
    const int MARGIN = 10;
    const int COL_GAP = 8;
    const int COL_W = (800 - 2 * MARGIN - 2 * COL_GAP) / 3;

    // All columns should be 254px
    TEST_ASSERT_EQUAL(254, COL_W);

    // Verify columns fit within display
    int COL1_X = MARGIN;
    int COL2_X = COL1_X + COL_W + COL_GAP;
    int COL3_X = COL2_X + COL_W + COL_GAP;

    TEST_ASSERT_EQUAL(10, COL1_X);
    TEST_ASSERT_EQUAL(272, COL2_X);
    TEST_ASSERT_EQUAL(534, COL3_X);

    // Right edge should fit in 800px display
    TEST_ASSERT_TRUE(COL3_X + COL_W <= 800);
}

void test_layout_rows_fit_display(void) {
    const int HDR_H = 48;
    const int ROW1_Y = HDR_H + 6;
    const int ROW1_H = 200;
    const int ROW2_Y = ROW1_Y + ROW1_H + 8;
    const int ROW2_H = 200;

    // Both rows should fit in 480px height
    TEST_ASSERT_TRUE(ROW2_Y + ROW2_H <= 480);
    TEST_ASSERT_EQUAL(54, ROW1_Y);
    TEST_ASSERT_EQUAL(262, ROW2_Y);
}

// --- Pixel boundary tests ---

void test_px_clipping_negative(void) {
    memset(fb, 0x55, ALLSCREEN_BYTES);

    // Should not crash or corrupt memory
    px(-1, 0, BLACK);
    px(0, -1, BLACK);
    px(-100, -100, BLACK);

    // First byte should be unchanged
    TEST_ASSERT_EQUAL_HEX8(0x55, fb[0]);
}

void test_px_clipping_overflow(void) {
    memset(fb, 0x55, ALLSCREEN_BYTES);

    // Should not crash or corrupt memory
    px(800, 0, BLACK);
    px(0, 480, BLACK);
    px(9999, 9999, BLACK);

    // Last byte should be unchanged
    TEST_ASSERT_EQUAL_HEX8(0x55, fb[ALLSCREEN_BYTES - 1]);
}
