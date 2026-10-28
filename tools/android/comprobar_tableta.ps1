# Step 1 of CLAUDE.md on the tablet: the device's data, the fast-core choice, keyboard play and controller or
# keyboard hot-plug mid-race. What adb can see is checked here; what only a person can see (the car steers, the
# touch controls hide) is asked and recorded. Everything goes to out/tablet/, the summary to out/tablet/paso1.txt.
#
#   powershell -ExecutionPolicy Bypass -File tools\android\comprobar_tableta.ps1 [-Instalar] [-Paquete <package>]
#
# -Instalar installs android\app\build\outputs\apk\release\app-release.apk first. The package defaults to the .dev
# build (com.nfsmw.android.dev).
param(
    [switch]$Instalar,
    [string]$Paquete = 'com.nfsmw.android.dev'
)
# Not 'Stop': Windows PowerShell 5.1 then stops at the first line adb writes to stderr. Failures throw.
$ErrorActionPreference = 'Continue'
$raiz = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$salida = Join-Path $raiz 'out\tablet'
New-Item -ItemType Directory -Force $salida | Out-Null
$resumen = New-Object System.Collections.Generic.List[string]
$fallos = 0

function Anotar([string]$estado, [string]$texto) {
    $linea = '{0,-5} {1}' -f $estado, $texto
    $resumen.Add($linea)
    $color = @{ OK = 'Green'; FALLO = 'Red'; INFO = 'Gray'; DUDA = 'Yellow' }[$estado]
    Write-Host $linea -ForegroundColor $color
    if ($estado -eq 'FALLO') { $script:fallos++ }
}
function Shell([string]$orden) { (& adb shell $orden | Out-String).Trim() }
function Guardar([string]$nombre, $texto) { Set-Content -Encoding UTF8 -Path (Join-Path $salida $nombre) -Value $texto }
function Preguntar([string]$pregunta) {
    do { $r = (Read-Host "$pregunta [s/n/-]").Trim().ToLower() } while ($r -notin @('s', 'n', '-', 'y'))
    if ($r -eq '-') { Anotar 'DUDA' "$pregunta (not tried)" }
    elseif ($r -in @('s', 'y')) { Anotar 'OK' $pregunta }
    else { Anotar 'FALLO' $pregunta }
}

if (-not (Get-Command adb -ErrorAction SilentlyContinue)) { throw 'adb is not on PATH (platform-tools).' }
if (((& adb get-state) | Out-String).Trim() -ne 'device') { throw 'No device: check adb devices -l and the USB debugging prompt on the tablet.' }

# --- The device ------------------------------------------------------------------------------------------
$modelo = Shell 'getprop ro.product.model'
Anotar 'INFO' ("{0}, SoC {1}, Android {2}" -f $modelo, (Shell 'getprop ro.soc.model'), (Shell 'getprop ro.build.version.release'))
$cpus = Shell 'for c in /sys/devices/system/cpu/cpu[0-9]*; do echo ${c##*/} $(cat $c/cpufreq/cpuinfo_max_freq 2>/dev/null) $(cat $c/cpu_capacity 2>/dev/null); done'
Guardar 'cpus.txt' $cpus
$nucleos = foreach ($l in ($cpus -split "`r?`n")) {
    $p = $l.Trim() -split '\s+'
    if ($p.Count -ge 2 -and $p[0] -match '^cpu(\d+)$') {
        [pscustomobject]@{ cpu = [int]$Matches[1]; khz = [long]$p[1]; cap = $(if ($p.Count -ge 3) { [long]$p[2] } else { 0 }) }
    }
}
$nucleos = $nucleos | Sort-Object cpu
Anotar 'INFO' ('cores (cpu:MHz/capacity): ' + (($nucleos | ForEach-Object { '{0}:{1}/{2}' -f $_.cpu, ($_.khz / 1000), $_.cap }) -join ' '))
$maximo = ($nucleos | Measure-Object khz -Maximum).Maximum
$esperados = (($nucleos | Where-Object { $_.khz -eq $maximo }).cpu) -join ','

$pantalla = Shell 'dumpsys display'
Guardar 'display.txt' $pantalla
$modos = [regex]::Matches($pantalla, 'fps=([\d.]+)') | ForEach-Object { [double]$_.Groups[1].Value } | Sort-Object -Unique
Anotar 'INFO' ('display refresh rates: ' + ($modos -join ', ') + ' Hz')
$memoria = (Shell 'cat /proc/meminfo') -split "`r?`n" | Select-Object -First 1
Anotar 'INFO' ("$memoria".Trim())

$vk = Shell 'cmd gpu vkjson'
Guardar 'vkjson.json' $vk
$campos = 'deviceName', 'apiVersion', 'maxBoundDescriptorSets', 'textureCompressionBC', 'textureCompressionETC2',
          'textureCompressionASTC_LDR', 'runtimeDescriptorArray', 'descriptorBindingPartiallyBound',
          'shaderSampledImageArrayNonUniformIndexing', 'shaderFloat16'
$vistos = foreach ($c in $campos) {
    $m = [regex]::Match($vk, '"' + $c + '"\s*:\s*("?[^",\r\n}]*"?)')
    if ($m.Success) { "$c=$($m.Groups[1].Value)" }
}
Anotar 'INFO' ('Vulkan: ' + ($vistos -join ' '))
if ($vk -match '"runtimeDescriptorArray"\s*:\s*0' -or $vk -match '"descriptorBindingPartiallyBound"\s*:\s*0') {
    Anotar 'INFO' 'descriptor indexing incomplete: use API grafica -> OpenGL ES'
}

# --- The build -------------------------------------------------------------------------------------------
if ($Instalar) {
    $apk = Join-Path $raiz 'android\app\build\outputs\apk\release\app-release.apk'
    & adb install -r $apk
    if ($LASTEXITCODE -ne 0) { throw "adb install failed: $apk" }
}
if (-not ((Shell "pm path $Paquete") -match '^package:')) { throw "$Paquete is not installed (build it, or run with -Instalar)." }
$version = (Shell "dumpsys package $Paquete") -split "`r?`n" | Where-Object { $_ -match 'versionName' } | Select-Object -First 1
Anotar 'INFO' ("$Paquete $version".Trim())

# --- Start-up: cores and arguments -----------------------------------------------------------------------
& adb logcat -b main -b system -b events -c
Write-Host ''
Write-Host 'On the tablet: open the launcher, TECLADO -> Jugar con teclado ON (remap a key in Teclas if you want to'
Write-Host 'test that too), then Jugar. Start a race, drive a few seconds with the keyboard.'
Read-Host 'Press Enter here when you are in the race' | Out-Null
$arranque = (& adb logcat -d -v time -s NFSMW-rex NFSMW SDL | Out-String)
Guardar 'arranque.txt' $arranque

$descripcion = [regex]::Match($arranque, '\[android\] nucleos \(cpu:reloj/capacidad/parte\): (.*)')
if ($descripcion.Success) { Anotar 'INFO' ('game sees: ' + $descripcion.Groups[1].Value.Trim()) }
else { Anotar 'FALLO' 'no core line in the log (is this build older than the fast-core change?)' }
$eleccion = [regex]::Match($arranque, '\[android\] juego en los nucleos ([\d,]+) de (\d+)')
if ($eleccion.Success) {
    $lista = $eleccion.Groups[1].Value
    if ($lista -eq $esperados) { Anotar 'OK' "game on cores $lista (the fastest clock)" }
    else { Anotar 'FALLO' "game on cores $lista, expected $esperados" }
} else {
    $sin = [regex]::Match($arranque, '\[android\] [^\r\n]*afinidad sin cambios')
    Anotar 'FALLO' ('affinity not set: ' + $(if ($sin.Success) { $sin.Value } else { 'no line' }))
}
$argumentos = [regex]::Match($arranque, 'argumentos del juego: (.*)')
if (-not $argumentos.Success) { Anotar 'FALLO' 'no argument line in the log' }
elseif ($argumentos.Groups[1].Value -match '--mnk_mode=true') {
    Anotar 'OK' 'keyboard play passed to the game (--mnk_mode=true)'
    $teclas = [regex]::Matches($argumentos.Groups[1].Value, '--keybind_\S+') | ForEach-Object { $_.Value }
    Anotar 'INFO' ('remapped: ' + $(if ($teclas) { $teclas -join ' ' } else { 'none (SDK defaults)' }))
} else { Anotar 'FALLO' 'keyboard play is off in the arguments (TECLADO -> Jugar con teclado)' }
# Steps 2 to 4: how BC textures are kept, half precision, ADPF and the thermal step-down.
$etc2 = $arranque -split "`r?`n" | Where-Object { $_ -match '\[compatibilidad\] (BC\d|ETC2)|C5a: media precision|\[ritmo\]|\[termico\]' } | Select-Object -First 16
foreach ($l in $etc2) { Anotar 'INFO' (($l -replace '^.*?(\[compatibilidad\]|\[nativo\]|\[ritmo\]|\[termico\])', '$1').Trim()) }

Preguntar 'The keyboard drives the car (A/D steer, E accelerates, Q brakes, or your remap)'
Preguntar 'The touch controls hid at the first key press'

# --- Hot-plug mid-race -----------------------------------------------------------------------------------
$pid1 = Shell "pidof $Paquete"
& adb logcat -b main -b system -b events -c
Write-Host ''
Write-Host 'Still in the race: switch the controller off and on again, then disconnect and reconnect the keyboard'
Write-Host '(Bluetooth off/on on it, or unplug USB). Drive a few seconds with each afterwards.'
Read-Host 'Press Enter here when done' | Out-Null
$enchufe = (& adb logcat -d -v time -b main -b system -b events | Out-String)
Guardar 'enchufe.txt' $enchufe
$pid2 = Shell "pidof $Paquete"
$relanzado = $enchufe -split "`r?`n" | Where-Object { $_ -match '(wm|am)_relaunch' -and $_ -match 'GameActivity' }
$sdl = $enchufe -split "`r?`n" | Where-Object { $_ -match '\bSDL\b.*on(Create|Destroy)\(\)' }
if (-not $pid2) { Anotar 'FALLO' 'the game process is gone' }
elseif ($pid2 -ne $pid1) { Anotar 'FALLO' "the game process restarted ($pid1 -> $pid2)" }
elseif ($relanzado -or $sdl) { Anotar 'FALLO' ('the game screen was recreated: ' + ((@($relanzado) + @($sdl) | Select-Object -First 2) -join ' | ')) }
else { Anotar 'OK' 'no restart of the game screen while devices connected' }
$cambios = ($enchufe -split "`r?`n" | Where-Object { $_ -match 'onConfigurationChanged' }).Count
Anotar 'INFO' "configuration changes handled in place: $cambios"

Preguntar 'After reconnecting, the controller drives'
Preguntar 'After reconnecting, the keyboard drives'
Preguntar 'The race carried on with no black screen or freeze'

# --- Summary ---------------------------------------------------------------------------------------------
$resumen.Insert(0, ('{0} {1} {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm'), $modelo, $Paquete))
Guardar 'paso1.txt' $resumen
Write-Host ''
Write-Host ("{0} failures. Summary in {1}" -f $fallos, (Join-Path $salida 'paso1.txt'))
exit $(if ($fallos) { 1 } else { 0 })
