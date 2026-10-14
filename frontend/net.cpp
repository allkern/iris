/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "net.hpp"

namespace iris::net {

struct Session {
    CURL* curl = nullptr;
};

bool init(LogSource* log) {
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (res != CURLE_OK) {
        iris_error(log, "curl_global_init() failed: {}", curl_easy_strerror(res));

        return false;
    }

    return true;
}

void cleanup() {
    curl_global_cleanup();
}

static size_t write_callback(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)ptr, size * nmemb);

    return size * nmemb;
}

Session* open_session() {
    CURL* curl = curl_easy_init();

    if (!curl) {
        return nullptr;
    }

    Session* session = new Session();

    session->curl = curl;

    return session;
}

void close_session(Session* session) {
    if (!session) {
        return;
    }

    curl_easy_cleanup(session->curl);

    delete session;
}

DownloadResult download(Session* session, const std::string& url) {
    if (!session) {
        return { -1, "" };
    }

    CURL* curl = session->curl;

    std::string data;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Iris");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    CURLcode result = curl_easy_perform(curl);

    long response_code = 0;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (result != CURLE_OK) {
        return { (int)response_code, "" };
    }

    return { (int)response_code, std::move(data) };
}

DownloadResult download(const std::string& url) {
    Session* session = open_session();

    DownloadResult result = download(session, url);

    close_session(session);

    return result;
}

}
