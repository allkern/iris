#include <filesystem>
#include <string>

#include "iris.hpp"
#include "ee/vu_def.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "ps2.hpp"

constexpr unsigned char g_gamecontrollerdb_data[] = {
#embed "../deps/SDL_GameControllerDB/gamecontrollerdb.txt"
};
constexpr unsigned int g_gamecontrollerdb_size = sizeof(g_gamecontrollerdb_data);

namespace iris {

void KeyboardDevice::handle_event(Instance* iris, SDL_Event* event) {
    auto ievent = input::sdl_event_to_input_event(event);
    auto action = input::get_input_action(iris, m_slot, ievent.u64);

    if (!action)
        return;

    input::execute_action(iris, *action, m_slot, event->type == SDL_EVENT_KEY_DOWN ? 1.0f : 0.0f);
}

void GamepadDevice::handle_event(Instance* iris, SDL_Event* event) {
    auto ievent = input::sdl_event_to_input_event(event);
    auto action = input::get_input_action(iris, m_slot, ievent.u64);

    if (!action)
        return;

    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        input::execute_action(iris, *action, m_slot, 1.0f);
    } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        input::execute_action(iris, *action, m_slot, 0.0f);
    } else if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        // Convert from -32768->32767 to -1.0->1.0 and take absolute value
        float value = fabs(event->gaxis.value / 32767.0f);

        input::execute_action(iris, *action, m_slot, value);
    }
}

}

namespace iris::input {

void load_db_default(Instance* iris) {
    SDL_IOStream* ios = SDL_IOFromConstMem(g_gamecontrollerdb_data, g_gamecontrollerdb_size);

    SDL_AddGamepadMappingsFromIO(ios, true);
}

bool load_db_from_file(Instance* iris, const char* path) {
    if (SDL_AddGamepadMappingsFromFile(path) == -1)
        return false;

    return true;
}

#define IEVENT(event, id, mod) \
    (((uint64_t)event << 32) | (((id & 0xf0000fff) | ((mod & 0xffff) << 12)) & 0xffffffff))

void init_default_mapping(Instance* iris, int id) {
    Mapping& map = iris->input.input_maps[id];

    if (id == 0) {
        map.name = "Keyboard (default)";

        map.map.clear();

        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_X     , SDL_KMOD_NONE), IRIS_DS_BT_CROSS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_A     , SDL_KMOD_NONE), IRIS_DS_BT_SQUARE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_W     , SDL_KMOD_NONE), IRIS_DS_BT_TRIANGLE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_D     , SDL_KMOD_NONE), IRIS_DS_BT_CIRCLE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_RETURN, SDL_KMOD_NONE), IRIS_DS_BT_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_S     , SDL_KMOD_NONE), IRIS_DS_BT_SELECT);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_UP    , SDL_KMOD_NONE), IRIS_DS_BT_UP);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_DOWN  , SDL_KMOD_NONE), IRIS_DS_BT_DOWN);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_LEFT  , SDL_KMOD_NONE), IRIS_DS_BT_LEFT);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_RIGHT , SDL_KMOD_NONE), IRIS_DS_BT_RIGHT);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_Q     , SDL_KMOD_NONE), IRIS_DS_BT_L1);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_E     , SDL_KMOD_NONE), IRIS_DS_BT_R1);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_1     , SDL_KMOD_NONE), IRIS_DS_BT_L2);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_3     , SDL_KMOD_NONE), IRIS_DS_BT_R2);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_Z     , SDL_KMOD_NONE), IRIS_DS_BT_L3);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_C     , SDL_KMOD_NONE), IRIS_DS_BT_R3);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_I     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_J     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_K     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_L     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_T     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_F     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_G     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_H     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_0     , SDL_KMOD_NONE), IRIS_S14X_SW_SERVICE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_9     , SDL_KMOD_NONE), IRIS_S14X_SW_TEST);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_8     , SDL_KMOD_NONE), IRIS_S14X_SW_ENTER);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_7     , SDL_KMOD_NONE), IRIS_S14X_SW_UP);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_6     , SDL_KMOD_NONE), IRIS_S14X_SW_DOWN);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_1     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P1_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_2     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P2_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_3     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P3_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_4     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P4_START);
    } else {
        map.name = "Gamepad (default)";

        map.map.clear();

        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_SOUTH         , SDL_KMOD_NONE), IRIS_DS_BT_CROSS);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_WEST          , SDL_KMOD_NONE), IRIS_DS_BT_SQUARE);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_NORTH         , SDL_KMOD_NONE), IRIS_DS_BT_TRIANGLE);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_EAST          , SDL_KMOD_NONE), IRIS_DS_BT_CIRCLE);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_START         , SDL_KMOD_NONE), IRIS_DS_BT_START);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_BACK          , SDL_KMOD_NONE), IRIS_DS_BT_SELECT);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_UP       , SDL_KMOD_NONE), IRIS_DS_BT_UP);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_DOWN     , SDL_KMOD_NONE), IRIS_DS_BT_DOWN);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_LEFT     , SDL_KMOD_NONE), IRIS_DS_BT_LEFT);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_RIGHT    , SDL_KMOD_NONE), IRIS_DS_BT_RIGHT);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_LEFT_SHOULDER , SDL_KMOD_NONE), IRIS_DS_BT_L1);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_KMOD_NONE), IRIS_DS_BT_R1);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_LEFT_STICK    , SDL_KMOD_NONE), IRIS_DS_BT_L3);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_RIGHT_STICK   , SDL_KMOD_NONE), IRIS_DS_BT_R3);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFT_TRIGGER    , SDL_KMOD_NONE), IRIS_DS_BT_L2);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER   , SDL_KMOD_NONE), IRIS_DS_BT_R2);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFTY           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_LEFTY           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_NEG);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFTX           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_LEFTX           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_NEG);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHTY          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_RIGHTY          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_NEG);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHTX          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_RIGHTX          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_NEG);
    }
}

bool init(Instance* iris) {
    if (!iris->paths.gcdb_path.size()) {
        iris_info(&iris->log.input, "Adding default database");

        load_db_default(iris);
    } else {
        iris_info(&iris->log.input, "Adding database from file \'{}\'", iris->paths.gcdb_path.c_str());

        load_db_from_file(iris, iris->paths.gcdb_path.c_str());
    }

    iris->input.input_devices[0] = new KeyboardDevice();

    if (iris->input.input_maps.size() == 0) {
        Mapping map;

        map.name = "Keyboard (default)";
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_X     , SDL_KMOD_NONE), IRIS_DS_BT_CROSS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_A     , SDL_KMOD_NONE), IRIS_DS_BT_SQUARE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_W     , SDL_KMOD_NONE), IRIS_DS_BT_TRIANGLE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_D     , SDL_KMOD_NONE), IRIS_DS_BT_CIRCLE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_RETURN, SDL_KMOD_NONE), IRIS_DS_BT_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_S     , SDL_KMOD_NONE), IRIS_DS_BT_SELECT);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_UP    , SDL_KMOD_NONE), IRIS_DS_BT_UP);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_DOWN  , SDL_KMOD_NONE), IRIS_DS_BT_DOWN);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_LEFT  , SDL_KMOD_NONE), IRIS_DS_BT_LEFT);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_RIGHT , SDL_KMOD_NONE), IRIS_DS_BT_RIGHT);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_Q     , SDL_KMOD_NONE), IRIS_DS_BT_L1);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_E     , SDL_KMOD_NONE), IRIS_DS_BT_R1);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_1     , SDL_KMOD_NONE), IRIS_DS_BT_L2);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_3     , SDL_KMOD_NONE), IRIS_DS_BT_R2);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_Z     , SDL_KMOD_NONE), IRIS_DS_BT_L3);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_C     , SDL_KMOD_NONE), IRIS_DS_BT_R3);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_I     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_J     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_K     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_L     , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_T     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_F     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_G     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_NEG);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_H     , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_POS);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_0     , SDL_KMOD_NONE), IRIS_S14X_SW_SERVICE);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_9     , SDL_KMOD_NONE), IRIS_S14X_SW_TEST);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_8     , SDL_KMOD_NONE), IRIS_S14X_SW_ENTER);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_7     , SDL_KMOD_NONE), IRIS_S14X_SW_UP);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_6     , SDL_KMOD_NONE), IRIS_S14X_SW_DOWN);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_1     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P1_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_2     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P2_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_3     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P3_START);
        map.map.insert(IEVENT(EventType::KEYBOARD, SDLK_4     , SDL_KMOD_LSHIFT), IRIS_S14X_SW_P4_START);

        iris->input.input_maps.push_back(map);

        map.map.clear();
        map = {};

        map.name = "Gamepad (default)";
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_SOUTH         , SDL_KMOD_NONE), IRIS_DS_BT_CROSS);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_WEST          , SDL_KMOD_NONE), IRIS_DS_BT_SQUARE);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_NORTH         , SDL_KMOD_NONE), IRIS_DS_BT_TRIANGLE);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_EAST          , SDL_KMOD_NONE), IRIS_DS_BT_CIRCLE);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_START         , SDL_KMOD_NONE), IRIS_DS_BT_START);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_BACK          , SDL_KMOD_NONE), IRIS_DS_BT_SELECT);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_UP       , SDL_KMOD_NONE), IRIS_DS_BT_UP);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_DOWN     , SDL_KMOD_NONE), IRIS_DS_BT_DOWN);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_LEFT     , SDL_KMOD_NONE), IRIS_DS_BT_LEFT);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_DPAD_RIGHT    , SDL_KMOD_NONE), IRIS_DS_BT_RIGHT);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_LEFT_SHOULDER , SDL_KMOD_NONE), IRIS_DS_BT_L1);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_KMOD_NONE), IRIS_DS_BT_R1);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_LEFT_STICK    , SDL_KMOD_NONE), IRIS_DS_BT_L3);
        map.map.insert(IEVENT(EventType::GAMEPAD_BUTTON  , SDL_GAMEPAD_BUTTON_RIGHT_STICK   , SDL_KMOD_NONE), IRIS_DS_BT_R3);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFT_TRIGGER    , SDL_KMOD_NONE), IRIS_DS_BT_L2);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER   , SDL_KMOD_NONE), IRIS_DS_BT_R2);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFTY           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_LEFTY           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTV_NEG);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_LEFTX           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_LEFTX           , SDL_KMOD_NONE), IRIS_DS_AX_LEFTH_NEG);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHTY          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_RIGHTY          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTV_NEG);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_POS, SDL_GAMEPAD_AXIS_RIGHTX          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_POS);
        map.map.insert(IEVENT(EventType::GAMEPAD_AXIS_NEG, SDL_GAMEPAD_AXIS_RIGHTX          , SDL_KMOD_NONE), IRIS_DS_AX_RIGHTH_NEG);

        iris->input.input_maps.push_back(map);
    }

#undef IEVENT

    // Ensure default mappings are in the correct order
    if (iris->input.input_maps[0].name == "Gamepad (default)") {
        auto map = iris->input.input_maps[0];

        iris->input.input_maps[0] = iris->input.input_maps[1];
        iris->input.input_maps[1] = map;
    }

    // Use keyboard mapping for slot 0 and none for slot 1 by default
    if (iris->input.input_map[0] <= 1) {
        iris->input.input_map[0] = 0;
    }

    if (iris->input.input_map[1] <= 1) {
        iris->input.input_map[1] = -1;
    }

    return true;
}

InputAction* get_input_action(Instance* iris, int slot, uint64_t input) {
    if (iris->input.input_map[slot] == -1)
        return nullptr;

    return iris->input.input_maps[iris->input.input_map[slot]].map.get_value(input);
}

static inline void change_button(Instance* iris, int slot, float value, uint32_t button) {
    if (!iris->input.ds[slot]) return;

    if (value > 0.5f) {
        dev::ds::button_press(iris->input.ds[slot], button);
    } else {
        dev::ds::button_release(iris->input.ds[slot], button);
    }
}

static inline void change_s14x_switch(Instance* iris, float value, uint32_t mask) {
    if (!iris->ps2->s14x_ioboard)
        return;

    if (value > 0.5) {
        s14x::ioboard::press_switch(iris->ps2->s14x_ioboard, mask);
    } else {
        s14x::ioboard::release_switch(iris->ps2->s14x_ioboard, mask);
    }
}

void execute_action(Instance* iris, InputAction action, int slot, float value) {
    if (!iris->input.ds[slot])
        return;

    switch (action) {
        case IRIS_DS_BT_SELECT: change_button(iris, slot, value, dev::ds::SELECT); break;
        case IRIS_DS_BT_L3: change_button(iris, slot, value, dev::ds::L3); break;
        case IRIS_DS_BT_R3: change_button(iris, slot, value, dev::ds::R3); break;
        case IRIS_DS_BT_START: change_button(iris, slot, value, dev::ds::START); break;
        case IRIS_DS_BT_UP: change_button(iris, slot, value, dev::ds::UP); break;
        case IRIS_DS_BT_RIGHT: change_button(iris, slot, value, dev::ds::RIGHT); break;
        case IRIS_DS_BT_DOWN: change_button(iris, slot, value, dev::ds::DOWN); break;
        case IRIS_DS_BT_LEFT: change_button(iris, slot, value, dev::ds::LEFT); break;
        case IRIS_DS_BT_L2: change_button(iris, slot, value, dev::ds::L2); break;
        case IRIS_DS_BT_R2: change_button(iris, slot, value, dev::ds::R2); break;
        case IRIS_DS_BT_L1: change_button(iris, slot, value, dev::ds::L1); break;
        case IRIS_DS_BT_R1: change_button(iris, slot, value, dev::ds::R1); break;
        case IRIS_DS_BT_TRIANGLE: change_button(iris, slot, value, dev::ds::TRIANGLE); break;
        case IRIS_DS_BT_CIRCLE: change_button(iris, slot, value, dev::ds::CIRCLE); break;
        case IRIS_DS_BT_CROSS: change_button(iris, slot, value, dev::ds::CROSS); break;
        case IRIS_DS_BT_SQUARE: change_button(iris, slot, value, dev::ds::SQUARE); break;
        case IRIS_DS_BT_ANALOG: change_button(iris, slot, value, dev::ds::ANALOG); break;
        case IRIS_DS_AX_RIGHTV_POS: dev::ds::analog_change(iris->input.ds[slot], dev::ds::RIGHT_V, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_RIGHTV_NEG: dev::ds::analog_change(iris->input.ds[slot], dev::ds::RIGHT_V, 0x7f - (value * 0x7f)); break;
        case IRIS_DS_AX_RIGHTH_POS: dev::ds::analog_change(iris->input.ds[slot], dev::ds::RIGHT_H, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_RIGHTH_NEG: dev::ds::analog_change(iris->input.ds[slot], dev::ds::RIGHT_H, 0x7f - (value * 0x7f)); break;
        case IRIS_DS_AX_LEFTV_POS: dev::ds::analog_change(iris->input.ds[slot], dev::ds::LEFT_V, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_LEFTV_NEG: dev::ds::analog_change(iris->input.ds[slot], dev::ds::LEFT_V, 0x7f - (value * 0x7f)); break;
        case IRIS_DS_AX_LEFTH_POS: dev::ds::analog_change(iris->input.ds[slot], dev::ds::LEFT_H, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_LEFTH_NEG: dev::ds::analog_change(iris->input.ds[slot], dev::ds::LEFT_H, 0x7f - (value * 0x7f)); break;
        case IRIS_S14X_SW_SERVICE: change_s14x_switch(iris, value, s14x::ioboard::SERVICE); break;
        case IRIS_S14X_SW_TEST: change_s14x_switch(iris, value, s14x::ioboard::TEST); break;
        case IRIS_S14X_SW_ENTER: change_s14x_switch(iris, value, s14x::ioboard::ENTER); break;
        case IRIS_S14X_SW_UP: change_s14x_switch(iris, value, s14x::ioboard::UP); break;
        case IRIS_S14X_SW_DOWN: change_s14x_switch(iris, value, s14x::ioboard::DOWN); break;
        case IRIS_S14X_SW_P1_START: change_s14x_switch(iris, value, s14x::ioboard::P1_START); break;
        case IRIS_S14X_SW_P2_START: change_s14x_switch(iris, value, s14x::ioboard::P2_START); break;
        case IRIS_S14X_SW_P3_START: change_s14x_switch(iris, value, s14x::ioboard::P3_START); break;
        case IRIS_S14X_SW_P4_START: change_s14x_switch(iris, value, s14x::ioboard::P4_START); break;
        case IRIS_INPUT_ACTION_MAX: break;
    }
}

InputEvent sdl_event_to_input_event(SDL_Event* event) {
    InputEvent ievent = {};

    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            ievent.type = EventType::KEYBOARD;
            ievent.id = event->key.key;

            // Devious hack, we have enough spare bits in the 
            // SDL_Keycode so we can actually do this
            const uint16_t mask =
                SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT |
                SDL_KMOD_LCTRL  | SDL_KMOD_RCTRL  |
                SDL_KMOD_LALT   | SDL_KMOD_RALT;

            ievent.id |= (event->key.mod & mask) << 12;
        } break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            ievent.type = EventType::GAMEPAD_BUTTON;
            ievent.id = event->gbutton.button;
        } break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            if (event->gaxis.value > 0) {
                ievent.type = EventType::GAMEPAD_AXIS_POS;
            } else {
                ievent.type = EventType::GAMEPAD_AXIS_NEG;
            }

            ievent.id = event->gaxis.axis;
        } break;
    }

    return ievent;
}

std::string get_default_screenshot_filename(Instance* iris) {
    SDL_Time t;
    SDL_DateTime dt;

    SDL_GetCurrentTime(&t);
    SDL_TimeToDateTime(t, &dt, true);

    char buf[512];

    sprintf(buf, "Screenshot-%04d-%02d-%02d_%02d-%02d-%02d-%d",
            dt.year, dt.month, dt.day,
            dt.hour, dt.minute, dt.second,
            iris->screenshot_counter + 1
    );

    std::string str(buf);

    switch (iris->screenshot_format) {
        case render::PNG: str += ".png"; break;
        case render::BMP: str += ".bmp"; break;
        case render::JPG: str += ".jpg"; break;
        case render::TGA: str += ".tga"; break;
    }

    return str;
}

int get_screenshot_jpg_quality(Instance* iris) {
    switch (iris->screenshot_jpg_quality_mode) {
        case render::MINIMUM: return 1;
        case render::LOW:     return 25;
        case render::MEDIUM:  return 50;
        case render::HIGH:    return 90;
        case render::MAXIMUM: return 100;
        case render::CUSTOM: return iris->screenshot_jpg_quality;
    }

    return 90;
}

bool save_screenshot(Instance* iris, std::string path) {
    std::filesystem::path fn(path);

    std::string directory = iris->paths.snap_path;
    
    if (iris->paths.snap_path.empty()) {
        directory = "snap";
    }

    std::filesystem::path p(directory);
    std::string absolute_path;
    std::string filename;

    if (path.size()) {
        filename = path;
    } else {
        filename = get_default_screenshot_filename(iris);
    }

    if (p.is_absolute()) {
        absolute_path = p.string();
    } else {
        absolute_path = iris->paths.pref_path + p.string();
    }

    absolute_path += "/" + filename;

    if (fn.is_absolute()) {
        absolute_path = fn.string();
    }

    void* ptr = nullptr;
    int width = 0, height = 0, offset = 0;

    if (iris->screenshot_mode == render::INTERNAL) {
        gs::renderer::Image* image = iris->screenshot_shader_processing ? &iris->vk.output_image : &iris->vk.image;

        ptr = vulkan::read_image(iris,
            image->image,
            image->format,
            image->width,
            image->height
        );

        width = image->width;
        height = image->height;
    } else {
        ptr = vulkan::read_image(iris,
            iris->vk.main_window_data.Frames[0].Backbuffer,
            iris->vk.main_window_data.SurfaceFormat.format,
            iris->vk.main_window_data.Width,
            iris->vk.main_window_data.Height
        );

        width = iris->vk.main_window_data.Width;
        height = iris->vk.main_window_data.Height;
        
        if (!iris->fullscreen) {
            offset = iris->ui.menubar_height;
            height -= iris->ui.menubar_height;
        }
    }

    if (!ptr) {
        push_info(iris, "Couldn't save screenshot");

        return false;
    }

    uint32_t* buf = (uint32_t*)malloc((width * 4) * height);

    memcpy(buf, ((uint32_t*)ptr) + offset * width, (width * 4) * height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            buf[x + (y * width)] |= 0xff000000;
        }
    }

    int r = 0;

    switch (iris->screenshot_format) {
        case render::PNG:
            r = stbi_write_png(absolute_path.c_str(), width, height, 4, buf, width * 4);
            break;
        case render::BMP:
            r = stbi_write_bmp(absolute_path.c_str(), width, height, 4, buf);
            break;
        case render::JPG:
            r = stbi_write_jpg(absolute_path.c_str(), width, height, 4, buf, get_screenshot_jpg_quality(iris));
            break;
        case render::TGA:
            r = stbi_write_tga(absolute_path.c_str(), width, height, 4, buf);
            break;
    }

    iris_info(&iris->log.input, "Saving screenshot to '{}' ({}x{}, {} bpp): {}",
        absolute_path.c_str(),
        width, height, 32,
        r ? "Success" : "Failure"
    );

    free(ptr);
    free(buf);

    if (!r) {
        push_info(iris, "Couldn't save screenshot");

        return false;
    }

    iris->screenshot_counter++;

    push_info(iris, "Screenshot saved as '" + filename + "'");

    return true;
}

void handle_keydown_event(Instance* iris, SDL_Event* event) {
    SDL_Keycode key = event->key.key;

    switch (key) {
        case SDLK_SPACE: {
            iris->debug.pause = !iris->debug.pause;

            // vulkan::wait_idle(iris);
        } break;
        case SDLK_F9: {
            vulkan::wait_idle(iris);

            bool saved = save_screenshot(iris);
        } break;
        case SDLK_F11: {
            iris->fullscreen = !iris->fullscreen;

            SDL_SetWindowFullscreen(iris->window, iris->fullscreen ? true : false);
        } break;
        case SDLK_F1: {
            iris_info(&iris->log.input, "Sending poweroff signal");
            cdvd::power_off(iris->ps2->cdvd);
        } break;

        case SDLK_F2: {
            iris->ps2->vu0->disable = !iris->ps2->vu0->disable;
            iris->ps2->vu1->disable = !iris->ps2->vu1->disable;
        } break;
    }

    iris->input.last_input_event_read = false;
    iris->input.last_input_event_value = 1.0f;
    iris->input.last_input_event = sdl_event_to_input_event(event);

    if (SDL_GetWindowFlags(iris->window) & SDL_WINDOW_INPUT_FOCUS) {
        if (iris->input.input_devices[0]) iris->input.input_devices[0]->handle_event(iris, event);
        if (iris->input.input_devices[1]) iris->input.input_devices[1]->handle_event(iris, event);
    }
}

void handle_keyup_event(Instance* iris, SDL_Event* event) {
    // Add special keyup handling here if needed

    if (iris->input.input_devices[0]) iris->input.input_devices[0]->handle_event(iris, event);
    if (iris->input.input_devices[1]) iris->input.input_devices[1]->handle_event(iris, event);
}

}