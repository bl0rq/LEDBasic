#include "tui_test.h"

int gFails = 0;
int gPass = 0;

void runEditorTests();
void runAppTests();
void runFileBrowserTests();
void runFileMenuTests();
void runDeviceClientTests();

int main() {
    runEditorTests();
    runAppTests();
    runFileBrowserTests();
    runFileMenuTests();
    runDeviceClientTests();
    std::printf("%d passed, %d failed\n", gPass, gFails);
    return gFails ? 1 : 0;
}
