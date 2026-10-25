#pragma once

#include <cstdint>

#include "core/input.h"

// keyboard and mouse as the console's pad on port 0 (input_kbm.cpp)
namespace kbm {

// settings (game_info.cpp keeps them in settings.toml, the F1 panel edits them): the right-button
// drag's camera speed in percent, and whether the game goes quiet while another window is active
extern uint32_t mouse_camera_speed;
extern bool mute_in_background;

// app::GameInfo::message: the window thread, after ImGui. Reads key and mouse messages and only
// takes WM_SETCURSOR over the picture (the cursor is hidden there unless the F1 panel is up), so
// ImGui and the host's own F3 still see the rest. The wheel arrives here as WM_MOUSEWHEEL.
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);

// app::GameInfo::pad: any guest thread, each poll. ORs the keyboard and mouse into port 0 and
// reports that port connected, pad or no pad. Never clears what a real controller pressed.
void Apply(int port, input::Pad& pad);

}  // namespace kbm
