// Hooks for default.xbe (The Sims Bustin' Out, Europe). A strong definition replaces the recompiled function at
// that address. Ported from the USA project (abilliontokens/bustinout-recomp, src/game/hooks.cpp); the Europe image
// has its D3D and data sections +0xA0 from the USA one and its .text functions at or above 0x1217B0 at +0x110.
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "cpu/x86_ops.h"
#include "xdk/hle.h"

namespace {
// SIMSBO_NO_STEP=1 switches the frame-step hooks off for the run, so a defect can be bisected against the stock
// behaviour without a rebuild. Off means the recompiled function runs exactly as the image wrote it.
bool HookOff(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] && std::strcmp(v, "0") != 0;
}
}  // namespace

// D3DDevice_InsertCallback(Flags, pCallback, Context), Europe 0x24D0D0 (USA 0x24D030; the D3D section is +0xA0).
//
// On the console this writes a callback token into the push buffer and the D3D interrupt handler runs
// pCallback(Context) once the GPU reaches it. The SDK has no push-buffer callback path: the token lands in a scratch
// buffer the renderer walks for draws only, so the callback would never run, and the engine thread that waits on
// the semaphore the callback releases would wait forever (the game never draws a frame).
//
// The host renderer has no GPU timeline the guest can be behind, so the callback runs immediately: by the time this
// returns, everything queued before it has been handed to the render thread. Argument order and retn 0xC were read
// from the guest body by the USA port (abilliontokens/bustinout-recomp, docs/frame-rate.md): the second stack
// argument is the callback, the third its context, the first only selects the longer token form.
HLE_STDCALL(D3DDevice_InsertCallback, 3) {
    uint32_t callback = a[1], context = a[2];
    if (callback != 0) ::kernel::CallGuest(ctx, callback, {context});
    return 0;
}

// The frame step, sub_00123400 (USA sub_001232F0 + 0x110): this in ecx, float elapsed_seconds on the stack, retn 4.
// Called once a frame from the frame tick sub_00123540 below (call at +0x77 of it).
//
// The engine measures real time and then throws the precision away: this quantiser divides the elapsed seconds by
// the tick size at [0x35D2B8] (USA 0x35D218), truncates to a tick count stored at [0x30AB70] (USA 0x30AAD0), clamps
// it to 1..15 and adds it to the running total at [0x35D2D0] (USA 0x35D230). The tick size is set once by the frame
// object's constructor sub_00123B10: 0.02 s when [0x2E3E18] is 1, else 1/60 s; in this Europe image [0x2E3E18] is
// the constant 1 in .rdata, so the shipped game always runs the PAL 0.02 s quantum. The caller then makes the one
// delta the rest of the engine multiplies by out of ticks * tick size. A frame shorter than one tick still advances
// the world by a whole tick: at 60 fps this image runs 1.2x too fast, and uncapped the speed grows without bound.
//
// The fix keeps the engine's measurement and replaces only the quantiser: ticks of 1 ms, the engine's own
// maximum-step ceiling kept (15 ticks of 0.02 s = 0.30 s on this PAL image; the USA port's 0.25 s was its 15 x 1/60),
// and a seconds-per-tick of dt / ticks written back into [0x35D2B8], so the caller's `ticks * [0x35D2B8]` is exactly
// the time that really passed. The full derivation, with every instruction address in the USA image, is in the USA
// project's docs/frame-rate.md and src/game/hooks.cpp. The one other reader of [0x35D2B8] outside the frame step is
// the "CPU:" debug readout at 0x165B03, display only.
X86_EXTERN_FUNC(__imp__sub_00123400);
X86_FUNC(sub_00123400) {
    if (HookOff("SIMSBO_NO_STEP")) { __imp__sub_00123400(ctx, base); return; }
    constexpr double kTick = 0.001;  // seconds per tick, 0.02 (PAL) before this
    constexpr int kMaxTicks = 300;   // 0.30 s, the engine's own ceiling of 15 ticks of 0.02 s
    const float elapsed = X86_MEMF32(ctx.esp + 0x4u);
    // not (elapsed > 0): a NaN or a negative reading takes the smallest step, as the clamp did
    const double dt = elapsed > 0.0f ? std::min(double(elapsed), kMaxTicks * kTick) : kTick;
    const int ticks = std::clamp(int(dt / kTick + 0.5), 1, kMaxTicks);
    // once, at Report level so a release log shows the hook is live (the engine calls this from one thread)
    static bool reported = false;
    if (!reported) {
        reported = true;
        LOGR("simsbo: frame step hook live, first elapsed %.4f s -> %d ticks of %.6f s", double(elapsed), ticks, dt / ticks);
    }
    X86_MEMF32(0x35D2B8u) = float(dt / ticks);  // ticks * this == the time that really passed
    X86_MEM32(0x30AB70u) = uint32_t(ticks);
    X86_MEM32(0x35D2D0u) = X86_MEM32(0x35D2D0u) + uint32_t(ticks);
    ctx.eax = uint32_t(ticks);
    ctx.esp += 8;  // retn 4
}

// The frame tick itself, sub_00123540 (USA sub_00123430 + 0x110): this in ecx, no stack arguments. Reached through
// the game frame subclass's thunk sub_00165F40 at vtable+0x18 of 0x2EEDC8 (USA 0x2EED28 + 0xA0).
//
// This wrapper writes nothing of its own. It sets the frame object's fixed-step byte at this+5 for the duration of
// the original call and puts the saved value back afterwards, so the tick's own product path (cmp byte [esi+5], 0,
// then cvtsi2ss of the tick count at [0x30AB70] and mulss by [0x35D2B8]) makes the delta out of the host quantiser's
// tick count and written-back tick size, instead of the default path's three-frame average of an elapsed clamped to
// a whole tick at the bottom. Without it, replacing the quantiser alone would leave that average in charge. The byte
// is clear in play (the constructor and the game's frame subclass clear it, nothing sets it) and nothing else reads
// it during the tick; it is restored whatever it was.
X86_EXTERN_FUNC(__imp__sub_00123540);
X86_FUNC(sub_00123540) {
    if (HookOff("SIMSBO_NO_STEP")) { __imp__sub_00123540(ctx, base); return; }
    const uint32_t frame = ctx.ecx;
    const uint8_t fixed_step = X86_MEM8(frame + 0x5u);
    X86_MEM8(frame + 0x5u) = 1;
    __imp__sub_00123540(ctx, base);  // pops its own return
    X86_MEM8(frame + 0x5u) = fixed_step;
}
