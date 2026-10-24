# Kaynnistaa pelin, kuvaa sen ikkunan annetuilla sekunneilla, ajaa halutessa SIMSBO_STEP-nappainsekvenssin ja sulkee pelin.
#
# Kayttoa:
#   .\kaappaa-peli-ikkuna.ps1 -Ajat 12,30,55 -Etuliite koe
#   .\kaappaa-peli-ikkuna.ps1 -Ajat 14,30,60 -Askeleet '16:START','20:START','24:A','28:A','32:RIGHTx3','36:A','40:A' -Etuliite kavely
#
# -Ajat      sekunnit, joilla ikkuna kuvataan: out\kuvakaappaukset\<Etuliite>-<sekunnit>s.png
# -Askeleet  'sekunti:TOKEN' -parit. TOKEN kirjoitetaan out\askel.txt-tiedostoon (SIMSBO_STEP, game_info.cpp StepPad:
#            START, A, B, X, Y, BACK, UP, DOWN, LEFT, RIGHT, LSUP, LSDOWN, LSLEFT, LSRIGHT, BLACK, WHITE, LT, RT,
#            LTHUMB, RTHUMB; NIMIxN = N painallusta) ja 2 s myohemmin tilalle NONE, jotta sama token kelpaa uudelleen.
# -Nappaimet 'sekunti:NAPPAIMET' -parit: peli-ikkuna aktivoidaan (AppActivate) ja NAPPAIMET lahetetaan SendKeys-
#            muodossa, esim. '16:{ENTER}' tai '30:wwww'. Testaa nappaimisto->pad-polun (input_kbm.cpp), joka
#            vaatii ikkunalle fokuksen. Ikkuna jaa aktiiviseksi, joten ala kirjoita koneella sen aikana.
# -Kesto     montako sekuntia peli pidetaan kaynnissa (oletus: suurin aika + 3 s)
# -Ymparisto ymparistomuuttujat pelille, esim. @{ SIMSBO_NO_STEP = '1' }
#
# Ikkuna nostetaan hetkeksi paallimmaiseksi (SetWindowPos TOPMOST), koska taustalta kaynnistetty peli jaa muiden
# ikkunoiden taakse, ja PrintWindow antaa D3D12-ikkunasta mustaa. Loppuun tulostuu lokin loppu.
param(
    [int[]]$Ajat = @(12, 30, 55),
    [string[]]$Askeleet = @(),
    [string[]]$Nappaimet = @(),
    [int]$Kesto = 0,
    [hashtable]$Ymparisto = @{},
    [string]$Etuliite = 'kuva',
    [string]$Exe = (Join-Path $PSScriptRoot 'out\build\release\SimsBustinOut.exe')
)
$ErrorActionPreference = 'Stop'
$dir = Split-Path $Exe
$kuvat = Join-Path $PSScriptRoot 'out\kuvakaappaukset'
New-Item -ItemType Directory -Force $kuvat | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Collections.Generic;
public class PeliIkkuna {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L, T, Rt, B; }
  public static List<IntPtr> Find(uint pid) { var r = new List<IntPtr>(); EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) r.Add(h); return true; }, IntPtr.Zero); return r; }
}
"@
$TOP = [IntPtr](-1); $NOTOP = [IntPtr](-2); $FLAGS = [uint32](0x0001 -bor 0x0002 -bor 0x0010)  # NOSIZE|NOMOVE|NOACTIVATE

# peli lukee askeltiedoston 50 ms valein (fopen), joten kirjoitus voi osua lukuun: yritetaan uudelleen
function KirjoitaAskel($polku, $token) {
    for ($yritys = 0; $yritys -lt 20; $yritys++) {
        try { [IO.File]::WriteAllText($polku, "$token`n", [Text.Encoding]::ASCII); return } catch [IO.IOException] { Start-Sleep -Milliseconds 25 }
    }
    "VAROITUS: askelta '$token' ei saatu kirjoitettua"
}

function Kaappaa($h, $nimi) {
    $r = New-Object PeliIkkuna+R; [void][PeliIkkuna]::GetWindowRect($h, [ref]$r); $wd = $r.Rt - $r.L; $ht = $r.B - $r.T
    if ($wd -le 0 -or $ht -le 0) { return "$nimi : ikkuna ${wd}x${ht}, ei kuvattavaa" }
    [void][PeliIkkuna]::SetWindowPos($h, $TOP, 0, 0, 0, 0, $FLAGS); Start-Sleep -Milliseconds 800
    $bmp = New-Object System.Drawing.Bitmap $wd, $ht; $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $wd, $ht))
    $f = Join-Path $kuvat "$nimi.png"; $bmp.Save($f); $g.Dispose(); $bmp.Dispose()
    [void][PeliIkkuna]::SetWindowPos($h, $NOTOP, 0, 0, 0, 0, $FLAGS)
    "$nimi : ${wd}x${ht} -> $f"
}

# askeleet: aika -> token, ja NONE 2 s jokaisen jalkeen
$askelTiedosto = Join-Path $PSScriptRoot 'out\askel.txt'
$tapahtumat = @{}
foreach ($a in $Askeleet) {
    $osat = $a -split ':', 2
    if ($osat.Count -ne 2) { throw "askel '$a' ei ole muotoa sekunti:TOKEN" }
    $t = [int]$osat[0]
    $tapahtumat[$t] = $osat[1].Trim().ToUpperInvariant()
    if (-not $tapahtumat.ContainsKey($t + 2)) { $tapahtumat[$t + 2] = 'NONE' }
}
# nappaimet: aika -> SendKeys-merkkijono
$nappainTapahtumat = @{}
foreach ($n in $Nappaimet) {
    $osat = $n -split ':', 2
    if ($osat.Count -ne 2) { throw "nappain '$n' ei ole muotoa sekunti:NAPPAIMET" }
    $nappainTapahtumat[[int]$osat[0]] = $osat[1]
}
if ($Nappaimet.Count) { $shell = New-Object -ComObject WScript.Shell }

$kaikkiAjat = @($Ajat) + @($tapahtumat.Keys) + @($nappainTapahtumat.Keys)
if ($Kesto -le 0) { $Kesto = (($kaikkiAjat | Measure-Object -Maximum).Maximum) + 3 }

# ymparisto pelille (lapsi perii)
$vanhat = @{}
foreach ($k in $Ymparisto.Keys) { $vanhat[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, [string]$Ymparisto[$k]) }
if ($Askeleet.Count) {
    KirjoitaAskel $askelTiedosto 'NONE'
    $vanhat['SIMSBO_STEP'] = $env:SIMSBO_STEP; $env:SIMSBO_STEP = $askelTiedosto
}

$p = Start-Process -FilePath $Exe -WorkingDirectory $dir -PassThru
"kaynnistetty pid $($p.Id), kesto $Kesto s, kuvat @($($Ajat -join ','))" + $(if ($Askeleet.Count) { ", askeleet: $($Askeleet -join ' ')" } else { '' })
$kulunut = 0
$kuvattavat = @($Ajat | Sort-Object)
try {
    while ($kulunut -lt $Kesto) {
        Start-Sleep 1; $kulunut++
        if ($p.HasExited) { "peli kuoli $kulunut s kohdalla, exit $($p.ExitCode)"; break }
        if ($tapahtumat.ContainsKey($kulunut)) {
            KirjoitaAskel $askelTiedosto $tapahtumat[$kulunut]
            if ($tapahtumat[$kulunut] -ne 'NONE') { "${kulunut}s: askel $($tapahtumat[$kulunut])" }
        }
        if ($nappainTapahtumat.ContainsKey($kulunut)) {
            $aktivoitu = $shell.AppActivate($p.Id); Start-Sleep -Milliseconds 300
            $shell.SendKeys($nappainTapahtumat[$kulunut])
            "${kulunut}s: nappaimet '$($nappainTapahtumat[$kulunut])' (ikkuna aktivoitu: $aktivoitu)"
        }
        if ($kuvattavat -contains $kulunut) {
            $hs = [PeliIkkuna]::Find([uint32]$p.Id)
            if ($hs.Count) { try { Kaappaa $hs[0] "$Etuliite-${kulunut}s" } catch { "${kulunut}s: kaappaus epaonnistui: $_" } } else { "${kulunut}s: ei nakyvaa ikkunaa" }
        }
    }
} finally {
    "elossa lopussa: $(-not $p.HasExited)"
    if (-not $p.HasExited) { try { Stop-Process -Id $p.Id -Force -Confirm:$false -ErrorAction SilentlyContinue } catch {} }
    # ymparisto takaisin
    foreach ($k in $vanhat.Keys) { [Environment]::SetEnvironmentVariable($k, $vanhat[$k]) }
}

$log = Join-Path $dir 'logs\simsbustinout.log'
if (Test-Path $log) {
    "--- loki (rivit 19->) ---"
    Get-Content $log | Select-Object -Skip 18 | Select-Object -First 60 | ForEach-Object { $_.Substring(0, [Math]::Min(170, $_.Length)) }
}
