#include "WledJson.h"

#include <cstdio>
#include <string>

static int fail(const char* msg) {
    std::printf("FAIL: %s\n", msg);
    return 1;
}

int main() {
    using ledbasic::DeviceCatalog;
    using ledbasic::joinDeviceUrl;
    using ledbasic::parseProgramList;
    using ledbasic::urlEncodeQuery;

    if (joinDeviceUrl("http://192.168.1.20/", "/ledbasic/programs") !=
        "http://192.168.1.20/ledbasic/programs") {
        return fail("join strips a trailing slash");
    }
    if (joinDeviceUrl("wled.local", "ledbasic/programs") != "http://wled.local/ledbasic/programs") {
        return fail("join adds scheme and slash");
    }
    if (joinDeviceUrl("  http://wled.local:80/  ", "/ledbasic/frame") !=
        "http://wled.local:80/ledbasic/frame") {
        return fail("join keeps the port");
    }
    if (urlEncodeQuery("My_Effect") != "My_Effect") return fail("encode leaves names");
    if (urlEncodeQuery("a b") != "a%20b") return fail("encode spaces");

    const char* json =
        "{\"active\":\"Rainbow\",\"programs\":["
        "{\"name\":\"Rainbow\",\"origin\":\"builtin\",\"bytes\":10},"
        "{\"name\":\"MyChase\",\"origin\":\"user\",\"bytes\":4}"
        "]}";
    DeviceCatalog cat;
    if (!parseProgramList(json, cat)) return fail("parse");
    if (cat.active != "Rainbow" || cat.programs.size() != 2) return fail("catalog size");
    if (cat.programs[1].name != "MyChase" || cat.programs[1].origin != "user" || cat.programs[1].bytes != 4) {
        return fail("user entry");
    }
    if (parseProgramList("{}", cat)) return fail("empty object should fail");

    std::printf("wled client json ok\n");
    return 0;
}
