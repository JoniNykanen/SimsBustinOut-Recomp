# XDK 5558 -suunnitelma: The Sims Bustin' Out (Europe) → XboxXBE-Recomp

Tila tarkistettu levyltä 24.10.2026 (TILANNE.md päivitetty 14:41; tämä korvaa 14:28-leimatun suunnitelman). SDK `C:\XboxRecomp\XboxXBE-Recomp`, peli `C:\XboxRecomp\SimsBustinOut-Recomp`, scratchpad = `C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad` (jäljempänä `SCRATCH`).

## Tilannepäivitys (mikä on muuttunut 14:31–14:45)

| Kohde | Tila levyllä nyt |
|---|---|
| `XboxXBE-Recomp\xdk\5558\signatures.toml` + `layout.toml` (14:31:25) | forkin abilliontokens/XboxXBERecomp `bustinout-port` (commit adc40ab) tiedostot tavulleen; 242 symbolia, 194 oikeaa 5558-allekirjoitusta, 69 `xtlid`-id:tä, 2 anchoria (rivit 258–260), 12 data-reseptiä (262–274). Vaihe "kopioi 5849 → 5558" on siis ohi. |
| `XboxXBE-Recomp\xdk\5558-lainattu-5849\` (14:31:25) | vanhat 5849-kopiot KOKEILU-otsikolla, **yhä `xdk\`-kansion sisällä**. `cmake\XbeRecomp.cmake:34` globaa `xdk/*/layout.toml` → kopio `compiler\xdk\5558-lainattu-5849\`, ja `:92` ottaa `xdk/*/*.toml` codegenin DEPENDS-listaan. `SignatureDir` ei sitä valitse (`WantedBuilds` numeerisia, `tools\symbols\main.cpp:1017–1020`), mutta `:1024–1026` listaa sen "available"-rivillä. Siirretään pois (Vaihe 0). |
| `config\game.toml` (14:32:04, `written by xbe_symbols`) | aidolla 5558-sarjalla skannattu: 242 nimeä, 194 ≠ 0, 48 nollaa, 12/12 dataa (`DSound_StreamVtable*` käsin riveillä 271–272), `[functions] 0x2284DD` rivillä 15, `xdk = 5558`, `code_sections` kuusi sektiota, `[abi]` tyhjä (278). `D3DDevice_Reset = 0x246030` (98), `D3DDevice_InsertCallback = 0x24D0D0` (90), `IDirectSoundBuffer_PlayEx = 0x28E062` (196) ovat jo mukana. |
| **Regressio:** 4 nimeä putosi | forkin sarja ei tunne nimiä `D3DDevice_GetTile`, `D3DDevice_LoadVertexShaderProgram`, `D3DDevice_SetRenderTargetFast`, `IDirectSoundBuffer_SetEG`. Ne puuttuvat configista ja `generated\game\image_symbols.h`:sta (14:32:17). Käännös onnistui silti, koska runtimessa on fallback-#definet (`runtime\xdk\d3d8.cpp:47–57`, `dsound.cpp:84–86`, `xapi.cpp:9–12`), joten korvaukset `d3d8.cpp:1458` (GetTile), `:1055` (LoadVertexShaderProgram), `:549` (SetRenderTargetFast) ja `dsound.cpp:742` (SetEG `SYM`-listassa) ovat nyt **kuolleita stubeja**: peli ajaa omaa kirjastokoodiaan, host ei näe kutsuja. |
| Varmuuskopio 5849-pohjaisesta configista | **on olemassa, mutta vain Temp-kansiossa:** `SCRATCH\game.toml.lainattu-5849-varmuuskopio` (14:32:04, 16 313 t; GetTile rivi 89, LVSP 95, SRTF 140, SetEG 201) sekä raaka skannaus `SCRATCH\sims-koe-5849.toml` (13:53). `varmuuskopiot\`-kansiota pelikansiossa ei ole. |
| Käännös 14:32:25/14:32:50, ajo 14:33–14:34 | `out\iterointi-loki.txt`: peli ELOSSA 90 s, 0 virheriviä, "VALMIS". Peliloki `out\build\release\logs\simsbustinout.log` loppuu riviin `[0.374] dsound: effects image`. Ikkuna musta (TILANNE.md "Ajo 3"). "Kaatuminen 0,953 s" ja "nykyinen pullonkaula = signatures" eivät enää päde. |
| `src\game\hooks.cpp` | pelkkä mallipohjan kommentti. `D3DDevice_InsertCallback` on configissa ja `HAS_D3DDevice_InsertCallback 1` (`image_symbols.h:97–98`), mutta **upstream-runtime ei toteuta sitä** (grep `InsertCallback` `runtime\` → 0 osumaa) → nimi sitoutuu recompiloituun kirjastorunkoon, joka kirjoittaa push-puskuritokenin `0x00081D8C`, jota mikään ei aja. USA-portin `src\game\hooks.cpp`-kommentti (`SCRATCH\refs\bustinout-recomp\src\game\hooks.cpp`, `docs\frame-rate.md:138–146`): callbackin vapauttamaa semaforia odottava moottorisäie jää ikuiseen odotukseen. |
| HLE-nimitarkistus (`SCRATCH\tarkista-hle-nimet.ps1`, ajettu nyt) | runtime viittaa 225 nimeen (153 korvausta); image_symbols.h 254 SYM-riviä. **Kuolleita korvauksia 54:** ABSENT 7 (`D3D_BlockOnNonSurfaceResource` d3d8.cpp:418, `D3DDevice_GetRasterStatus` :477, `D3DDevice_GetTile` :1458, `D3DDevice_LoadVertexShaderProgram` :1055, `D3DDevice_RunPushBuffer` :1432, `D3DDevice_SetRenderTargetFast` :549, `XInputGetCapabilities` xapi.cpp:118) + ZERO 47 (37 verkko xnet.cpp, 3 Bink bink.cpp:11/33/46, 7 `crt_*` crt_math.cpp:27–35). Viittauksia ilman osoitetta 3: `D3D_FvfShader` (d3d8.cpp:36, suojattu), `IDirectSoundBuffer_SetEG` (dsound.cpp:86), `BinkWait` (bink.cpp:26). |

---

## A. Tilanne yhdellä kappaleella

Alkuperäinen pysähdys: `build.bat:53` ajaa `xbe_symbols`in, jonka `SignatureDir` (`tools\symbols\main.cpp:1016–1029`) etsii `xdk/<build>/signatures.toml`-tiedostoa `WantedBuilds`-järjestyksessä (D3D8-build ensin, `:1006–1013`); Simsin kaikki kirjastot ovat 5558, kansiota ei ollut → "no signatures for these library builds; available: 4831 5849" → `Scan` palautti 1 (`:1362–1363`) → `goto :fail`. Se on nyt ohi: forkin aito `xdk\5558\` on paikallaan ja Europe-XBE:llä tulos oli 194/242 funktiota, 10/12 dataa (+2 käsin = 12/12; `SCRATCH\sims-analyysi\xbe_symbols_eu_5558_raportti.txt` rivi 4; rivi 2 on `libraries:`-rivi, rivi 1 `default.xbe: title 45410043`, `main.cpp:1000–1002`). `.XTLID`-sektio (0xEF8 t = 479 merkintää, joista ensimmäinen on otsake `0 → 0x15B6` = 5558 ja 478 vientejä) on pelkkä `(export-id, osoite)`-taulukko (`main.cpp:514–522`): nimen antaa vain `signatures.toml`:n `xtlid`-kenttä, ja `NameSymbols` (`:1088–1100`) käyttää sitä vain nimille, joita tavut eivät löytäneet. Forkin 5558-sarjassa on 69 id:tä ja kaikki 69 löytyvät Simsin taulusta; id:t ovat pysyviä buildien yli (47/48 täsmäsi 5849-vertailussa). `.XTLID` nimeää vain exportit (ei `CDevice_KickOff`-tyyppisiä sisäisiä funktioita, ei `layout.toml`:n offseteja), joten se täydentää, ei korvaa, tavuallekirjoituksia. Nykyinen pullonkaula ei ole enää symbolit vaan: (1) neljä skannauksessa kadonnutta nimeä, (2) puuttuva `InsertCallback`-hookki ja (3) musta ruutu, jota erillinen selvitys ("sims-musta-ruutu") tutkii.

## B. Vaihtoehdot ja suositus

### B1. Automaattinen polku (tehty, luvut)

| Sarja `xdk\5558\`:ssä | Tulos Europe-XBE:llä | Lähde |
|---|---|---|
| 4831 kopioituna | 58/248 | TILANNE.md |
| 5849 kopioituna | 186/243 + 12/12 | TILANNE.md; simulaatio ennusti 157 tavuilla + 19 .XTLID:llä ≈ 176/243 (matcher §4, vakiovaihe simuloimatta) |
| forkin aito 5558 | **194/242 + 10/12** (+2 käsin → 12/12); 0 ristiriitaa 5849-ajoon, 9 nimeä lisää (`CDirectSoundStream_AddRef/Release`, `D3DDevice_BeginPush/EndPush/BlockOnFence/FlushVertexCache/SetPalette`, `D3D_BlockOnResource`, `D3D_KickOffAndWaitForIdle`) | xbe-raportti; raportti.txt rivi 4 |

Odotettu osumaprosentti nyt: 194/242 = 80 %. Simsille merkityksellisistä nimistä (242 − 41 sektiotonta = 201) 194 = 96,5 %. Aidosti löytymättä 7 `crt_*` (USA-portti ei löytänyt niitäkään, `refs\bustinout-recomp\docs\gap-analysis.md:33–38`). Lisäksi 4 nimeä, joita kumpikaan sarja ei tunne mutta runtime korvaa (`GetRasterStatus`, `RunPushBuffer`, `D3D_BlockOnNonSurfaceResource`, `XInputGetCapabilities`; lisätty SDK:hon 22.10.2026 commiteissa 91478e2/ab4cd9c/913f9f1, reference §6).

### B2. Käsityöpolku (Ghidra + xbe2pe.py)

Layout: **ei tarvita** — `xdk\5558\layout.toml` on olemassa, 45/45 avainta = 5849:n arvot, ja layout-raportti todisti 41 niistä suoraan Europe-XBE:stä (D.-osio). Symbolit: jäljellä 7 `crt_*` + 4 runtime-fallback-nimeä = 11 funktiota.

Tuntiarvio (oma arvio, raporteissa ei ole mitattuja tunteja):
- Jäljellä oleva työ Simsillä: 11 funktiota Ghidrassa, 2–4 h (crt-funktiot löytyvät x87-käskyistä, ks. Vaihe 5).
- Vertailukohta "tyhjästä": abilliontokens teki 5558-sarjan siemenestä (108/240) valmiiksi (194/242 + layout 45 avainta) haarautumisen 11.10. ja commitin 7f2e294 19.10.2026 välillä, eli ≤ 8 kalenteripäivää, IDA + sisarpelivertailu (status.md:23–45). Joshhhuaaa committoi 4831-layoutin ja -allekirjoitukset samana päivänä 22.10. (940c691, 18c1a2a), mutta configin täyttöaika ei näy historiasta: ei tiedossa. Layout-raportin tekijä luki 41 avainta ndisasm-listauksesta yhdessä sessiossa. Realistinen haarukka ilman valmista forkia: 2–4 työpäivää ei-ammattilaiselta.

### B3. Vaihda peliä

Buildin tunnistus: `xbe_symbols` tulostaa `libraries:`-rivin (raportin rivi 2; `main.cpp:1000–1002`) ja runtime lokittaa saman bootissa (`runtime\kernel\boot.cpp:104`); ilman buildia `SCRATCH\sims-analyysi\xtlid_parser.py <xbe>` listaa kirjastot. Tunnetut pelit raporttien mukaan:
- **5849:** Splinter Cell Double Agent (default.xbe + sc4_adv.xbe), BLACK, 007 From Russia with Love (molemmat `config\game.toml:7`), The Sims 2, The Warriors, Def Jam, DW5, Double Steal, Castlevania CoD, Shaolin Monks (reference §5).
- **4831:** 007 NightFire (default.xbe + Driving.xbe, `xdk\4831\layout.toml:1–2`; projektit eivät ole julkisia), HOTD3 (Chihiro, CrownParkComputing).
- **5558:** Sims Bustin' Out (USA/EU), The Urbz, OutRun 2 (Chihiro, layout eroaa → eri QFE), Top Spin (blogimaininta).
- Muut forkin haarat: 5233 (The Sims), 3925 (Fuzion Frenzy), 5028 (BMX XXX).

**Suositus:** ei vaihdeta peliä. Jatketaan aidolla 5558-sarjalla: palautetaan 4 nimeä, lisätään `InsertCallback`-hookki, käännetään, ja `--sign` vasta kun 11 puuttuvaa on etsitty.

## C. Askel askeleelta (järjestys: 0 → 1 → 2 → 3 → 4 → 5 → 6)

Kaikki cmd-komennot ajetaan pelikansiosta: `cd /d C:\XboxRecomp\SimsBustinOut-Recomp`. PowerShell-pätkät: `powershell -ExecutionPolicy Bypass`.

### Vaihe 0 — varmuuskopiot ja `xdk\`-kansion siivous (5 min)

```cmd
set SCRATCH=C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad
cd /d C:\XboxRecomp\SimsBustinOut-Recomp
mkdir varmuuskopiot
copy config\game.toml varmuuskopiot\game-5558-skannaus-14-32.toml
copy "%SCRATCH%\game.toml.lainattu-5849-varmuuskopio" varmuuskopiot\game-5849-lainattu-14-26.toml
copy "%SCRATCH%\sims-koe-5849.toml" varmuuskopiot\skannaus-5849-raaka.toml
copy "%SCRATCH%\sims-aito-5558.toml" varmuuskopiot\skannaus-5558-raaka.toml
copy "%SCRATCH%\tarkista-hle-nimet.ps1" tarkista-hle-nimet.ps1
move ..\XboxXBE-Recomp\xdk\5558-lainattu-5849 varmuuskopiot\xdk-5558-lainattu-5849
robocopy "%SCRATCH%\refs" varmuuskopiot\refs /E /NFL /NDL /NJH
robocopy "%SCRATCH%\sims-analyysi" varmuuskopiot\sims-analyysi /E /NFL /NDL /NJH
```
- `refs\` on 29 Mt (9 kloonia git-historioineen; `bustinout-recomp` 11 Mt sisältää 20 docs-tiedostoa ml. `boot-crash.md`, `d3d-gap-functions.md`, `playtest.md`, sekä `src\game\hooks.cpp`). `fork-sdk` on git-worktree (adc40ab, detached) eikä kopioidu toimivana; tee tarvittaessa uusi: `git -C varmuuskopiot\refs\abilliontokens-XboxXBERecomp worktree add ..\..\fork-sdk adc40ab`.
- Tarkista: `dir ..\XboxXBE-Recomp\xdk` näyttää vain `4831 5558 5849`. Seuraava `cmake --build` ajaa configuren uudelleen (`CONFIGURE_DEPENDS`), jolloin `out\build\release\compiler\xdk\5558-lainattu-5849\` poistuu kopiolistalta; vanhan kopiokansion voi poistaa käsin.

### Vaihe 1 — palauta neljä nimeä ja lisää nollarivit fallback-nimille (5 min)

Osoitteet ovat 5849-sarjan löytämiä (`varmuuskopiot\game-5849-lainattu-14-26.toml:89,95,140,201`) ja `IDirectSoundBuffer_SetEG` lisäksi .XTLID-id:llä `B0067 → 0x28DFCE` (xbe-raportti, DSOUND-lista). Nollarivit tekevät runtime-korvauksista näkyviä trap-stubeja fallback-#definejen sijaan ja vievät nimet `--sign`-vaiheessa 5558-sarjaan `{}`-merkintöinä (`main.cpp:761, 588`).

```powershell
$cfg = 'C:\XboxRecomp\SimsBustinOut-Recomp\config\game.toml'
$t = [IO.File]::ReadAllText($cfg)
if ($t -match 'D3DDevice_GetTile') { throw 'nimet on jo palautettu' }
$lisays = @"
[symbols]
# palautettu 14:32-skannauksen jalkeen: forkin 5558-sarja ei tunne naita; osoitteet 5849-sarjan loytamia (varmuuskopiot\game-5849-lainattu-14-26.toml)
D3DDevice_GetTile                                    = 0x245610
D3DDevice_LoadVertexShaderProgram                    = 0x24C0C0
D3DDevice_SetRenderTargetFast                        = 0x246360
IDirectSoundBuffer_SetEG                             = 0x28DFCE  # .XTLID B0067
# runtime-korvaus on, osoite ei tiedossa: 0 = trap-stubi jos peli kutsuu (ks. Vaihe 5)
D3DDevice_GetRasterStatus                            = 0
D3DDevice_RunPushBuffer                              = 0
D3D_BlockOnNonSurfaceResource                        = 0
XInputGetCapabilities                                = 0

"@
$t = $t -replace '(?m)^\[symbols\]\r?\n', $lisays
[IO.File]::WriteAllText($cfg, $t, (New-Object System.Text.UTF8Encoding($false)))
```
Mitä katsotaan: `Select-String config\game.toml -Pattern '^(D3DDevice_GetTile|IDirectSoundBuffer_SetEG|XInputGetCapabilities)'` → 3 riviä. `[functions] 0x2284DD` (rivi 15) ja `DSound_StreamVtable*` (271–272) pysyvät, koska configia ei skannata uudelleen.

### Vaihe 2 — `src\game\hooks.cpp`: `D3DDevice_InsertCallback` (10 min)

Korvaa tiedoston sisältö tällä (USA-portin hookki on nimipohjainen, joten se siirtyy sellaisenaan; vain kommenttien osoitteet on siirretty +0xA0, jolla kaikki 67 kirjastosektion .XTLID-symbolia EU-XBE:ssä sijaitsevat; USA 0x24D030 → EU 0x24D0D0 täsmää configin riviin 90):

```cpp
// Hooks for default.xbe (Europe). A strong definition replaces the recompiled function at that address.
#include "xdk/hle.h"

// D3DDevice_InsertCallback(Flags, pCallback, Context) at 0x24D0D0 (USA 0x24D030 + 0xA0).
// USA body, proven in abilliontokens/bustinout-recomp src/game/hooks.cpp: 0x24D04B writes the push
// token 0x00081D8C, [esp+0xC] (pCallback) is stored at token+4 (0x24D043/0x24D051), [esp+0x10]
// (Context) at token+8 (0x24D047/0x24D05A), Flags only selects the longer token form; retn 0xC.
// EU addresses assumed +0xA0 (0x24D0EB, 0x24D0E3/0x24D0F1, 0x24D0E7/0x24D0FA): not re-disassembled.
// On the console the GPU interrupt handler runs pCallback(Context) when it reaches the token. The SDK
// has no push-buffer callback path (the token lands in a scratch buffer the renderer walks for draws
// only), so the callback would never run and an engine thread waiting on the semaphore it releases
// would idle forever. The host renderer has no GPU timeline the guest can be behind, so run it now.
HLE_STDCALL(D3DDevice_InsertCallback, 3) {
    uint32_t callback = a[1], context = a[2];
    if (callback != 0) ::kernel::CallGuest(ctx, callback, {context});
    return 0;
}
```
- `HLE_STDCALL` (`runtime\xdk\hle.h:110–114`) vaatii `SYM_D3DDevice_InsertCallback`-rivin `image_symbols.h`:ssa → on (rivi 97). `CallGuest` on sama kutsu, jota runtime käyttää vblank-säikeessä (`d3d8.cpp:310`).
- USA-hookin toinen osa (frame-step `sub_001232F0`/`sub_00123430`) on `.text`-osoitteisiin sidottu; EU:n `.text` on 176 tavua pidempi ja entry 0x100EA2 → 0x100F8C, joten se ei siirry ilman uutta osoitehakua. Jätetään myöhemmäksi (F8).

### Vaihe 3 — käännä ja tarkista nimet (2–3 min)

```cmd
kaanna-uudelleen.bat
powershell -ExecutionPolicy Bypass -File tarkista-hle-nimet.ps1 -VainOngelmat
```
- Configin muutos laukaisee `xbe_recomp`in + partitioiden käännöksen (`XbeRecomp.cmake:92–103`; TILANNE: 48 s muutoksen jälkeen, 1 min 40 s kokonaan). Virhe `FAILED|error:|ninja: build stopped` → pysähdy.
- Skriptin odotettu tuloste: "KUOLLEET KORVAUKSET" = 37 verkko + 3 Bink + 7 `crt_*` + 4 nollariviä (`GetRasterStatus`, `RunPushBuffer`, `BlockOnNonSurfaceResource`, `XInputGetCapabilities`) = 51, kaikki tilassa ZERO; **ABSENT-rivejä ei saa olla**. `GetTile`, `LoadVertexShaderProgram`, `SetRenderTargetFast`, `SetEG` eivät enää näy listassa.

### Vaihe 4 — aja ja lue loki (2 min/kierros)

```cmd
powershell -ExecutionPolicy Bypass -File korjaa-puuttuvat-funktiot.ps1 -MaxKierrokset 5 -KestoSekunteja 60
```
Lokista `out\build\release\logs\simsbustinout.log` (ja `out\iterointi-loki.txt`) katsotaan:
1. `indirect call to XXXXXXXX has no recompiled function` + `add 0x... to [functions]` (`runtime\cpu\dispatch.cpp:163–165`) → skripti lisää osoitteen `[functions]`-tauluun ja kääntää uudelleen. USA-portilla toinen tällainen (`0x66980`) tuli vasta Create-a-Sim-nimensyötössä (gap-analysis.md:137–149); EU:ssa eri osoite.
2. `kernel export N (nimi) is not implemented` (`dispatch.cpp:178–180`) → kernel-thunkin aukko; paikataan `hooks.cpp`:hen `KERNEL_EXPORT(ordinal, Nimi, nargs)`-rivillä 007FRWL-mallin mukaan (`refs\007FRWL-XBERecomp\src\game\hooks.cpp`, 14 riviä).
3. `guest trap at ...` (`dispatch.cpp:248–256`) → peli kutsui 0-nimeä; trapin nimi kertoo, mikä `[symbols]`-rivi pitää etsiä Ghidrassa (Vaihe 5).
4. `... is unknown for this image, add it to the config` (`runtime\xdk\layout.h:23–28`) → puuttuva `[data]`- tai `[layout]`-avain.
5. Jos loki loppuu yhä riviin `dsound: effects image` ilman virheitä ja kuva on musta (`out\kuvakaappaukset\kierros-01.png`; huom. ikkuna jää selaimen taakse, TILANNE.md:n TOPMOST-kikka), hookki ei yksin riittänyt → tulos "sims-musta-ruutu"-selvitykselle: seuraavat epäillyt ovat renderöintipuolella (USA: `docs\black-scene.md` D2/D3 olivat Vulkan-puolen vikoja; Windows D3D12 -polkua USA-portti ei koskaan ajanut, `docs\windows-target.md:455–458`; forkin D3D12-muutokset `SCRATCH\fork-d3d12-diff.txt`).

### Vaihe 5 — 11 puuttuvaa nimeä Ghidrassa (2–4 h, voi tehdä mustan ruudun selvityksen rinnalla)

```cmd
python ..\XboxXBE-Recomp\tools\symbols\xbe2pe.py config\game.toml out\sims-eu.exe
```
Ghidra: Import → PE, x86:LE:32, image base 0x10000; exporteissa näkyvät `sub_XXXXXXXX` (recompilerin entryt) ja nimetyt `[symbols]`-osumat (`xbe2pe.py:38–47`).
- **7 × `crt_*`** (`.text`): EU = USA + 0x110 CRT-ryhmässä (todistettu: `crt_acos` 0x21EAF1 → 0x21EC01, `crt_asin` 0x220879 → 0x220989, `crt_pow` 0x220A24 → 0x220B34; config rivit 51–57), mutta USA ei löytänyt näitä seitsemää, joten siirrettävää osoitetta ei ole. Etsi samasta linkkiryhmästä (0x21EC01–0x220B34 ympäristö) x87-käskyillä: `fsin` → `crt_sin`, `fcos` → `crt_cos`, `fptan` → `crt_tan`, `fpatan` → `crt_atan`, `fsqrt` → `crt_sqrt`, `fyl2x` + ln2-/log10(2)-vakio → `crt_log`/`crt_log10`. Kirjaa **pinoversion** osoite (`sin`, ei `_CIsin`; `crt_math.cpp:8`). Syy nimetä: recompiloitu CRT-matematiikka lukee vanhentuneet liput ja voi palauttaa NaN:n satunnaisesti (`crt_math.cpp:1–3`); Sims-vaikutus ei tiedossa (USA pelasi ilman).
- **`XInputGetCapabilities`** (XPP): stdcall `(handle, pCaps)`, `ret 8`. Simsin XPP-.XTLID:n nimeämättömät id:t: `0x10039 → 0x2DAFE5`, `0x1003B → 0x2DAD67`, `0x10034 → 0x2DB0C2`, `0x10031 → 0x2DB0FE`, `0x10035 → 0x2DDB2D` (nimetyt: `XInputOpen` 0x2DAD05, `XInputClose` 0x2DAD5B, `XInputGetState` 0x2DAF3F, `XInputSetState` 0x2DAFB2, `XGetDevices` 0x2DB103, `XGetDeviceChanges` 0x2DB125). Hypoteesi, ei todistettu: 0x2DAFE5 (GetState/SetState-pari jatkuu). Varmista `ret 8` ja rakenteen koko Ghidrassa.
- **`D3DDevice_GetRasterStatus`, `RunPushBuffer`, `D3D_BlockOnNonSurfaceResource`** (D3D): eivät ole 5849- eivätkä 5558-sarjassa; 4831-sarjassa ei tarkistettu. `GetRasterStatus` lukee miniportin vblank-kenttiä (`[dev+0x1DE8]` count, `[dev+0x1DF8]` field, layout-taulukko D.) ja on `ret 4`; `d3d-gap-functions.md` (USA-sensus 760 D3D-entryä, +0xA0) listaa 16 miniporttiin koskevaa funktiota, joista nimeämättömät ovat "future same-shape crashes" (gap-analysis.md:40–45). Vasta trap tai kaatuminen kertoo, kutsuuko Sims näitä: jos ei, nolla riittää.

Jokainen löytö: osoite `config\game.toml`:n `[symbols]`-riville, `kaanna-uudelleen.bat`, `tarkista-hle-nimet.ps1`.

### Vaihe 6 — `[xtlid]`-taulu ja `--sign` (30 min, kun Vaihe 5 on tehty tai hyväksytty keskeneräisenä)

1. Lisää configiin `[xtlid]`-taulu 5849-sarjan 37 XNET/XONLINE-id:lle, jotta ne säilyvät 5558-sarjassa tulevia 5558-titlejä (The Urbz) varten (`main.cpp:671–674` lukee, `:754–758` kirjoittaa `{xtlid = ...}`; forkin sarjassa niitä ei ole, 69 vs 106 id:tä):
   ```powershell
   $rivit = Select-String ..\XboxXBE-Recomp\xdk\5849\signatures.toml -Pattern '^(\w+) = .*xtlid = (0x[34][0-9A-Fa-f]+)' | ForEach-Object { '{0} = {1}' -f $_.Matches[0].Groups[1].Value, $_.Matches[0].Groups[2].Value }
   "`r`n[xtlid]`r`n# export ids of XNET/XONLINE functions this title does not link, from xdk\5849`r`n" + ($rivit -join "`r`n") | Add-Content config\game.toml -Encoding ascii
   ```
   Tarkista: `$rivit.Count` = 37.
2. Allekirjoita erilliseen tiedostoon ensin, älä suoraan SDK:hon:
   ```cmd
   out\build\release\xbe_symbols.exe --sign config\game.toml --out varmuuskopiot\signatures-5558-eu.toml
   ```
   Tulosteesta (`PrintSigned`, `main.cpp:945–954`): `N of M functions signed, A anchors, D data` — odotus ≥ 198 of 250 (194 + 4 palautettua; Vaihe 5:n löydöt lisäävät), 12 data. **Ei saa näkyä** rivejä `... is not on the linear sweep` (`:716–717`) eikä `... does not match itself` (`:739–743`) nimille `D3DDevice_Reset`, `D3DDevice_InsertCallback`, `D3DDevice_GetTile`, `D3DDevice_LoadVertexShaderProgram`, `D3DDevice_SetRenderTargetFast`, `IDirectSoundBuffer_SetEG`; jos näkyy, osoite ei ole käskyalku tai funktio on kaksoiskappale → `--skip nimi` ja etsi uudelleen. `no function uses DSound_StreamVtable` (`:882`) tarkoittaa, että upstreamin anchor-uudelleenkäyttö (`AnchorAt`, commit cf6b32c) ei ratkaissut forkin tunnettua aukkoa (kaksi anchoria samalla hashilla, `xdk\5558\signatures.toml:259–260`) → arvot pysyvät käsin configissa.
3. Round-trip: skannaa uudella sarjalla erilliseen tiedostoon ja vertaa configiin:
   ```cmd
   mkdir varmuuskopiot\xdk-koe\5558
   copy varmuuskopiot\signatures-5558-eu.toml varmuuskopiot\xdk-koe\5558\signatures.toml
   out\build\release\xbe_symbols.exe assets\default.xbe varmuuskopiot\xdk-koe --library SimsBustinOut.dll --out varmuuskopiot\roundtrip.toml
   fc config\game.toml varmuuskopiot\roundtrip.toml
   ```
   Hyväksyntä: `found N of M` sama kuin signattu määrä, erot vain kommenteissa, `[functions]`-taulussa ja `DSound_StreamVtable*`-riveissä. Vasta sitten: `copy varmuuskopiot\signatures-5558-eu.toml ..\XboxXBE-Recomp\xdk\5558\signatures.toml` (ensin `copy ..\XboxXBE-Recomp\xdk\5558\signatures.toml varmuuskopiot\signatures-5558-fork-adc40ab.toml`). Sign-vaihe täyttää `xtlid`-kentät automaattisesti kaikille nimille, joiden osoite on Simsin .XTLID:ssä (`:762–763`): D3D-exporteista enintään 64 (5849:ssä vain 3).
4. **Sääntö jatkossa:** `xbe_symbols`-skannaus kirjoitetaan aina erilliseen tiedostoon, ei `--out config\game.toml` (14:32-ylikirjoitus pudotti 4 nimeä).

## D. `layout.toml` 5558:ssa

Tiedosto on jo paikallaan (forkin, 45 avainta, otsikko viittaa USA-XBE:hen ja `D3D__pDevice (0x264948)` = EU 0x2649E8 − 0xA0). Rivi 5 lupaa UNPROVEN-merkinnät, mutta yhtään avainriviä ei ole merkitty → kaikki 45 arvoa ovat 5849:n arvot. Arvoja **ei muuteta**; kommenttien USA-osoitteet voi halutessaan päivittää EU-osoitteiksi alla olevasta.

| Ryhmä | Avaimet | Lähde | Varmistus EU-XBE:stä (layout-raportti §5, `SCRATCH\D3D.asm`/`DSOUND.asm`/`XPP.asm`) |
|---|---|---|---|
| Samat 4831:ssä ja 5849:ssä (18) | `CDevice_Push 0x0`, `PushThreshold 0x4`, `Flags 0x8`, `BaseVertex 0x1C`, `DirtyFlags_Texture 0x4800`, `TextureState_Stage 32`, `Stream_Size 12`, `Stream_Stride 0x0`, `Stream_Buffer 0x8`, `PixelShader_Definition 0x8`, `CDirectSoundStream_Vtable 0x0`, `Vtable2 0x4`, `EffectsImage_Skip0 0x804`, `Skip1 0x80C`, `Desc 0x818`, `XPP_Current 0x0`, `Changed 0x4`, `Previous 0x8` | kopioitavissa | Push/Threshold: `D3DDevice_SetTexture` 0x2472B2/0x2472B5 → `call 0x24D650`; Flags: `Direct3D_CreateDevice` 0x24448F `[0x2649F8]` + `and eax,0x10`; DirtyFlags: 0x2472E1/0x2472E7 (`or eax,0x4800`, globaali 0x264548); TextureState_Stage: `GetTextureStageState` 0x2453DC `shl eax,5` + `[eax*4+0x264550]`; EffectsImage: DSOUND 0x2940B3/0x2940B9/0x2940C2; Vtable-pari: DSOUND-konstruktorit 0x29A39A/0x29A3A0; XPP: `XGetDevices` 0x2DB10E/0x2DB110/0x2DB116, `XGetDeviceChanges` 0x2DB12E. `Stream_*` ja `PixelShader_Definition` eivät erikseen paikannettu (formaattivakioita, samat kaikissa kolmessa buildissa). |
| Siirtyneet `CDevice_*` (27) | `IndexBuffer 0x7A0`, `ZScale 0x948`, `SuperSampleX/Y 0x95C/0x960`, `BackBufferScale 0x96C`, `PixelShader/VertexShader 0x784/0x794`, `Transforms 0xC60`, `Viewport 0xEE0`, `ScreenSpaceOffset 0xEF8`, `Textures 0xF88`, `Palettes 0xF98`, `RenderTarget/Depth 0x1A04/0x1A08`, `BackBuffer/FrontBuffer 0x1A14/0x1A18`, `AutoDepth 0x1A20`, `Time 0x2C`, miniportti +0x1C28: `VBlankCallback 0x1DB8`, `VBlankEvent 0x1DBC`, `DisplayFlags 0x1DDC`, `VBlankCount 0x1DE8`, `Field 0x1DF8`, `GammaRamps 0x1E04`, `GammaPending 0x2404`, `SwapCount 0x2478`, `Size 0x24A0` | 5849 = 5558, todistettu | `SetIndices` 0x2474FA/0x247522/0x247529; `GetViewportOffsetAndScale` 0x249AD8–0x249AE4; 0x247CEB/0x247CF1, 0x248895; 0x24986A/0x249874; 0x24E7F8, 0x24C14C/0x24C51E; `SetTransform` 0x245092–0x24509D (`shl ecx,6; lea edi,[ecx+ebx+0xC60]; mov ecx,0x10`); `SetViewport` 0x246D90–0x246DBC; 0x2481A3/0x2481B3; `SetTexture` 0x247270/0x2472A9; `SetPalette` 0x24744D/0x247485; `SetRenderTarget` 0x2460F7/0x246109/0x246136/0x24612E; 0x248476, 0x248AE0, 0x246BFD; 0x24FB6E/0x2460A8; miniportti `lea ecx,[esi+0x1C28]` 0x244245; `SetVerticalBlankCallback` 0x2454FA, käsittelijä 0x25215E/0x252151/0x2520C1/0x252124/0x252018; 0x245FE8/0x245FEE (`and eax,0x400000`), 0x24F3A6; `SetGammaRamp` 0x244F90, 0x244FD1; `Swap` 0x248CC4 (`inc dword [esi+0x2478]`); `Direct3D_CreateDevice` 0x2444DB `mov ecx,0x928; rep stosd` → 0x24A0. 4831-arvot (0x21B4/0x21C0/0xB38/0x2550/0x23C0) esiintyvät D3D-sektiossa 0 kertaa, 5849-arvot 15–27 kertaa. |

- OutRun 2:n 5558-layout (`refs\OutRun2-XBERecomp\config\xdk-5558.toml`: `IndexBuffer 0x79C`, `Textures 0xF78`, `Viewport 0xED0`, `Transforms 0xC50`) on ristiriidassa, mutta EU-XBE:n käskyt todistavat forkin arvot (esim. `[esi+0x7A0]` SetIndicesissä) → Chihiro/eri QFE, ei koske Simsiä.
- Mitä runtime tekee avaimilla: 20 pakollista heti `Direct3D_CreateDevice`-HLE:ssä (`d3d8.cpp:334–340`, `XDK_REQUIRE` → abort "is unknown for this image" `layout.h:23–28`); `Palettes` (794), `Flags` (1477/1488), `EffectsImage_*` (`dsound.cpp:865–868`), `CDirectSoundStream_Vtable*` (1099–1107), `XPP_*` (`xapi.cpp:30–33, 53–60`) vasta kutsuttaessa; vblank-kolmikko, `Viewport`, `AutoDepth`, `BackBufferScale`, `DisplayFlags`, `Gamma*`, `PixelShader_Definition` valinnaisia (`!= kUnknown`); `Time`, `PixelShader`, `ScreenSpaceOffset`, `Field` vain dokumentaatiota. Väärän arvon oireet: Push/Threshold → access violation ensimmäisessä piirrossa; Size liian suuri → `memset` roskaa `D3D_Device`-globaalin jälkeiset muuttujat; RenderTarget/BackBuffer → NULL-dereferenssi; vblank-kolmikko → `SetEvent` roskaosoitteeseen tai `BlockUntilVerticalBlank` jumissa; Vtable → kaatuminen ensimmäisessä DirectSound-streamissa.
- Prosessiparannus (ei pakollinen): skripti, joka laskee `[reg+disp]`-esiintymät jokaiselle `xdk\*\layout.toml`-avaimelle annetun XBE:n D3D-sektiosta → tunnistaa buildin sekunneissa.

## E. Riskit

1. **LTCG:** ei koske Simsiä. Kirjastonimi `D3D8` (ei `D3D8LTCG`, `main.cpp:1313`, `WantedBuilds :1008`), kaikki 8 kirjastoa 1.0.5558 (D3D8 QFE 4), .XTLID-funktiot stdcall ja `ret`-koot täsmäävät 5849-referenssiin (`Direct3D_CreateDevice` 0x244460 `ret 0x18`, `DirectSoundCreate` 0x29167A `ret 0xC`), `[abi]` tyhjä. `CheckConventions` (`main.cpp:1237–1259`) raportoi silti erikseen nimille, jotka löytyvät vain .XTLID:llä/vakioilla → `[abi]`-rivit `["?", ...]` tarkistetaan käsin.
2. **Kuolleet korvaukset (toteutui 14:32):** nimi puuttuu configista → fallback-#define (`d3d8.cpp:28–73`, `dsound.cpp:28–87`, `crt_math.cpp:15–25`, `xapi.cpp:9–12`) tekee käännöksestä onnistuneen, mutta `HLE_*`-runko sitoutuu osoitteeseen `0xFFFFFExx`, jota kukaan ei kutsu; peli ajaa oman kirjastokoodinsa, joka lukee miniporttia, jota host ei rakenna (sama muoto kuin `D3DDevice_Reset`-kaatuminen `[NULL+0x3214]`, USA `status.md:139–149`). Torjunta: `tarkista-hle-nimet.ps1` jokaisen configmuutoksen jälkeen; skannaus aina erilliseen tiedostoon.
3. **Trap-stubit (0-nimet):** laukeavat vain kutsuttaessa (`dispatch.cpp:248–256`). 41 sektiotonta (Bink/XNET/XONLINE) ovat oikein nollia. 7 `crt_*`: pelin oma kopio ajetaan recompiloituna → mahdollinen satunnainen NaN (`crt_math.cpp:1–3`); vaikutus Simsiin ei tiedossa.
4. **Epäsuorat kutsut:** osoittimen kautta kutsutut funktiot puuttuvat analyysistä (USA: `0x2283CD` bootissa, `0x66980` nimensyötössä; gap-analysis.md:137–149); `korjaa-puuttuvat-funktiot.ps1` lisää ne, mutta sen "VALMIS" todistaa vain käynnistyksen — USA:n bringup-silmukka antoi väärän läpäisyn, koska sokea ohjaus ei päässyt nimensyötön ohi (gap-analysis.md:154–162).
5. **SDK-koodimuutokset:** 5558 ei vaadi runtime-muutoksia (4831-precedentti: 940c691/18c1a2a muuttivat vain toml-tiedostoja + 3 kommenttiriviä; forkin 4d685e5: "5849 and 5558 are untouched"). Riski on muualla: upstreamissa ei ole `InsertCallback`-toteutusta (pelihookki hoitaa), ja forkin 50 tiedoston muutokset (+7865/−382 commitista 65e959d) sisältävät Sims-kohtaisia renderöintikorjauksia (introvideon 1:1-piirto, kuvasuhde, D3D12 `SuperSampleScale`), joita upstreamissa (+3569/−331, 46 tiedostoa) ei ole. Forkin Windows-polkua ei ole koskaan ajettu (`windows-target.md:455–460`), joten musta ruutu voi olla upstreamin D3D12-polun tai forkista puuttuvan korjauksen vika — päätös fork vs upstream kuuluu "sims-musta-ruutu"-selvitykselle.
6. **`xdk\5558-lainattu-5849` kansiossa `xdk\`:** cmake-glob kopioi sen `compiler\xdk\`-kansioon ja ottaa sen DEPENDS-listaan (`XbeRecomp.cmake:34, 92`); harmiton valinnalle, harhaanjohtava "available"-rivillä. Vaihe 0 siirtää sen.
7. **Temp-kansio:** kaikki referenssikloonit, varmuuskopio-config ja analyysiskriptit ovat `SCRATCH`-kansiossa, joka on `%LocalAppData%\Temp` → katoaa siivouksessa. Vaihe 0 kopioi ne.
8. **`--sign` SDK:n päälle:** ylikirjoittaa forkin tiedoston; round-trip (Vaihe 6.3) ennen kopiointia, forkin versio talteen.

## F. Avoimet kysymykset (raportit eivät vastaa)

1. Onko musta ruutu `InsertCallback`-odotus, D3D12-renderöinnin vika vai jokin muu — Vaihe 4 vastaa ensimmäiseen; USA-portti ei koskaan ajanut Windows-polkua. Lokissa ei ole yhtään riviä `dsound: effects image` -rivin jälkeen: ei tiedossa, lokittaako runtime `Direct3D_CreateDevice`n tai ensimmäisen `Swap`in lainkaan.
2. 7 `crt_*`-osoitetta: kukaan ei ole löytänyt (USA 0/7). x87-käskyhaku (Vaihe 5) on hypoteesi.
3. Kutsuuko Sims funktioita `GetRasterStatus`, `RunPushBuffer`, `D3D_BlockOnNonSurfaceResource`, `XInputGetCapabilities`? Ei tiedossa; .XTLID-id → nimi -kytkentä näille puuttuu (XPP-ehdokkaat 0x10039/0x1003B/0x10034/0x10031/0x10035). Onko ne 4831-sarjassa: ei tarkistettu.
4. Ratkaiseeko upstreamin `xbe_symbols --sign` (AnchorAt-uudelleenkäyttö, cf6b32c) `DSound_StreamVtable*`-anchorien kaksoisosuman 0x28CE1E? Vaihe 6.3 testaa.
5. `InsertCallback`-rungon EU-käskyosoitteet (+0xA0) ja `.text`-thunkin EU-osoite (USA 0x16E6B0): oletettu, ei disassembloitu. Vaikuttaa vain kommentteihin.
6. Tuntimäärät: raporteissa ei ole mitattuja tunteja, vain kalenterinäyttö (fork 11.–19.10.; 4831 committoitu 22.10.).
7. Simsin .XTLID:n 64 D3D-id:stä 5849 tuntee 3; `--sign` Simsistä nimeää vain löydetyt. Loput id:t (esim. 0x800C9–0x800CD) jäävät nimettömiksi: ei tiedossa, mitkä exportit ne ovat.
8. USA-portin `.text`-hookit (frame-step `sub_001232F0`/`sub_00123430`, aspect-getter `sub_001287C0`) ja `cheats.cpp`: EU-osoitteet ei tiedossa (`.text` eroaa, XAPI-osa +0x110/+0x112).
9. OutRun 2:n eri 5558-layout: mikä QFE? Merkityksetön Simsille, mutta tulevalle Chihiro-työlle.
10. Pelaako kukaan testiä ohjauksella: Windows-kbm ikkunaviesteillä on forkissa ajamatonta koodia; upstreamin näppäimistö→pad-mäppäys (`input::Keyboard`, BLACK `game_info.cpp:112–118`) vaatii `kGame.pad`-hookin, jota `src\common\game_info.cpp` ei vielä aseta — ei tarkistettu.

## Liite: `tarkista-hle-nimet.ps1` (tallennettu `SCRATCH\tarkista-hle-nimet.ps1`, Vaihe 0 kopioi pelikansioon)

```powershell
# tarkista-hle-nimet.ps1
# Listaa SDK:n runtime-koodin HLE-nimet (HLE_STDCALL/CDECL/FASTCALL/WRAP-korvaukset seka SYM/GUEST/GUEST_CALL/HAS_/XDK_DATA-viittaukset)
# ja vertaa niita pelin generated\game\image_symbols.h-tiedostoon:
#   OK      = nimi on configissa ja osoite != 0 (korvaus sitoutuu pelin funktioon)
#   ZERO    = nimi on configissa arvolla 0 (HAS_ = 0): korvaus on kuollut stubi, host ei nae pelin kutsuja
#   ABSENT  = nimi puuttuu configista kokonaan: korvaus ei sitoudu mihinkaan (kaannos onnistuu vain, jos runtimessa on fallback-#define)
# Kaytto pelikansiosta:  powershell -ExecutionPolicy Bypass -File .\tarkista-hle-nimet.ps1
#                        (SDK oletetaan vierestä: ..\XboxXBE-Recomp; muuten -Sdk <polku>)
param(
    [string]$Sdk    = (Join-Path (Split-Path $PSScriptRoot -Parent) 'XboxXBE-Recomp'),
    [string]$Header = (Join-Path $PSScriptRoot 'generated\game\image_symbols.h'),
    [switch]$VainOngelmat
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Header)) { throw "image_symbols.h puuttuu: $Header (aja kaannos ensin)" }
$runtime = Join-Path $Sdk 'runtime'
if (-not (Test-Path $runtime)) { throw "SDK:n runtime-kansio puuttuu: $runtime" }

# 1) image_symbols.h: SYM_<nimi> <osoite>, HAS_<nimi> <0|1>
$sym = @{}; $has = @{}
foreach ($l in Get-Content $Header) {
    if ($l -match '^#define SYM_(\w+)\s+([0-9A-Fa-f]+)') { $sym[$Matches[1]] = $Matches[2] }
    elseif ($l -match '^#define HAS_(\w+)\s+(\d)') { $has[$Matches[1]] = [int]$Matches[2] }
}

# 2) runtime: korvaukset ja viittaukset
$defs = @{}; $refs = @{}
$files = Get-ChildItem $runtime -Recurse -Include *.cpp,*.h
foreach ($f in $files) {
    $n = 0
    foreach ($l in Get-Content $f.FullName) {
        $n++
        $rel = $f.FullName.Substring($Sdk.Length + 1) + ':' + $n
        # HLE_*-korvaukset seka crt_math.cpp:n CRT_MATH(nimi, fn) ja X86_FUNC(GUEST(nimi))
        foreach ($m in [regex]::Matches($l, '(?:HLE_(?:STDCALL|CDECL|FASTCALL|WRAP)|CRT_MATH|X86_FUNC\(GUEST)\(\s*([A-Za-z]\w*)')) {
            $name = $m.Groups[1].Value
            if ($name -eq 'name' -or $name.EndsWith('_')) { continue }   # makrojen omat parametrit ja nimiprefiksit (hle.h, d3d8.cpp:686, dsound.cpp:941/1125)
            if (-not $defs.ContainsKey($name)) { $defs[$name] = $rel }
        }
        foreach ($m in [regex]::Matches($l, '\b(SYM|GUEST|GUEST_ORIG|GUEST_CALL|XDK_DATA)\(\s*([A-Za-z]\w*)')) {
            $name = $m.Groups[2].Value
            if ($name -eq 'name') { continue }   # makromaarittelyt hle.h/layout.h
            if (-not $refs.ContainsKey($name)) { $refs[$name] = $rel }
        }
        foreach ($m in [regex]::Matches($l, '\bHAS_([A-Za-z]\w*)')) {
            $name = $m.Groups[1].Value
            if (-not $refs.ContainsKey($name)) { $refs[$name] = $rel }
        }
    }
}

# 3) vertailu
$rows = @()
$kaikki = @($defs.Keys) + @($refs.Keys) | Sort-Object -Unique
foreach ($name in $kaikki) {
    $tyyppi = if ($defs.ContainsKey($name)) { 'korvaus' } else { 'viittaus' }
    $tila = if (-not $sym.ContainsKey($name) -and -not $has.ContainsKey($name)) { 'ABSENT' }
            elseif ($has.ContainsKey($name) -and $has[$name] -eq 0) { 'ZERO' }
            else { 'OK' }
    $missa = if ($defs.ContainsKey($name)) { $defs[$name] } else { $refs[$name] }
    $rows += [pscustomobject]@{ Nimi = $name; Tyyppi = $tyyppi; Tila = $tila; Osoite = $sym[$name]; Lahde = $missa }
}
$ongelmat = $rows | Where-Object { $_.Tila -ne 'OK' }
if (-not $VainOngelmat) {
    "runtime viittaa {0} nimeen ({1} korvausta); image_symbols.h: {2} SYM-rivia" -f $rows.Count, $defs.Count, $sym.Count
}
"KUOLLEET KORVAUKSET (HLE_* on, mutta nimi ABSENT tai ZERO): {0}" -f (@($ongelmat | Where-Object Tyyppi -eq 'korvaus')).Count
$ongelmat | Where-Object Tyyppi -eq 'korvaus' | Sort-Object Tila, Nimi | Format-Table Nimi, Tila, Lahde -AutoSize | Out-String -Width 200
"VIITTAUKSET ILMAN OSOITETTA (SYM/GUEST_CALL/HAS_-suojatut, yleensa harmittomia): {0}" -f (@($ongelmat | Where-Object Tyyppi -eq 'viittaus')).Count
$ongelmat | Where-Object Tyyppi -eq 'viittaus' | Sort-Object Tila, Nimi | Format-Table Nimi, Tila, Lahde -AutoSize | Out-String -Width 200
```

Nykyinen tuloste (14:32-käännöksen `image_symbols.h`): 54 kuollutta korvausta = 7 ABSENT (`D3D_BlockOnNonSurfaceResource`, `D3DDevice_GetRasterStatus`, `D3DDevice_GetTile`, `D3DDevice_LoadVertexShaderProgram`, `D3DDevice_RunPushBuffer`, `D3DDevice_SetRenderTargetFast`, `XInputGetCapabilities`) + 47 ZERO (37 verkko, `BinkDoFrame/GetSummary/NextFrame`, `crt_atan/cos/log/log10/sin/sqrt/tan`); viittauksia ilman osoitetta 3 (`D3D_FvfShader`, `IDirectSoundBuffer_SetEG`, `BinkWait`). Vaiheiden 1–3 jälkeen ABSENT-rivejä ei saa olla.

Tiedostot, joita tämä suunnitelma tuotti: `C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad\tarkista-hle-nimet.ps1` (uusi, ajettu); `C:\Users\jonin\system_upgrades.md` (lisätty merkintä: skannaus erilliseen tiedostoon + nimitarkistus käännöksen esivaiheeksi).