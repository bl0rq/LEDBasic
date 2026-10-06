#include "tui_test.h"

int gFails = 0;
int gPass = 0;

void runEditorTests();
void runAppTests();

int main() {
    runEditorTests();
    runAppTests();
    std::printf("%d passed, %d failed\n", gPass, gFails);
    return gFails ? 1 : 0;
}
