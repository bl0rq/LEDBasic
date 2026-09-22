#ifndef LEDBASIC_WLED_JSON_H
#define LEDBASIC_WLED_JSON_H

#include <string>
#include <vector>

namespace ledbasic {

struct DeviceProgram {
    std::string name;
    std::string origin;
    int bytes = 0;
};

struct DeviceCatalog {
    std::string active;
    std::vector<DeviceProgram> programs;
};

std::string joinDeviceUrl(const std::string& base, const std::string& path);
std::string urlEncodeQuery(const std::string& value);
bool parseProgramList(const std::string& json, DeviceCatalog& out);

}

#endif
