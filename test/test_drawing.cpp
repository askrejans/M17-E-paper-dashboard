#include <unity.h>
#include "display/drawing.h"
#include "display/epd_display.h"
#include <stdlib.h>
#include "font_8x16.h"

extern unsigned char *fb;

void test_px_pack(void) {
    if (!fb) {
        fb = (unsigned char*)malloc(ALLSCREEN_BYTES);
    }
    memset(fb, 0, ALLSCREEN_BYTES);

    // (0,0) -> first 2 bits (bits 6-7)
    px(0,0, white);
    TEST_ASSERT_EQUAL_HEX8(0x40, fb[0]);

    // (1,0) -> next two bits (bits 4-5), set to YELLOW (2)
    px(1,0, yellow);
    TEST_ASSERT_EQUAL_HEX8(0x60, fb[0]);
}

void test_fillRect_and_pack(void) {
    if (!fb) {
        fb = (unsigned char*)malloc(ALLSCREEN_BYTES);
    }
    memset(fb, 0, ALLSCREEN_BYTES);

    // fill a 2x2 white square at origin
    fillRect(0,0,2,2, white);

    // bytes for row 0 (pixels 0 and 1) packed into fb[0]
    // white (1) at pixel0 -> 0x40, white at pixel1 -> 0x10 => combined 0x50
    TEST_ASSERT_EQUAL_HEX8(0x50, fb[0]);

    // row 1 starts at index 800 -> byte index 800>>2 == 200
    TEST_ASSERT_EQUAL_HEX8(0x50, fb[200]);
}

void test_rect_border(void) {
    if (!fb) {
        fb = (unsigned char*)malloc(ALLSCREEN_BYTES);
    }
    // populate with visible pattern (0x55) so drawing BLACK clears bits
    memset(fb, 0x55, ALLSCREEN_BYTES);

    rect(0,0,3,3);

    // top-left pixel should have been cleared in the border
    TEST_ASSERT_NOT_EQUAL(0x55, fb[0]);
}

void test_rectWithTitle_changes_titlebar(void) {
    if (!fb) {
        fb = (unsigned char*)malloc(ALLSCREEN_BYTES);
    }
    memset(fb, 0x55, ALLSCREEN_BYTES);

    rectWithTitle(10,10,40,30, "Hi");

    // Title bar area should have changed from the 0x55 pattern
    int tx = 10 + 1;
    int ty = 10 + 1;
    uint32_t idx = (ty * 800 + tx) >> 2;
    TEST_ASSERT_NOT_EQUAL(0x55, fb[idx]);
}

