# Iteroi: käynnistä peli, lue loki, lisää lokin ehdottama puuttuva funktio config\game.toml:n [functions]-tauluun,
# käännä uudelleen ja toista. Pysähtyy, kun peli pysyy käynnissä KestoSekunteja tai kun virhe ei ole automaattisesti korjattava.
# Käyttö: .\korjaa-puuttuvat-funktiot.ps1 [-MaxKierrokset 15] [-KestoSekunteja 60]
param(
    [int]$MaxKierrokset = 15,
    [int]$KestoSekunteja = 60
)
$ErrorActionPreference = 'Stop'
$juuri  = $PSScriptRoot
$exeDir = Join-Path $juuri 'out\build\release'
$exe    = Join-Path $exeDir 'SimsBustinOut.exe'
$config = Join-Path $juuri 'config\game.toml'
$loki   = Join-Path $juuri 'out\iterointi-loki.txt'
$kuvat  = Join-Path $juuri 'out\kuvakaappaukset'
New-Item -ItemType Directory -Force $kuvat | Out-Null
Add-Type -AssemblyName System.Drawing, System.Windows.Forms

function Kirjaa([string]$m) {
    $r = "[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $m; Write-Host $r
    # toinen prosessi (lokin lukija) voi pitää tiedostoa hetken auki: yritetään uudelleen
    for ($y = 1; $y -le 10; $y++) {
        try { [IO.File]::AppendAllText($loki, $r + "`r`n", [Text.Encoding]::UTF8); return } catch { Start-Sleep -Milliseconds 500 }
    }
    Write-Host "lokiin ei saatu kirjoitettua: $r"
}
function Kuvakaappaus([string]$nimi) {
    $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
    $p = Join-Path $kuvat $nimi; $bmp.Save($p); $g.Dispose(); $bmp.Dispose(); return $p
}
function UusinLoki { Get-ChildItem (Join-Path $exeDir 'logs') -Filter 'simsbustinout*.log' | Sort-Object LastWriteTime | Select-Object -Last 1 }

Kirjaa "=== silmukka alkaa, max $MaxKierrokset kierrosta, kesto $KestoSekunteja s ==="
for ($k = 1; $k -le $MaxKierrokset; $k++) {
    Kirjaa "--- kierros ${k}: käynnistys ---"
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru
    $odotettu = 0
    while (-not $p.HasExited -and $odotettu -lt $KestoSekunteja) { Start-Sleep 2; $odotettu += 2 }
    $elossa = -not $p.HasExited
    $kuva = Kuvakaappaus ("kierros-{0:D2}.png" -f $k)
    if ($elossa) { Stop-Process -Id $p.Id -Force -Confirm:$false; Start-Sleep 2 }

    $lokiTiedosto = UusinLoki
    $rivit = if ($lokiTiedosto) { Get-Content $lokiTiedosto.FullName } else { @() }
    $virheet = $rivit | Where-Object { $_ -match '\] (error|warn)' }
    Kirjaa ("peli {0} ({1} s), loki {2}, virherivejä {3}, kuva {4}" -f ($(if ($elossa) { 'ELOSSA' } else { "kuoli, exit $($p.ExitCode)" }), $odotettu, $lokiTiedosto.Name, $virheet.Count, $kuva))
    $virheet | Select-Object -First 12 | ForEach-Object { Kirjaa "    $_" }

    if ($elossa -and $virheet.Count -eq 0) { Kirjaa "=== VALMIS: peli pysyi käynnissä $KestoSekunteja s ilman virheitä ==="; break }

    $osoitteet = @($rivit | ForEach-Object { if ($_ -match 'add 0x([0-9A-Fa-f]+) to \[functions\]') { $Matches[1].ToUpper() } } | Select-Object -Unique)
    if ($osoitteet.Count -eq 0) {
        if ($elossa) { Kirjaa "=== PYSÄHDYS: peli elossa mutta lokissa virheitä, joita ei voi korjata automaattisesti ===" }
        else         { Kirjaa "=== PYSÄHDYS: peli kuoli eikä loki ehdota [functions]-lisäystä. Vaatii käsityötä. ===" }
        break
    }

    $t = [IO.File]::ReadAllText($config)
    $lisatyt = @()
    foreach ($a in $osoitteet) {
        if ($t -match "(?m)^0x$a\s*=") { Kirjaa "0x$a on jo configissa, mutta peli kaatuu silti siihen -> PYSÄHDYS"; break }
        $t = $t -replace '(?m)^\[functions\]\r?\n', "[functions]`r`n0x$a = `"`"`r`n"
        $lisatyt += "0x$a"
    }
    if ($lisatyt.Count -eq 0) { break }
    [IO.File]::WriteAllText($config, $t, (New-Object System.Text.UTF8Encoding($false)))
    Kirjaa ("lisätty [functions]: {0}; käännetään uudelleen" -f ($lisatyt -join ', '))

    $alku = Get-Date
    $tuloste = & cmd.exe /c "`"$juuri\kaanna-uudelleen.bat`" 2>&1"
    $kesto = [int]((Get-Date) - $alku).TotalSeconds
    $virhe = $tuloste | Where-Object { $_ -match 'FAILED|error:|ninja: build stopped' } | Select-Object -First 5
    if ($virhe) { Kirjaa "=== PYSÄHDYS: käännös epäonnistui ($kesto s) ==="; $virhe | ForEach-Object { Kirjaa "    $_" }; break }
    Kirjaa "käännös ok ($kesto s)"
}
Kirjaa "=== silmukka päättyi ==="
