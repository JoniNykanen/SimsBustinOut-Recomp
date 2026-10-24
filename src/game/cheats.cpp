// The console cheats, entered the game's own way (common/cheats.h, offered in the F1 panel). Ported from the USA
// project (abilliontokens/bustinout-recomp, src/game/cheats.cpp, which documents the mechanism instruction by
// instruction and docs/cheats.md); the addresses below are the Europe image's, read from the recompiled code.
//
// Recogniser. The application's per-frame update sub_000122C0 (USA sub_00012280 + 0x40) runs the cheat-code
// recogniser sub_00011F60 (USA 0x11F20) whenever a pad reports a fresh press, under three conditions it checks first:
// the cheat latch byte at 0x3DD010 clear (cmp at 0x1230B), the pad manager present ([0x35D490] != 0, 0x1231E) and
// the recogniser object present ([app+0x1E4] != 0, 0x123B2). The recogniser keeps the last six single-button presses
// and matches them against an eight-row table the options constructor sub_000720A0 builds in code into the options
// object at 0x310C38 (mov ecx, 0x310C38 at 0x11EC5 / 0x11F27 / 0x12088). On a match it calls the setter with the row.
//
// Setter. sub_000720A0 (USA sub_00072010 + 0x90; ecx = options, one stack argument = row, retn 4) flips bit `row` of
// the cheat mask word at [options+0x76] (xor [eax+0x76], dx at 0x720B8) and plays the cheat-on sound 0x350B962D
// (push at 0x72108) or the cheat-off sound 0x4A7EF64D (0x7210F). The mask is what the cheat gnome's SimAntics trees
// read as globals 46 and 47, and what the unlock tests read bit by bit. Every reader first checks the latch byte at
// 0x3DD010: while it is set, codes are neither recognised nor entered and the mask reads as zero.
//
// So the host enters a code the way a matched sequence does: the same call, from the same place in the frame. The
// override of sub_000122C0 below calls sub_000720A0 for every code the F1 panel asked for, on the title thread,
// before the frame's own input pass, and only under the same three conditions the game recognises codes under.
#include <atomic>
#include <cstdint>

#include "common/cheats.h"
#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace {

// A guest address the host may read: inside one of the regions the guest maps (runtime/kernel/memory.h) and on
// committed pages, since a stale pointer into the reserved but uncommitted user range faults the host on the first read.
bool GuestPtr(uint32_t va, uint32_t size) {
    if (size == 0 || va + size < va) return false;
    const uint32_t end = va + size;
    const bool in_range =
        (va >= mem::kUserLow && end <= mem::kUserHigh) ||
        (va >= mem::kContiguousBase && end <= uint32_t(mem::kContiguousBase + mem::kPhysicalSize)) ||
        (va >= mem::kSystemBase && end <= uint32_t(mem::kSystemBase + mem::kSystemSize));
    if (!in_range) return false;
    for (uint32_t page = va & ~(mem::kPage - 1u); page < end; page += mem::kPage)
        if (!mem::IsCommitted(page)) return false;
    return true;
}

// The options object the code table and the mask live in (USA 0x310B98 + 0xA0)
constexpr uint32_t kOptions = 0x310C38u;
// [options+4]: the OR of every table row; zero until sub_000720A0 has run
constexpr uint32_t kValidMask = kOptions + 4u;
// [options+0x76]: the cheat mask itself (0x720B8: xor [eax+0x76], dx)
constexpr uint32_t kMask = kOptions + 0x76u;
// the cheat latch: while set, the mask getter returns zero (USA 0x3DCF70 + 0xA0)
constexpr uint32_t kLatch = 0x3DD010u;
// the pad manager the frame update checks before it recognises codes (0x1231E; USA 0x35D3F0 + 0xA0)
constexpr uint32_t kPadManager = 0x35D490u;

// sub_000720A0: the options object in ecx, the table row as its one stack argument (retn 4)
constexpr uint32_t kEnterCode = 0x720A0u;
constexpr hle::Map kEnterAbi = hle::Make({hle::ecx, hle::stack});

// the latch is on a different page from the options object, so it is validated on its own. Unreadable counts as
// set: the game never takes codes without having read it clear.
bool LatchClear() { return GuestPtr(kLatch, 1) && *static_cast<const uint8_t*>(kernel::HostPtr(kLatch)) == 0; }

// the cheat mask as the gnome sees it: the latch hides it, exactly as the game's getter does
uint32_t GameMask() {
    if (!GuestPtr(kOptions, 0x78)) return 0;
    if (!LatchClear()) return 0;
    return kernel::R16(kMask);
}

const char* Name(int code) {
    switch (code) {
        case cheats::kGnome: return "Cheat gnome";
        case cheats::kMoney: return "Give Money";
        case cheats::kMotives: return "Fill Motives";
        case cheats::kObjects: return "Unlock All Objects";
        case cheats::kLocations: return "Unlock All Locations";
        case cheats::kSocials: return "Unlock All Socials";
        case cheats::kSkins: return "Unlock All Skins";
        case cheats::kTeamPhoto: return "Team picture";
    }
    return "unknown";
}

// codes the panel asked to enter, one bit each; a second click before the title thread takes the first cancels it,
// as entering the code twice would
std::atomic<uint32_t> g_toggle{0};

// The title thread, at the top of its frame: enter what the panel asked for. The conditions are checked again here,
// in the game's own form, because the click came from the render thread a frame ago.
void EnterPending(X86Context& ctx) {
    const uint32_t pending = g_toggle.exchange(0, std::memory_order_relaxed);
    if (!pending) return;
    // every address is validated before it is read; the recogniser's address comes from the guest's ecx (this), so
    // its +0x1E4 field is checked as the pointer it is
    const bool latch_clear = LatchClear();                                                       // 0x1230B
    const bool pads = GuestPtr(kPadManager, 4) && kernel::R32(kPadManager) != 0;                 // 0x1231E
    const uint32_t recogniser_at = ctx.ecx + 0x1E4u;
    const bool recogniser = recogniser_at >= ctx.ecx && GuestPtr(recogniser_at, 4) &&
                            kernel::R32(recogniser_at) != 0;                                     // 0x123B2
    if (!latch_clear || !pads || !recogniser) {
        LOGI("cheats: codes %02X not entered, the game was not in a state to take them", pending);
        return;
    }
    // flags are live only across tail calls, but the update reads them on entry: keep them as found
    const uint32_t cf = ctx.cf, zf = ctx.zf, sf = ctx.sf, of = ctx.of, pf = ctx.pf, df = ctx.df;
    for (int code = 0; code < 8; code++) {
        if (!(pending & (1u << code))) continue;
        hle::Call(ctx, kEnterCode, kEnterAbi, {kOptions, uint32_t(code)});
        const uint32_t mask = GameMask();
        LOGI("cheats: entered %s (code %d): %s, the game's cheat mask is now %02X", Name(code), code,
             (mask >> code) & 1u ? "on" : "off", mask);
    }
    ctx.cf = cf;
    ctx.zf = zf;
    ctx.sf = sf;
    ctx.of = of;
    ctx.pf = pf;
    ctx.df = df;
}

}  // namespace

namespace cheats {

bool InGame() {
    if (!GuestPtr(kOptions, 0x78)) return false;
    if (!LatchClear()) return false;
    // the table is built in code at startup: its combined mask is zero until sub_000720A0 has run
    return kernel::R16(kValidMask) != 0;
}

bool On(Code c) {
    const uint32_t bit = 1u << c;
    return ((GameMask() ^ g_toggle.load(std::memory_order_relaxed)) & bit) != 0;
}

void Toggle(Code c) {
    if (InGame()) g_toggle.fetch_xor(1u << c, std::memory_order_relaxed);
}

}  // namespace cheats

X86_EXTERN_FUNC(__imp__sub_000122C0);

// sub_000122C0: the application's per-frame update, whose first act on a fresh press is the game's own cheat-code
// recogniser; pending codes are entered just ahead of it, under the conditions the update itself checks
X86_FUNC(sub_000122C0) {
    LOG_FIRST(1, "cheats: frame hook reached");
    if (g_toggle.load(std::memory_order_relaxed)) EnterPending(ctx);
    __imp__sub_000122C0(ctx, base);
}
