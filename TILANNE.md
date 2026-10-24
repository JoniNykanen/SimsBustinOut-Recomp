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
  ObOpenObjectByName, NtSetInformationFile luokka 30) + gamma-suoja + picture_top. Työpuu puhdas; vain kokeilukansio
  `xdk-5558-lainattu-5849-koe\` on seuraamaton (voi poistaa). Pelirepo: `5ae8933` sessio 1, `3b46b70` sessio 2.
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
- **Ei testattu tässä sessiossa:** ääni (kuuluuko; kaiku puuttuu odotetusti, E.4), tallennus pelistä (kernel-exportit
  tulevat tässä vastaan), kamera hiirellä, kävely WASD:llä, ohjain. Nämä vaativat ihmisen koneen ääreen.
- Tarkistustyönkulku (4 katselmointia: disassembly, recomp-output + override-mekanismi, SDK-muutokset, porttauksen
  uskollisuus) ajettiin rinnakkain; löydökset kirjataan alle kun valmis.

## Seuraavaksi (sessio 3)
1. **Käsin pelitesti** (Joni): `out\build\release\SimsBustinOut.exe`, Enter ×2 → Continue → Sim 1 → ✓ → talo. Kuuntele
   ääni, kävele WASD:llä, kamera hiiren oikealla napilla + rullalla, tallenna pelistä (Start-valikko). Kirjaa viat.
2. Jos tallennus kaatuu tai lokissa on `kernel export N (…) is not implemented`: nimi `runtime\kernel\ordinals.cpp`:stä,
   toteutus forkin `bustinout-port`-haarasta (`…\scratchpad\refs\abilliontokens-XboxXBERecomp`, jos tallella).
3. Vaihe 2 (valinnainen): diagnostiikkabuild ilman NDEBUG:ia → `boot: kernel imports resolved, 0 missing` ja
   `d3d12: all black gamma ramp ignored` -rivit kertovat, olivatko patchit tarpeen.
4. Vaihe 9 (valinnainen): overlay F1/F2 (asetukset, fps) ja cheatit USA-projektista, karsinta Musta-ruutu-selvitys.md:n
   mukaan; `settings.toml` display_mode 'borderless' pelaamiseen.
5. Julkaisu: `release.bat` (pelikoodi käännetään pelaajan omasta XBE:stä) + SDK-haaran `sims-5558` tarjoaminen upstreamiin
   (3 committia ovat yleiskäyttöisiä: 5558-sarja, SuperSample-malli, kernel/gamma/picture_top).
- Puuttuvat D3D/DSound-symbolit etsitään tarvittaessa käsin: `python ..\XboxXBE-Recomp\tools\symbols\xbe2pe.py
  config\game.toml sims.exe` → Ghidra → `config\game.toml` `[symbols]`. Kun config on täydellinen:
  `xbe_symbols --sign config\game.toml --out ..\XboxXBE-Recomp\xdk\5558\signatures.toml`.

## Huomioita
- SDK-selvityksen raportti (mitä 5558-tuki vaatii) tallennetaan tähän kansioon nimellä `XDK-5558-selvitys.md`.
