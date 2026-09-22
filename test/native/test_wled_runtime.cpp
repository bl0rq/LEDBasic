#include "ledbasic_runtime.h"
#include <cstdio>

unsigned long ledbasic_mock_millis = 0;

int main() {
    if (ledbasicWledProgramCount() != 10) {
        std::printf("FAIL: expected 10 programs, got %d\n", ledbasicWledProgramCount());
        return 1;
    }
    if (!ledbasicWledEnsure(0, 10, 0)) {
        char err[160];
        ledbasicWledLastError(err, sizeof(err));
        std::printf("FAIL: ensure Rainbow: %s\n", err);
        return 1;
    }
    ledbasicWledApplySliders(0, 128, 200, 0, 0, false, false, false);
    if (!ledbasicWledRun(0, 500)) {
        char err[160];
        ledbasicWledLastError(err, sizeof(err));
        std::printf("FAIL: run: %s\n", err);
        return 1;
    }
    uint8_t r = 0, g = 0, b = 0;
    if (!ledbasicWledGetPixel(0, 0, &r, &g, &b)) {
        std::printf("FAIL: get pixel\n");
        return 1;
    }
    bool any = false;
    for (int i = 0; i < ledbasicWledNumLeds(0); i++) {
        uint8_t pr, pg, pb;
        ledbasicWledGetPixel(0, i, &pr, &pg, &pb);
        if (pr || pg || pb) any = true;
    }
    if (!any) {
        std::printf("FAIL: Rainbow produced all black\n");
        return 1;
    }
    if (!ledbasicWledSetNamedParam("speed", 40.0f)) {
        std::printf("FAIL: set named param\n");
        return 1;
    }
    LedBasicWledParam params[LEDBASIC_WLED_MAX_PARAMS];
    int n = ledbasicWledGetParams(0, params, LEDBASIC_WLED_MAX_PARAMS);
    if (n < 1) {
        std::printf("FAIL: get params\n");
        return 1;
    }
    ledbasicWledResetAll();
    std::printf("wled runtime ok (%d programs, pixel0=%u,%u,%u)\n", ledbasicWledProgramCount(), r, g, b);
    return 0;
}
