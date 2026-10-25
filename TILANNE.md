# Sims Bustin' Out -recomp: tilanne

Päivitetty 24.10.2026.

## Tehty
- Pelikansio luotu `C:\XboxRecomp\uusi-peli.ps1`:llä, SDK vieressä `C:\XboxRecomp\XboxXBE-Recomp`.
- Levykuva purettu `assets\`-kansioon (59 tiedostoa, 2,0 Gt, `default.xbe` mukana).
- VS Build Toolsiin asennettu Clang + CMake + Ninja. `xbe_symbols.exe` kääntyy ja toimii.
- **Ongelma:** peli on XDK-buildia **5558**, SDK tukee vain 4831 ja 5849.
- **Koe:** 5849:n `signatures.toml` lainattuna 5558-kansioon löytää **186/243 symbolia ja 12/12 datakohdetta**.
  4831:llä vain 58/248. Puuttuvat 57: verkko (WSA*/XNet*/socket*/XOnline*), Bink (peli käyttää XMV:tä),
  CRT-matematiikka (crt_sin/cos/tan/atan/log/log10/sqrt) sekä D3D: `D3DDevice_BeginPush`, `EndPush`,
  `BlockOnFence`, `FlushVertexCache`, `SetPalette`, `D3D_BlockOnResource`, `D3D_KickOffAndWaitForIdle`,
  ja DSound: `CDirectSoundStream_AddRef`/`Release`.
- `xdk\5558\` luotu SDK:hon lainatuilla 5849-tiedostoilla (merkitty KOKEILU-kommentilla). `layout.toml`:n
  arvoja ei ole varmennettu 5558-koodista.

## Käännös ja ensimmäiset ajot (24.10. klo 13:55–14:28)
- Koko recompilointi + kääntö kesti vain **1 min 40 s** (ei 30–90 min). Uudelleenkääntö configin muutoksen
  jälkeen 48 s. Exe: `out\build\release\SimsBustinOut.exe`, loki `out\build\release\logs\simsbustinout.log`.
- Ajo 1: kaatui 0,6 s: "indirect call to 002284DD has no recompiled function" → lisätty `0x2284DD = ""`
  configin `[functions]`-tauluun (automaattisesti, `korjaa-puuttuvat-funktiot.ps1`).
- Ajo 2: pääsi pidemmälle (D3D12 adapter ok, dsound effects image ok), kaatui 0,95 s D3D-kirjastokoodissa:
  `exception C0000005, read guest 00003214`, esi=0, ecx=edi=0x266618, kutsupino 0x253191 → 0x247A68 →
  0x24FEEB/0x24FF5C → 0x246063 (kaikki D3D-sektiossa) → 0x16F0D2 (peli). Eli D3D-koodi luki NULL-osoittimen
  kentästä +0x3214. 5849:n layout sanoo CDevice_Size = 0x24A0, joten 5558:n laiterakenne on eri kokoinen tai
  host kirjoittaa laiteosoittimen väärään globaaliin (lainatulla signatuurilla löydetty `D3D_pDevice`?).
  **Tämä on käsityötä:** selvitä 0x253191:n ympäristö (mikä globaali luetaan), vertaa `config\game.toml`:n
  `[data] D3D_pDevice` -osoitteeseen ja 5558:n CDevice-rakenteeseen.
- Työkalut: `kaanna-uudelleen.bat` (uudelleenkääntö), `korjaa-puuttuvat-funktiot.ps1` (automaattisilmukka
  [functions]-lisäyksille; loki `out\iterointi-loki.txt`, kuvat `out\kuvakaappaukset\`).

## Läpimurto: valmis 5558-tuki löytyi (24.10. klo 14:30)
- GitHubissa on **abilliontokens/bustinout-recomp** (USA-XBE, SHA-1 6feea04a…, 22 committia, 19.10.2026):
  pelattava Linuxilla, Windows-exe ristiinkäännetty mutta ei ajettu. Kloonattu scratchpadiin
  (`refs/bustinout-recomp`), 20 docs-tiedostoa (boot-crash.md, gap-analysis.md, windows-target.md, playtest.md…).
- Sen SDK-fork **abilliontokens/XboxXBERecomp**, haara `bustinout-port`, commit 7f2e294: **aito `xdk/5558/`**
  (signatures.toml 256 symbolia + layout.toml, arvot todistettu tämän pelin koodista).
- Kopioitu SDK:hon `XboxXBE-Recomp\xdk\5558\` (lainatut 5849-kopiot siirretty `xdk\5558-lainattu-5849\`).
  Tulos Europe-XBE:llä: **194/242 symbolia, 10/12 dataa**; `D3DDevice_Reset = 0x246030` löytyi (se puuttui
  ja aiheutti ajon 2 kaatumisen, ks. USA-projektin docs/boot-crash.md: sama vika, sama korjaus).
  Puuttuvat 48 = verkko, Bink, CRT-matikka: USA-projektin gap-analysis.md mukaan 41 niistä on sektioita,
  joita XBE:ssä ei ole (0 on oikein), 7 aidosti löytymättä.
- `DSound_StreamVtable(2)` pudotettiin kaksiselitteisenä → arvot 0x2F90D8/0x2F90C8 otettu 5849-ajosta
  (USA-arvot 0x2F9038/0x2F9028 + 0xA0, täsmää). **Europe-XBE = USA-XBE + 0xA0 D3D/.rdata-sektioissa.**
- Käännetty 14:32: 27 536 funktiota (USA 27 492). Korjaussilmukka käynnistetty uudelleen (90 s/kierros).

## Ajo 3 aidolla 5558-configilla (14:33–14:45): pyörii, mutta ruutu on musta
- Peli pysyy käynnissä 90 s+ ilman yhtään virheriviä; loki loppuu riviin `dsound: effects image` (0,4 s).
- Ikkuna (`The Sims Bustin' Out`) on täysin musta 12 s ja 55 s kohdalla sekä reunattomana että ikkunoituna
  (`out\build\release\settings.toml`: display_mode = 'windowed', 1280x720). Kuvat `out\kuvakaappaukset\ikkuna-*.png`.
- Huom: taustalta käynnistetty peli-ikkuna jää selaimen taakse (Windows ei anna varastaa fokusta); kaappaus
  pitää tehdä nostamalla ikkuna SetWindowPos TOPMOST:lla ja CopyFromScreenillä (PrintWindow antaa D3D12:sta mustaa).
- USA-projektilla oli sama vaihe (docs/black-scene.md, movie-draw.md, movie-quad.md); korjaukset ovat heidän
  SDK-forkissaan. Fork haarautui upstreamista commitissa 65e959d: fork +7865/-382 (50 tiedostoa), upstream sen
  jälkeen +3569/-331 (46 tiedostoa). Forkin worktree scratchpadissa `fork-sdk`.
- Käynnissä: selvitys (workflow "sims-musta-ruutu") — diagnoosi, päätös fork vs upstream, askeleet.

## Ajo 4 (14:55): PELI PIIRTÄÄ — intro ja The Sims -logo näkyvät
- Ensimmäisen selvityksen (`XDK-5558-selvitys.md`) perusteella tehty kolme korjausta:
  1. `config\game.toml` `[symbols]`: lisätty 5849-ajon löytämät `D3DDevice_GetTile = 0x245610`,
     `D3DDevice_LoadVertexShaderProgram = 0x24C0C0`, `D3DDevice_SetRenderTargetFast = 0x246360`,
     `IDirectSoundBuffer_SetEG = 0x28DFCE` (forkin 5558-sarja ei tunne näitä nimiä → host-korvaukset olivat kuolleita).
  2. `src\game\hooks.cpp`: `HLE_STDCALL(D3DDevice_InsertCallback, 3)` ajaa callbackin heti (USA-projektin hook;
     upstream-runtime ei toteuta InsertCallbackia → moottorisäie odotti semaforia ikuisesti → ei piirtoa).
  3. `xdk\5558-lainattu-5849` siirretty pois `xdk\`-kansiosta (`XboxXBE-Recomp\xdk-5558-lainattu-5849-koe`),
     koska cmake globaa `xdk/*/`.
- Kääntö 24 s. Kuvat `out\kuvakaappaukset\hook-12s.png` (introvideon 3D-kohtaus), `hook-55s.png` (The Sims -logo).
- **Jäljellä oleva kuvavika:** kuva n. 2× liian suuri, vain vasen yläneljännes näkyy → pelin 2× supersamplattu
  takapuskuri esitetään 1:1 (fork korjasi: "honour the title's supersampled back buffer", CDevice_SuperSampleX/Y,
  USA docs/picture-rect.md, movie-quad.md). Lisäksi musta palkki ylhäällä (4:3 → 16:9 -kirjekuori) ja ikkunan
  alareunan n. 20 px jää piirtämättä. Selvitys "sims-musta-ruutu" (workflow) tutkii fork vs upstream -korjaukset.
- Peli-ikkuna ei saa fokusta taustalta käynnistettäessä: testaa itse ajamalla `out\build\release\SimsBustinOut.exe`.
  `settings.toml` siellä on nyt windowed 1280x720 (vaihda 'borderless' halutessasi).

## Ajo 5 (15:43, Joni itse): intro + logo ok, Start-napin jälkeen kaatui 14,6 s
- Exe oli siirtotyön käännös (15:41, SuperSampleScale 2x2 lokissa). Kaatuminen `C0000005 read guest 00000080`,
  kutsupino DSOUND: 0x29969D → 0x28C9EE → 0x28E083 (= `IDirectSoundBuffer_PlayEx` 0x28E062 sisällä) ← .text.
  Syy: upstream-runtime ei korvaa `PlayEx`:iä → recompiloitu Xbox-äänikirjasto ajettiin host-puskuriolion päällä
  → NULL-luku. Selvitys ennusti tämän ("seuraava seinä kun peli etenee").
- Korjaus: `XboxXBE-Recomp\runtime\xdk\dsound.cpp`: `BUFFER(PlayEx, 4)` Playn viereen (forkin c5ba47f mukaan),
  SYM-varamääritys ja nimi SYM-listaan. Kääntö odottaa supersample-siirtotyön valmistumista (sama out-kansio).
- Selvityksen mukaan seuraavat seinät: 6 kernel-exporttia (IoCompletion-perhe, ObOpenObjectByName,
  IoQueryVolumeInformation; forkin patch applikoituu puhtaasti) ja mustan gamma-rampin suojaus (fork 57c5e46).

## Session 1 lopputila (24.10. klo 16:00)
- SDK:n työpuussa (ei committoitu, `git -C C:\XboxRecomp\XboxXBE-Recomp diff`) on 4 tiedoston muutos, tallennettu myös
  `sdk-muutokset-2026-10-24.patch`:
  - SuperSampleScale-malli: `runtime\xdk\d3d8.cpp` (SetMultiSampleType :218, UpdateSuperSample :227, kutsut
    SetFrameBuffers/ApplyRenderTarget), `runtime\gpu\state.h:192` super_x/y, `runtime\gpu\d3d12.cpp:2022` c.surf * super.
    Loki: "d3d: CreateDevice multisample 2222, SuperSampleScale 2x2". Kuva oikean kokoinen: `out\kuvakaappaukset\super-12s.png`
    (vertaa `kontrolli-12s.png` = sama ilman korjausta, 2x zoom). Agentin tarkastuskierros jäi kesken (pysäytetty) →
    diff kannattaa lukea läpi: tarkista ettei D3DDevice_SetRenderTargetFast (:549) jätä c-38/c-37-vakioita vanhaan
    skaalaan (Musta-ruutu-selvitys.md 1b(iii)).
  - `runtime\xdk\dsound.cpp`: `BUFFER(PlayEx, 4)` + SYM-varamääritys FFFFFF0F + SYM-lista. Start-kaatuminen poissa:
    lopullinen ajo 15:57 pyöri 44+ s ilman virheitä (loki `out\build\release\logs\simsbustinout.log`).
- Exe `out\build\release\SimsBustinOut.exe` (15:55) + DLL (15:55:54) ovat ajantasaiset. **Huom:** jos vain runtime
  muuttuu, exe tulee DLL:ää uudemmaksi ja runtime "rakentaa" DLL:n itse käynnistyksessä (~50 s, prosessi vaihtuu);
  `kaanna-uudelleen.bat` koskettaa nyt DLL:n aikaleimaa käännöksen jälkeen, joten tätä ei enää tapahdu.
- `kaappaa-peli-ikkuna.ps1` jumittui kerran (15:58) kun peli pysyi elossa; syy ei selvinnyt. Inline-kaappaus
  (sama koodi suoraan PowerShell-istunnossa) toimi aina. Tutki tarvittaessa tai käytä inline-versiota.
- Selvitysraportit: `XDK-5558-selvitys.md` (5558-tuki, symbolit, layout) ja `Musta-ruutu-selvitys.md` (askel
  askeleelta vaiheet 0–9, valmiit patchit scratchpadissa `sims-patches\` — HUOM scratchpad on sessiokohtainen:
  `C:\Users\jonin\AppData\Local\Temp\claude\C--Users-jonin\75278e87-e40e-4339-b7d5-ff97bf464c9e\scratchpad\`;
  jos se on siivottu, patchit tehdään uudelleen §0.3:n komennoilla refs-repoista, jotka ovat samassa scratchpadissa:
  `refs\bustinout-recomp` (USA-peli), `refs\abilliontokens-XboxXBERecomp` (fork, haara origin/bustinout-port),
  `fork-sdk` (worktree). Kloonaa uudelleen GitHubista jos puuttuvat.)

## Sessio 2 (24.10. klo 16:00–16:35) — PELI PELATTAVA VALIKOSTA TALOON — LUE TÄMÄ ENSIN UUDESSA SESSIOSSA
Musta-ruutu-selvitys.md:n vaiheet 0, 3, 4, 6, 7 ja 8 tehty. Kaikki käännökset 6–24 s, ei yhtään virhettä.
- **Git-pohja (vaihe 0):** SDK:ssa haara `sims-5558` (`git -C C:\XboxRecomp\XboxXBE-Recomp log`): `3a267e8` xdk/5558,
  `2a23ae1` SuperSampleScale + PlayEx, `be45452` kernel-exportit (IoCompletion-perhe, IoQueryVolumeInformation,
  ObOpenObjectByName, NtSetInformationFile luokka 30) + gamma-suoja + picture_top. Työpuu puhdas (kokeilukansio
  `xdk-5558-lainattu-5849-koe\` poistettu sessiossa 2b). Pelirepo: `5ae8933` sessio 1, `3b46b70` sessio 2.
  `sdk-muutokset-2026-10-24.patch` on nyt historiaa (sama sisältö commitissa 2a23ae1).
- **Pelipuoli (vaihe 3):** `src\game\hooks.cpp` = InsertCallback + frame-step-hookit `sub_00123400` (kvantisoija) ja
  `sub_00123540` (frame tick) Europe-osoitteilla (data 0x35D2B8 / 0x30AB70 / 0x35D2D0); PAL 1,2×-ylinopeus poissa.
  Hook kirjoittaa releaseen kerran rivin `simsbo: frame step hook live, first elapsed … -> N ticks` = todiste, että
  DLL-mallin `X86_FUNC(sub_…)`-override toimii. Bisect: `SIMSBO_NO_STEP=1`. `[functions] 0x66A10` EI tarvittu:
  recompi löysi funktion itse (`generated\game\x86_func_mapping.cpp` rivi 2164).
  `src\common\input_kbm.cpp/.h` (USA:n Windows-polku ilman overlay-porttia): WASD = vasen tikku, nuolet = D-pad,
  Space = A, Esc/Backspace = B, Enter = Start, Tab = Back, Q/E/R/F + hiiren oikea nappi/rulla = kamera, X/Y, Z/C =
  liipaisimet, 1/2 = black/white. `src\common\game_info.cpp`: `.message = &kbm::Message`, `.pad = &PadHook`
  (SIMSBO_STEP-askellus + SIMSBO_AUTOSTART), `.picture_top = 16, .picture_height = 448`.
- **Ajot:** `kaappaa-peli-ikkuna.ps1` osaa nyt `-Askeleet '16:START','20:START','24:A','28:A','32:RIGHTx3','36:A','40:A'`
  (SIMSBO_STEP-tokenit, NONE 2 s välissä, kirjoitus uudelleenyrityksellä) ja `-Nappaimet '16:ENTER'` (keybd_event
  150 ms pito; SendKeys ei kelpaa, koska <1 ms painallus ei ehdi padin ruutukohtaiseen lukuun). Tulokset kansiossa
  `out\kuvakaappaukset\`: `s2a-12s` intro oikean kokoinen ilman palkkeja, `s2b-23s` päävalikko (Continue/Bust Out/
  Free Play/Credits = tallennus on olemassa), `s2b-27s` Select Save, `s2b-35s` Customize A Sim (RIGHTx3 vei
  kursorin ✓-merkkiin), `s2b-43s` Loading, **`s2b-75s` TALO LATAUTUNUT** (klubitontti, HUD, tutoriaalidialogi
  "Peter: Hey Pat!"). `s2d-23s`: näppäimistö-Enter ohitti intron (ikkunan pitää olla etualalla; SetForegroundWindow
  onnistuu taustalta vasta toisella yrityksellä). 78 s ajo ilman virheriviä; ei `kernel export … not implemented`,
  ei `indirect call … has no recompiled function`.
- **Käännös:** `kaanna-uudelleen.bat` toimii cmd:stä (tuplaklikkaus) ja Git Bashista (`cmd //c '…\kaanna-uudelleen.bat'`),
  mutta **ei** PowerShellin `&`-operaattorilla eikä `cmd /c "…bat 2>&1"`-muodossa (PowerShell rikkoo `%VSDIR%`- ja
  `>nul`-osat: "'n' is not recognized"). Lokissa `simsbo: frame step hook live …` = hook ja DLL-linkitys ajan tasalla.
- **Käsin pelitesti (Joni, 24.10. klo 16:35):** 3 min 41 s, "toimi ihan hyvin, ei ongelmia". Loki: Exit (0), ei yhtään
  virheriviä; tallennus kirjoitettiin (`hdd\E\UDATA\45410043\0069006D1352\SaveGame` + `Config` päivittyivät) → myös
  kernel-exportit ja tiedosto-IO toimivat. Ohjainta ei testattu (ei padia kytkettynä).
- Tarkistustyönkulku (4 rinnakkaista katselmointia, 610 k tokenia, 15 min): **0 vikaa**, 9 huomiota → alla.

### Tarkistustyönkulun tulos (sessio 2, 4 agenttia: disassembly / recomp-output / SDK / porttaus)
Varmistettu tavutasolla Europe-XBE:stä (capstone): `sub_00123400` ja `sub_00123540` alkavat oikeista osoitteista, kaikki
USA-hookin instruktiofaktat +0x110/+0xA0-siirrettyinä täsmäävät (fdiv [0x35D2B8] 0x123453/0x1234BE, fistp [0x30AB70]
0x123476/0x1234E1, add [0x35D2D0] ×3, `ret 4` ×3, `call 0x123400` 0x1235B7, thunk 0x165F40 `jmp 0x123540`, vtable
0x2EEDC8+0x18). Override-mekanismi: DLL:n `sub_X` on `/alternatename`-varanimi `__imp__sub_X`:lle; exe exporttaa vahvan
`sub_X`:n ja DLL linkittyy exen import-kirjastoon → sekä DLL:n sisäinen suora kutsu että mapping-taulu osuvat hookiin.
Hookin lisäys vaatii DLL:n uudelleenlinkityksen (cmake tekee sen itse; vanha DLL exen vieressä = hook ei näy).
InsertCallback-argumentit (a[1] callback, a[2] context, ret 0xC) ja PlayEx (a[3] flags, ret 16) oikein. Kernel-patchin
kaikki tunnisteet ovat upstreamissa, ordinaalit 76/191/223/227/232/243 kerran kussakin. Porttaus: hook-rungot,
input_kbm ja StepPad/AutostartPad tavulleen USA:n mukaiset listattuja muutoksia lukuun ottamatta.
Huomiot (ei toimenpiteitä, paitsi 1):
1. **Tehty:** `kMaxTicks` 250 → 300: Europe-kuva on kiinteästi PAL (`[0x2E3E18]` = vakio 1 .rdata:ssa, 1/60-haara 0x123B8E
   on kuollutta koodia), joten moottorin katto on 15 × 0,02 = 0,30 s. Vaikuttaa vain > 250 ms ruutuihin (latauspätkät).
2. `[0x35D2B8]`:llä on kuudes lukija 0x165B03 ("CPU:"-debug-lukema, `%.2f`): näyttää ~20× suurempia lukuja hookin kanssa;
   vain näyttöä, sama USA:ssa.
3. `this+5`-tavu pysyy asetettuna `sub_00123540`:n häntäkutsun (`jmp [edx+8]` 0x1236C5) ajan ja getter `sub_00123500`
   palauttaisi silloin tick-määrän (1..300) eikä 1:tä. Kutsuuko mikään sitä tickin sisällä: ei varmennettu; sama USA:ssa.
4. `D3DDevice_SetRenderTargetFast` (0x246360): HLE laskee SuperSampleScalen uudelleen `ApplyRenderTarget`issa, kirjasto
   ei; **ei kutsuta tässä pelissä** (yksikään recompiloitu funktio ei kutsu sub_00246360). Fidelity-korjaus tarvittaessa:
   siirrä `UpdateSuperSample();` d3d8.cpp:568 → `D3DDevice_SetRenderTarget`iin rivin 575 jälkeen.
5. Gamma-suoja muuttaa **kaikki** mustat rampit identiteetiksi → pelin fade-to-black SetGammaRampilla näyttäisi viimeisen
   ruudun kirkkaana. Sama USA:ssa. Jos siirtymä näyttää oudolta käsin pelatessa, tämä on syy.
6. Vertex-ohjelman pisteet (D3DPT_POINTLIST) ovat supersample-pikselin kokoisia = puoli ruutupikseliä; render scale < 2
   → voi jäädä rasteroimatta. Ei tiedetä, piirtääkö peli pisteitä.
7. `IoQueryVolumeInformation`: `VolumeOf` ei tunnista `\??\D:`-etuliitettä → purettu assets-kansio (ei ISO) raportoidaan
   FATX-kiintolevynä, ei optisena. Jos tallennus/levyn tunnistus käyttäytyy oudosti, tämä on epäilty.
8. `picture_top/height` 16/448 pätee vain 640×480-tilaan (720p-tila olisi 1200×660); peli käyttää vain 640×480:aa.
9. `.missing`-teksti game_info.cpp:ssä poikkeaa USA:sta ("DATA and movies folders") — sessio 1:n sanamuoto, ei muutettu.

## Sessio 2b (24.10. klo 16:35–17:05) — viimeistely: overlay, cheatit, julkaisupaketti, upstream-haarat
- **F1/F2-overlay portattu** USA-projektista: `src\common\overlay.cpp/.h` (ImGui 1.91, `.ui = &overlay::Draw`
  game_info.cpp:ssä). F1 = asetukset (näyttötila, resoluutio [harmaana borderlessissä], vsync, fps-katto ml. upstreamin
  "Monitor refresh rate" = `app::kFpsCapRefresh`, render scale 50–200 %, anisotropia, näppäinkartta, cheatit; Apply/Save),
  F2 = fps, frame time -käyrä, host/render/aspect ja backend-laskurit. Karsittu USA:sta: aspect ratio-, ultrawide- ja
  HDR-rivit (ei upstream-vastinetta) ja `g_perf_hle_ns`. `input_kbm.cpp` ei syötä padia, kun F1 on auki
  (`overlay::SettingsOpen()`). F2-paneelin tausta tummennettiin (alpha 0,45 → 0,80), koska harmaat rivinimet eivät
  näkyneet kirkkaan kohtauksen päällä; nimisarakkeeseen lisättiin väljyyttä.
- **Cheatit portattu** (`src\game\cheats.cpp`, `src\common\cheats.h`): optiot 0x310C38 (USA 0x310B98 + 0xA0), salpa
  0x3DD010, pad manager 0x35D490, setter `sub_000720A0` (retn 4, ecx = optiot), hook per-frame-päivitykseen
  `sub_000122C0` (USA 0x12280 + 0x40). Koodit syötetään pelin omalla setterillä title-säikeessä vain, kun peli itse
  tunnistaisi ne (salpa auki, pad manager ja tunnistin olemassa). Testaamatta pelin sisällä (F1-ruudut ovat harmaana,
  kunnes talo on ladattu); mekanismi varmennettu disassemblystä (tarkistustyönkulku, 0 vikaa).
- **Ajot:** `s2f-14s/20s` F1- ja F2-paneelit piirtyvät; `s2j-14s` borderless 2560×1440, render target **1920×1440
  (100 %)**, 16:9 (widescreen TV), 60 fps vsyncillä, 0 late / 0 block. `settings.toml`: borderless, width/height 0
  (= työpöytä; 960×720 arvoilla render target jäi 960×720), vsync, fps_cap 60.
- **Julkaisupaketti:** `release.bat` ajettu → `SimsBustinOut.zip` (57 MB: exe, `compiler\` ilman work-kansiota, tyhjä
  `assets`, README.md, THIRD_PARTY.txt, VC-runtime-DLL:t). Ei pelikoodia: ensimmäinen käynnistys kääntää sen pelaajan
  omasta XBE:stä. Kokeilukansiot `xdk-5558-lainattu-5849-koe` (SDK) ja `compiler\xdk\5558-lainattu-5849` poistettu.
- **README.md** kirjoitettu julkaisua varten (ominaisuudet, näppäinkartta, kääntöohje, kiitokset abilliontokensille).
- **Upstream-PR:t avattu (Jonin lupa klo 17:10):** fork `https://github.com/JoniNykanen/XboxXBERecomp` (remote `joni`),
  haarat `sims-5558` (koko työhaara), `xdk-5558` (3f05c44: layout.toml + uudelleen generoidut signatuurit 198/246,
  round-trip 258/258 identtinen) → **PR #15** `https://github.com/TGP482/XboxXBERecomp/pull/15`, ja
  `supersample-picture-rows` (7aae77d SuperSample + PlayEx, 7347667 picture rows abilliontokensin tekijyydellä) →
  **PR #16** `https://github.com/TGP482/XboxXBERecomp/pull/16`. Kernel-osa jätettiin pois (upstreamin PR #7 kattaa
  saman), gamma-suoja pois (huomio 5). PR #8 (runtime-korjaukset) voi konfliktoida #16:n kanssa; luvattu rebase, jos #8
  menee ensin sisään. Tarkistettu ennen avaamista: haaroissa vain SDK-koodia ja XDK-taulukoita, ei pelitiedostoja;
  pelirepo seuraa vain src/config/docs-tiedostoja (.gitignore: assets, out, generated, hdd, *.zip).
  Työpuu: `…\882f5d8e-…\scratchpad\sdk-upstream-pr` (git worktree SDK-kloonista), PR-tekstit `pr-body-*.md` siellä.
- **Graafiset parannukset USA-projektissa (3 agentin survey, `graafiset-survey.txt` scratchpadissa):** kaikki toimiviksi
  todistetut ovat jo tässä buildissa tai upstreamissa (picture rows, SuperSampleScale, gamma-suoja, 16:9 ilman palkkeja,
  render scale, anisotropia, fps-katto + oikea nopeus). Ainoa aidosti puuttuva ja toimivaksi mitattu: **HDR-ulostulo**
  (fork-commit b3b8b9d, scRGB FP16 -swap chain, ~410 riviä, d3d12.cpp-osa cherry-pickautuu upstreamiin puhtaasti, vain
  settings.cpp/README konfliktoivat; hyödyttää vain HDR-näytöllä). Pieniä, vaarattomia: UV-rajaus 2D-spriteille
  (583d612, poistaa fonttiatlaksen saumat render scale > 100 %), ikkunan nuolikursori (583d612, 3 riviä main.cpp),
  CopyByRegion-skaalakorjaus. **Ei kannata:** Hor+ ultrawide (rikki USA:ssakin, peruttu kahdesti), dirty-state-optimointi
  (23 konfliktia, vain suorituskyky), Vulkan-korjaukset (ei D3D12-vastinetta tarvita). Supersample-alassuodatusta ei
  USA:ssa tehty.

## Sessio 2c (24.10. klo 17:20–17:30) — kaveripaketti pelitiedostoineen
- **`SimsBustinOut-PC-pelitiedostoineen.zip`** (1,7 Gt, 85 tiedostoa, projektikansiossa, gitignoroitu) = `SimsBustinOut.zip`:n
  sisältö + `assets\` (59 tiedostoa, 2,0 Gt) + valmis `SimsBustinOut.dll` + testattu `settings.toml`, kaikki yläkansiossa
  `SimsBustinOut\`. Ei `compiler\work`-kansiota, ei `hdd\`-tallennuksia, lokeja eikä pdb:tä.
- **Valmis DLL ilman uudelleenkääntöä:** runtime (`game_code.cpp` `Current()`) pitää leimaamatonta DLL:ää ajantasaisena,
  kun se on exeä uudempi; exe ja DLL olivat 79 ms päässä toisistaan ja zipin DOS-aikaleima on 2 s tarkkuudella, joten DLL:n
  aikaleima nostettiin pakettiin 60 s exeä uudemmaksi (17:07:16 vs 17:06:16). Jos purkuohjelma ei säilytä aikaleimoja,
  runtime kääntää koodin itse `compiler\`-kansiosta (varapolku, ~2 min, 1 Gt levyä).
- **Testattu:** `7z t` ok; puhdas purku scratchpadiin + 40 s ajo: loki ilman `Game code: building`-riviä (ei käännöstä),
  `default.xbe` löytyi, D3D12-laite + `frame step hook live`, ei yhtään virheriviä. Exe tarvitsee vain Windowsin omia DLL:iä
  (d3d12, dxgi, D3DCOMPILER_47, UCRT) + mukana olevat msvcp140/vcruntime140(_1); SDL staattinen (llvm-objdump tarkistettu).
- Paketti tehtiin käsin staging-kansiolla; `release.bat kaverille` -tila ja SDK:n hash-leima kirjattu `system_upgrades.md`:hen.

## Sessio 3 (24.10. klo 18:00–20:10) — pelirepo julkaistu, parannuskartoitus
- **Pelirepo julkinen:** `https://github.com/JoniNykanen/SimsBustinOut-Recomp` (haara `main`, remote `origin`, 23 tiedostoa, ei
  pelidataa). Ei vielä LICENSE-tiedostoa. SDK-fork `https://github.com/JoniNykanen/XboxXBERecomp` (sims-5558).
- **Parannuskartoitus** (workflow, 9 lukijaa + 2 tarkastajaa/ehdotus, pysäytetty kesken): 89 ehdotusta, 61 läpäisi, 9 hylätty,
  19 tarkistamatta. Koko taulukko: `Parannuskartoitus-2026-10-24.md` (ei committoitu). Tärkeimmät:
  1. **Upstreamissa on vahingossa avattu PR #17 "Sims 5558"** (koko sims-5558-haara, tyhjä kuvaus, avattu 17:45): tuplaa #15+#16
     ja tuo kernel-commitin be45452, joka piti jättää pois. Sulje: `gh pr close 17 --repo TGP482/XboxXBERecomp --comment "…"`,
     ÄLÄ poista haaraa. Lisäksi: väite "PR #7 kattaa be45452:n" on väärä — #7 kattaa vain ordinaalit 76 ja 232 (ja määrittelee ne
     eri tiedostoissa → linkkivirhe rebasessa); iocp 191/223/227, ObOpenObjectByName 243 ja class 30 ovat vain meillä.
  2. **`XInputGetCapabilities = 0x2DAD67` puuttuu `config\game.toml` [symbols]-taulusta** (.XTLID 0x1003B): pelin tärinäkysely
     sub_00126560 ajaa recompiloitua XPP-koodia HLE-kahvan päällä ja lukee caps-tavut pinoroskasta. Yksi rivi + käännös.
  3. **Release-loki pudottaa kaikki LOGI/LOGW-rivit** (NDEBUG → kVerbose=false): `cheats: entered …` (cheats.cpp:105/114),
     `simsbo step: holding` (game_info.cpp:46) ja `LOG_FIRST` eivät voi näkyä → vaihda LOGR. Aja kerran verbose-build
     (Musta-ruutu-selvitys Vaihe 2: -O2 ilman NDEBUG) ja lue dsoundin "exports running their original code" -rivi.
  4. **Lisenssi:** 7/8 src-tiedostoa on 70–100 % rivi-identtisiä abilliontokensin USA-portin kanssa, jolla ei ole lisenssiä.
     Avaa issue (pyydä MIT/lupa, kerro että Europe toimii natiivisti Windowsilla heidän SDK-työllään), lisää oma MIT LICENSE.
  5. **Kieli:** Europe-levy on monikielinen (FR/DE/IT/ES/NL quickdat.arc, ea_games_*.xmv), runtime vastaa XC_LANGUAGE=1 aina.
     Nollakoodinen testi: `assets\system.cnf` `localization_test = 3` (FR). Toteutus: settings.toml `language` → xboxkrnl.cpp
     case 7, tai pelipuolen hook sub_001001F3. Pelissä on myös oma kielivalintaruutu (optiotavu 0x310D9C = 0xFF).
  - Pieniä varmoja: PR #15:n layout.toml-otsake väittää USA-kuvaa (korjaa ennen katselmointia) + [xtlid]-taulu 37 verkko-id:lle;
    sims-5558 kantaa yhä forkin 194/242-sarjaa → `git checkout xdk-5558 -- xdk/5558/signatures.toml`; SDK:n `abt`-remote
    osoittaa Temp-kansioon → `git remote set-url abt https://github.com/abilliontokens/XboxXBERecomp.git`; CMakeLists.txt:n
    FetchContent-varapolku hakee upstream mainin (ei 5558:aa) → `JoniNykanen/XboxXBERecomp` tag be45452; README: oletukset
    (aniso 0), "pura kirjoitettavaan kansioon", ohjainnapit Black=RB/White=LB/L,R=liipaisimet; `git rm sdk-muutokset-*.patch`.
  - Huomiot 4, 5 ja 7 eivät koske tätä peliä (SetGammaRamp 0 kutsujaa; IoQueryVolumeInformation tavoittamaton). Seitsemän
    crt_*-nollaa ovat lopullisia (EU-kuvassa ei runkoja, x87 inline) → Vaihe 5:n Ghidra-haku turha. Intro 43,4 s vs ffprobe
    44,9 s → XMV tahdistuu ääneen, ok. PR #4/#5/#6 ei tarvita (Reset-rivit ovat pelin omia Reset-kutsuja).
  - Upstream-seuranta: PR #8 korvaa #16:n SuperSampleScale-mallin omallaan (varma konflikti) ja sen AddKeyboard syöttää
    näppäimistön porttiin 0 ennen PadHookia (W/S=liipaisimet, Q/E=white/black → vahinkocheatit) → tarvitaan guard; PR #9 tuo
    saman PlayEx-hunkin (kommentoi #16:een); PR #3 muuttaa XMV-soittimen FlushEx-sulkupolkua → testaa video→video ennen mergeä.
  - Hylätty: StepPad SDK:hon (upstreamissa on jo `harness/pad_script.cpp`, SCRIPT-ympäristömuuttuja), PersistDisplay-korjaus
    (peli ei kutsu), SetFilter/SetI3DL2Source (peli ei kutsu, thunk luettiin väärin), XMV-vblank (kuollutta koodia).

## Sessio 4 (25.10.2026) — parannukset A–D
- **Pelirepo `058b67c` (pushattu):** `XInputGetCapabilities = 0x2DAD67` configiin (image_symbols.h: HAS_ 1); pelin LOGI-rivit
  (cheats, step, autostart, kbm) → LOGR; LOG_FIRST (= LOGI) korvattu omalla kerran-lipulla; cheats-kommentit: taulunrakentaja
  sub_00072130, setteri sub_000720A0; F1 Input -osioon Controller-rivi (Black = RB/R1/R jne.); README: kloonausosoite,
  kirjoitettava kansio, oletukset (aniso pois), ohjaintaulukko, License-osio; **LICENSE (MIT)**; CMake FetchContent →
  `JoniNykanen/XboxXBERecomp` sims-5558; `sdk-muutokset-*.patch` poistettu. Kääntö 25 s.
- **SDK sims-5558 (pushattu joni-remoteen):** `15761bc` upstream-PR #1:n 1.#INF → asfloat (Wine/Proton), `b65d71f`
  signatures.toml xdk-5558-haarasta. `abt`-remote → `https://github.com/abilliontokens/XboxXBERecomp.git`.
- **Julkiset:** PR #17 suljettu kommentilla (haara jäi). PR #15:een commit `f21dcfd`: layout.toml-otsake nimeää USA-osoitteet ja
  Europe +0xA0. Lisenssipyyntö: https://github.com/abilliontokens/bustinout-recomp/issues/3 (odottaa vastausta; jos he eivät
  anna lupaa, README:n License-osio ja siirretty koodi pitää miettiä uudelleen).
- **Testit (kuvat `out\kuvakaappaukset\s4-*`):**
  - Cheatit pelissä toimivat: F1 → Cheat gnome + Give money → loki `cheats: entered Cheat gnome (code 0): on, … mask 01` ja
    `Give Money (code 1): on, … 03`. Tonttua ei näkynyt kuvakulmassa, Give Money -valintaa tontun valikosta ei kokeiltu.
  - Nopeus: pelikello 25 peliminuuttia / 30,85 s fps_cap 60:llä ja 25 / 30,92 s rajattomalla (fps_cap 0, vsync pois) →
    maailman nopeus ei riipu fps:stä. Yksi RIGHT-pulssi siirtää CAS-rivillä tasan yhden kohdan myös rajattomana. 2 s
    nuolinäppäimen pito: tulos epäselvä (4 kohdan kiertävä rivi); kameran zoom/kierto >60 fps:llä yhä mittaamatta. F2:n fps-
    lukemaa rajattomana ei saatu (peli ei saanut fokusta).
  - Kieli: `assets\system.cnf` `localization_test = 3` **ei vaikuta** (valikot englanniksi) → kieli vaatii hookin
    (sub_001001F3 / xboxkrnl case 7) tai pelin oman kielivalintaruudun avaamisen tyhjällä tallennuksella. Palautettu 255:ksi.
  - s2b-askelsarja ei ole deterministinen: CAS-kursori jäi kasvoihin (tarvittiin RIGHT + A + A "Are you sure?" → Yes).
- Ei tehty (tarkoituksella): [xtlid]-taulu (#44A), SIMSBO_UI (#6), hiiriherkkyys (#7), cvttsd2si (#9), FindGame-sääntö (#11),
  kartoituksen tarkistamatta jääneet #70–#88.

## Seuraavaksi (sessio 5)
0. Seuraa issueta abilliontokens/bustinout-recomp#3 ja PR:iä #15/#16.
1. **Seuraa upstream-PR:iä #15 ja #16** (`gh pr view 15 --repo TGP482/XboxXBERecomp`): vastaa katselmointiin, rebase
   #16, jos #8 menee ensin sisään. Kun PR:t on yhdistetty, README.md:n kääntöohje voi osoittaa suoraan upstreamiin.
2. Cheatit: varmennettu sessiossa 4 (loki). Valinnainen: Give Money tontun valikosta.
   `cheats: entered Give Money (code 1): on`.
3. Valinnaiset grafiikkaportit, jos Jonilla on HDR-näyttö: b3b8b9d HDR-ulostulo SDK-haaraan. Muuten UV-rajaus +
   nuolikursori (pienet).
4. Pelaa pidempään ja kirjaa viat: jos lokissa on `kernel export N (…) is not implemented` tai `indirect call … has no
   recompiled function`, nimi `runtime\kernel\ordinals.cpp`:stä / `korjaa-puuttuvat-funktiot.ps1`; toteutukset forkin
   `bustinout-port`-haarasta. Ohjain: SDL3 tunnistaa XInput-padit suoraan, testaamatta.
5. Seuraava peli: `uusi-peli.ps1`, ja LUE MINUT.md:n ohje (XDK-build + GitHub-forkit ensin).
- Puuttuvat D3D/DSound-symbolit etsitään tarvittaessa käsin: `python ..\XboxXBE-Recomp\tools\symbols\xbe2pe.py
  config\game.toml sims.exe` → Ghidra → `config\game.toml` `[symbols]`. Kun config on täydellinen:
  `xbe_symbols --sign config\game.toml --out ..\XboxXBE-Recomp\xdk\5558\signatures.toml`.

## Huomioita
- SDK-selvityksen raportti (mitä 5558-tuki vaatii) tallennetaan tähän kansioon nimellä `XDK-5558-selvitys.md`.
