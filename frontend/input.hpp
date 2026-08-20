#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <SDL3/SDL.h>
#include "bidirectional_map.hpp"

namespace iris {

struct Instance;

enum class InputController {
    DUALSHOCK2

    // Large To-do list here, we're missing the Namco GunCon
    // controllers, JogCon, NegCon, Buzz! Buzzer, the Train
    // controllers, Taiko Drum Master controller, the Dance Dance
    // Revolution mat, Guitar Hero controllers, etc.
};

struct InputDevice {
    int m_slot;

    void set_slot(int slot) {
        this->m_slot = slot;
    }

    int get_slot() {
        return this->m_slot;
    }

    virtual ~InputDevice() = default;

    virtual int get_type() = 0;
    virtual void handle_event(Instance* iris, SDL_Event* event) = 0;
};

class KeyboardDevice : public InputDevice {
public:
    int get_type() override {
        return 0;
    }

    void handle_event(Instance* iris, SDL_Event* event) override;
};

class GamepadDevice : public InputDevice {
    SDL_JoystickID id;

public:
    GamepadDevice(SDL_JoystickID id) : id(id) {}

    int get_type() override {
        return 1;
    }

    SDL_JoystickID get_id() {
        return id;
    }

    void handle_event(Instance* iris, SDL_Event* event) override;
};

union InputEvent {
    struct {
        uint32_t id;
        uint32_t type;
    };

    uint64_t u64;
};

enum EventType {
    KEYBOARD,
    GAMEPAD_BUTTON,
    GAMEPAD_AXIS_POS,
    GAMEPAD_AXIS_NEG
};

enum InputAction : uint32_t {
    IRIS_DS_BT_CROSS,
    IRIS_DS_BT_CIRCLE,
    IRIS_DS_BT_SQUARE,
    IRIS_DS_BT_TRIANGLE,
    IRIS_DS_BT_START,
    IRIS_DS_BT_SELECT,
    IRIS_DS_BT_ANALOG,
    IRIS_DS_BT_UP,
    IRIS_DS_BT_DOWN,
    IRIS_DS_BT_LEFT,
    IRIS_DS_BT_RIGHT,
    IRIS_DS_BT_L1,
    IRIS_DS_BT_R1,
    IRIS_DS_BT_L2,
    IRIS_DS_BT_R2,
    IRIS_DS_BT_L3,
    IRIS_DS_BT_R3,
    IRIS_DS_AX_RIGHTV_POS,
    IRIS_DS_AX_RIGHTV_NEG,
    IRIS_DS_AX_RIGHTH_POS,
    IRIS_DS_AX_RIGHTH_NEG,
    IRIS_DS_AX_LEFTV_POS,
    IRIS_DS_AX_LEFTV_NEG,
    IRIS_DS_AX_LEFTH_POS,
    IRIS_DS_AX_LEFTH_NEG,

    IRIS_S14X_SW_DOWN,
    IRIS_S14X_SW_UP,
    IRIS_S14X_SW_ENTER,
    IRIS_S14X_SW_TEST,
    IRIS_S14X_SW_SERVICE,
    IRIS_S14X_SW_P1_START,
    IRIS_S14X_SW_P2_START,
    IRIS_S14X_SW_P3_START,
    IRIS_S14X_SW_P4_START,

    IRIS_S2X6_SW_COIN1,
    IRIS_S2X6_SW_COIN2,
    IRIS_S2X6_SW_TEST,
    IRIS_INPUT_ACTION_MAX
};

struct Mapping {
    std::string name;
    bidirectional_map <uint64_t, InputAction> map;
};

namespace input {
    bool init(Instance* iris);
    void init_default_mapping(Instance* iris, int id);
    void load_db_default(Instance* iris);
    bool load_db_from_file(Instance* iris, const char* path);
    InputAction* get_input_action(Instance* iris, int slot, uint64_t input);
    InputEvent sdl_event_to_input_event(SDL_Event* event);
    std::string get_default_screenshot_filename(Instance* iris);
    void execute_action(Instance* iris, InputAction action, int slot, float value);
    bool save_screenshot(Instance* iris, std::string path = "");
    void handle_keydown_event(Instance* iris, SDL_Event* event);
    void handle_keyup_event(Instance* iris, SDL_Event* event);
}

}
