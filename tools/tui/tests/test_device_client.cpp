#include "DeviceClient.h"
#include "tui_test.h"

namespace {

using namespace ledbasic;

HttpResult okJson(const std::string& body, int status = 200) {
    HttpResult r;
    r.transportOk = true;
    r.status = status;
    r.body = body;
    return r;
}

void testBaseUrlNormalization() {
    DeviceClient c;
    c.setBaseUrl("  192.168.4.1///  ");
    CHECK(c.baseUrl == "http://192.168.4.1", "url: adds http and trims slashes");

    c.setBaseUrl("HTTP://LEDBASIC.LOCAL/");
    CHECK(c.baseUrl == "http://LEDBASIC.LOCAL", "url: handles uppercase HTTP scheme");

    c.setBaseUrl("https://board.local:8443/extra?a=1#frag");
    CHECK(c.baseUrl == "https://board.local:8443/extra", "url: strips query and fragment");

    c.setBaseUrl("http:///");
    CHECK(c.baseUrl.empty(), "url: bare scheme clears baseUrl");

    c.setBaseUrl("   ");
    CHECK(c.baseUrl.empty(), "url: whitespace clears baseUrl");
}

void testStatusKeyCollisionAndEscapes() {
    // User program named "brightness" and SSID named "palette" must not hijack real JSON keys.
    HttpResult r = okJson(
        R"({"mode":"sta","ssid":"palette","program":"brightness","builtin":false,"unsaved":false,"brightness":180,"palette":"Heat","error":null})");
    DeviceStatus st = parseDeviceStatus(r);
    CHECK(st.ok, "status: parses valid JSON");
    CHECK(st.program == "brightness", "status: reads program named brightness");
    CHECK(st.master == 180, "status: real brightness is not hijacked by program name");
    CHECK(st.palette == "Heat", "status: real palette is not hijacked by ssid");
    CHECK(!st.unsaved, "status: unsaved is false");
    CHECK(st.fault.empty(), "status: null error produces no fault");

    // Error message containing '}' and escaped quotes must not truncate.
    HttpResult errStatus = okJson(
        R"({"mode":"sta","program":"Test","unsaved":false,"brightness":200,"palette":"Rainbow","error":{"line":4,"column":7,"message":"Unexpected character '}' near \"x\""}})");
    DeviceStatus stErr = parseDeviceStatus(errStatus);
    CHECK(stErr.ok, "status error: parses ok");
    CHECK(stErr.fault == "4:7 Unexpected character '}' near \"x\"",
          "status error: preserves '}' and escaped quotes with line:col");
}

void testRejectsHtmlAndTruncatedJson() {
    HttpResult html = okJson("<!doctype html><html><body>LEDBasic</body></html>");
    DeviceStatus st = parseDeviceStatus(html);
    CHECK(!st.ok && st.message == "invalid device response", "status: rejects HTML 200");

    std::vector<DeviceProgram> progs;
    std::string err;
    CHECK(!parseDevicePrograms(html, progs, err), "programs: rejects HTML 200");

    DeviceLed led;
    CHECK(!parseDeviceLed(html, led, err), "led: rejects HTML 200");

    HttpResult trunc = okJson(R"({"mode":"sta","brightness":128)");
    CHECK(!parseDeviceStatus(trunc).ok, "status: rejects truncated JSON");
}

void testDiagnosticsAndRestart() {
    HttpResult bad = okJson(
        R"({"ok":false,"error":"program rejected","diagnostics":[{"line":2,"column":3,"message":"Unexpected character '}'"}]})",
        400);
    CHECK(deviceResultMessage(bad) == "device 400 program rejected 2:3 Unexpected character '}'",
          "diag: extracts message containing '}'");

    CHECK(deviceWillRestart(okJson(R"({"ok":true,"restart":true})")), "restart: detects true");
    CHECK(!deviceWillRestart(okJson(R"({"ok":false,"error":"\"restart\":true"})")),
          "restart: ignores substring inside error string");
}

void testProgramsAndLedParsing() {
    HttpResult progsHttp = okJson(
        R"([{"name":"Rainbow","builtin":true,"bytes":400,"active":true},{"name":"Custom_1","builtin":false,"bytes":120,"active":false}])");
    std::vector<DeviceProgram> progs;
    std::string err;
    CHECK(parseDevicePrograms(progsHttp, progs, err), "programs: parses array");
    CHECK(progs.size() == 2 && progs[0].name == "Rainbow" && progs[0].active &&
          progs[1].name == "Custom_1" && !progs[1].builtin && progs[1].bytes == 120,
          "programs: extracts all fields");

    HttpResult ledHttp = okJson(
        R"({"type":"ws2811","order":"RGB","length":120,"pin":14,"brightness":90,"measure":{"active":false,"end":120,"max":1024}})");
    DeviceLed led;
    CHECK(parseDeviceLed(ledHttp, led, err), "led: parses object with nested measure");
    CHECK(led.type == "ws2811" && led.order == "RGB" && led.length == 120 && led.pin == 14 && led.brightness == 90,
          "led: extracts all fields");
}

}  // namespace

void runDeviceClientTests() {
    testBaseUrlNormalization();
    testStatusKeyCollisionAndEscapes();
    testRejectsHtmlAndTruncatedJson();
    testDiagnosticsAndRestart();
    testProgramsAndLedParsing();
}
