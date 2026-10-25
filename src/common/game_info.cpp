#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

#include "app/app.h"
#include "common/input_kbm.h"
#include "common/language.h"
#include "common/overlay.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace {

namespace fs = std::filesystem;

// settings.toml keys of the game's own (the input ones are in input_kbm.h)
bool g_ntsc_videos = true;

// Step mode (SIMSBO_STEP=<path>): each new token written to that file is a burst of timed pulses
// generated here, on the game's own poll clock: NAME or NAMExN (START, A, B, X, Y, BACK, UP, DOWN,
// LEFT, RIGHT, LSUP, LSDOWN, LSLEFT, LSRIGHT, BLACK, WHITE, LT, RT, LTHUMB, RTHUMB; anything else =
// none), so a driver can do press -> screenshot -> decide -> press with an exact press count.
// Takes precedence over the autostart cycle below. Only ever ORs bits/bytes in, never clears a real
// press. Unset: no-op, pad untouched. From the USA project (abilliontokens/bustinout-recomp).
bool StepPad(int port, input::Pad& pad) {
    static const char* path = std::getenv("SIMSBO_STEP");
    if (!path || !path[0] || port != 0) return false;
    using namespace std::chrono;
    // the pad hook runs outside input::g_lock and the title may poll from more than one guest
    // thread; the token and its two timestamps change together, so every poll holds this lock
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);
    static std::string held;
    static int64_t last_read = 0, held_since = 0;
    const int64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (now - last_read >= 50) {
        last_read = now;
        std::string token;
        if (FILE* f = std::fopen(path, "r")) {
            char buf[32] = {};
            if (std::fgets(buf, sizeof buf, f)) token = buf;
            std::fclose(f);
        }
        while (!token.empty() && (token.back() == '\n' || token.back() == '\r' || token.back() == ' ')) token.pop_back();
        if (token != held) {
            held = token;
            held_since = now;
            LOGR("simsbo step: holding '%s' (port 0)", held.c_str());
        }
    }
    if (held.empty() || held == "NONE") return true;
    // Pulse train for the current token: NAMExN = N pulses (default 1), each 130 ms on / 130 ms off
    // for buttons, 160/160 for stick deflections (they are polled against a threshold). After the
    // burst the input rests, however long the driver leaves the token in the file.
    std::string name = held;
    int count = 1;
    if (const size_t x = held.find_last_of("xX"); x != std::string::npos && x + 1 < held.size() &&
        held.find_first_not_of("0123456789", x + 1) == std::string::npos) {
        count = std::atoi(held.c_str() + x + 1);
        name = held.substr(0, x);
        if (count < 1) count = 1;
        if (count > 40) count = 40;
    }
    const bool stick = name.starts_with("LS");
    const int64_t half = stick ? 160 : 130;
    const int64_t phase = (now - held_since) / half;
    if (phase >= 2 * count || (phase & 1)) return true;
    pad.connected = true;
    if (name == "START") pad.buttons = uint16_t(pad.buttons | 0x0010);
    else if (name == "BACK") pad.buttons = uint16_t(pad.buttons | 0x0020);
    else if (name == "UP") pad.buttons = uint16_t(pad.buttons | 0x0001);
    else if (name == "DOWN") pad.buttons = uint16_t(pad.buttons | 0x0002);
    else if (name == "LEFT") pad.buttons = uint16_t(pad.buttons | 0x0004);
    else if (name == "RIGHT") pad.buttons = uint16_t(pad.buttons | 0x0008);
    else if (name == "A") pad.analog[0] = 0xFF;
    else if (name == "B") pad.analog[1] = 0xFF;
    else if (name == "X") pad.analog[2] = 0xFF;
    else if (name == "Y") pad.analog[3] = 0xFF;
    else if (name == "LSUP") pad.ly = 30000;
    else if (name == "LSDOWN") pad.ly = -30000;
    else if (name == "LSLEFT") pad.lx = -30000;
    else if (name == "LSRIGHT") pad.lx = 30000;
    else if (name == "BLACK") pad.analog[4] = 0xFF;
    else if (name == "WHITE") pad.analog[5] = 0xFF;
    else if (name == "LT") pad.analog[6] = 0xFF;
    else if (name == "RT") pad.analog[7] = 0xFF;
    else if (name == "LTHUMB") pad.buttons = uint16_t(pad.buttons | 0x0040);
    else if (name == "RTHUMB") pad.buttons = uint16_t(pad.buttons | 0x0080);
    return true;
}

// Opt-in self-test (SIMSBO_AUTOSTART): a scripted pad on port 0, repeating a 10 s cycle from ~2 s
// after the first poll: Start held 0-0.5 s, D-pad Up held 3-3.5 s, then A held 5-5.5 s, all
// released between. The intro/attract movies run for minutes and ignore early presses, so the
// cycle repeats: a Start lands whenever the title is responsive and an A whenever a menu is. It
// never confirms a Sim in Create-a-Sim, so it is a smoke test only; use SIMSBO_STEP for a walk.
void AutostartPad(int port, input::Pad& pad) {
    if (StepPad(port, pad)) return;
    static const bool enabled = [] {
        const char* v = std::getenv("SIMSBO_AUTOSTART");
        return v && v[0] && std::strcmp(v, "0") != 0;
    }();
    if (!enabled || port != 0) return;
    using namespace std::chrono;
    static std::atomic<int64_t> first_ms{0};
    static std::atomic<bool> start_down{false}, a_down{false}, up_down{false};
    const int64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    int64_t expected = 0;
    if (first_ms.compare_exchange_strong(expected, now)) {
        LOGR("simsbo autostart: enabled, first poll on port 0; scripted Start/A cycle from ~2 s");
        return;
    }
    const int64_t elapsed = now - first_ms.load(std::memory_order_relaxed);
    const int64_t phase = elapsed >= 2000 ? (elapsed - 2000) % 10000 : -1;
    const bool start = phase >= 0 && phase < 500;
    const bool up = phase >= 3000 && phase < 3500;
    const bool a = phase >= 5000 && phase < 5500;
    if (start) {
        pad.connected = true;
        pad.buttons = uint16_t(pad.buttons | 0x0010);  // Xbox Start
        if (!start_down.exchange(true))
            LOGR("simsbo autostart: synthetic Start pressed (port 0, wButtons=0x%04X)", pad.buttons);
    } else if (start_down.exchange(false)) {
        LOGR("simsbo autostart: synthetic Start released (port 0)");
    }
    if (up) {
        pad.connected = true;
        pad.buttons = uint16_t(pad.buttons | 0x0001);  // Xbox D-pad Up
        if (!up_down.exchange(true)) LOGR("simsbo autostart: synthetic DPadUp pressed (port 0, wButtons=0x%04X)", pad.buttons);
    } else if (up_down.exchange(false)) {
        LOGR("simsbo autostart: synthetic DPadUp released (port 0)");
    }
    if (a) {
        pad.connected = true;
        if (pad.analog[0] < 0xFF) pad.analog[0] = 0xFF;  // Xbox A, pressure byte
        if (!a_down.exchange(true)) LOGR("simsbo autostart: synthetic A pressed (port 0, analog[0]=0xFF)");
    } else if (a_down.exchange(false)) {
        LOGR("simsbo autostart: synthetic A released (port 0)");
    }
}

// A release log shows whether a controller was seen at all: each port's plug and unplug as SDL
// reported it, before the keyboard layer claims port 0 as connected.
void LogPad(int port, const input::Pad& pad) {
    static std::atomic<uint8_t> seen{0};
    const uint8_t bit = uint8_t(1u << (port & 7));
    const bool was = (seen.load(std::memory_order_relaxed) & bit) != 0;
    if (pad.connected == was) return;
    if (pad.connected) seen.fetch_or(bit, std::memory_order_relaxed);
    else seen.fetch_and(uint8_t(~bit), std::memory_order_relaxed);
    LOGR("simsbo: controller %s on port %d", pad.connected ? "connected" : "disconnected", port + 1);
}

// every polled pad: the scripted self-test first, then the keyboard and mouse (input_kbm.cpp).
// Both only ever add to what a real controller reported.
void PadHook(int port, input::Pad& pad) {
    LogPad(port, pad);
    AutostartPad(port, pad);
    kbm::Apply(port, pad);
}

// the first rumble the game asks for, once, so a release log shows the motors were reached
void RumbleHook(int port, uint16_t& left, uint16_t& right) {
    static std::atomic<bool> once{false};
    if ((left || right) && !once.exchange(true))
        LOGR("simsbo: first rumble on port %d (%u / %u)", port + 1, unsigned(left), unsigned(right));
}

// The saves live in one place only (hdd\E\UDATA\45410043 beside the exe). Each start copies them to
// save-backups\<date and time> first, unless nothing changed since the newest copy, and keeps the ten
// newest copies: a bad build or a crash while saving can never take the only save with it.
void BackUpSaves() {
    const std::string suffix = app::Instance() > 1 ? std::to_string(app::Instance()) : "";
    const fs::path src = app::ExeDir() / ("hdd" + suffix) / "E" / "UDATA" / "45410043";
    const fs::path root = app::ExeDir() / ("save-backups" + suffix);
    std::error_code ec;
    if (!fs::is_directory(src, ec)) return;
    // what the saves look like now: every file's path, size and time
    std::string contents;
    int files = 0;
    for (auto it = fs::recursive_directory_iterator(src, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        files++;
        contents += fs::relative(it->path(), src, ec).generic_string() + " " + std::to_string(it->file_size(ec)) + " " +
                    std::to_string(it->last_write_time(ec).time_since_epoch().count()) + "\n";
    }
    if (files == 0) return;
    std::vector<fs::path> backups;
    for (auto& e : fs::directory_iterator(root, ec))
        if (e.is_directory(ec)) backups.push_back(e.path());
    std::sort(backups.begin(), backups.end());  // named by date and time: oldest first
    if (!backups.empty()) {
        std::ifstream f(backups.back() / "contents.txt", std::ios::binary);
        if (std::string((std::istreambuf_iterator<char>(f)), {}) == contents) return;
    }
    char name[32];
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    std::strftime(name, sizeof name, "%Y-%m-%d_%H-%M-%S", &local);
    const fs::path dst = root / name;
    fs::create_directories(dst, ec);
    fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        LOGW("simsbo: save backup to %s failed: %s", dst.string().c_str(), ec.message().c_str());
        return;
    }
    std::ofstream(dst / "contents.txt", std::ios::binary) << contents;
    backups.push_back(dst);
    for (size_t i = 0; i + 10 < backups.size(); i++) fs::remove_all(backups[i], ec);
    LOGR("simsbo: saves backed up to %s (%d files)", dst.string().c_str(), files);
}

// The Europe disc carries every video twice, a 25 fps PAL cut and a 29.97 fps NTSC cut of the same
// length (sims_intro_pal.xmv / sims_intro.xmv, ea_games_pal.xmv / ea_games.xmv, the reward films).
// The game picks the PAL set by the constant 1 at [0x2E3E18] in .rdata, a choice made for a 50 Hz
// TV. A PC display runs at 60 Hz or more, where 25 fps judders and the NTSC cut plays evenly, so the
// constant becomes 0 before the game starts. Its other reader is the frame object's tick size
// (0.02 s or 1/60 s), which the frame-step hook (hooks.cpp) replaces every frame anyway.
// ntsc_videos = false in settings.toml keeps the PAL set.
// settings.toml `language`, a key of language::kChoices; options hold numbers and flags only
void LoadSettings(const void* table) {
    const auto& t = *static_cast<const toml::table*>(table);
    language::choice = language::Find(t["language"].value_or(std::string("auto")));
}
void SaveSettings(void* table) {
    static_cast<toml::table*>(table)->insert_or_assign("language", language::kChoices[language::choice].key);
}

void Loaded() {
    if (!g_ntsc_videos) return;
    *static_cast<uint32_t*>(kernel::HostPtr(0x2E3E18)) = 0;
    LOGR("simsbo: NTSC (29.97 fps) videos; settings.toml ntsc_videos = false for the PAL ones");
}

}  // namespace

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"The Sims Bustin' Out",         // window and splash
    .short_title = L"The Sims Bustin' Out",   // message boxes
    .name = L"SimsBustinOut",                 // window classes, crash dumps, handover event
    .log = "simsbustinout",                   // log file prefix
    .settings = "settings.toml",              // settings file beside the exe
    .splash = L"splash",                      // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "SimsBustinOut"}},  // each recompiled xbe and its exe, for title launches
    // game settings kept in the settings file
    .options = {{"mouse_camera_speed", nullptr, &kbm::mouse_camera_speed},
                {"mute_in_background", &kbm::mute_in_background, nullptr},
                {"ntsc_videos", &g_ntsc_videos, nullptr}},
    .load = &LoadSettings,                    // reads more from the settings file's toml::table: language
    .save = &SaveSettings,                    // writes more into it
    .init = &BackUpSaves,                     // after the settings load, before the window and boot
    .loaded = &Loaded,                        // image in memory, before its entry point runs: NTSC videos
    .window = nullptr,                        // the game window is up
    .message = &kbm::Message,                 // window messages first; true takes one
    .ui = &overlay::Draw,                     // ImGui drawing each frame: F1 settings and cheats, F2 performance
    .pad = &PadHook,                          // keyboard and mouse as port 0, SIMSBO_STEP / SIMSBO_AUTOSTART
    .rumble = &RumbleHook,                    // changes a rumble before it is set; logs the first one
    // This title draws a 640x448 picture inside its 640x480 back buffer (the mode table asks for a
    // 640x480 back buffer and stores the separate 640x448 picture size; Europe 0x1295BE / 0x129658).
    // Centred, the picture starts at row (480 - 448) / 2 = 16. USA docs/picture-rect.md.
    .picture_top = 16,
    .picture_height = 448,
};
