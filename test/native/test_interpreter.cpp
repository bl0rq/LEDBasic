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
    CHECK(!c.loadProgram(String("param speed typo(1)\nsetup\nend\nloop(time)\nend\n")),
          "unknown param type fails load");
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
  show()
end
)";
    CHECK(c.loadProgram(String(src)), "color program loads");
    c.runSetup();
    c.runLoop(0);
    CHECK(leds[1].r == 1 && leds[1].g == 2 && leds[1].b == 3, "rgb() packed color via setled");
    CHECK(leds[0].r > 0 || leds[0].g > 0 || leds[0].b > 0, "hsv() produced a color");
    CHECK(leds[2].r > 0 || leds[2].g > 0 || leds[2].b > 0, "negative hue wrapped");
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

static void testPhysicalShow() {
    FastLED.showCount = 0;
    CRGB leds[4];
    BasicLEDController c(leds, 4);
    CHECK(c.loadProgram(String("setup\nend\nloop(time)\n  fill(1,2,3)\n  show()\nend\n")), "physical show program");
    c.runLoop(0);
    CHECK(FastLED.showCount >= 1, "physical show() calls FastLED.show");
}

int main() {
    testParams();
    testParseFail();
    testReload();
    testHsvSetled();
    testStringConcat();
    testPowerAndRandom();
    testForLoopWrites();
    testVirtualNoPhysicalShow();
    testExamplesLoad();
    testPhysicalShow();

    std::printf("%d passed, %d failed\n", gPass, gFails);
    return gFails ? 1 : 0;
}
