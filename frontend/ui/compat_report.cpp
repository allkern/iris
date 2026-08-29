#include <string>
#include <algorithm>
#include <cctype>

#include "iris.hpp"
#include "config.hpp"

#include "res/IconsMaterialSymbols.h"
#include "ps2.hpp"

namespace iris {

static const char* compat_rating_names[] = {
    "0 - Nothing",
    "1 - Intro",
    "2 - Menus",
    "3 - Ingame",
    "4 - Playable",
    "5 - Perfect"
};

static std::string url_encode(const std::string& str) {
    static const char hex[] = "0123456789ABCDEF";

    std::string out;
    out.reserve(str.size() * 3);

    for (unsigned char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(c);
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xf]);
        }
    }

    return out;
}

static std::string get_disc_serial(Instance* iris) {
    if (!iris->ps2 || !iris->ps2->cdvd || !iris->ps2->cdvd->disc)
        return "";

    char buf[128];

    if (!iop::disc::get_serial(iris->ps2->cdvd->disc, buf))
        return "";

    std::string serial = buf;

    std::replace(serial.begin(), serial.end(), '_', '-');
    serial.erase(std::remove(serial.begin(), serial.end(), '.'), serial.end());

    return serial;
}

void CompatReport::on_open() {
    arcade = iris->arcade_id.size() != 0;
    id = arcade ? iris->arcade_id : get_disc_serial(iris);
    rating = 3;
    comment[0] = '\0';
}

void CompatReport::on_render() {
    using namespace ImGui;

    TextDisabled(arcade ? "Arcade set name" : "Serial");

    if (id.empty()) {
        TextDisabled("No serial detected");
    } else {
        Text("%s", id.c_str());
    }

    Spacing();

    SetNextItemWidth(220.0f);
    Combo("Rating", &rating, compat_rating_names, IM_ARRAYSIZE(compat_rating_names));

    Spacing();

    TextDisabled("Comment");
    InputTextMultiline("##comment", comment, sizeof(comment), ImVec2(400.0f, 100.0f));

    Spacing();
    Separator();
    Spacing();

    BeginDisabled(id.empty());

    if (Button(ICON_MS_SEND " Create report")) {
        std::string url =
            "https://iris-compat.vercel.app/report?platform=" + std::string(arcade ? "arcade" : "retail") +
            "&id=" + url_encode(id) +
            "&rating=" + std::to_string(rating) +
            "&comment=" + url_encode(comment) +
            "&commit=" + url_encode(IRIS_COMMIT);

        SDL_OpenURL(url.c_str());

        open = false;
    }

    EndDisabled();

    SameLine();

    if (Button(ICON_MS_CLOSE " Cancel")) {
        open = false;
    }
}

}
