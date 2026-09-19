#include "BasicInterpreter.h"
#include "BasicExamples/Rainbow.h"
#include "BasicExamples/PulsingCenter.h"
#include "BasicExamples/BikeRolling.h"
#include "BasicExamples/BackgroundStars.h"
#include <cstdio>
#include <cstdlib>

unsigned long ledbasic_mock_millis = 0;

static int gFails = 0;
static int gPass = 0;

#define CHECK(cond, msg) do { \
    if (cond) { gPass++; } \
    else { gFails++; std::printf("FAIL: %s\n", msg); } \
} while (0)

static void testParams() {
    CRGB leds[10];
    BasicLEDController c(leds, 10);
    const char* src = R"(
param speed number(25.0, 5.0, 100.0, 1.0)
param enabled boolean(true)
param offset number(-3.0, -10.0, 10.0, 1.0)
param color_scheme enum(["Red-Blue", "Green-Blue", "Rainbow"])

setup
  brightness(128)
end

loop(time)
  show()
end
)";
    CHECK(c.loadProgram(String(src)), "param program loads");
    auto params = c.getAllParameters();
    CHECK(params.size() == 4, "four parameters discovered");

    Parameter* speed = c.getParameter("speed");
    CHECK(speed && speed->type == PARAM_NUMBER, "speed is number");
    CHECK(speed && speed->currentValue.numberValue == 25.0f, "speed default 25");

    Parameter* enabled = c.getParameter("enabled");
    CHECK(enabled && enabled->type == PARAM_BOOLEAN, "enabled is boolean");
    CHECK(enabled && enabled->currentValue.numberValue == 1.0f, "enabled default true");

    Parameter* offset = c.getParameter("offset");
    CHECK(offset && offset->currentValue.numberValue == -3.0f, "negative default");
    CHECK(offset && offset->minValue == -10.0f, "negative min");

    Parameter* scheme = c.getParameter("color_scheme");
    CHECK(scheme && scheme->type == PARAM_ENUM && scheme->enumValues.size() == 3, "enum values");
}

static void testParseFail() {
    CRGB leds[4];
    BasicLEDController c(leds, 4);
    CHECK(!c.loadProgram(String("setup\n  ???\nend\n")), "invalid tokens fail load");
    {
        auto diags = c.getDiagnostics();
        CHECK(!diags.empty(), "invalid tokens produce diagnostics");
        CHECK(!diags.empty() && diags[0].line == 2, "unexpected char is on line 2");
        CHECK(!diags.empty() && diags[0].kind == Diagnostic::Lex, "unexpected char is a lex error");
    }
    CHECK(!c.loadProgram(String("param speed typo(1)\nsetup\nend\nloop(time)\nend\n")),
          "unknown param type fails load");
    {
        auto diags = c.getDiagnostics();
        CHECK(!diags.empty(), "unknown param type produces diagnostics");
        CHECK(!diags.empty() && diags[0].line >= 1, "param type error has a line");
        CHECK(!diags.empty() && diags[0].kind == Diagnostic::Parse, "param type error is parse");
    }
    CHECK(!c.loadProgram(String("setup\n  clear()\n")), "unterminated setup fails load");
    CHECK(!c.loadProgram(String(
        "setup\nend\nloop(time)\n"
        "if 1\n  while 1\n    x = 1\nelse\n  x = 2\nend\n"
        "end\n")),
        "while missing end cannot steal outer else");
}

static void testReload() {
    CRGB leds[8];
    BasicLEDController c(leds, 8);
    CHECK(c.loadProgram(String(Rainbow::program)), "rainbow loads");
    c.runSetup();
    c.runLoop(1000);
    CHECK(c.loadProgram(String("setup\nend\nloop(time)\nend\n")), "reload empty-ish program");
    c.runSetup();
    c.runLoop(2000);
    CHECK(true, "reload did not crash");
}

static void testHsvSetled() {
    CRGB leds[5];
    for (int i = 0; i < 5; i++) leds[i] = CRGB::Black;
    BasicLEDController c(leds, 5);
    const char* src = R"(
setup
  clear()
end
loop(time)
  setled(0, hsv(0, 255, 255))
  setled(1, rgb(1, 2, 3))
  sethsv(2, -90, 255, 255)
  sethsv(3, 270, 255, 255)
  show()
end
)";
    CHECK(c.loadProgram(String(src)), "color program loads");
    c.runSetup();
    c.runLoop(0);
    CHECK(leds[1].r == 1 && leds[1].g == 2 && leds[1].b == 3, "rgb() packed color via setled");
    CHECK(leds[0].r > 0 || leds[0].g > 0 || leds[0].b > 0, "hsv() produced a color");
    CHECK(leds[2].r == leds[3].r && leds[2].g == leds[3].g && leds[2].b == leds[3].b,
          "negative hue -90 wraps to the same color as 270");
}

static void testEquality() {
    CRGB leds[2];
    BasicLEDController c(leds, 2);
    const char* src = R"(
setup
  s = "a" == "b"
  t = "a" == "a"
  u = rgb(1, 2, 3) == rgb(1, 9, 3)
  v = rgb(1, 2, 3) == rgb(1, 2, 3)
  w = 1 == 1
end
loop(time)
end
)";
    CHECK(c.loadProgram(String(src)), "equality program loads");
    c.runSetup();
    CHECK(c.getNumberVariable("s") == 0.0, "distinct strings are not equal");
    CHECK(c.getNumberVariable("t") == 1.0, "identical strings are equal");
    CHECK(c.getNumberVariable("u") == 0.0, "colors with same red are not equal");
    CHECK(c.getNumberVariable("v") == 1.0, "identical colors are equal");
    CHECK(c.getNumberVariable("w") == 1.0, "numeric equality still works");
}

static void testStringConcat() {
    CRGB leds[2];
    BasicLEDController c(leds, 2);
    const char* src = R"(
setup
  a = "a" + "b"
  b = "n" + 3
end
loop(time)
end
)";
    CHECK(c.loadProgram(String(src)), "string concat program loads");
    c.runSetup();
    CHECK(c.getStringVariable("a") == "ab", "\"a\" + \"b\" == \"ab\"");
}

static void testPowerAndRandom() {
    CRGB leds[2];
    BasicLEDController c(leds, 2);
    const char* src = R"(
setup
  p = 2 * 3 ** 2
  z = random(0)
end
loop(time)
end
)";
    CHECK(c.loadProgram(String(src)), "power program loads");
    c.runSetup();
    CHECK(c.getNumberVariable("p") == 18.0, "2*3**2 == 18");
    CHECK(c.getNumberVariable("z") == 0.0, "random(0) == 0");
}

static void testForLoopWrites() {
    CRGB leds[6];
    for (int i = 0; i < 6; i++) leds[i] = CRGB::Black;
    BasicLEDController c(leds, 6);
    const char* src = R"(
setup
  clear()
end
loop(time)
  for i = 0 to numled()-1
    setled(i, 9, 8, 7)
  next
end
)";
    CHECK(c.loadProgram(String(src)), "for-loop program loads");
    c.runSetup();
    c.runLoop(0);
    CHECK(leds[0].r == 9 && leds[5].b == 7, "for loop wrote all LEDs");
}

static void testVirtualNoPhysicalShow() {
    FastLED.showCount = 0;
    CRGB phys[10];
    VirtualStripManager mgr;
    mgr.addPhysicalStrip(phys, 10);
    VirtualStrip* vs = mgr.createStrip(0, 0, 10, 0, BLEND_REPLACE);
    CHECK(vs != nullptr, "createStrip");
    CHECK(vs->loadProgram(String(Rainbow::program)), "virtual rainbow loads");
    vs->runSetup();
    int showsAfterSetup = FastLED.showCount;
    vs->runLoop(500);
    CHECK(FastLED.showCount == showsAfterSetup, "virtual runLoop does not FastLED.show");
    mgr.renderToPhysical();
    bool any = false;
    for (int i = 0; i < 10; i++) {
        if (phys[i] != CRGB::Black) any = true;
    }
    CHECK(any, "virtual rainbow composited to physical");
}

static void testExamplesLoad() {
    CRGB leds[20];
    BasicLEDController c(leds, 20);
    CHECK(c.loadProgram(String(Rainbow::program)), "Rainbow example");
    auto rp = c.getAllParameters();
    CHECK(rp.size() == 3, "Rainbow has 3 params");

    CHECK(c.loadProgram(String(PulsingCenter::program)), "PulsingCenter example");
    CHECK(c.getParameter("pulse_speed") != nullptr, "PulsingCenter declares pulse_speed");
    CHECK(c.getParameter("color_hue") != nullptr, "PulsingCenter declares color_hue");
    CHECK(c.getParameter("pulse_width") != nullptr, "PulsingCenter declares pulse_width");
    c.runSetup();
    c.runLoop(800);

    CHECK(c.loadProgram(String(BikeRolling::program)), "BikeRolling example");
    CHECK(c.getParameter("head_brightness") != nullptr, "BikeRolling has head_brightness");
}

static void testVirtualClearOnRemove() {
    CRGB phys[4];
    for (int i = 0; i < 4; i++) phys[i] = CRGB(9, 9, 9);
    VirtualStripManager mgr;
    mgr.addPhysicalStrip(phys, 4);
    VirtualStrip* vs = mgr.createStrip(0, 0, 4, 0, BLEND_REPLACE);
    CHECK(vs != nullptr, "createStrip for clear test");
    CHECK(vs->loadProgram(String(
        "setup\n  fill(1,2,3)\nend\nloop(time)\n  fill(4,5,6)\nend\n")),
        "fill program loads");
    vs->runSetup();
    vs->runLoop(0);
    mgr.renderToPhysical();
    CHECK(phys[0].r == 4, "virtual fill composited");
    mgr.removeAllStrips();
    CHECK(phys[0].r == 0 && phys[0].g == 0 && phys[0].b == 0,
          "removeAllStrips clears stale virtual pixels");
}

static void testPhysicalShow() {
    FastLED.showCount = 0;
    CRGB leds[4];
    BasicLEDController c(leds, 4);
    CHECK(c.loadProgram(String("setup\nend\nloop(time)\n  fill(1,2,3)\n  show()\nend\n")), "physical show program");
    c.runLoop(0);
    CHECK(FastLED.showCount >= 1, "physical show() calls FastLED.show");
}

static unsigned long gTestClock = 0;
static unsigned long testMillisFn(void*) { return gTestClock; }
static void testDelayFn(int ms, void*) { gTestClock += (unsigned long)ms; }

static void testInjectedClock() {
    CRGB leds[2];
    BasicLEDController c(leds, 2);
    gTestClock = 0;
    c.setClock(testMillisFn, testDelayFn);
    const char* src = R"(
setup
  delay(50)
  t = millis()
end
loop(time)
end
)";
    CHECK(c.loadProgram(String(src)), "clock program loads");
    c.runSetup();
    CHECK(c.getNumberVariable("t") == 50.0, "delay(50) advanced injected millis to 50");
}

struct LineRec {
    int lines[64];
    int depths[64];
    int n;
};

static void recHook(int line, int column, int depth, void* user) {
    (void)column;
    LineRec* r = (LineRec*)user;
    if (r->n < 64) {
        r->lines[r->n] = line;
        r->depths[r->n] = depth;
        r->n++;
    }
}

static void testDebugHook() {
    CRGB leds[2];
    BasicLEDController c(leds, 2);
    const char* src =
        "setup\n"
        "  x = 1\n"
        "  y = 2\n"
        "end\n"
        "loop(time)\n"
        "  x = x + 1\n"
        "  show()\n"
        "end\n";
    CHECK(c.loadProgram(String(src)), "debug program loads");
    LineRec r;
    r.n = 0;
    c.setDebugHook(recHook, &r);
    c.runSetup();
    CHECK(r.n == 2, "setup hits two assignment statements");
    CHECK(r.n >= 1 && r.lines[0] == 2, "first setup statement is line 2");
    CHECK(r.n >= 2 && r.lines[1] == 3, "second setup statement is line 3");
    CHECK(r.n >= 1 && r.depths[0] == 1, "top-level statement depth is 1");
    int afterSetup = r.n;
    c.runLoop(0);
    CHECK(r.n == afterSetup + 2, "loop hits assignment and show()");
    CHECK(r.n >= afterSetup + 1 && r.lines[afterSetup] == 6, "loop assignment is line 6");
    c.setDebugHook(nullptr, nullptr);
}

static void testSnippetEval() {
    CRGB leds[2];
    BasicLEDController c(leds, 2);
    CHECK(c.loadProgram(String("setup\n  x = 4\nend\nloop(time)\nend\n")), "snippet program loads");
    c.runSetup();
    Value result;
    Diagnostic err;
    CHECK(c.evalSnippet(String("1+2"), result, err), "snippet 1+2 succeeds");
    CHECK(result.asNumber() == 3.0f, "1+2 == 3");
    CHECK(c.evalSnippet(String("x = 9"), result, err), "snippet assignment succeeds");
    CHECK(c.getNumberVariable("x") == 9.0, "x is 9 after snippet");
    CHECK(result.asNumber() == 9.0f, "assignment snippet returns new value");
    CHECK(!c.evalSnippet(String("???"), result, err), "bad snippet fails");
    CHECK(err.kind == Diagnostic::Lex || err.kind == Diagnostic::Parse, "bad snippet has diagnostic kind");

    auto vars = c.getAllVariables();
    bool sawX = false;
    for (size_t i = 0; i < vars.size(); i++) {
        if (vars[i].name == "x") sawX = true;
    }
    CHECK(sawX, "getAllVariables includes x");
}

static void testRuntimeDiagnostic() {
    CRGB leds[2];
    BasicLEDController c(leds, 2);
    const char* src =
        "setup\n"
        "  for i = 0 to 10 step 0\n"
        "    x = 1\n"
        "  next\n"
        "end\n"
        "loop(time)\n"
        "end\n";
    CHECK(c.loadProgram(String(src)), "zero-step program loads");
    c.runSetup();
    CHECK(c.hasRuntimeError(), "zero step is a runtime error");
    auto diags = c.getDiagnostics();
    bool saw = false;
    for (size_t i = 0; i < diags.size(); i++) {
        if (diags[i].kind == Diagnostic::Runtime && diags[i].line == 2) saw = true;
    }
    CHECK(saw, "runtime diagnostic is on the for line");
}

int main() {
    testParams();
    testParseFail();
    testReload();
    testHsvSetled();
    testEquality();
    testStringConcat();
    testPowerAndRandom();
    testForLoopWrites();
    testVirtualNoPhysicalShow();
    testVirtualClearOnRemove();
    testExamplesLoad();
    testPhysicalShow();
    testInjectedClock();
    testDebugHook();
    testSnippetEval();
    testRuntimeDiagnostic();

    std::printf("%d passed, %d failed\n", gPass, gFails);
    return gFails ? 1 : 0;
}
