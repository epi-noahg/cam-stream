// Tests for detector zone-label → game Throw translation.

#include "detection/DetectionService.hpp"

#include <iostream>

using namespace dart::detect;
using dart::game::Throw;

static int g_failures = 0;
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  "        \
                      << #cond << "\n";                                        \
        }                                                                      \
    } while (0)

static bool eq(const Throw& t, int v, int m) {
    return t.value == v && t.multiplier == m;
}

int main() {
    CHECK(eq(zoneToThrow("T20"), 20, 3));
    CHECK(eq(zoneToThrow("D5"), 5, 2));
    CHECK(eq(zoneToThrow("20"), 20, 1));
    CHECK(eq(zoneToThrow("1"), 1, 1));
    CHECK(eq(zoneToThrow("Bull"), 50, 1));
    CHECK(eq(zoneToThrow("50"), 50, 1));
    CHECK(eq(zoneToThrow("25"), 25, 1));
    CHECK(eq(zoneToThrow("MISS"), 0, 1));
    CHECK(eq(zoneToThrow(""), 0, 1));
    CHECK(zoneToThrow("T20").hitValue() == 60);
    CHECK(zoneToThrow("D5").hitValue() == 10);
    CHECK(zoneToThrow("Bull").hitValue() == 50);

    if (g_failures == 0) std::cout << "All zone-parse tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
