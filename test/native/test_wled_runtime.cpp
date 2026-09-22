#include "ledbasic_runtime.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

    if (std::strcmp(ledbasicBuiltinName(0), "Rainbow") != 0) {
        std::printf("FAIL: builtin 0 is not Rainbow\n");
        return 1;
    }
    if (ledbasicBuiltinIndex("nope") != -1 || ledbasicWledEnsure(0, 4, "nope")) {
        std::printf("FAIL: unknown name should not load\n");
        return 1;
    }
    ledbasicWledResetAll();

    struct MemFile {
        std::string name;
        std::string data;
    };
    static std::vector<MemFile> mem;
    LedBasicFs memFs = {};
    memFs.user = &mem;
    memFs.list = [](void* user, LedBasicVisitFn visit, void* visitUser) -> bool {
        auto* files = static_cast<std::vector<MemFile>*>(user);
        for (const auto& f : *files) {
            if (!visit(f.name.c_str(), (int)f.data.size(), visitUser)) return true;
        }
        return true;
    };
    memFs.read = [](void* user, const char* name, char* buf, int cap) -> int {
        auto* files = static_cast<std::vector<MemFile>*>(user);
        for (const auto& f : *files) {
            if (f.name != name) continue;
            if (!buf) return (int)f.data.size();
            if (cap < (int)f.data.size()) return -2;
            if (!f.data.empty()) std::memcpy(buf, f.data.data(), f.data.size());
            return (int)f.data.size();
        }
        return -1;
    };
    memFs.write = [](void* user, const char* name, const char* data, int len) -> bool {
        auto* files = static_cast<std::vector<MemFile>*>(user);
        std::string body(data, data && len > 0 ? (size_t)len : 0);
        for (auto& f : *files) {
            if (f.name == name) {
                f.data = std::move(body);
                return true;
            }
        }
        files->push_back(MemFile{name, std::move(body)});
        return true;
    };
    memFs.remove = [](void* user, const char* name) -> bool {
        auto* files = static_cast<std::vector<MemFile>*>(user);
        for (auto it = files->begin(); it != files->end(); ++it) {
            if (it->name == name) {
                files->erase(it);
                return true;
            }
        }
        return false;
    };
    ledbasicWledSetFs(&memFs);

    if (ledbasicUserWrite(&memFs, "Rainbow", "x", 1) != LEDBASIC_STORE_BUILTIN) {
        std::printf("FAIL: builtin overwrite\n");
        return 1;
    }
    if (ledbasicUserWrite(&memFs, "9bad", "x", 1) != LEDBASIC_STORE_BAD_NAME) {
        std::printf("FAIL: bad name\n");
        return 1;
    }
    if (ledbasicUserWrite(&memFs, "TooBig", "x", LEDBASIC_MAX_SOURCE + 1) != LEDBASIC_STORE_TOO_BIG) {
        std::printf("FAIL: size cap\n");
        return 1;
    }

    const char* red =
        "setup\n  clear()\nend\nloop(time)\n  for i = 0 to numled()-1\n"
        "    setled(i, 255, 0, 0)\n  next\n  show()\nend\n";
    const char* green =
        "setup\n  clear()\nend\nloop(time)\n  for i = 0 to numled()-1\n"
        "    setled(i, 0, 255, 0)\n  next\n  show()\nend\n";
    if (ledbasicWledWriteUser("MyRed", red, (int)std::strlen(red)) != LEDBASIC_STORE_OK) {
        std::printf("FAIL: write MyRed\n");
        return 1;
    }
    if (!ledbasicWledSetActiveName("MyRed")) {
        std::printf("FAIL: activate MyRed\n");
        return 1;
    }
    if (std::strcmp(ledbasicWledActiveName(), "MyRed") != 0) {
        std::printf("FAIL: active name\n");
        return 1;
    }
    if (!ledbasicWledEnsure(0, 4, ledbasicWledActiveName()) || !ledbasicWledRun(0, 10)) {
        char err[160];
        ledbasicWledLastError(err, sizeof(err));
        std::printf("FAIL: run MyRed: %s\n", err);
        return 1;
    }
    ledbasicWledGetPixel(0, 0, &r, &g, &b);
    if (r < 200 || g != 0 || b != 0) {
        std::printf("FAIL: MyRed pixel %u,%u,%u\n", r, g, b);
        return 1;
    }
    if (ledbasicWledWriteUser("MyRed", green, (int)std::strlen(green)) != LEDBASIC_STORE_OK) {
        std::printf("FAIL: rewrite MyRed\n");
        return 1;
    }
    if (!ledbasicWledEnsure(0, 4, "MyRed") || !ledbasicWledRun(0, 10)) {
        char err[160];
        ledbasicWledLastError(err, sizeof(err));
        std::printf("FAIL: reload MyRed: %s\n", err);
        return 1;
    }
    ledbasicWledGetPixel(0, 0, &r, &g, &b);
    if (g < 200 || r != 0 || b != 0) {
        std::printf("FAIL: reloaded pixel %u,%u,%u\n", r, g, b);
        return 1;
    }

    std::string tiny = "setup\nend\nloop(time)\nend\n";
    // MyRed already occupies one slot. Fill the rest, then one more must fail.
    for (int i = 1; i < LEDBASIC_MAX_USER; i++) {
        char name[16];
        std::snprintf(name, sizeof(name), "U%d", i);
        int st = ledbasicWledWriteUser(name, tiny.c_str(), (int)tiny.size());
        if (st != LEDBASIC_STORE_OK) {
            std::printf("FAIL: write %s status %d\n", name, st);
            return 1;
        }
    }
    if (ledbasicWledWriteUser("Overflow", tiny.c_str(), (int)tiny.size()) != LEDBASIC_STORE_TOO_MANY) {
        std::printf("FAIL: expected too many\n");
        return 1;
    }
    if (ledbasicWledRemoveUser("MyRed") != LEDBASIC_STORE_OK) {
        std::printf("FAIL: delete MyRed\n");
        return 1;
    }
    if (std::strcmp(ledbasicWledActiveName(), "Rainbow") != 0) {
        std::printf("FAIL: delete should fall back to Rainbow, got %s\n", ledbasicWledActiveName());
        return 1;
    }
    if (!ledbasicWledEnsure(0, 4, 0)) {
        std::printf("FAIL: legacy index ensure\n");
        return 1;
    }

    LedBasicProgramInfo info[40];
    int listed = ledbasicWledListPrograms(info, 40);
    if (listed < 10) {
        std::printf("FAIL: list count %d\n", listed);
        return 1;
    }
    bool sawUser = false;
    for (int i = 0; i < listed; i++) {
        if (info[i].origin == LEDBASIC_ORIGIN_USER) sawUser = true;
    }
    if (!sawUser) {
        std::printf("FAIL: list missing user scripts\n");
        return 1;
    }

    ledbasicWledResetAll();
    std::printf("wled runtime ok (%d programs, pixel0=%u,%u,%u)\n", ledbasicWledProgramCount(), r, g, b);
    return 0;
}
