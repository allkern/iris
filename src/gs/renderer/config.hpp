#pragma once

struct hardware_config {
	int super_sampling = 0;
	bool force_progressive = false;
	bool overscan = false;
	bool crtc_offsets = false;
    bool disable_mipmaps = false;
	bool unsynced_readbacks = false;
	bool backbuffer_promotion = false;
	bool allow_blend_demote = false;
};