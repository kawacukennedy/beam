#pragma once
#include <string>
#include <functional>

class AutoUpdate {
public:
    AutoUpdate();
    ~AutoUpdate();

    void check_for_updates(std::function<void(bool update_available, const std::string& version)> callback);
    void download_and_install(const std::string& url);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};