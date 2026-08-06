#include <string>
#include <algorithm>
#include <cctype>

#include "iris.hpp"
#include "config.hpp"

#include "res/IconsMaterialSymbols.h"
#include "ps2.hpp"

namespace iris {

const char* compat_rating_names[] = {
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

void show_compat_report(Instance* iris) {
    using namespace ImGui;

    static int rating = 3;
    static char comment[1024] = "";
    static std::string serial;

    if (imgui::BeginEx("Report compatibility", &iris->ui.show_compat_report, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (IsWindowAppearing()) {
            serial = get_disc_serial(iris);
            rating = 3;
            comment[0] = '\0';
        }

        TextDisabled("Serial");
        if (serial.empty()) {
            TextDisabled("No serial detected");
        } else {
            Text("%s", serial.c_str());
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

        BeginDisabled(serial.empty());

        if (Button(ICON_MS_SEND " Create report")) {
            std::string url =
                "https://iris-compat.vercel.app/report?serial=" + url_encode(serial) +
                "&rating=" + std::to_string(rating) +
                "&comment=" + url_encode(comment) +
                "&commit=" + url_encode(IRIS_COMMIT);

            SDL_OpenURL(url.c_str());

            iris->ui.show_compat_report = false;
        }

        EndDisabled();

        SameLine();

        if (Button(ICON_MS_CLOSE " Cancel")) {
            iris->ui.show_compat_report = false;
        }
    } End();
}

}
