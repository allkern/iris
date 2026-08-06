#include "net.hpp"

namespace iris::net {

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

struct url {
    std::string scheme;
    std::string host;
    uint16_t port;
    std::string path;
};

url parse_url(std::string uri) {
    url result;

    size_t scheme_end = uri.find("://");

    result.scheme = uri.substr(0, scheme_end);
    size_t host_start = scheme_end + 3;

    size_t port_start = uri.find(':', host_start);
    size_t path_start = uri.find('/', host_start);

    if (port_start != std::string::npos && (path_start == std::string::npos || port_start < path_start)) {
        result.host = uri.substr(host_start, port_start - host_start);
        result.port = std::stoi(uri.substr(port_start + 1, path_start - port_start - 1));
    } else {
        result.host = uri.substr(host_start, path_start - host_start);
        result.port = (result.scheme == "https") ? 443 : 80;
    }

    if (path_start != std::string::npos) {
        result.path = uri.substr(path_start);
    } else {
        result.path = "/";
    }

    return result;
}

size_t curl_write_callback(void *ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*) ptr, size * nmemb);

    return size * nmemb;
}

DownloadResult download(std::string url) {
    auto parsed_url = parse_url(url);

    std::string reconstructed_url = parsed_url.scheme + "://" + parsed_url.host;

    if (parsed_url.port == 443 || parsed_url.port == 80) {
        reconstructed_url += parsed_url.path;
    } else {
        reconstructed_url += ":" + std::to_string(parsed_url.port) + parsed_url.path;
    }

    std::string data;

    CURL* curl = curl_easy_init();

    if (!curl) {
        return { -1, "" };
    }

    curl_easy_setopt(curl, CURLOPT_URL, reconstructed_url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
 
    /* Perform the request, result gets the return code */
    CURLcode result = curl_easy_perform(curl);

    long response_code = 0;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);

    /* Check for errors */
    if (result != CURLE_OK) {
        return { (int)response_code, "" };
    }

    return { (int)response_code, data };
}

}