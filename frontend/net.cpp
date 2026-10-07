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
