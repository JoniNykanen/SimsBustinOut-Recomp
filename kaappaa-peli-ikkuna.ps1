# Kaynnistaa pelin, kuvaa sen ikkunan annetuilla sekunneilla ja sulkee pelin.
# Kayttoa: .\kaappaa-peli-ikkuna.ps1 -Ajat 12,30,55 -Etuliite koe
# Kuvat: out\kuvakaappaukset\<Etuliite>-<sekunnit>s.png. Ikkuna nostetaan hetkeksi paallimmaiseksi (SetWindowPos TOPMOST),
# koska taustalta kaynnistetty peli jaa muiden ikkunoiden taakse, ja PrintWindow antaa D3D12-ikkunasta mustaa.
param(
    [int[]]$Ajat = @(12, 30, 55),
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

function Kaappaa($h, $nimi) {
    $r = New-Object PeliIkkuna+R; [void][PeliIkkuna]::GetWindowRect($h, [ref]$r); $wd = $r.Rt - $r.L; $ht = $r.B - $r.T
    [void][PeliIkkuna]::SetWindowPos($h, $TOP, 0, 0, 0, 0, $FLAGS); Start-Sleep -Milliseconds 800
    $bmp = New-Object System.Drawing.Bitmap $wd, $ht; $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $wd, $ht))
    $f = Join-Path $kuvat "$nimi.png"; $bmp.Save($f); $g.Dispose(); $bmp.Dispose()
    [void][PeliIkkuna]::SetWindowPos($h, $NOTOP, 0, 0, 0, 0, $FLAGS)
    "$nimi : ${wd}x${ht} -> $f"
}

$p = Start-Process -FilePath $Exe -WorkingDirectory $dir -PassThru
$kulunut = 0
foreach ($t in ($Ajat | Sort-Object)) {
    while ($kulunut -lt $t -and -not $p.HasExited) { Start-Sleep 1; $kulunut++ }
    if ($p.HasExited) { "peli kuoli $kulunut s kohdalla, exit $($p.ExitCode)"; break }
    $hs = [PeliIkkuna]::Find([uint32]$p.Id)
    if ($hs.Count) { Kaappaa $hs[0] "$Etuliite-${t}s" } else { "${t}s: ei nakyvaa ikkunaa" }
}
"elossa lopussa: $(-not $p.HasExited)"
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -Confirm:$false }
$log = Join-Path $dir 'logs\simsbustinout.log'
if (Test-Path $log) { "--- loki (rivit 19->) ---"; Get-Content $log | Select-Object -Skip 18 | Select-Object -First 40 | ForEach-Object { $_.Substring(0, [Math]::Min(170, $_.Length)) } }
