#ifndef LEDBASIC_WLED_CLIENT_H
#define LEDBASIC_WLED_CLIENT_H

#include "WledJson.h"

#include <string>

namespace ledbasic {

struct DeviceResponse {
    int status = 0;
    std::string body;
    std::string error;
};

class WledClient {
public:
    void setBaseUrl(std::string base);
    const std::string& baseUrl() const { return base_; }
    bool ready() const { return !base_.empty(); }

    bool listPrograms(DeviceCatalog& out, std::string& err) const;
    bool getSource(const std::string& name, std::string& source, std::string& err) const;
    bool putSource(const std::string& name, const std::string& source, std::string& err) const;
    bool removeProgram(const std::string& name, std::string& err) const;
    bool activate(const std::string& name, std::string& err) const;

private:
    std::string base_;
    DeviceResponse request(const std::string& method, const std::string& path,
                           const std::string& body, const std::string& contentType) const;
};

}

#endif
