// Keyboard and mouse as the console's pad on port 0.
//
// The game reads nothing but an XINPUT_GAMEPAD: there is no second input path to add a PC control
// scheme to, so the keyboard and the mouse are translated into the same report a controller
// produces. Port 0 is reported connected whether or not a pad is plugged in - a PC always has a
// keyboard - and a real pad on that port is merged with, never overridden: buttons are ORed in
// and an axis is only moved if the keys push it further than the stick already does. Both can be
// used at once, which is what the camera-on-the-mouse, walk-on-the-keys layout below is for.
//
// Layout (the console input each binding drives):
//
//   W A S D                  left stick          walk; menus that read the stick
//   arrow keys               D-pad               menu and dialog navigation
//   mouse, right button held right stick         camera, as a stick deflected from where the
//                                                button went down: rotate and zoom
//   Q / E                    right stick X       rotate the camera left / right
//   R / F                    right stick Y       zoom the camera in / out
//   mouse wheel              right stick Y pulse zoom the camera in / out
//   space                    A                   accept, interact
//   escape, backspace        B                   cancel, back
//   X / Y                    X / Y
//   Z / C                    left / right trigger
//   1 / 2                    black / white
//   enter                    Start               pause menu; "press Start"
//   tab                      Back
//   shift / ctrl, middle btn left / right stick click
//
// The camera is the right stick and nothing else: the game's camera update reads the analog axes,
// rotating on X and, on Y, moving distance, field of view and pitch together (stick up is zoom
// in). The triggers, black/white and the stick clicks move no camera at all, which is why the
// wheel drives the Y axis. Deflection under ~24% of full is inside the game's own deadzone, and
// rotation direction follows its invert-camera-rotation option.
//
// The SDK host owns a Win32 window and SDL3 is initialised there for gamepads only, with no SDL
// window at all, so keyboard and mouse state comes from the host's window messages through
// kbm::Message (wired as app::GameInfo::message in game_info.cpp); held keys are released on focus
// loss, because an alt-tabbed game gets no WM_KEYUP for them. Apply() merges that snapshot into
// the pad on whichever guest thread polls it.
//
// While the F1 settings panel is up the overlay owns the keyboard and the mouse and nothing here
// reaches the game. This is the Windows path of the USA project's file
// (abilliontokens/bustinout-recomp, src/common/input_kbm.cpp) without its SDL/Linux branch.
#include "common/input_kbm.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iterator>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "common/overlay.h"
#include "core/log.h"

namespace kbm {

namespace {

// the wheel is a notch, not a position: each notch holds the zoom axis for a while
std::atomic<int64_t> g_zoom_until{0};  // steady-clock milliseconds
std::atomic<int> g_zoom_dir{0};        // +1 in, -1 out

constexpr float kDragRadius = 140.0f;  // pixels from the press point for a fully deflected stick
constexpr int64_t kZoomMsPerNotch = 90;
constexpr int64_t kZoomMsMax = 450;  // a spun wheel does not queue up seconds of zoom

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// one wheel notch: hold the zoom direction for a while; a reversed wheel drops what is left of
// the other direction, and a spun wheel does not queue up seconds of zoom
void PulseWheel(int notches) {
    if (notches == 0) return;
    const int dir = notches > 0 ? 1 : -1;
    const int64_t now = NowMs();
    int64_t until = g_zoom_dir.exchange(dir, std::memory_order_relaxed) == dir
                        ? std::max(g_zoom_until.load(std::memory_order_relaxed), now)
                        : now;
    until += kZoomMsPerNotch * std::abs(notches);
    g_zoom_until.store(std::min(until, now + kZoomMsMax), std::memory_order_relaxed);
}

// the logical keys Apply reads
enum class Key : size_t {
    WalkLeft,   // A
    WalkRight,  // D
    WalkDown,   // S
    WalkUp,     // W
    CamLeft,    // Q
    CamRight,   // E
    CamOut,     // F
    CamIn,      // R
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Start,       // enter
    Back,        // tab
    LeftClick,   // shift
    RightClick,  // ctrl
    A,           // space
    B,           // escape or backspace
    X,
    Y,
    Black,  // 1
    White,  // 2
    LeftTrigger,   // Z
    RightTrigger,  // C
    Count,
};

// one frame's keyboard and mouse; Apply is the only reader
struct State {
    bool down[size_t(Key::Count)] = {};
    bool rmb = false;  // right mouse button: the camera stick
    bool mmb = false;  // middle mouse button: right stick click
    float mouse_x = 0.0f, mouse_y = 0.0f;    // window pixels
    float anchor_x = 0.0f, anchor_y = 0.0f;  // where the right button went down

    bool Down(Key key) const { return down[size_t(key)]; }
    void Set(Key key, bool held) { down[size_t(key)] = held; }
};

// -1, 0 or +1 from a pair of keys
float Axis(bool neg, bool pos) { return float(pos ? 1 : 0) - float(neg ? 1 : 0); }

// a direction into the console's stick: clamped to the circle a real stick reaches, so a diagonal
// is not 1.41 times a cardinal, and only applied where it pushes further than the pad already does
void Deflect(float x, float y, int16_t& out_x, int16_t& out_y) {
    const float len = std::sqrt(x * x + y * y);
    if (len > 1.0f) {
        x /= len;
        y /= len;
    }
    const int vx = int(x * 32767.0f), vy = int(y * 32767.0f);
    if (std::abs(vx) > std::abs(int(out_x))) out_x = int16_t(vx);
    if (std::abs(vy) > std::abs(int(out_y))) out_y = int16_t(vy);
}

void Press(uint8_t& analog) { analog = 0xFF; }  // the Xbox pressure byte; the game tests != 0

// the window thread writes these through Message, the guest's polling threads read them
std::atomic<bool> g_down[256];      // by virtual-key code
std::atomic<bool> g_focus{false};   // the game window has the keyboard
std::atomic<bool> g_rmb{false};     // right mouse button: the camera stick
std::atomic<bool> g_mmb{false};     // middle mouse button: right stick click
std::atomic<int> g_mouse_x{0}, g_mouse_y{0};    // client pixels, last move
std::atomic<int> g_anchor_x{0}, g_anchor_y{0};  // where the right button went down

bool DownVk(int vk) { return g_down[vk].load(std::memory_order_relaxed); }

void ReleaseAll() {
    for (std::atomic<bool>& key : g_down) key.store(false, std::memory_order_relaxed);
    g_rmb.store(false, std::memory_order_relaxed);
    g_mmb.store(false, std::memory_order_relaxed);
    g_zoom_until.store(0, std::memory_order_relaxed);
}

bool Focused() { return g_focus.load(std::memory_order_relaxed); }

void Collect(State& state) {
    state.Set(Key::WalkLeft, DownVk('A'));
    state.Set(Key::WalkRight, DownVk('D'));
    state.Set(Key::WalkDown, DownVk('S'));
    state.Set(Key::WalkUp, DownVk('W'));
    state.Set(Key::CamLeft, DownVk('Q'));
    state.Set(Key::CamRight, DownVk('E'));
    state.Set(Key::CamOut, DownVk('F'));
    state.Set(Key::CamIn, DownVk('R'));
    state.Set(Key::DpadUp, DownVk(VK_UP));
    state.Set(Key::DpadDown, DownVk(VK_DOWN));
    state.Set(Key::DpadLeft, DownVk(VK_LEFT));
    state.Set(Key::DpadRight, DownVk(VK_RIGHT));
    state.Set(Key::Start, DownVk(VK_RETURN));
    state.Set(Key::Back, DownVk(VK_TAB));
    state.Set(Key::LeftClick, DownVk(VK_SHIFT));
    state.Set(Key::RightClick, DownVk(VK_CONTROL));
    state.Set(Key::A, DownVk(VK_SPACE));
    state.Set(Key::B, DownVk(VK_ESCAPE) || DownVk(VK_BACK));
    state.Set(Key::X, DownVk('X'));
    state.Set(Key::Y, DownVk('Y'));
    state.Set(Key::Black, DownVk('1'));
    state.Set(Key::White, DownVk('2'));
    state.Set(Key::LeftTrigger, DownVk('Z'));
    state.Set(Key::RightTrigger, DownVk('C'));
    state.rmb = g_rmb.load(std::memory_order_relaxed);
    state.mmb = g_mmb.load(std::memory_order_relaxed);
    state.mouse_x = float(g_mouse_x.load(std::memory_order_relaxed));
    state.mouse_y = float(g_mouse_y.load(std::memory_order_relaxed));
    state.anchor_x = float(g_anchor_x.load(std::memory_order_relaxed));
    state.anchor_y = float(g_anchor_y.load(std::memory_order_relaxed));
}

}  // namespace

bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    (void)hwnd;
    (void)result;
    switch (msg) {
        case WM_KEYDOWN:
            if (wp < 256) g_down[wp].store(true, std::memory_order_relaxed);
            break;
        case WM_KEYUP:
            if (wp < 256) g_down[wp].store(false, std::memory_order_relaxed);
            break;
        // alt is the window's (alt-tab, alt-F4): WM_SYSKEY* is deliberately not tracked
        case WM_SETFOCUS:
            g_focus.store(true, std::memory_order_relaxed);
            break;
        case WM_KILLFOCUS:
            g_focus.store(false, std::memory_order_relaxed);
            ReleaseAll();  // keys held at alt-tab get no WM_KEYUP
            break;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) {
                g_focus.store(false, std::memory_order_relaxed);
                ReleaseAll();
            } else {
                g_focus.store(true, std::memory_order_relaxed);
            }
            break;
        case WM_MOUSEMOVE:
            g_mouse_x.store(int(int16_t(lp & 0xFFFF)), std::memory_order_relaxed);
            g_mouse_y.store(int(int16_t((lp >> 16) & 0xFFFF)), std::memory_order_relaxed);
            break;
        case WM_RBUTTONDOWN:
            g_anchor_x.store(int(int16_t(lp & 0xFFFF)), std::memory_order_relaxed);
            g_anchor_y.store(int(int16_t((lp >> 16) & 0xFFFF)), std::memory_order_relaxed);
            g_mouse_x.store(g_anchor_x.load(std::memory_order_relaxed), std::memory_order_relaxed);
            g_mouse_y.store(g_anchor_y.load(std::memory_order_relaxed), std::memory_order_relaxed);
            g_rmb.store(true, std::memory_order_relaxed);
            break;
        case WM_RBUTTONUP:
            g_rmb.store(false, std::memory_order_relaxed);
            break;
        case WM_MBUTTONDOWN:
            g_mmb.store(true, std::memory_order_relaxed);
            break;
        case WM_MBUTTONUP:
            g_mmb.store(false, std::memory_order_relaxed);
            break;
        case WM_MOUSEWHEEL:
            PulseWheel(GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA);
            break;
        default:
            break;
    }
    return false;  // never takes a message
}

void Apply(int port, input::Pad& pad) {
    if (port != 0) return;
    pad.connected = true;  // the keyboard is the port 0 pad when no controller is plugged in
    // no focus: nothing is held. The overlay's settings panel owns both devices while it is up
    if (!Focused() || overlay::SettingsOpen()) return;

    State state;
    Collect(state);

    static std::atomic<bool> logged{false};

    // left stick: walk, and the menus that read the stick
    const float lx = Axis(state.Down(Key::WalkLeft), state.Down(Key::WalkRight));
    const float ly = Axis(state.Down(Key::WalkDown), state.Down(Key::WalkUp));
    if (lx != 0.0f || ly != 0.0f) Deflect(lx, ly, pad.lx, pad.ly);

    // right stick: the camera. X rotates, Y zooms; keys, the right-button drag and the wheel's
    // zoom pulse add up before the circle clamp
    float rx = Axis(state.Down(Key::CamLeft), state.Down(Key::CamRight));
    float ry = Axis(state.Down(Key::CamOut), state.Down(Key::CamIn));
    if (state.rmb) {
        rx += (state.mouse_x - state.anchor_x) / kDragRadius;
        // screen y grows downward, the stick's upward
        ry -= (state.mouse_y - state.anchor_y) / kDragRadius;
    }
    if (NowMs() < g_zoom_until.load(std::memory_order_relaxed))
        ry += float(g_zoom_dir.load(std::memory_order_relaxed));
    if (rx != 0.0f || ry != 0.0f) Deflect(rx, ry, pad.rx, pad.ry);

    uint16_t buttons = 0;
    if (state.Down(Key::DpadUp)) buttons |= 0x0001;
    if (state.Down(Key::DpadDown)) buttons |= 0x0002;
    if (state.Down(Key::DpadLeft)) buttons |= 0x0004;
    if (state.Down(Key::DpadRight)) buttons |= 0x0008;
    if (state.Down(Key::Start)) buttons |= 0x0010;  // Start
    if (state.Down(Key::Back)) buttons |= 0x0020;   // Back
    if (state.Down(Key::LeftClick)) buttons |= 0x0040;  // left stick click
    if (state.Down(Key::RightClick) || state.mmb) buttons |= 0x0080;  // right stick click
    pad.buttons = uint16_t(pad.buttons | buttons);

    uint8_t analog[8] = {};
    if (state.Down(Key::A)) Press(analog[0]);            // A
    if (state.Down(Key::B)) Press(analog[1]);            // B
    if (state.Down(Key::X)) Press(analog[2]);            // X
    if (state.Down(Key::Y)) Press(analog[3]);            // Y
    if (state.Down(Key::Black)) Press(analog[4]);        // black
    if (state.Down(Key::White)) Press(analog[5]);        // white
    if (state.Down(Key::LeftTrigger)) Press(analog[6]);  // left trigger
    if (state.Down(Key::RightTrigger)) Press(analog[7]); // right trigger
    for (int i = 0; i < 8; i++)
        if (analog[i] > pad.analog[i]) pad.analog[i] = analog[i];

    const bool active = buttons != 0 || lx != 0.0f || ly != 0.0f || rx != 0.0f || ry != 0.0f ||
                        std::any_of(std::begin(analog), std::end(analog), [](uint8_t b) { return b != 0; });
    if (active && !logged.exchange(true))
        LOGI("input: keyboard and mouse on port 0 (WASD walk, arrows D-pad, right drag and wheel camera)");
}

}  // namespace kbm
