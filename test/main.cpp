#include "gtest/gtest.h"

#ifdef ABSL_HAVE_ADDRESS_SANITIZER
extern "C" const char* __asan_default_options() {
    return "detect_odr_violation=1";
}
#endif // ABSL_HAVE_ADDRESS_SANITIZER

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
