#include <unity.h>
#include "utils/utils.h"

void test_console_log_functions(void) {
    // Call logging helpers to ensure they compile and run
    logTitle("UNIT TEST");
    logKV("Key", "Value");
    logKVf("Temp", 23.5f, "C");

    // Nothing to assert — success is that functions execute without crash
    TEST_ASSERT_TRUE(true);
}
