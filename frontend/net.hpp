#pragma once

#include <string>

#include <curl/curl.h>

#include "log.hpp"

namespace iris::net {

struct DownloadResult {
    int status;
    std::string body;
};

struct Session;

bool init(LogSource* log);
void cleanup();

Session* open_session();
void close_session(Session* session);

DownloadResult download(Session* session, const std::string& url);
DownloadResult download(const std::string& url);

}
