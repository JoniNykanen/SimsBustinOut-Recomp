// Hooks for default.xbe (The Sims Bustin' Out, Europe). A strong definition replaces the recompiled function at
// that address.
#include "cpu/x86_ops.h"
#include "xdk/hle.h"

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
