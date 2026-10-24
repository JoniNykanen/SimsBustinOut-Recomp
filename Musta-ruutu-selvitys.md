# Toimintasuunnitelma: The Sims Bustin' Out (Europe, XDK 5558) upstream-SDK:lla Windows/D3D12 — korjattu versio

Pelikansio `C:\XboxRecomp\SimsBustinOut-Recomp` (git-repo), SDK `C:\XboxRecomp\XboxXBE-Recomp` (git-klooni `TGP482/XboxXBERecomp`, HEAD `e0de3e4`, työpuu puhdas lukuun ottamatta seuraamattomia `xdk/5558/` ja `xdk-5558-lainattu-5849-koe/`), fork-worktree `…\scratchpad\fork-sdk` (**detached HEAD `adc40ab`**, ei haaralla), refs-repo `…\scratchpad\refs\abilliontokens-XboxXBERecomp` (paikallinen haara vain `main`; forkin haara on vain `refs/remotes/origin/bustinout-port`; remote `up` = `C:\XboxRecomp\XboxXBE-Recomp`).

## Mitä tässä versiossa on korjattu kriitikon huomioiden mukaan

| Kohta | Korjaus |
|---|---|
| Vaihe 0 fetch | `git fetch fork bustinout-port` ei toimi (refs-repossa ei paikallista haaraa). Toimiva muoto: `git -C $S fetch fork refs/remotes/origin/bustinout-port:refs/remotes/fork/bustinout-port` (tuottaa `adc40ab`) tai remote suoraan GitHubiin. Lisäksi: forkin neljä tarvittavaa muutosta on nyt **valmiina patch-tiedostoina** scratchpadissa ja `git apply --check` on ajettu upstream-SDK:ta vasten (tulokset §0.3), joten cherry-pick-komentoja ei tarvita lainkaan. |
| Vaihe 1b(i) | Korvattava alue on `d3d8.cpp:213-217` (rivi 217 on `if (!g_own_constants) {…}`-lohkon sulkeva `}`), ei `:213-216`. Snippet sisältää sulkevan sulun. |
| Vaihe 1b(ii) | `kDevRenderTarget` asetetaan `d3d8.cpp:271`, `kDevDepth` `:272`, `kDevBackBuffer` `:273`; `UpdateSuperSample()` korvaa rivit `:276-277`. |
| §0-taulukko / käännös | `configs`-glob on `cmake\XbeRecomp.cmake:92` (`:85` = `function(add_xbe_image …)`), `add_custom_command` `:93-104`, `DEPENDS xbe_recomp ${configs} …` `:100`; `runtime/*/*.cpp`-glob `:75`. |
| A.2 | `sub_128BF0` on **titlen oma** `.text`-funktio (palauttaa skaalan 2.0, koska laite luodaan `MultiSampleType = 0x2222`); kirjaston osuus on `0x24AAC0` (render target setup) ja `0x249980` (`GetViewportOffsetAndScale`). |
| Vaihe 5 PlayEx | Forkin hunk `fork-sdk\runtime\xdk\dsound.cpp:670-677` sisältää `g_stat_plays++;` (ei upstreamissa) eikä tunne upstreamin `v.has_gain = false;` (`dsound.cpp:973`). Ohje: kopioi **upstreamin** `Play`-runko (`:972-979`) nimellä `PlayEx`, `BUFFER(PlayEx, 4)`; `BUFFER`-makro `:939`. Fallback-id ei saa olla forkin `FFFFFF0D` (se on upstreamissa `Pause`n, `:81`) → `FFFFFF0F`. |
| 1b(iii) / E.2 | `D3DDevice_SetRenderTargetFast` (`d3d8.cpp:549-553`) kutsuu vain `ApplyRenderTarget`ia, ei `UpdateViewportConstants()`ia → c-38/c-37 jäävät edellisen targetin skaalaan. Kirjattu riskiksi ja bisect-kohteeksi, vaihtoehtoinen yhden rivin korjaus annettu. Symboli on elävä: `config\game.toml:21` = `0x246360`, `image_symbols.h:201-202` `HAS … 1`. |
| Swap-laskuri | Yhden rivin `LOGR` lisätty Vaiheeseen 1 (näkyy releasessakin). Huom: `d3d8.cpp:614` on `SetDev(kDevSwapCount, n);` ja `:615` `return n;` — kriitikon `Dev(kDevSwapCount) + 1` rivin 615 edessä tulostaisi n+1; käytä `n`:ää. |
| Ääni | `dsp_image.cpp` tunnistaa vain 5849-DSP-kuvan → Europe-loki `dsound: effects image, reverb 0, effects 0 crosstalk,` = kaiku/efektit puuttuvat odotetusti. Kirjattu E:hen, ettei sitä debugata PlayEx-vikana. |
| Testaus | `SIMSBO_AUTOSTART` antaa väärän hyväksynnän (ei vahvista Simiä); D1 (`0x66A10`) ja talon latautuminen todennetaan vain `SIMSBO_STEP`-kävelyllä (tokenit `docs/black-scene.md:274-279`). Vaihe 8. |
| F3 / D3D12-debug | F3-kaappaus vaatii vähintään kaksi `Present`-kutsua F3:n jälkeen; `XBE_D3D12_DEBUG` ei ole kytketty `ID3D12InfoQueue`en → viestit vain DebugView'ssa ja vaativat Graphics Tools -ominaisuuden. Vaihe 2 ja A.4. |
| Vaihe 7 | `80fb58b` (`app.h` +5, `d3d12.cpp` +9/−1) applikoituu upstreamiin puhtaasti (`git apply --check` OK, todettu uudelleen tässä sessiossa) → otetaan jo Vaiheessa 1 kohtana 1d, jolloin palkkihypoteesi todennetaan samassa ajossa. Kernel-patch: `files.cpp` +224, `iocp.cpp` +183 (uusi), `kernel.h` +5, `objects.cpp` +40/−8; includet `<chrono>` ja `kernel/exports.h` (on upstreamissa: `runtime\kernel\exports.h`), `<cctype>` tulee `objects.cpp`:hen. |

---

## §0 Lähtötilanne — levyltä tässä sessiossa todetut faktat

### 0.1 Tilanne on edennyt kontekstista: peli piirtää jo (Ajo 4)

Konteksti sanoo "ikkuna on täysin musta", mutta levyllä on uudempi tila, joka vahvistaa raporttien pääehdokkaan:

- `C:\XboxRecomp\SimsBustinOut-Recomp\src\game\hooks.cpp` sisältää jo `HLE_STDCALL(D3DDevice_InsertCallback, 3) { … ::kernel::CallGuest(ctx, callback, {context}); … }` (USA-projektin `src/game/hooks.cpp:48-52`:n runko).
- `TILANNE.md` ("Ajo 4 (14:55): PELI PIIRTÄÄ — intro ja The Sims -logo näkyvät"): kolme korjausta — (1) `config\game.toml` `[symbols]`: `D3DDevice_GetTile = 0x245610`, `D3DDevice_LoadVertexShaderProgram = 0x24C0C0`, `D3DDevice_SetRenderTargetFast = 0x246360`, `IDirectSoundBuffer_SetEG = 0x28DFCE`; (2) InsertCallback-hook; (3) `xdk\5558-lainattu-5849` siirretty pois `xdk\`-kansiosta (cmake globaa `xdk/*/`, `XbeRecomp.cmake:92`). Kuvat `out\kuvakaappaukset\hook-12s.png`, `hook-55s.png`.
- **Jäljellä oleva vika (TILANNE.md):** kuva n. 2× liian suuri, vain vasen yläneljännes näkyy; musta palkki ylhäällä; ikkunan alareunan n. 20 px jää piirtämättä.
- Release-loki `out\build\release\logs\simsbustinout.log` (14:58) päättyy yhä riviin `[0.471] dsound: effects image, reverb 0, effects 0 crosstalk,` — odotettua (0.3).

Jos luet tätä tilanteessa, jossa ruutu on taas musta, A.4 kertoo mitä ajetaan.

### 0.2 Upstream-SDK:n rivit, joihin suunnitelma nojaa (tarkistettu `C:\XboxRecomp\XboxXBE-Recomp`, e0de3e4)

| Tiedosto:rivi | Sisältö |
|---|---|
| `runtime\core\log.h:18-22` | `#ifdef NDEBUG → kVerbose = false` |
| `runtime\core\log.cpp:55` | `if (!kVerbose && lv < Level::Error) return;` → release kirjoittaa vain `LOGE`/`LOGR` |
| `runtime\xdk\d3d8.cpp:101-102` | `kDevBackBuffer`, `kDevSuperX` (= `XDK_CDevice_SuperSampleX`) |
| `d3d8.cpp:207` | `bool g_own_constants` |
| `d3d8.cpp:210-217` | `UpdateViewportConstants()`: `:213` `hw/hh`, `:214-217` `if (!g_own_constants) {…}` |
| `d3d8.cpp:247` | `void SetFrameBuffers(const char* caller, uint32_t pp)`; `:248-249` lukee `pp+0/4/8/12/0x20/0x24/0x30`, **ei `pp+0x10`** |
| `d3d8.cpp:252-253` | `LOGI("d3d: %s %ux%u format %X buffers %u depth %u format %X interval %X flags %X", caller, …)` — CreateDevice/Reset-loki on olemassa (LOGI), ilman multisample-kenttää |
| `d3d8.cpp:270-277` | `:270` viewport, `:271` `SetDev(kDevRenderTarget, g_buffers[1])`, `:272` `kDevDepth`, `:273` `kDevBackBuffer`, `:274` `kDevFrontBuffer`, `:275` `float one = 1.0f, zs = …`, **`:276-277` `kDevSuperX/Y = 1.0`** |
| `d3d8.cpp:280-283` | `BackBufferScale = one` (käyttää `one`-muuttujaa → `:275` jää) |
| `d3d8.cpp:285-287` | `g_state.rt`, `g_state.zs`, `UpdateViewportConstants()` |
| `d3d8.cpp:522-533` | `ApplyRenderTarget()`; `:532` `g_state.zs = SurfaceOf(zs);` |
| `d3d8.cpp:536-547` | `D3DDevice_SetRenderTarget` → `ApplyRenderTarget` + viewport + `UpdateViewportConstants()` (`:545`) |
| `d3d8.cpp:549-553` | `D3DDevice_SetRenderTargetFast` → vain `ApplyRenderTarget` |
| `d3d8.cpp:578-616` | `D3DDevice_Swap`: gamma-pending `:594-600` (`i = SwapCount & 1` `:595`), `Walk(Dev(kDevPush))` `:601`, `Present` `:602`, `SetDev(kDevSwapCount, n)` `:614`, `return n` `:615`. Ei Swap-lokia. |
| `d3d8.cpp:627-634` | `D3DDevice_Reset` HLE (host) |
| `d3d8.cpp:921` | `HLE_FASTCALL(D3DDevice_SetVertexShaderConstant1Fast, 0)` — oikea 5558:lle |
| `runtime\gpu\state.h:186-187` | `ss_offset_x/y`, `zscale` — structin viimeiset kentät (`:188` `};`) |
| `runtime\gpu\render_thread.cpp:79` | viimeinen lähetettävä alue `{At(s, &s.material), sizeof(State) - At(s, &s.material)}` → `zscale`n perään lisätyt kentät kulkevat render-säikeelle automaattisesti |
| `runtime\gpu\d3d12.cpp:756-758` | `XBE_D3D12_DEBUG` → `EnableDebugLayer()`, **ei `ID3D12InfoQueue`a** (grep 0) |
| `d3d12.cpp:2019-2021` | `FillSurfaceConstants(const State& s, GuestExtent size, DrawConstants& c)`: `surf[0] = size.w`, `surf[1] = size.h` |
| `d3d12.cpp:2190` | ainoa kutsu `FillSurfaceConstants(s, size, c);`; `:2192` `if (k.topo == Points) c.surf[3] = TargetScale();` |
| `d3d12.cpp:2822-2837` | `UploadGammaRamp()`: `gamma_ = !identity` |
| `d3d12.cpp:2844`, `:2852` | `BlitFrame`, `float uv[4] = {0, 0, 1, 1};` |
| `d3d12.cpp:2928` | `LogPerf()` (LOGI); `:2964-2995` `Present` (`:2966-2970` capture, `:2993` `if (diag::Perf()) LogPerf()`) |
| `d3d12.cpp:3294-3297` | `SetGamma`: `memcpy` + `ramp_dirty_ = true`, ei suojaa mustalle rampille |
| `runtime\xdk\dsound.cpp:68-70`, `:80-82` | fallback-blokit (`StopEx FFFFFF0A`, `Pause FFFFFF0D`); käytetyt id:t `FFFFFF00…FFFFFF0E`, `FFFFFFFF` |
| `dsound.cpp:939` | `#define BUFFER(name, nargs)`; `:972-979` `BUFFER(Play, 4)` (`:973` `v.has_gain = false;`); `:985` `StopEx`; `:990` `Pause`. Ei `PlayEx`, ei `g_stat_plays` (grep 0) |
| `cmake\XbeRecomp.cmake:75` | `file(GLOB XBE_RUNTIME_SOURCES CONFIGURE_DEPENDS ${XBE_SDK_DIR}/runtime/*/*.cpp)` → uusi `iocp.cpp` tulee mukaan ilman cmake-muutosta |
| `XbeRecomp.cmake:85` / `:92` / `:93-104` / `:100` | `function(add_xbe_image …)` / `configs`-glob (`config/*.toml` + `xdk/*/*.toml`) / `add_custom_command` / `DEPENDS xbe_recomp ${configs} …` → `game.toml`-muutos käynnistää recompin |
| `runtime\kernel\boot.cpp:129`, `:133` | `LOGW("boot: kernel import %u (%s) has no implementation")`, `LOGI("boot: kernel imports resolved, %u missing")` |
| `runtime\cpu\dispatch.cpp:173-181` | puuttuva kernel-export → `LOGE` + `x86_trap` (`:248-256`: `LOGE`, `LogGuestStack`, `abort`) |
| `runtime\app\main.cpp:129-131` | `WM_KEYDOWN` + `VK_F3` → `DumpThreads()` |
| `runtime\app\app.h:77-98` | `GameInfo` (`.message` `:94`, `.ui` `:95`, `.pad` `:96`); **ei `picture_top`-kenttää** |
| `xdk\5558\layout.toml:15-16` | `CDevice_SuperSampleX = 0x95C`, `Y = 0x960`; `:20` `RenderTarget 0x1A04`; `:22` `BackBuffer 0x1A14`; `:39-40` `GammaRamps 0x1E04`, `GammaPending 0x2404` |

Pelikansio: `config\game.toml:11` `library = "SimsBustinOut.dll"`; `:14-15` `[functions] 0x2284DD = ""`; `:21` `SetRenderTargetFast = 0x246360`; `:65` `D3D_BlockOnResource = 0x24D6D0`; `:75` `BlockOnFence = 0x245460`; `:76` `BlockUntilVerticalBlank = 0x245530`; `:94` `InsertCallback = 0x24D0D0`; `:96` `IsFencePending = 0x24D0B0`; `:102` `Reset = 0x246030`; `:105` `SetGammaRamp = 0x244F60`; `:143` `SetRenderTarget = 0x2460E0`; `:163` `Swap = 0x249690`; `:167` `Direct3D_CreateDevice = 0x244460`; `:200` `IDirectSoundBuffer_PlayEx = 0x28E062`; `:268` `D3D_Device = 0x2649F0`. `generated\game\image_symbols.h:99-100` InsertCallback `HAS 1`, `:117-118` Reset, `:123-124` SetGammaRamp, `:201-202` SetRenderTargetFast, `:289-290` PlayEx `HAS 1`. `CMakePresets.json`: presetit `debug/release/relwithdebinfo`; `out\build\release\CMakeCache.txt:49` `CMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG`, `:52` `RELWITHDEBINFO=-O2 -DNDEBUG`. `kaanna-uudelleen.bat` kääntää vain `out\build\release`. `src\common\game_info.cpp`: `.message/.ui/.pad` kaikki `nullptr`.

### 0.3 Forkin tarvittavat muutokset valmiina patch-tiedostoina (tehty ja tarkistettu tässä sessiossa)

Kansio `C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad\sims-patches\`:

| Tiedosto | Lähde | `git -C C:\XboxRecomp\XboxXBE-Recomp apply --check` |
|---|---|---|
| `gamma-musta-ramppi-d3d12.patch` | `57c5e46` `runtime/gpu/d3d12.cpp` (+14) | **OK** |
| `gamma-pending-slotit-d3d8.patch` | `57c5e46` `runtime/xdk/d3d8.cpp` (+13/−4) | oletuskontekstilla **EI** (`patch failed: runtime/xdk/d3d8.cpp:503` — upstream lisäsi `Walk(Dev(kDevPush));` `:601` Presentin eteen); **`-C2` OK** ("Context reduced to (2/2) to apply fragment at 593") |
| `picture-top-rivit.patch` | `80fb58b` (`app.h` +5, `d3d12.cpp` +9/−1) | **OK** |
| `kernel-exportit.patch` | `git diff 65e959d origin/bustinout-port -- runtime/kernel/{iocp.cpp,files.cpp,kernel.h,objects.cpp}` (444 lisäystä, 8 poistoa) | **OK** |
| kaikki neljä yhdessä `-C2` | | **OK** |

Jos scratchpad on siivottu, patchit syntyvät uudelleen (huom. `--output=` **ennen** `--`-erotinta; sen jälkeen git tulkitsee sen poluksi):

```powershell
$R = 'C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad\refs\abilliontokens-XboxXBERecomp'
$P = 'C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad\sims-patches'
New-Item -ItemType Directory -Force $P | Out-Null
git -C $R show --output="$P\gamma-musta-ramppi-d3d12.patch" 57c5e46 -- runtime/gpu/d3d12.cpp
git -C $R show --output="$P\gamma-pending-slotit-d3d8.patch" 57c5e46 -- runtime/xdk/d3d8.cpp
git -C $R show --output="$P\picture-top-rivit.patch" 80fb58b
git -C $R diff --output="$P\kernel-exportit.patch" 65e959d origin/bustinout-port -- runtime/kernel/iocp.cpp runtime/kernel/files.cpp runtime/kernel/kernel.h runtime/kernel/objects.cpp
```

`apply --check` OK tarkoittaa, että hunkit osuvat; se **ei** todista kääntyvyyttä (ks. E.5).

---

## A. Diagnoosi

### A.1 Musta ruutu: syy oli `D3DDevice_InsertCallback` — vahvistettu Ajossa 4

Mekanismi (sdk-diff 0B, game-src 0.2/2.1, docs-render §3.3, upstream-diff §5 C): `config\game.toml:94` nimeää `D3DDevice_InsertCallback = 0x24D0D0` (USA `0x24D030` + 0xA0; token `0x81D8C` löytyy Europe-XBE:stä osoitteesta `0x24D0ED` = USA `0x24D04B` + 0xA0), `image_symbols.h:99-100` sanoo `HAS … 1`, mutta upstreamin `runtime\xdk\d3d8.cpp` ei toteuta funktiota (grep 0 osumaa) → recompiloitu guest-runko kirjoitti push-puskuriin tokenin `0x00081D8C`, jota hostin `Walk` ei tunne → callback ei koskaan ajanut → "an engine thread waiting on the semaphore that callback releases would idle-wait forever" (USA `src/game/hooks.cpp:39-43`) → ei yhtään `D3DDevice_Swap`ia → ikkunan `BLACK_BRUSH`-tausta (`main.cpp:271`), prosessi elossa, ei virheitä. Kun hook lisättiin (`src\game\hooks.cpp`), peli piirsi (TILANNE.md Ajo 4, `hook-12s.png`, `hook-55s.png`). Tämä sulkee pois gamma-hypoteesin (upstream-diff A) **tämän** mustan ruudun syynä: kuva näkyi 12 s ja 55 s kohdalla ilman gamma-suojaa.

Miksi loki oli mykkä: `-O3 -DNDEBUG` (`CMakeCache.txt:49`) → `log.h:18-22` `kVerbose = false` → `log.cpp:55` pudottaa `LOGI`/`LOGW`/`LOG_FIRST`-rivit, joihin kuuluvat `d3d: CreateDevice/Reset` (`d3d8.cpp:252`), `boot: kernel import … has no implementation` (`boot.cpp:129`) ja kaikki `perf:`-rivit (`d3d12.cpp:2928`, `:2993`). Siksi `XBE_PERF=1` ei tuottanut mitään. (game-src §5:n väite "upstream LOGI ei ole NDEBUG-portitettu" on virheellinen; neljä muuta raporttia ja `log.h`/`log.cpp` sanovat päinvastaista. game-src 0.2:n "ei `d3d: CreateDevice/Reset`-lokirivejä" on myös virheellinen: `d3d8.cpp:252` lokittaa ne `caller`-nimellä; Swap-lokia upstreamissa ei ole.)

### A.2 Nykyinen kuvavika: supersample 0x2222 ei ole mallinnettu upstreamissa

- Title luo laitteen `MultiSampleType = 0x2222` (`D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_GAUSSIAN`; USA `sub_128BD0` kohdassa `0x173F56`); **titlen oma** `sub_128BF0` palauttaa skaalan 2.0 kaikissa moodeissa paitsi 720p (`docs/movie-quad.md`); kirjaston render-target-setup `0x24AAC0` pitää `SuperSampleScale`n back bufferilla ja `0x249980` (`GetViewportOffsetAndScale`) laskee c-38/c-37 sillä. Pelin 2D kirjoittaa positiot 1280×896-pikseleissä (`c121.x = 1280`, `c122.y = 896`, `docs/movie-draw.md:22-93`), ja se koskee **kaikkea 2D:tä** (valikot, HUD, tekstit: `docs/guest-projection.md:199-274`).
- Upstream: `SetFrameBuffers` ei lue `pp+0x10`:tä (`:248-249`) ja kirjoittaa `kDevSuperX/Y = 1.0` (`:276-277`); `UpdateViewportConstants` (`:210-217`) ei tunne skaalaa; `state.h`:ssa ei `super_x`:ää; `FillSurfaceConstants` (`d3d12.cpp:2019`) antaa `surf = extent`. Tulos täsmälleen TILANNE.md:n oire: 2× suurennus, vasen yläneljännes (USA ennen korjausta 2.0× @ (111.06, 33.06) 50,6 dB, jälkeen 1.0× 52,0 dB, `docs/movie-draw.md:211-241`).
- Palkit: title piirtää 640×448-kuvan 640×480-puskuriin (`docs/picture-rect.md:50-71`; Europe-mode-taulu sama: `mov [eax],280h` `0x1295BE`, `mov [esi+248h],1E0h` `0x12964E`, `mov [esi+0Ch],1C0h` `0x129658`), ja upstreamin `BlitFrame` (`d3d12.cpp:2852` `uv = {0,0,1,1}`) näyttää koko framen → 16 mustaa riviä ylhäällä ja alhaalla; lisäksi 4:3-kirjekuori 16:9-ikkunassa. "Alareunan ~20 px jää piirtämättä": syy **ei tiedossa**; hypoteesi 16 riviä × render scale 1.5 ≈ 24 px, joka todennetaan suoraan `80fb58b`:llä (Vaihe 1d).

### A.3 Mitä upstreamista puuttuu ja mitä se tarkoittaa (tärkeysjärjestys nyt)

1. **SuperSample-malli** (A.2) — kuva väärän kokoinen. Pakollinen heti.
2. **`picture_top/picture_height`** (`80fb58b`) — palkit. Halpa, applikoituu puhtaasti.
3. **Gamma-suoja** (`57c5e46`): upstream `SetGamma` (`d3d12.cpp:3294`) soveltaa täysin mustan rampin, `UploadGammaRamp` (`:2822-2837`) asettaa `gamma_ = true` → LUT tekisi kaikesta mustaa; Swap tarkistaa vain slotin `SwapCount & 1` (`:595`). Lähettääkö tämä title nollarampin: **ei tiedossa** (`docs/boot-crash.md:99-100` vahvistaa vain suojan olemassaolon; Ajo 4 ei osoittanut mustaa 12/55 s kohdalla). Halpa vakuutus.
4. **6 kernel-exporttia** (76, 191, 223, 227, 232, 243): `tools/kernel_coverage.py` Europe-XBE:lle → 154/160; kutsu trappaa (`dispatch.cpp:173-181`, LOGE näkyy releasessakin) → ei vielä kutsuttu; tulee vastaan tiedosto-IO:ssa/tallennuksessa.
5. **`IDirectSoundBuffer_PlayEx`** (`game.toml:200` = `0x28E062`, `HAS 1`): upstreamissa ei HLE:tä → guest-runko ajetaan host-puskuriobjektia vasten (forkin `Pause`-kommentti: sisäinen osoitin null → fault). Ei vielä tapahtunut (ei `error:`-riviä).
6. Pelipuoli: PAL-tick `[0x2E3E18] = 1` → 0,02 s → peli kulkee 60 fps:ssä **1,2× liian nopeasti** ilman frame-step-hookia (game-src 0.4); D1 `0x66A10` (Simin vahvistus) puuttuu `[functions]`-taulusta; peli lukee vain Xbox-padia.

### A.4 Jos ruutu on taas musta: diagnostiikka ennen mitään muuta

Kolme koetta (win-diag §5), tulkinnat:

**Koe 1 — F3-kaappaus + guest-pinot release-buildilla (ei käännöstä).** `XBE_CAPTURE_DIR` ja F3 toimivat releasessa: kaappaus kirjoittaa tiedostot suoraan ja `LogGuestStack` käyttää `LOGE`-tasoa (`dispatch.cpp:232-245`). **Kaappaus vaatii vähintään kaksi `Present`iä F3:n jälkeen** (aloitus Presentissä N `d3d12.cpp:2969`, `final.bmp` Presentissä N+1 `:2967`).

```powershell
$dir = 'C:\XboxRecomp\SimsBustinOut-Recomp\out\build\release'
$env:XBE_PERF = '1'
$env:XBE_CAPTURE_DIR = 'C:\XboxRecomp\SimsBustinOut-Recomp\out\kaappaukset'
$p = Start-Process -FilePath "$dir\SimsBustinOut.exe" -WorkingDirectory $dir -PassThru -RedirectStandardError "$dir\logs\stderr.txt"
Start-Sleep -Seconds 15
Add-Type -Namespace W -Name U32 -MemberDefinition @'
[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
[DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
'@
$h = [W.U32]::FindWindowW('SimsBustinOut', $null)
if ($h -eq [IntPtr]::Zero) { throw 'ikkunaa ei loydy' }
[W.U32]::PostMessageW($h, 0x0100, [IntPtr]0x72, [IntPtr]0x003D0001)   # WM_KEYDOWN VK_F3 -> main.cpp:129-131
Start-Sleep -Seconds 3
Get-ChildItem -Recurse $env:XBE_CAPTURE_DIR | Select-Object FullName, Length
$ft = Get-ChildItem -Recurse -Filter frame.txt $env:XBE_CAPTURE_DIR | Sort-Object LastWriteTime | Select-Object -Last 1
if ($ft) { Get-Content $ft.FullName -TotalCount 80 }
Get-Content "$dir\logs\simsbustinout.log" | Select-String 'eax |\[esp\+|perf:|d3d|error' | Select-Object -First 80
```

| Tulos | Merkitys |
|---|---|
| Ei hakemistoa `out\kaappaukset\HHMMSS` | ei yhtään `Present`iä → guest ei kutsu `D3DDevice_Swap`ia. Lue lokin `error:   [esp+XXX] 00XXXXXX`-rivit ja vertaa `game.toml`:n osoitteisiin: paluuosoite heti `0x245530` (BlockUntilVerticalBlank), `0x24D6D0` (BlockOnResource), `0x245460` (BlockOnFence), `0x24D0B0` (IsFencePending) tai `0x249690` (Swap) perästä kertoo, missä säie odottaa |
| Hakemisto, mutta `frame.txt` 0 tavua eikä `final.bmp` | täsmälleen yksi Present F3:n jälkeen → odota ja paina F3 uudelleen |
| `frame.txt` ilman `draw`-rivejä, `final.bmp` musta | guest swappaa muttei piirrä |
| `draw rt <data>`-rivejä, `NNNN_<data>.bmp` sisältää kuvaa, `final.bmp` musta | piirrot muuhun pintaan kuin etupuskuriin / host esittää väärän kohteen |
| `final.bmp` sisältää kuvaa, ruutu musta | host-puoli: `BlitFrame`/swapchain/TDR → `Get-WinEvent -FilterHashtable @{LogName='System'; Id=4101} -MaxEvents 5` ja `XBE_D3D12_DEBUG` (alla) |

**Koe 2 — prosessin tila (30 s):**
```powershell
$a = (Get-Process SimsBustinOut).TotalProcessorTime; Start-Sleep 10
$b = (Get-Process SimsBustinOut).TotalProcessorTime; ($b - $a).TotalSeconds
(Get-Process SimsBustinOut).Threads | Group-Object WaitReason | Select-Object Count, Name
```
~0 s CPU / 10 s → kaikki guest-säikeet odotuksessa (InsertCallback-tyyppinen deadlock); ~10 s → yksi ydin pyörii (spin: `IsFencePending`/`IsBusy`).

**Koe 3 — verbose-build + `XBE_PERF`:** Vaihe 2. Tulkinta: `perf: per frame N draws` → kehyksiä tulee; ei perf-rivejä → ei Swapia; `N = 0` → ei piirtoja; `N > 0` ja musta → host-puoli (koe 1:n BMP:t).

`XBE_D3D12_DEBUG`: `d3d12.cpp:756-758` kytkee vain `EnableDebugLayer()`, ei `ID3D12InfoQueue`a → validointiviestit näkyvät **vain** debuggerissa tai Sysinternals DebugView'ssa (Capture Win32 päällä ennen pelin käynnistystä), ja vaativat Windowsin Graphics Tools -ominaisuuden: `Get-WindowsCapability -Online -Name 'Tools.Graphics.DirectX*'`; jos `NotPresent` (admin): `Add-WindowsCapability -Online -Name 'Tools.Graphics.DirectX~~~~0.0.1.0'`. Ilman sitä kutsu epäonnistuu hiljaa.

### A.5 Ei tiedossa

Lähettääkö title nollarampin; onko `SetGammaRamp` inlinettu jonnekin (pending-slot-pariteetti); milloin kernel-exportteja 76/191/223/227/232/243 kutsutaan; milloin `PlayEx`iä; alareunan ~20 px:n syy; kuinka D3D12-supersample-aritmetiikka käyttäytyy (forkin `d3d12.cpp`-osaa ei ole koskaan ajettu, `docs/movie-draw.md:159-166`).

---

## B. Päätös ja perustelu

| Vaihtoehto | Työmäärä | Riski | Arvio |
|---|---|---|---|
| **(1) Siirrä forkin tarvittavat hunkit upstreamiin** | SDK: 4 patchia (apply-check OK, §0.3) + supersample käsin (~35 riviä kolmessa tiedostossa) + PlayEx (~10 riviä) + Swap-LOGR (1 rivi) ≈ 1–2 h; pelipuoli (hooks.cpp, input_kbm, game_info.cpp, game.toml) ≈ 1–2 h; käännökset 25 s–2 min | **Pieni–keskitaso.** Jokainen askel on erikseen käännettävä ja ajettava, git-palautettava. Avoin: D3D12-supersample ajetaan ensimmäistä kertaa (sama riski kaikilla poluilla); kernel-patchin ensimmäinen natiivikäännös | **Valitaan** |
| (2) Vaihda SDK forkkiin (`sdk/`-kansio pelikansioon, `CMakeLists.txt:10` valitsee sen ensin) | Rakenne: forkin `XbeRecomp.cmake` on Linux/cross-painotteinen (ei DLL-mallia → `game.toml:11` `library` ohitetaan, OK; forkin `xbe_symbols` ei tunne `--library`-optiota → `build.bat` kaatuu, jos config pitää tehdä uudelleen); ImGui 1.91.9b:n vanhentunut `ImGui_ImplDX12_Init` (`docs/windows-target.md:461-491`, `d3d12.cpp:903`); forkin `Constant1Fast` (`185057b`, `HLE_FASTCALL(…,1)`) on **väärä 5558:lle** (ret 0, yksi vakio; poppaa 4 ylimääräistä tavua, upstream-diff F) → peruttava käsin. ≈ 0,5 pv kääntyväksi + tuntematon debug | **Keskitaso–korkea.** Windows-exeä ei ole koskaan ajettu natiivisti (vain Wine/vkd3d Create-a-Simiin asti); menettää 34 upstream-committia (DLL-malli `4256397`, dsound-uudelleenkirjoitus `1bb872b`, `31d9be8`, `09f0a37`, `c3d96da`); tulevat upstream-päivitykset = 29 konfliktilohkoa (`merge-tree`: `d3d8.cpp` 11/262 r.) + `xdk/5849/signatures.toml` duplikaattiavain | Vain nopeaksi A/B-kokeeksi erillisessä pelikansiokopiossa, jos (1):n supersample ei tuota oikeaa kuvaa |
| (3) Hybridi | = (1) SDK:lle + USA:n `src/` pelipuolelle; tämä on käytännössä valittu polku | | Sama kuin (1) |

Perustelu: Simsille olennainen forkkiosuus on pieni ja rajattu (supersample, gamma, picture_top, 6 kernel-exporttia, PlayEx); ~70 % forkin +7865 rivistä on Vulkan/SDL/Linux-koodia. Upstreamissa on jo ekvivalentit `D3DDevice_Reset`+puskurien uudelleenkäyttö (`31d9be8` ≈ `583d612`), inline-vakiopushit `0x1EA4`/`0xB80` (`ApplyConstant` ≈ `93d67ae`/`185057b`), `Pause`. Ei siirretä: dirty-merkintä `9844a28` (erittäin korkea konflikti), Hor+/aspect, HDR, `uvrect` (konflikti `shaders.cpp` `TexUv`/`rtshift`), capture-probe, perf-jako, cmake, Linux-tiedostot, forkin `Constant1Fast`.

---

## C. Askel askeleelta (valittu polku 1)

Kaikki PowerShell 5.1 -muodossa (ei `&&`). `$S = 'C:\XboxRecomp\XboxXBE-Recomp'`, `$G = 'C:\XboxRecomp\SimsBustinOut-Recomp'`, `$R` ja `$P` kuten §0.3, `$U = 'C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad\refs\bustinout-recomp'` (USA-projekti).

### Vaihe 0 — git-pohja (5 min)

```powershell
git -C $S checkout -b sims-5558
git -C $S add xdk/5558
git -C $S commit -m "xdk: 5558 layout and signatures (abilliontokens bustinout-port 7f2e294)"
git -C $G add -A
git -C $G commit -m "Ajo 4: InsertCallback-hook, 5849-loytamat symbolit"
```

Valinnainen, vain selailua (`git log`/`show` SDK:n sisältä) varten — **korjattu muoto**, koska refs-repossa ei ole paikallista `bustinout-port`-haaraa ja `fork-sdk` on detached HEAD:
```powershell
git -C $S remote add fork $R
git -C $S fetch fork refs/remotes/origin/bustinout-port:refs/remotes/fork/bustinout-port
git -C $S log -1 --format="%h %s" fork/bustinout-port      # odotus: adc40ab build: cross-compile the Windows target …
```
(tai `git -C $S remote add fork https://github.com/abilliontokens/XboxXBERecomp; git -C $S fetch fork bustinout-port`). Patchien applikointi ei tarvitse tätä.

### Vaihe 1 — SuperSample-malli + Swap-laskuri + picture_top → oikea kuva

**1a `runtime\gpu\state.h`** — rivin `:187` (`float zscale = 16777215.0f;`) perään, ennen `};` (`:188`):
```cpp
    // SuperSampleScale: render target pixels per pixel of the title's frame on a supersampled back buffer, 1 elsewhere
    float super_x = 1, super_y = 1;
```
Kulkee render-säikeelle automaattisesti (`render_thread.cpp:79` viimeinen alue `material`→loppu).

**1b `runtime\xdk\d3d8.cpp`** — neljä muutosta:

(i) `UpdateViewportConstants()`: korvaa rivit **`:213-217`** (rivi 213 `float hw = …`, rivit 214-217 `if (!g_own_constants) { … }` sulkevaan `}`:hen asti) näillä viidellä rivillä (fork `fork-sdk\runtime\xdk\d3d8.cpp:361-369` ilman Hor+-termejä `FrameFoldScale()`/`FrameCentreShift()` ja ilman forkin `Set()`-merkintää; kaava = kirjaston `0x249980`: `offset = X*SSX + ScreenSpaceOffsetX + W*SSX/2`, `scale = W*SSX/2`):
```cpp
    float hw = v.w * 0.5f, hh = v.h * 0.5f, sx = g_state.super_x, sy = g_state.super_y;
    if (!g_own_constants) {
        g_state.vsc[58] = {hw * sx, -hh * sy, (v.max_z - v.min_z) * g_state.zscale, 0};
        g_state.vsc[59] = {v.x * sx + g_state.ss_offset_x + hw * sx, v.y * sy + g_state.ss_offset_y + hh * sy, v.min_z * g_state.zscale, 0};
    }
```

(ii) Uudet funktiot ennen `SetFrameBuffers`ia (esim. rivin `:243` jälkeen, ennen `:245`:n kommenttia; `Dev/SetDev/kDev*` on määritelty `:101-103`, joten näkyvät). Fork `fork-sdk\runtime\xdk\d3d8.cpp:286-305`, `Set(g_state.…)` → suora sijoitus:
```cpp
uint32_t g_super_x = 1, g_super_y = 1;  // the presentation's, on its back buffers

// only a supersampling type (bit 0x2000) scales: x in bits 4..7, y in bits 0..3. 0x2222 -> 2x2; 0x11, 0 and 0x1xxx -> 1
void SetMultiSampleType(uint32_t type) {
    bool super = type & 0x2000;
    g_super_x = super ? std::max(1u, type >> 4 & 0xF) : 1;
    g_super_y = super ? std::max(1u, type & 0xF) : 1;
}

// SuperSampleScale as the library's render target setup (5558: 0x24AAC0) keeps it: the presentation's when the
// target is the device's back buffer, 1 on any other surface. Into the device (0x95C/0x960) and into g_state.
void UpdateSuperSample() {
    bool back = Dev(kDevRenderTarget) == Dev(kDevBackBuffer);
    float x = back ? float(g_super_x) : 1.0f, y = back ? float(g_super_y) : 1.0f;
    std::memcpy(HostPtr(kDevice + kDevSuperX), &x, 4);
    std::memcpy(HostPtr(kDevice + kDevSuperY), &y, 4);
    g_state.super_x = x;
    g_state.super_y = y;
}
```
`SetFrameBuffers` (`:247`): rivin `:249` perään `uint32_t multisample = R32(pp + 0x10); SetMultiSampleType(multisample);`; muuta `:252-253`:n `LOGI` muotoon `"d3d: %s %ux%u format %X buffers %u multisample %X depth %u format %X interval %X flags %X"` ja lisää `multisample` argumentiksi `count`:n perään; korvaa rivit **`:276-277`** (kaksi `memcpy(… kDevSuperX/Y …, &one, 4)`) yhdellä rivillä `UpdateSuperSample();`. Rivi `:275` (`float one = …, zs = …`) jää, koska `one` käytetään `:281-282` ja `zs` `:278`. Järjestys on oikea: `kDevRenderTarget` (`:271`) ja `kDevBackBuffer` (`:273`) on asetettu ennen vertailua, ja `g_state.super_x/y` ovat valmiit ennen `UpdateViewportConstants()`ia (`:287`).

(iii) `ApplyRenderTarget()` (`:522-533`): rivin `:532` (`g_state.zs = SurfaceOf(zs);`) perään `UpdateSuperSample();`. Kattaa `SetRenderTarget` (`:536`, joka kutsuu `UpdateViewportConstants()` `:545`), `SetRenderTargetFast` (`:549-553`) ja `Reset` (`:630`, jota seuraa `SetFrameBuffers`). **Huom. Fast-polku:** `:549-553` ei kutsu `UpdateViewportConstants()`ia, joten `vsc[58]/[59]` jäävät edellisen targetin skaalaan, kunnes title kutsuu `SetViewport`ia (sama kuin konsolilla, jossa vakiot päivittyvät vasta `SetViewport`illa). Älä lisää mitään ensin; jos render-to-texture-vaiheet (views) näyttävät 2×/1× väärin, kokeile (a) `:551`:n perään `UpdateViewportConstants();` tai (b) poista `D3DDevice_SetRenderTargetFast`-rivi `game.toml:21`:stä testiksi (ks. E.2).

(iv) `D3DDevice_Swap`: rivin **`:614`** (`SetDev(kDevSwapCount, n);`) perään yksi rivi, joka näkyy releasessakin (`g_lock` on hallussa, ei tarvitse atomia):
```cpp
    if (n <= 8) LOGR("d3d: Swap %u", n);
```

**1c `runtime\gpu\d3d12.cpp`** — `:2019-2021`:
```cpp
// a vertex program's screen space positions are the render target's pixels, SuperSampleScale of them to each of the
// frame's on a supersampled back buffer; fixed function and pretransformed positions stay in the frame's
void FillSurfaceConstants(const State& s, GuestExtent size, bool vertex_program, DrawConstants& c) {
    c.surf[0] = float(size.w) * (vertex_program ? s.super_x : 1.0f);
    c.surf[1] = float(size.h) * (vertex_program ? s.super_y : 1.0f);
```
ja kutsu `:2190` → `FillSurfaceConstants(s, size, sh.prog, c);` (`sh.prog` on jo käytössä `:2193` `FillFogConstants(s, sh.prog, c)`). Upstreamin `:2192` `surf[3] = TargetScale()` pisteille jää. Tarkista `Select-String -Path "$S\runtime\gpu\d3d12.cpp" -Pattern 'FillSurfaceConstants\('` → täsmälleen 2 osumaa (määrittely + kutsu).

**1d picture_top/height** (fork `80fb58b`, applikoituu puhtaasti):
```powershell
git -C $S apply "$P\picture-top-rivit.patch"
```
→ `app.h` saa `uint32_t picture_top = 0, picture_height = 0;` (`GameInfo`n loppuun), `BlitFrame` (`d3d12.cpp:2852`) ottaa `uv = {0, v0, 1, dv}`. Pelikansio `src\common\game_info.cpp`: `.rumble = nullptr,`-rivin perään `.picture_top = 16, .picture_height = 448,` (USA `game_info.cpp:182-183`, perustelu `docs/picture-rect.md`; Europe-mode-taulu A.2). Ilman SDK-patchia nämä kaksi riviä eivät käänny.

**Käännös:** `kaanna-uudelleen.bat` (`cmake --build out\build\release`; SDK-lähteet ovat `XbeRecomp.cmake:75`:n globissa, `game_info.cpp` `CMakeLists.txt:24`:n `src/*/*.cpp`-globissa; `game.toml` ei muuttunut → ei recompia; ~25 s TILANNE.md:n mukaan).

**Tarkistukset ajon jälkeen** (`out\build\release\SimsBustinOut.exe`, loki `logs\simsbustinout.log`):
1. `d3d: Swap 1 … 8` LOGR-riveinä ensimmäisen sekunnin sisällä (USA-vertailu: `Swap 1` 0,18 s). Puuttuu → ei Presentiä → A.4.
2. Kuva 12 s: intro 1.0×, keskitetty, 4:3 kirjekuori 16:9-ikkunassa, ei 16 rivin palkkeja ylä-/alareunassa. Jos yhä 2× → (i)/(ii)/(iii) ei ajettu: lisää väliaikaisesti `LOGR("d3d: %s multisample %X super %ux%u", caller, multisample, g_super_x, g_super_y);` `SetFrameBuffers`iin; odotus `Reset … multisample 2222 super 2x2`.
3. 55 s: The Sims -logo/valikko oikean kokoinen (sama c121..c124-polku kuin intro).
4. Alareunan ~20 px: jos 1d poisti sen, hypoteesi (16 riviä × 1.5) vahvistui; jos ei, syy on muu (ei tiedossa) → F3-kaappaus Vaihe 2.
5. Tarkka todennus (valinnainen): F3-kaappaus (A.4, kaksi Presentiä) → `frame.txt`:n introdraw `draw rt … count 4` + `vs_*.hlsl`: odotetut lukuarvot `surf (1280, 960)` ja `c121.w = 1.0625` (`docs/movie-draw.md:168-183`); USA:n `c121.w = 111.0625` sisälsi Vulkan-Hor+-siirron 110, jota upstreamissa ei ole.

Jos kuva on oikein, committaa: `git -C $S commit -am "d3d: SuperSampleScale model for supersampled back buffers (from abilliontokens bustinout-port); present only the picture rows (80fb58b); Swap counter"`.

### Vaihe 2 — Diagnostiikkabuild ilman NDEBUG:ia (käytä, kun jokin tarkistus ei täsmää)

Presetit `release` ja `relwithdebinfo` sisältävät molemmat `-DNDEBUG`:n (`CMakeCache.txt:49`, `:52`). Erillinen hakemisto, sama ympäristö kuin `kaanna-uudelleen.bat` (aja cmd:ssä tai tee näistä `kaanna-diagnostiikka.bat` — oma päätöksesi, Rule 3):
```bat
set "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
call "%VSDIR%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul
set "VSCMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VSCMAKE%\CMake\bin;%VSCMAKE%\Ninja;%VSDIR%\VC\Tools\Llvm\x64\bin;%PATH%"
cd /d C:\XboxRecomp\SimsBustinOut-Recomp
cmake -S . -B out\build\diagnostiikka -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 -DCMAKE_C_FLAGS_RELEASE=-O2 -DCMAKE_CXX_FLAGS_RELEASE=-O2
cmake --build out\build\diagnostiikka
copy /y out\build\release\settings.toml out\build\diagnostiikka\
```
(`generated\game` on yhteinen molemmille hakemistoille — älä käännä niitä samanaikaisesti.) Ajo: `$env:XBE_PERF='1'` + `Start-Process … -RedirectStandardError "$dir\logs\stderr.txt"`. Verbose-loki on `logs\simsbustinout_NNN.log` (`log.cpp:46-49`) ja Info-rivit puskuroituvat 4 kt (`log.cpp:70-73`, fflush vain ≥ Warn) → lue `stderr.txt`, se on reaaliaikainen.

Odotetut rivit ja päätös: `boot: kernel imports resolved, 6 missing` + 6 kpl `boot: kernel import 76/191/223/227/232/243 … has no implementation` (odotettua ennen Vaihetta 4); `d3d: CreateDevice 640x480 format 1E buffers 1 multisample 2222 depth 1 format 2E interval 2 flags 130` (Vaihe 1:n muokattu loki); `d3d12: 640x480 guest, 1280x720 window, render scale 1.500`; `d3d: Reset … multisample 2222` (USA 0,28 s); `d3d: vertex shader … 11 instructions` (3,5 s); `d3d: MakeSpace from …`; `open D:\MODELS.ARC`, `open D:\RLETEXTU.ARC` (4,5 s); `open d:\sims_intro.xmv` + `dsound: stream #2 … rate=48000` (10,5 s); `perf: … fps … per frame N draws (M raw) …` joka sekunti (intro 1 draw/frame, valikko kymmeniä, talous ~2076–2178; `docs/playtest.md:28-31`). F3 antaa nyt myös `thread %X`-otsikot pinojen eteen. TDR-epäily: `Get-WinEvent -FilterHashtable @{LogName='System'; Id=4101} -MaxEvents 5`; D3D12-validointi: A.4:n `XBE_D3D12_DEBUG` + DebugView.

### Vaihe 3 — Pelipuolen pakolliset (ennen pelattavuustestiä)

3a `config\game.toml` `[functions]` (`:14-15`): lisää `0x66A10 = ""  # D1: Simin vahvistus-callback (USA 0x66980 + 0x90)` (game-src 2.9: `mov [edi+60h],0x66A10` kohdassa `0x69731`, runko `0x66A10..0x66F16`). Muutos käynnistää recompin (`XbeRecomp.cmake:92/:100`, ~48 s).

3b `src\game\hooks.cpp` ← USA `hooks.cpp` kokonaan (InsertCallback-runko on sama kuin nykyinen; Linux-lohko `:173-214` on `#if !defined(_WIN32)`), substituutiot (game-src 2.2, mitatut osoitteet):
```powershell
$src = Get-Content "$U\src\game\hooks.cpp" -Raw
$src = $src -replace 'sub_001232F0','sub_00123400' -replace 'sub_00123430','sub_00123540' -replace '0x35D218u','0x35D2B8u' -replace '0x30AAD0u','0x30AB70u' -replace '0x35D230u','0x35D2D0u' -replace 'X86_FUNC\(__imp__','X86_EXTERN_FUNC(__imp__'
Set-Content -Encoding utf8 "$G\src\game\hooks.cpp" $src
```
Perusteet: `sub_00123430` → `0x123540` (vtable `0x2EED28+0x18` + 0xA0 → thunk `0x165F40` `E9 FB D5 FB FF`), `sub_001232F0` → `0x123400` (`call` kohdassa `+0x77`), data `0x35D218→0x35D2B8`, `0x30AAD0→0x30AB70`, `0x35D230→0x35D2D0` (`.data` +0xA0, koodista varmistettu). Upstreamin DLL-malli vaatii `X86_EXTERN_FUNC(__imp__sub_…)` (`x86_ops.h:44-45`, `templates\game\src\game\hooks.cpp:5-8`). PAL: `kMaxTicks = 250` (0,25 s) on hieman tiukempi kuin PAL-moottorin oma katto 15 × 0,02 = 0,30 s → halutessasi `kMaxTicks = 300`. Bisect-kytkin `SIMSBO_NO_STEP=1`.

3c Näppäimistö/hiiri → pad 0 ja skriptattu pad: kopioi `$U\src\common\input_kbm.cpp` ja `.h` → `$G\src\common\` (ei osoitteita; `input::Pad` identtinen `runtime\core\input.h:7-12`). `game_info.cpp`: ota USA:n versio (`StepPad` `:31-97`, `AutostartPad` `:99-142`, `.message = &kbm::Message`, `.pad = &PadHook`) ja muuta: `.log = "simsbustinout"` (skriptit lukevat `simsbustinout.log`), `.name` jää `L"SimsBustinOut"`, `#include "common/overlay.h"` pois ja `.ui = nullptr` (overlay vaatii karsinnan, Vaihe 9), `.picture_top/height` 16/448 vain jos 1d tehty. Näppäinkartta `input_kbm.cpp:10-26`: WASD = vasen tikku, nuolet = D-pad, Space = A, Esc/Backspace = B, Enter = Start.

Käännös `kaanna-uudelleen.bat`. Tarkistus: Enter ohittaa intron; valikko reagoi; peli ei kulje 1,2× (vertaa `SIMSBO_NO_STEP=1`-ajoon).

### Vaihe 4 — Kernel-exportit (6 kpl)

```powershell
git -C $S apply "$P\kernel-exportit.patch"
git -C $S status --short     # M runtime/kernel/files.cpp, kernel.h, objects.cpp; ?? runtime/kernel/iocp.cpp
```
Sisältö: `iocp.cpp` (uusi, 183 r.: `NtCreateIoCompletion` 191, `NtSetIoCompletion` 227, `NtRemoveIoCompletion` 223, `NtUserIoApcDispatcher` 232, `PostIoCompletion/NewIoCompletion`), `files.cpp` +224 (`IoQueryVolumeInformation` 76, `ObOpenObjectByName` 243, `NtSetInformationFile` luokka 30, `Finish()` postaa porttiin, `File::completion_port/key`), `kernel.h` +5 (`kInvalidInfoClass`, prototyypit), `objects.cpp` +40/−8 (`FromPointer` tyyppi 4 → `ObjType::Queue`, `FindNamed` case-insensitive, `ToDeadline`-saturointi, `TimerLoop` `wait_for`-katto; `<cctype>`). `iocp.cpp` tulee mukaan `XbeRecomp.cmake:75`:n `CONFIGURE_DEPENDS`-globilla (ninja ajaa cmaken uudelleen). Käännös `kaanna-uudelleen.bat`. Tarkistus: kääntyy (ensimmäinen natiivi clang/MSVC-STL-käännös, E.5); diagnostiikkabuildissa `boot: kernel imports resolved, 0 missing`; myöhemmin ei `kernel export N (…) is not implemented`-riviä (LOGE, näkyy releasessa).

### Vaihe 5 — `IDirectSoundBuffer_PlayEx`

Upstreamin `runtime\xdk\dsound.cpp`: (a) fallback-blokki `:80-82`:n (`Pause`) perään — **ei** forkin `FFFFFF0D`, joka on upstreamissa jo `Pause`n:
```cpp
#ifndef SYM_IDirectSoundBuffer_PlayEx
#define SYM_IDirectSoundBuffer_PlayEx FFFFFF0F
#define HAS_IDirectSoundBuffer_PlayEx 0
#endif
```
(Europe: `image_symbols.h:289-290` antaa `0028E062`/`HAS 1`, joten fallbackia ei käytetä, mutta sen on oltava uniikki samassa TU:ssa, koska `HLE_STDCALL` määrittelee `sub_<SYM>`-funktion, `hle.h:100-114`.) (b) `Play`n (`:972-979`) perään **upstreamin Play-runko** nimellä PlayEx, ei forkin riviä riviltä (forkin `:670-677` sisältää `g_stat_plays++`, jota upstreamissa ei ole, eikä `v.has_gain = false;`):
```cpp
// (this, reference time low, reference time high, flags): Play with a scheduled start. The game passes time 0
// (play now); a future time plays now too, the scheduling offset is not reproduced. Flags share Play's low bits.
BUFFER(PlayEx, 4) {
    v.has_gain = false;
    v.playing = true;
    v.looping = a[3] & 1;
    if (a[3] & 2) v.pos = 0;
    v.sync = a[3] & 4;
    return 0;
}
```
Käännös. Tarkistus: ei `read guest`/`exception`-riviä äänten alkaessa; äänet kuuluvat (kaiku/efektit puuttuvat odotetusti, E.4).

### Vaihe 6 — Gamma-suoja (`57c5e46`)

```powershell
git -C $S apply "$P\gamma-musta-ramppi-d3d12.patch"
git -C $S apply -C2 "$P\gamma-pending-slotit-d3d8.patch"
```
d3d12-osa: `SetGamma` (`:3294`) korvaa täysin mustan rampin identiteetillä + `LOG_FIRST(4, "d3d12: all black gamma ramp ignored …")`. d3d8-osa: Swap (`:594-600`) käy molemmat `GammaPending`-slotit (tämän swapin viimeisenä); `-C2` tarvitaan, koska upstreamin `:601` `Walk(Dev(kDevPush));` rikkoo kolmirivisen kontekstin. Jos haluat nähdä releasessa, lähettääkö title mustan rampin, vaihda patchin `LOG_FIRST` → `LOGR("d3d12: all black gamma ramp ignored")`. Käännös. Tarkistus: introvideon ja sen jälkeisen vaiheen aikana ei mustaa; lokissa mahdollinen `all black gamma ramp`-rivi kertoo, että suoja oli tarpeen.

### Vaihe 7 — picture_top (`80fb58b`)

Tehty jo Vaiheessa 1d. Jos jätit sen pois: `git -C $S apply "$P\picture-top-rivit.patch"` + `game_info.cpp` `.picture_top = 16, .picture_height = 448`.

### Vaihe 8 — Pelattavuustesti (`SIMSBO_STEP`, ei `SIMSBO_AUTOSTART`)

`SIMSBO_AUTOSTART` antoi USA:ssa **väärän hyväksynnän**: se ei koskaan vahvista Simiä Create-a-Simissä (`docs/playtest.md:80-85`, `docs/status.md:368-370`), joten D1-korjaus (`0x66A10`) ja talon latautuminen todennetaan vain ohjatulla kävelyllä (`docs/black-scene.md:274-279`):
```powershell
$dir = "$G\out\build\release"
$env:SIMSBO_STEP = "$G\out\askel.txt"; $env:XBE_PERF = '1'
Set-Content -Encoding ascii $env:SIMSBO_STEP 'NONE'
$p = Start-Process -FilePath "$dir\SimsBustinOut.exe" -WorkingDirectory $dir -PassThru -RedirectStandardError "$dir\logs\stderr.txt"
Start-Sleep 14
foreach ($t in 'START','START','A','A','RIGHTx3','A','A') { Set-Content -Encoding ascii $env:SIMSBO_STEP $t; Start-Sleep 2; Set-Content -Encoding ascii $env:SIMSBO_STEP 'NONE'; Start-Sleep 2 }
```
Talous nousee ~30 s myöhemmin. Lokista: ei `indirect call to 00066A10 has no recompiled function` (D1 korjattu); ei `kernel export … not implemented`; diagnostiikkabuildissa `perf: per frame ~2076 draws`. Jos uusi `indirect call … has no recompiled function` ilmenee, `korjaa-puuttuvat-funktiot.ps1` lisää sen `[functions]`-tauluun automaattisesti. Jos kaatuminen on "pieni offset NULL:sta, `ecx/edi = D3D_Device+0x1C28` (= `0x266618`)", syy on nimeämätön XDK-funktio (`docs/boot-crash.md:144-151`); Europe-ehdokkaat laskennallisesti `0x2441C0`, `0x247710`, `0x2481E0`, `0x248B70`, `0x24D080`/`0x24D270` (USA + 0xA0, ei varmennettu).

### Vaihe 9 — Valinnaiset, myöhemmin

`runtime\app\main.cpp:247` `wc.hCursor = LoadCursorW(nullptr, IDC_ARROW)` (konflikti nolla); `d3d12.cpp` `CopyByRegion` src_scale (`583d612`, vain eri render-skaaloilla); `SkippedDraw`/texture-lokit (`4824050`/`700bb6a`, keskitason konflikti `Draw()`:ssa); cheatit (`src\game\cheats.cpp` + `src\common\cheats.h`: `sub_00012280→sub_000122C0`, `kEnterCode 0x72010→0x720A0`, `kOptions 0x310B98→0x310C38`, `kLatch 0x3DCF70→0x3DD010`, `kPadManager 0x35D3F0→0x35D490`, `X86_EXTERN_FUNC(__imp__sub_000122C0)`) — vaatii overlayn; overlay F1/F2 karsittuna (game-src 2.6: poista `aspect/ultrawide/hdr`-kentät `:58-66`, `kAspects :81`, `kHdrKeys :84-85`, `BackendWiden() :201-208 → 1.0f`, `:263-268`, `:288-289`, `:308-316`, `:339-348`, `:410-421`, `:439-482`; lisää `kFpsCapRefresh = 1` `kFpsCaps`-listaan). Ei siirretä: `9844a28` dirty-merkintä, Hor+/aspect (`5bbdb3d`, `c36cd6f`, `11e6475`), HDR (`b3b8b9d`), `uvrect`, capture-probe, perf-jako, `cmake/XbeRecomp.cmake`, `game_files.cpp`, Linux-tiedostot, forkin `Constant1Fast`.

---

## D. Pelikohtainen koodi — mitä tuodaan heti, mitä myöhemmin, ja osoitteiden muunnos

### D.1 Heti (pakolliset)

| Mitä | Lähde (USA) | Europe-osoitteet | Tila |
|---|---|---|---|
| `D3DDevice_InsertCallback`-HLE | `src/game/hooks.cpp:48-52` | `0x24D0D0` (config `:94`), osoiteriippumaton (sidotaan nimellä) | **tehty** (Ajo 4) |
| D1 entry point | `config/game.toml:15` `0x66980` | `0x66A10 = ""` (+0x90) | Vaihe 3a |
| Frame-step `sub_001232F0` + tick `sub_00123430` | `hooks.cpp:54-171` | `sub_00123400`, `sub_00123540`; data `0x35D2B8`, `0x30AB70`, `0x35D2D0` | Vaihe 3b — PAL 1,2×-nopeus ilman tätä |
| Näppäimistö/hiiri → pad | `src/common/input_kbm.cpp/.h` | ei osoitteita | Vaihe 3c |
| `SIMSBO_STEP`/`AUTOSTART` | `game_info.cpp:17-149` | ei osoitteita | Vaihe 3c |
| `picture_top = 16, picture_height = 448` | `game_info.cpp:182-183` + SDK `80fb58b` | Europe-mode-taulu `0x1295BE/0x1295CE/0x12964E/0x129658` (+0x110) | Vaihe 1d |

### D.2 Myöhemmin

Cheatit + overlay (Vaihe 9); aspect-getter `sub_001287C0` → `0x1288D0` (vtable `0x2ECB08+0xA0`) **vain Linux** (`#if !defined(_WIN32)`, `docs/ultrawide.md:1-12` kieltää `SIMSBO_HORPLUS`:n) — ei siirretä; `kProjectionReturns`-paluuosoitteita ei haettu (alue +0x40…+0x110, tarvittaisiin `call [reg+0x90]`-kuviohaku).

### D.3 Osoitteiden muunnos Europe-XBE:lle (mitattu `assets\default.xbe`:stä, sha1 `f7b7c864…`)

**Sääntö 1 — kaikki muut sektiot kuin `.text` = USA + 0xA0:** `D3D` `0x243D00→0x243DA0`, `XGRPH`, `DSOUND`, `XMV`, `XPP`, `.rdata` `0x2E1C00→0x2E1CA0`, **`.data` `0x3083A0→0x308440`**, `DOLBY`. Koskee kaikkia `[symbols]`/`[data]`-osoitteita ja data-vakioita (koodista varmistettu: `cvtsi2ss [0x30AB70]; mulss [0x35D2B8]`, `cmp byte [0x3DD010],0`, `mov ecx,0x310C38`, `mov [0x35D240],esi`).

**Sääntö 2 — `.text`: ei vakiosiirtymää**, portaat: `0x12280→0x122C0` (+0x40); `0x66980→0x66A10`, `0x72010→0x720A0` (+0x90); `0xF9AC0→0xF9B70` (+0xB0); entry `0x100EA2→0x100F8C` (+0xEA); **kaikki ≥ `0x1217B0` +0x110** (`0x123400`, `0x123540`, `0x1288D0`, `0x1294D0`, `0x165F40`, `0x2284DD`, `0x22AD81`). Kolme luotettavaa reittiä: (1) vtable-slotti `.rdata`:ssa: lue dword Europe-XBE:stä osoitteesta USA-slotti + 0xA0; (2) immediate-vakio docsien disassemblystä (ei-osoitteet samoina tavuina: `0x2BB5C755`, `0x350B962D`/`0x4A7EF64D`, `0x81D8C`, `0x280/0x1E0/0x1C0`; osoite-immediatet +0xA0-korjattuina), funktion alku = osuma − USA-offset, tarkistus prologilla; (3) rel32-seuranta tunnetusta funktiosta. Skriptit: `…\scratchpad\europe_xbe_haku.py`, `europe_xbe_haku2.py` (importtaavat USA:n `tools/xbe_info.py`). Ei voida tehdä ilman USA-XBE:tä: tavu tavulta -vertailu rungoista (vain pistetarkistukset hook-offseteissa +0x77/+0x7C/+1/+0x4B/+0x5E/+0xF2/+0x68/+0x6F/+4); `0x2E3A50` (15.0f) ja QPC `0x22AC60→0x22AD70` ovat päätelty, ei kuvioitu.

---

## E. Riskit ja avoimet kysymykset

1. **D3D12-supersample ajetaan ensimmäistä kertaa.** Forkin `d3d12.cpp`-osa kirjoitettiin kääntämättä ja ristiinkäännettiin ajamatta (`docs/movie-draw.md:159-166`). Vertailuluvut: `surf (1280, 960)`, `c121.w = 1.0625`; upstreamissa ei Hor+-siirtoa (USA:n 110 ei päde). `0x1xxx`-tyypit jäävät 1:een, `CDevice_BackBufferScale` (`0x96C/0x970`) jää 1.0 (konsoli 2.0), ei supersample-suodatinta (`docs/movie-draw.md:291-317`). "render_percent vaikuttaa zoomiin" on kumottu — ei jahdata.
2. **`SetRenderTargetFast` ja c-38/c-37.** Elävä symboli (`game.toml:21` `0x246360`, `HAS 1`), jota forkissa ei ollut lainkaan. Fast-polku ei päivitä `vsc[58]/[59]` (`d3d8.cpp:549-553`) → back buffer ↔ render-to-texture -vaihdoissa 2×/1×-skaala voi jäädä väärin, kunnes title kutsuu `SetViewport`ia. Bisect: (a) `UpdateViewportConstants()` Fast-HLE:hen, (b) symboli pois `game.toml:21`:stä testiksi — onko guest-runko `0x246360` turvallinen ajaa (koskeeko miniporttia): **ei tiedossa**.
3. **Gamma:** lähettääkö title nollarampin tai onko `SetGammaRamp` inlinettu (pending-slot-pariteetti): ei tiedossa. Suoja on halpa; Ajo 4 ei osoittanut tarvetta.
4. **Ääni on odotetusti vaillinainen:** upstreamin `runtime\xdk\dsp_image.cpp` (463 r., `1bb872b`) tunnistaa vain 5849-DSP-kuvan ("the same in every 5849 image seen"); Europe-loki `dsound: effects image, reverb 0, effects 0 crosstalk,` = 5558-kuvaa ei tunnistettu → kaiku/efektit puuttuvat. **Älä debuggaa tätä PlayEx-vikana.** Forkissa sama tilanne.
5. **Kernel-patch: `apply --check` OK ≠ kääntyy.** `files.cpp`-lisäykset nojaavat apureihin (`Upper`, `MountedDisc()`, `Now100ns()`, `W64/R64`, `G<uint8_t>`, `fs::space`) ja `kernel/exports.h`:hen; upstream muutti `files.cpp`:ssä vain `NtDeviceIoControlFile`ä (`db0492e`), joten todennäköisesti kääntyy, mutta tämä on `iocp.cpp`:n ensimmäinen natiivi clang/MSVC-STL-käännös (`docs/windows-target.md` §8.2 "MSVC-STL-nitit"). `NtSetInformationFile`-luokka 30 ja `Finish()`-posti ovat samassa patchissa.
6. **PAL-nopeus ja tick-katto:** `[0x2E3E18] = 1` → 0,02 s tick; ilman Vaihetta 3b 1,2× (60 fps); `kMaxTicks = 250` vs moottorin 300. Per-call-kuluttajia (auto-repeat, zoom, `docs/frame-rate.md:157-203`) ei ole todistettu USA:ssakaan.
7. **Alueliput:** upstream `xboxkrnl.cpp:1226-1237` = NTSC-M (`XC_FACTORY_AV_REGION 0x00400100`) → Europe-title näkee NTSC-konsolin; vaikutus ei tiedossa (ei havaittua kuvavikaa; 480p-bitti säilyy).
8. **Testauksen rajoite:** `SIMSBO_AUTOSTART` ei vahvista Simiä → `0x66A10` ja talon lataus todennetaan vain `SIMSBO_STEP`-kävelyllä (Vaihe 8); `bringup_loop`-tyyppinen silmukka antaa väärän hyväksynnän.
9. **Tulevat NULL+offset-kaatumiset:** 16 D3D-funktiota koskee `[CDevice+0x1C28]`:a; nimeämättömät USA `0x244120`, `0x247670`, `0x248140`, `0x248AD0`, `0x24CFE0/0x24D1D0` → Europe laskennallisesti +0xA0 (ei varmennettu). Korjaus on aina nimeäminen `[symbols]`-tauluun.
10. **Diagnostiikan rajat:** F3 vaatii kaksi Presentiä; `XBE_D3D12_DEBUG` ilman `ID3D12InfoQueue`a → DebugView + Graphics Tools; TDR vain `Get-WinEvent Id=4101`:stä; release-lokissa vain `LOGE`/`LOGR`. `ImGui`-paneelia ei ole (`.ui = nullptr`), joten Present-polun kontrollina sitä ei voi käyttää ennen overlayn siirtoa.
11. **Ei siirrettävää, mutta helppo siirtää vahingossa:** forkin `185057b` `Constant1Fast` (yli-poppaa 4 tavua 5558:lla; Europe-runko `0x24BB10` päättyy `C3`); `xdk/5849/signatures.toml`-merge tuottaisi duplikaatin `IDirectSoundBuffer_Pause` (ei koske patch-polkua).
12. **Avoimet:** alareunan ~20 px (hypoteesi 16 × 1.5 = 24 px, Vaihe 1d todentaa); `.text`-insertioiden (+0x40…+0x110) sisältö tuntematon, ei tarpeen hookeille; `D3DDevice_LoadVertexShader = 0x24C060`/`SelectVertexShader = 0x24C130` (`game.toml:98, :103`) toimivat upstreamin vertex-ohjelmamuistilla, jota USA-playtest ei koskaan ajanut; `09f0a37`:n render-target-puolen pikselin offset vaikuttaa views-lukuihin eri tavalla kuin forkissa — mahdollinen myöhempi kuvaero, ei tiedossa.

Lähteet: viisi raporttia (docs-render, sdk-diff, upstream-diff, game-src, win-diag) ja tässä sessiossa levyltä tarkistetut rivit (§0.2), `git apply --check` -tulokset (§0.3), `TILANNE.md` Ajo 4, USA `src/game/hooks.cpp`, `src/common/game_info.cpp`, `docs/black-scene.md:274-279`, `docs/playtest.md:80-85`, `docs/status.md:368-370`.

system_upgrades-ehdotus (parent kirjaa): `uusi-peli.ps1`-pohjan `CMakePresets.json`:iin oma `diagnostiikka`-preset (`CMAKE_BUILD_TYPE=Release`, `CMAKE_C/CXX_FLAGS_RELEASE=-O2`, ei `-DNDEBUG`) ja `kaanna-uudelleen.bat`:iin parametri `diagnostiikka`, jotta bring-up-vaiheessa `LOGI`/`LOGW`/`XBE_PERF` toimivat ilman käsin kirjoitettua cmake-riviä; `NDEBUG` vasta julkaisubuildiin.