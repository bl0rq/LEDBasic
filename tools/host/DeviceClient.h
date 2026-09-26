#pragma once

#include <string>
#include <vector>

namespace ledbasic {

struct HttpResult {
    bool transportOk = false;
    int status = 0;
    std::string body;
    std::string error;
};

struct DeviceStatus {
    bool ok = false;
    std::string mode;
    std::string program;
    bool unsaved = false;
    int master = 255;
    std::string fault;
    std::string message;
};

struct DeviceProgram {
    std::string name;
    bool builtin = false;
    int bytes = 0;
    bool active = false;
};

struct DeviceLed {
    std::string type = "ws2812";
    std::string order = "GRB";
    int length = 60;
    int pin = 16;
    int brightness = 128;
};

class DeviceClient {
public:
    std::string baseUrl;

    void setBaseUrl(std::string url);
    HttpResult request(const std::string& method, const std::string& path,
                       const std::string& body, const std::string& contentType) const;

    DeviceStatus status() const;
    HttpResult runSource(const std::string& source) const;
    HttpResult upload(const std::string& name, const std::string& source, bool activate) const;
    HttpResult activate(const std::string& name) const;
    HttpResult removeProgram(const std::string& name) const;
    bool programs(std::vector<DeviceProgram>& out, std::string& error) const;
    bool programSource(const std::string& name, std::string& out, std::string& error) const;
    bool led(DeviceLed& out, std::string& error) const;
    HttpResult putLed(const DeviceLed& led) const;
    HttpResult setBrightness(int brightness) const;
};

std::string deviceUrlPath();
bool loadDeviceUrl(std::string& url);
bool saveDeviceUrl(const std::string& url);

bool validUserProgramName(const std::string& name);
bool isBuiltinProgramName(const std::string& name);
std::string programNameFromPath(const std::string& path);
std::string deviceResultMessage(const HttpResult& result);
bool deviceWillRestart(const HttpResult& result);

}  // namespace ledbasic
