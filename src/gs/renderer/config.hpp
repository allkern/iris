#pragma once

namespace iris::gs::renderer {

struct HardwareConfig {
    int super_sampling = 0;
    bool super_sampled_quads = false;
    bool force_progressive = false;
    bool overscan = false;
    bool crtc_offsets = false;
    bool disable_mipmaps = false;
    bool unsynced_readbacks = false;
    bool backbuffer_promotion = false;
    bool allow_blend_demote = false;
    bool invert_fields = false;

    // Analog video
    bool enable_analog_video = false;
    int analog_cable = 0;
    int analog_system = 0;
    bool line_comb = false;
    bool skip_notch = false;
};

}
