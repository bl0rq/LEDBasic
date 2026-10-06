#include "tui_test.h"

int gFails = 0;
int gPass = 0;

void runEditorTests();
void runAppTests();
void runFileMenuTests();

int main() {
    runEditorTests();
    runAppTests();
    runFileMenuTests();
    std::printf("%d passed, %d failed\n", gPass, gFails);
    return gFails ? 1 : 0;
}
