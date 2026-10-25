// the in-game overlay: F2 a performance panel, F1 a settings panel, both off until asked for.
// app::GameInfo::ui calls Draw on the render thread, inside the renderer's own ImGui frame and
// under app::UiLock, so nothing here may block, allocate per frame or trust that a context, a
// window size or a pad exists.
//
// Ported from the USA project (abilliontokens/bustinout-recomp, src/common/overlay.cpp) to the
// upstream SDK's settings: the ultrawide (Hor+) and HDR controls are left out, because the upstream
// D3D12 host has no such settings. The aspect ratio is the SDK's own aspect setting.
#include "common/overlay.h"

#include "common/cheats.h"
#include "common/input_kbm.h"
#include "common/language.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include <imgui.h>

#include "app/app.h"
#include "gpu/backend.h"

namespace overlay {

namespace {

// the guest creates its device at 640x480 once and never resizes it, so the host frame target is
// that times the render scale
constexpr float kGuestW = 640.0f, kGuestH = 480.0f;

constexpr int kSamples = 120;  // frame times behind the graph, two seconds at 60 fps

// the frame time ring: written every frame whether or not the panel is up, so the graph is
// already full the moment F2 opens it
struct Ring {
    float ms[kSamples] = {};
    int next = 0;   // where the next sample goes, and the oldest one once the ring is full
    int count = 0;  // samples in the ring, up to kSamples
};
Ring g_ring;

bool g_perf_open = false, g_settings_open = false;
bool g_settings_focus = false;  // the settings window takes focus the frame it opens
float g_ui_scale = 0.0f;        // style and font scale in use, 0 before the first frame
// g_settings_open is the render thread's; this is the copy the window and guest threads read
std::atomic<bool> g_settings_up{false};

// the settings panel edits a copy and only writes it back on Apply, so a half typed resolution
// never reaches the renderer
struct Draft {
    app::DisplayMode mode = app::DisplayMode::Borderless;
    uint32_t width = 0, height = 0;
    bool vsync = true;
    uint32_t fps_cap = 60;
    int render_percent = 100;
    uint32_t aniso = 0;
    int aspect = 0;  // app::kAspects
    int mouse_camera_speed = 100;
    bool mute_in_background = true;
    int language = 0;  // language::kChoices
};
Draft g_draft;
app::Mode g_desktop{0, 0};   // read when the panel opens: the mode lookup is a syscall
char g_status[80] = "";      // what the last Apply or Save did
float g_status_left = 0.0f;  // seconds it stays up

constexpr app::Mode kResolutions[] = {
    {1280, 720},  {1280, 800},  {1600, 900},  {1680, 1050}, {1920, 1080}, {1920, 1200},
    {2560, 1080}, {2560, 1440}, {2560, 1600}, {3440, 1440}, {3840, 1080}, {3840, 1600},
    {3840, 2160}, {5120, 1440},
};
// 0 unlimited, app::kFpsCapRefresh just under the monitor's refresh rate, else a frame rate
constexpr uint32_t kFpsCaps[] = {0, app::kFpsCapRefresh, 30, 60, 72, 120, 144, 165, 240};
constexpr uint32_t kAniso[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"As the game asks", "2x", "4x", "8x", "16x"};
const char* const kModeNames[] = {"Fullscreen", "Borderless", "Windowed"};
const char* const kAspectNames[] = {"From the display's shape", "4:3", "16:9"};

// the dark style this overlay wears, built once at scale 1 and rescaled from it
const ImGuiStyle& BaseStyle() {
    static const ImGuiStyle base = [] {
        ImGuiStyle s;
        ImGui::StyleColorsDark(&s);
        const ImVec4 accent{0.29f, 0.56f, 0.78f, 1.00f};  // muted steel blue, not the ImGui blue
        const ImVec4 accent_hot{0.40f, 0.68f, 0.92f, 1.00f};
        ImVec4* c = s.Colors;
        c[ImGuiCol_Text] = {0.90f, 0.92f, 0.94f, 1.00f};
        c[ImGuiCol_TextDisabled] = {0.48f, 0.52f, 0.58f, 1.00f};
        c[ImGuiCol_WindowBg] = {0.06f, 0.07f, 0.09f, 0.96f};
        c[ImGuiCol_ChildBg] = {0.09f, 0.10f, 0.13f, 0.60f};
        c[ImGuiCol_PopupBg] = {0.08f, 0.09f, 0.11f, 0.98f};
        c[ImGuiCol_Border] = {1.00f, 1.00f, 1.00f, 0.08f};
        c[ImGuiCol_BorderShadow] = {0.00f, 0.00f, 0.00f, 0.00f};
        c[ImGuiCol_FrameBg] = {0.15f, 0.17f, 0.21f, 1.00f};
        c[ImGuiCol_FrameBgHovered] = {0.20f, 0.23f, 0.28f, 1.00f};
        c[ImGuiCol_FrameBgActive] = {0.24f, 0.28f, 0.34f, 1.00f};
        c[ImGuiCol_TitleBg] = {0.06f, 0.07f, 0.09f, 1.00f};
        c[ImGuiCol_TitleBgActive] = {0.10f, 0.12f, 0.15f, 1.00f};
        c[ImGuiCol_TitleBgCollapsed] = {0.06f, 0.07f, 0.09f, 0.80f};
        c[ImGuiCol_ScrollbarBg] = {0.00f, 0.00f, 0.00f, 0.00f};
        c[ImGuiCol_ScrollbarGrab] = {0.24f, 0.28f, 0.34f, 1.00f};
        c[ImGuiCol_ScrollbarGrabHovered] = {0.31f, 0.36f, 0.43f, 1.00f};
        c[ImGuiCol_ScrollbarGrabActive] = accent;
        c[ImGuiCol_CheckMark] = accent_hot;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent_hot;
        c[ImGuiCol_Button] = {0.17f, 0.20f, 0.25f, 1.00f};
        c[ImGuiCol_ButtonHovered] = {0.24f, 0.33f, 0.43f, 1.00f};
        c[ImGuiCol_ButtonActive] = accent;
        c[ImGuiCol_Header] = {0.29f, 0.56f, 0.78f, 0.32f};
        c[ImGuiCol_HeaderHovered] = {0.29f, 0.56f, 0.78f, 0.55f};
        c[ImGuiCol_HeaderActive] = accent;
        c[ImGuiCol_Separator] = {1.00f, 1.00f, 1.00f, 0.10f};
        c[ImGuiCol_SeparatorHovered] = {0.29f, 0.56f, 0.78f, 0.55f};
        c[ImGuiCol_SeparatorActive] = accent;
        c[ImGuiCol_ResizeGrip] = {1.00f, 1.00f, 1.00f, 0.06f};
        c[ImGuiCol_ResizeGripHovered] = {0.29f, 0.56f, 0.78f, 0.55f};
        c[ImGuiCol_ResizeGripActive] = accent;
        c[ImGuiCol_PlotLines] = accent_hot;
        c[ImGuiCol_PlotLinesHovered] = accent_hot;
        c[ImGuiCol_TextSelectedBg] = {0.29f, 0.56f, 0.78f, 0.35f};
        c[ImGuiCol_NavHighlight] = accent;
        s.WindowPadding = {18.0f, 16.0f};
        s.FramePadding = {10.0f, 6.0f};
        s.ItemSpacing = {10.0f, 8.0f};
        s.ItemInnerSpacing = {8.0f, 6.0f};
        s.CellPadding = {8.0f, 5.0f};
        s.IndentSpacing = 18.0f;
        s.ScrollbarSize = 12.0f;
        s.GrabMinSize = 12.0f;
        s.WindowBorderSize = 1.0f;
        s.ChildBorderSize = 1.0f;
        s.PopupBorderSize = 1.0f;
        s.FrameBorderSize = 0.0f;
        s.WindowRounding = 9.0f;
        s.ChildRounding = 7.0f;
        s.FrameRounding = 5.0f;
        s.PopupRounding = 7.0f;
        s.ScrollbarRounding = 9.0f;
        s.GrabRounding = 5.0f;
        s.TabRounding = 6.0f;
        s.WindowTitleAlign = {0.5f, 0.5f};
        s.SeparatorTextBorderSize = 1.0f;
        s.SeparatorTextPadding = {0.0f, 10.0f};
        s.SeparatorTextAlign = {0.0f, 0.5f};
        s.AntiAliasedLines = true;
        s.AntiAliasedFill = true;
        return s;
    }();
    return base;
}

// readable on a 3840x2160 window instead of microscopic: everything grows with the output height,
// in quarter steps so the style is only rebuilt when the window really changes size
void ScaleStyle(ImGuiIO& io) {
    float want = std::clamp(std::round(io.DisplaySize.y / 1080.0f * 4.0f) / 4.0f, 1.0f, 4.0f);
    if (std::fabs(want - g_ui_scale) < 1e-3f) return;
    g_ui_scale = want;
    ImGui::GetStyle() = BaseStyle();
    ImGui::GetStyle().ScaleAllSizes(want);
    io.FontGlobalScale = want;  // ImGui picks the font size up at the next NewFrame
}

void Sample(float dt) {
    // a stall between presents (a load, an alt-tab) would otherwise flatten the graph for seconds
    g_ring.ms[g_ring.next] = std::clamp(dt * 1000.0f, 0.0f, 200.0f);
    g_ring.next = (g_ring.next + 1) % kSamples;
    if (g_ring.count < kSamples) g_ring.count++;
}

// the host frame target for a render scale, as the renderer builds it
void FrameTarget(float scale, uint32_t& w, uint32_t& h) {
    w = uint32_t(std::max<long>(std::lround(kGuestW * scale), 1));
    h = uint32_t(std::max<long>(std::lround(kGuestH * scale), 1));
}

void FpsCapName(uint32_t cap, char* text, size_t size) {
    if (cap == 0) std::snprintf(text, size, "Unlimited");
    else if (cap == app::kFpsCapRefresh) std::snprintf(text, size, "Monitor refresh rate");
    else std::snprintf(text, size, "%u fps", cap);
}

void DrawPerf(double fps) {
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiStyle& st = ImGui::GetStyle();
    // pinned to the top right corner, out of the way of anything the game puts on screen
    ImGui::SetNextWindowPos({io.DisplaySize.x - st.WindowPadding.x, st.WindowPadding.y}, ImGuiCond_Always,
                            {1.0f, 0.0f});
    ImGui::SetNextWindowSize({270.0f * g_ui_scale, 0.0f}, ImGuiCond_Always);  // height auto fits
    // dark enough that the greyed row labels read over a bright scene, still see-through
    ImGui::SetNextWindowBgAlpha(0.80f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMouseInputs;
    if (!ImGui::Begin("##simsbo_perf", nullptr, kFlags)) {
        ImGui::End();
        return;
    }

    float last = 0.0f, lo = 0.0f, hi = 0.0f, sum = 0.0f;
    if (g_ring.count > 0) {
        // the ring fills from 0, so entries below count are always the real samples
        lo = hi = g_ring.ms[0];
        for (int i = 0; i < g_ring.count; i++) {
            float v = g_ring.ms[i];
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            sum += v;
        }
        last = g_ring.ms[(g_ring.next + kSamples - 1) % kSamples];
    }

    ImGui::PushStyleColor(ImGuiCol_Text, st.Colors[ImGuiCol_PlotLines]);
    ImGui::Text("%.1f fps", fps);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, st.ItemSpacing.x * 2.0f);
    ImGui::Text("%.2f ms", last);

    char graph[48];
    std::snprintf(graph, sizeof graph, "%.1f - %.1f ms, avg %.1f", lo, hi,
                  g_ring.count ? sum / float(g_ring.count) : 0.0f);
    // values_offset is the oldest sample: the write cursor once the ring has wrapped
    ImGui::PlotLines("##frametime", g_ring.ms, g_ring.count, g_ring.count == kSamples ? g_ring.next : 0, graph,
                     0.0f, hi * 1.25f + 1.0f, {ImGui::GetContentRegionAvail().x, 44.0f * g_ui_scale});

    float scale = app::RenderScale(uint32_t(kGuestH));
    uint32_t rw = 0, rh = 0;
    FrameTarget(scale, rw, rh);
    const bool wide = app::Widescreen();

    // the widest label, with room to spare: the drawn text came out a little wider than the measure
    const float col = ImGui::CalcTextSize("capture").x + st.ItemSpacing.x * 3.0f;
    char value[48];
    auto row = [col, &value](const char* label) {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(col);
        ImGui::TextUnformatted(value);
    };
    std::snprintf(value, sizeof value, "%.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
    row("host");
    std::snprintf(value, sizeof value, "%u x %u  (%u%%)", rw, rh, app::Get().render_percent);
    row("render");
    std::snprintf(value, sizeof value, wide ? "16:9  (widescreen TV)" : "4:3");
    row("aspect");

    // backend counters (gpu/backend.h): cumulative title-side time, shown in milliseconds
    std::snprintf(value, sizeof value, "%.1f ms", double(gpu::g_perf_capture_ns.load(std::memory_order_relaxed)) / 1e6);
    row("capture");
    std::snprintf(value, sizeof value, "%.1f ms / %llu", double(gpu::g_perf_wait_ns.load(std::memory_order_relaxed)) / 1e6,
                  (unsigned long long)gpu::g_perf_waits.load(std::memory_order_relaxed));
    row("wait");
    std::snprintf(value, sizeof value, "%llu late / %llu block",
                  (unsigned long long)gpu::g_perf_sync_late.load(std::memory_order_relaxed),
                  (unsigned long long)gpu::g_perf_sync_block.load(std::memory_order_relaxed));
    row("sync");

    ImGui::Spacing();
    ImGui::TextDisabled("F1 settings");
    ImGui::End();
}

void SyncDraft() {
    const app::Settings& s = app::Get();
    g_draft.mode = s.display_mode;
    g_draft.width = s.width;
    g_draft.height = s.height;
    g_draft.vsync = s.vsync;
    g_draft.fps_cap = s.fps_cap;
    g_draft.render_percent = int(s.render_percent);
    g_draft.aniso = s.aniso;
    g_draft.aspect = int(std::min(s.aspect, 2u));
    g_draft.mouse_camera_speed = int(kbm::mouse_camera_speed);
    g_draft.mute_in_background = kbm::mute_in_background;
    g_draft.language = int(language::choice);
    g_desktop = app::Desktop();
    g_status[0] = '\0';
    g_status_left = 0.0f;
}

void Status(const char* text) {
    std::snprintf(g_status, sizeof g_status, "%s", text);
    g_status_left = 4.0f;
}

void Apply() {
    app::Settings& s = app::Get();
    // the renderer re-reads the settings every present; only the window's own mode or size needs the
    // display re-applied (an ApplyDisplay recentres a windowed window and switches a fullscreen mode)
    bool relayout = s.display_mode != g_draft.mode || s.width != g_draft.width || s.height != g_draft.height;
    s.display_mode = g_draft.mode;
    s.width = g_draft.width;
    s.height = g_draft.height;
    s.vsync = g_draft.vsync;
    s.fps_cap = g_draft.fps_cap;
    s.render_percent = uint32_t(std::max(g_draft.render_percent, 1));
    s.aniso = g_draft.aniso;
    s.aspect = uint32_t(g_draft.aspect);
    kbm::mouse_camera_speed = uint32_t(std::clamp(g_draft.mouse_camera_speed, 25, 400));
    kbm::mute_in_background = g_draft.mute_in_background;
    language::choice = uint32_t(g_draft.language);
    if (relayout) app::ApplyDisplay();
}

// the label column of a settings row, with the control stretched across the rest
void Row(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

// what a control really does, under it, wrapped to the column
void Note(const char* text) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", text);
    ImGui::PopTextWrapPos();
}

bool BeginRows(const char* id, float label_width) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit)) return false;
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, label_width);
    ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void DrawDisplay() {
    Row("Display mode");
    int mode = int(g_draft.mode);
    if (ImGui::Combo("##mode", &mode, kModeNames, int(std::size(kModeNames)))) g_draft.mode = app::DisplayMode(mode);

    // Borderless always fills the display it is on, so a chosen resolution can never size its window:
    // the control is greyed there rather than offered and silently ignored. It stays editable in the
    // two modes that honour it, and the stored value survives a mode switch.
    const bool borderless = g_draft.mode == app::DisplayMode::Borderless;
    Row("Resolution");
    ImGui::BeginDisabled(borderless);
    char text[48];
    if (g_draft.width && g_draft.height) std::snprintf(text, sizeof text, "%u x %u", g_draft.width, g_draft.height);
    else std::snprintf(text, sizeof text, "Desktop (%u x %u)", g_desktop.width, g_desktop.height);
    if (ImGui::BeginCombo("##resolution", text)) {
        std::snprintf(text, sizeof text, "Desktop (%u x %u)", g_desktop.width, g_desktop.height);
        if (ImGui::Selectable(text, !g_draft.width || !g_draft.height)) g_draft.width = g_draft.height = 0;
        for (const app::Mode& m : kResolutions) {
            std::snprintf(text, sizeof text, "%u x %u", m.width, m.height);
            if (ImGui::Selectable(text, g_draft.width == m.width && g_draft.height == m.height)) {
                g_draft.width = m.width;
                g_draft.height = m.height;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    Note(borderless ? "Borderless follows the display, so no window size applies here; the render "
                      "scale below still counts from this size, so Desktop renders at the display's "
                      "full height. Choose Windowed or Fullscreen to set a size."
                    : "Windowed sizes the window to this. Fullscreen switches the display to this "
                      "mode. Desktop is the display's own size.");

    Row("Vertical sync");
    ImGui::Checkbox("##vsync", &g_draft.vsync);

    Row("Frame rate cap");
    FpsCapName(g_draft.fps_cap, text, sizeof text);
    if (ImGui::BeginCombo("##fpscap", text)) {
        for (uint32_t cap : kFpsCaps) {
            FpsCapName(cap, text, sizeof text);
            if (ImGui::Selectable(text, g_draft.fps_cap == cap)) g_draft.fps_cap = cap;
        }
        ImGui::EndCombo();
    }
    Note("Caps the presented frame rate; the game still steps by the time that really passed.");

    Row("Aspect ratio");
    ImGui::Combo("##aspect", &g_draft.aspect, kAspectNames, int(std::size(kAspectNames)));
    Note("16:9 is the game's own widescreen mode, as on a widescreen TV; 4:3 shows it with bars at the "
         "sides. The game reads the TV's shape when it starts: restart it after a change.");
}

void DrawQuality() {
    Row("Render scale");
    ImGui::SliderInt("##renderscale", &g_draft.render_percent, 50, 200, "%d %%");
    // the same arithmetic the renderer does: the output height, this share of it, over the guest's
    app::Mode out = g_draft.width && g_draft.height ? app::Mode{g_draft.width, g_draft.height} : g_desktop;
    float scale = std::max(out.height * (g_draft.render_percent / 100.0f) / kGuestH, 0.1f);
    uint32_t rw = 0, rh = 0;
    FrameTarget(scale, rw, rh);
    char text[80];
    std::snprintf(text, sizeof text, "%u x %u frame target, from the guest's 640 x 480.", rw, rh);
    Note(text);

    Row("Anisotropic filtering");
    int aniso = 0;
    for (int i = 0; i < int(std::size(kAniso)); i++)
        if (kAniso[i] == g_draft.aniso) aniso = i;
    if (ImGui::Combo("##aniso", &aniso, kAnisoNames, int(std::size(kAnisoNames)))) g_draft.aniso = kAniso[aniso];
    Note("Forced only where the game already filters across mip levels.");
}

// the fixed keyboard-and-mouse bindings (input_kbm.cpp): shown, not edited - the layout is the
// port's control scheme, not a per-player setting
void DrawInput() {
    Row("Move / menus");
    Note("W A S D moves (left stick); arrow keys are the D-pad.");
    Row("Camera");
    Note("Hold the right mouse button and drag, or Q / E to rotate and R / F or the mouse wheel to zoom.");
    Row("Buttons");
    Note("Space A, Escape/Backspace B, X X, Y Y, Z / C triggers, 1 / 2 black / white, Enter Start, "
         "Tab Back, Shift / Ctrl (or middle button) stick clicks.");
    Row("Mouse camera speed");
    ImGui::SliderInt("##mousecam", &g_draft.mouse_camera_speed, 25, 400, "%d %%");
    Note("How far the right-button drag turns and zooms the camera for the same hand movement.");
    Row("Controller");
    Note("Black = RB / R1 / R, White = LB / L1 / L; L and R in the cheat codes are the triggers "
         "(LT / RT, L2 / R2, ZL / ZR). Face buttons go by position, Xbox layout.");
}

// The game's own console cheats (common/cheats.h). Each one is entered the way its pad code is, so
// a checkbox is the game's own state of that code and what follows is the game's: the cheat gnome
// appears, and its menu offers the money and motives codes that are on.
struct CheatRow {
    cheats::Code code;
    const char* label;
    const char* note;
};
constexpr CheatRow kCheatRows[] = {
    {cheats::kGnome, "Cheat gnome",
     "R, L, Down, Black, Left, B: the blue gnome appears at the house. The two below are picked "
     "from its menu."},
    {cheats::kMoney, "Give money",
     "L, Black, Right, X, Left: adds Give Money to the gnome, 10,000 Simoleons a pick."},
    {cheats::kMotives, "Fill motives",
     "R, Left, B, Down, White: adds Fill Motives to the gnome."},
    {cheats::kObjects, "Unlock all objects", "Black, Up, Y, Down, R: every buy-mode object available."},
    {cheats::kLocations, "Unlock all locations", "Black, Down, R, L, Down, Y."},
    {cheats::kSocials, "Unlock all socials", "L, R, A, Down, Black."},
    {cheats::kSkins, "Unlock all skins", "L, B, A, Black, Down, Y: the Create-a-Sim items available."},
    {cheats::kTeamPhoto, "Team picture", "R, Black, Down: the development team's picture."},
};

void DrawCheats(bool ingame) {
    // a click only queues the code: the title thread enters it at the top of its next frame
    ImGui::BeginDisabled(!ingame);
    for (const CheatRow& r : kCheatRows) {
        Row(r.label);
        bool on = cheats::On(r.code);
        ImGui::PushID(int(r.code));
        if (ImGui::Checkbox("##cheat", &on)) cheats::Toggle(r.code);
        ImGui::PopID();
        Note(r.note);
    }
    ImGui::EndDisabled();
}

void DrawSettings() {
    const ImGuiIO& io = ImGui::GetIO();
    // centred, auto fitting its height, and resizable once it is up
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({620.0f * g_ui_scale, 0.0f}, ImGuiCond_Appearing);
    if (g_settings_focus) ImGui::SetNextWindowFocus();
    g_settings_focus = false;
    bool open = true;
    if (!ImGui::Begin("Settings###simsbo_settings", &open,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) g_settings_open = false;
        return;
    }

    const float label_width = ImGui::CalcTextSize("Anisotropic filtering").x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SeparatorText("Display");
    if (BeginRows("##display_rows", label_width)) {
        DrawDisplay();
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Quality");
    if (BeginRows("##quality_rows", label_width)) {
        DrawQuality();
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Input");
    if (BeginRows("##input_rows", label_width)) {
        DrawInput();
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Game");
    if (BeginRows("##game_rows", label_width)) {
        Row("Language");
        if (ImGui::BeginCombo("##language", language::kChoices[g_draft.language].name)) {
            for (int i = 0; i < int(std::size(language::kChoices)); i++)
                if (ImGui::Selectable(language::kChoices[i].name, g_draft.language == i)) g_draft.language = i;
            ImGui::EndCombo();
        }
        Note("The game's text, all on the disc. Read when the game starts: Save, then restart it.");
        Row("Mute in background");
        ImGui::Checkbox("##mutebg", &g_draft.mute_in_background);
        Note("Silent while another window is active; the game keeps running.");
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Cheats");
    const bool ingame = cheats::InGame();
    Note(ingame ? "The game's own pad codes, entered as if typed on the pad. Each click turns one on or off, "
                  "and none is saved."
                : "Codes go in only while the game is running with its cheat latch clear: not before it "
                  "has started, and not in a state that has latched cheats off.");
    if (BeginRows("##cheat_rows", label_width)) {
        DrawCheats(ingame);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    const float button = ImGui::GetFontSize() * 6.0f;
    if (ImGui::Button("Apply", {button, 0.0f})) {
        Apply();
        Status("Applied: the renderer takes it from the next frame.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save", {button, 0.0f})) {
        Apply();
        app::Save();
        Status("Applied and written to settings.toml.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", {button, 0.0f})) open = false;

    if (g_status_left > 0.0f) {
        g_status_left -= io.DeltaTime;
        ImGui::TextDisabled("%s", g_status);
    } else {
        ImGui::TextDisabled("F1 closes this panel, F2 shows the frame times.");
    }
    ImGui::End();
    if (!open) g_settings_open = false;
}

}  // namespace

void Draw(double fps) {
    // the renderer calls this inside its frame, but a hook that cannot be reached by a broken
    // context is worth the two compares: this runs on the render thread
    if (!ImGui::GetCurrentContext()) return;
    ImGuiIO& io = ImGui::GetIO();

    // the pad belongs to the game alone: no overlay window can ever act on a button the player
    // pressed, whatever the host's ImGui backend feeds it

    // minimized, or before the backend has a size: nothing to draw into
    if (io.DisplaySize.x < 1.0f || io.DisplaySize.y < 1.0f) return;
    ScaleStyle(io);
    Sample(io.DeltaTime);

    // SIMSBO_UI=settings or =perf opens that panel at the first frame: a screenshot of it, or a
    // player's report, needs no F-key to reach a window that may not have the focus
    static bool env_checked = false;
    if (!env_checked) {
        env_checked = true;
        if (const char* v = std::getenv("SIMSBO_UI")) {
            if (std::strcmp(v, "settings") == 0) {
                g_settings_open = true;
                SyncDraft();
                g_settings_focus = true;
            } else if (std::strcmp(v, "perf") == 0) {
                g_perf_open = true;
            }
        }
    }

    // the host feeds every key to ImGui under the same lock, so these land while the game has
    // focus. F3 is the host's thread dump
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        g_settings_open = !g_settings_open;
        if (g_settings_open) {
            SyncDraft();
            g_settings_focus = true;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) g_perf_open = !g_perf_open;
    // the keyboard-to-pad layer stands down while the panel has the keyboard (input_kbm.cpp)
    g_settings_up.store(g_settings_open, std::memory_order_relaxed);

    // the mouse is only ImGui's while the settings panel is up; closed, the overlay neither
    // captures a click nor touches the cursor, and io.WantCaptureMouse stays false. The wheel
    // reaches the keyboard-and-mouse pad as WM_MOUSEWHEEL in kbm::Message either way.
    if (g_settings_open) io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;

    if (!g_perf_open && !g_settings_open) return;  // closed: the overlay draws nothing else
    if (g_perf_open) DrawPerf(fps);
    if (g_settings_open) DrawSettings();
}

bool SettingsOpen() { return g_settings_up.load(std::memory_order_relaxed); }

}  // namespace overlay
