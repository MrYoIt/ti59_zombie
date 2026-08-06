# ============================================================
#  launch_ti59.ps1 — trova l'emulatore TI-59 Zombie sulla rete
#  locale e apre la sua pagina web (wifilink.cpp) nel browser.
#
#  USO:
#    .\launch_ti59.ps1                  scoperta automatica
#    .\launch_ti59.ps1 -Ip 192.168.1.26 salta la scoperta, usa questo IP
#
#  Come funziona (il device NON usa mDNS, quindi si scansiona
#  la sottorete):
#    1. Se passi -Ip, prova SOLO quello (con diagnostica dettagliata
#       se fallisce, per capire se e' un problema di porta o di
#       endpoint).
#    2. Altrimenti prova l'ultimo IP trovato in precedenza (cache).
#    3. Altrimenti scansiona TUTTE le sottoreti /24 rilevate sul PC
#       (non solo la prima — utile se hai VPN, Hyper-V, VirtualBox,
#       Docker o altre schede virtuali che altrimenti confondono la
#       scelta) con connect TCP asincrone, poi verifica ogni host
#       aperto con GET /api/status cercando "prog_len".
#    4. Salva l'IP trovato in cache e apre il browser.
# ============================================================
param(
    [string]$Ip = $null
)

# ─────────────────── CONFIGURAZIONE (modifica qui) ───────────────────
$Port           = 80              # WIFI_PORT in config.h — non l'ho visto,
                                   # 80 è il default più comune per WebServer;
                                   # cambialo qui se nel tuo config.h è diverso
$ProbePath       = "/api/status"  # endpoint usato per verificare il device
$CheckText       = '"prog_len"'   # stringa-impronta attesa nella risposta JSON
$RootPath        = "/"            # pagina da aprire nel browser una volta trovato
$ConnectTimeoutMs = 800            # attesa minima (ms) per la scansione TCP; il
                                   # polling puo' allungarsi fino a un tetto interno
                                   # se ci sono ancora connessioni in corso
$HttpTimeoutSec   = 2             # timeout per la verifica HTTP
$ApFallbackName   = "TI59-Zombie-Setup"  # nome dell'AP di setup, per il messaggio di aiuto
# ───────────────────────────────────────────────────────────────────

$CacheDir  = Join-Path $env:LOCALAPPDATA "TI59Zombie"
$CacheFile = Join-Path $CacheDir "last_ip.txt"

function Test-Ti59Status([string]$TargetIp, [bool]$Verbose = $false) {
    if ($Verbose) {
        # Diagnostica dettagliata: prima la connessione TCP grezza, poi l'HTTP.
        $tcp = New-Object System.Net.Sockets.TcpClient
        $task = $tcp.ConnectAsync($TargetIp, $Port)
        $task.Wait(1500) | Out-Null
        $tcpOk = $tcp.Connected
        $tcp.Close()
        Write-Host "  TCP connect a ${TargetIp}:${Port} -> $(if ($tcpOk) {'OK'} else {'FALLITO'})"
        if (-not $tcpOk) {
            Write-Host "  -> la porta $Port non risponde su questo IP. Controlla WIFI_PORT in config.h" -ForegroundColor Yellow
            Write-Host "     e aggiorna la variabile `$Port in cima a questo script se e' diversa da 80."
            return $false
        }
        try {
            $resp = Invoke-WebRequest -Uri "http://${TargetIp}:${Port}${ProbePath}" `
                        -TimeoutSec $HttpTimeoutSec -UseBasicParsing -ErrorAction Stop
            Write-Host "  HTTP GET $ProbePath -> status $($resp.StatusCode)"
            $preview = $resp.Content.Substring(0, [Math]::Min(200, $resp.Content.Length))
            Write-Host "  Corpo risposta (primi 200 caratteri): $preview"
            if ($resp.Content -like "*$CheckText*") {
                return $true
            } else {
                Write-Host "  -> risposta ricevuta ma non contiene $CheckText. Endpoint diverso da quanto atteso?" -ForegroundColor Yellow
                return $false
            }
        } catch {
            Write-Host "  HTTP GET $ProbePath -> ERRORE: $($_.Exception.Message)" -ForegroundColor Yellow
            return $false
        }
    }
    try {
        $resp = Invoke-WebRequest -Uri "http://${TargetIp}:${Port}${ProbePath}" `
                    -TimeoutSec $HttpTimeoutSec -UseBasicParsing -ErrorAction Stop
        return ($resp.Content -like "*$CheckText*")
    } catch {
        return $false
    }
}

function Open-Ti59([string]$TargetIp) {
    Write-Host "Trovato TI-59 Zombie su http://${TargetIp}:${Port}${RootPath}" -ForegroundColor Green
    New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null
    Set-Content -Path $CacheFile -Value $TargetIp -Encoding ASCII
    Start-Process "http://${TargetIp}:${Port}${RootPath}"
}

Write-Host "Cerco l'emulatore TI-59 Zombie..." -ForegroundColor Cyan

# ─── 0) IP passato esplicitamente (-Ip 192.168.1.26) ───
if ($Ip) {
    Write-Host "Provo l'IP indicato manualmente: $Ip" -ForegroundColor Cyan
    if (Test-Ti59Status $Ip $true) {
        Open-Ti59 $Ip
        return
    } else {
        Write-Host ""
        Write-Host "L'IP $Ip non ha superato la verifica (vedi diagnostica sopra)." -ForegroundColor Red
        Read-Host "Premi INVIO per uscire"
        return
    }
}

# ─── 1) Prova la cache (IP trovato l'ultima volta) ───
if (Test-Path $CacheFile) {
    $cachedIp = (Get-Content $CacheFile -ErrorAction SilentlyContinue | Select-Object -First 1).Trim()
    if ($cachedIp -and (Test-Ti59Status $cachedIp $false)) {
        Open-Ti59 $cachedIp
        return
    }
    if ($cachedIp) {
        Write-Host "L'IP salvato ($cachedIp) non risponde più, scansiono la rete..." -ForegroundColor Yellow
    }
}

# ─── 2) Determina TUTTE le proprie sottoreti /24 (non solo la prima:
#        VPN, Hyper-V, VirtualBox, Docker ecc. possono avere la
#        precedenza e far scansionare la rete sbagliata) ───
$localIps = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
            Where-Object {
                $_.IPAddress -notlike "127.*" -and
                $_.IPAddress -notlike "169.254.*" -and
                $_.PrefixOrigin -ne "WellKnown"
            } | Select-Object -ExpandProperty IPAddress

if (-not $localIps) {
    Write-Host "Impossibile determinare l'indirizzo IP locale del PC." -ForegroundColor Red
    Write-Host "Controlla di essere connesso al WiFi/rete di casa, poi riprova."
    Read-Host "Premi INVIO per uscire"
    return
}

$subnets = $localIps | ForEach-Object { ($_ -split '\.')[0..2] -join '.' } | Select-Object -Unique
Write-Host "Sottoreti rilevate: $($subnets -join ', ')" -ForegroundColor Cyan
Write-Host "Scansione sulla porta $Port ..." -ForegroundColor Cyan

# ─── 3) Scansione veloce: connect TCP asincrone in parallelo, su
#        tutte le sottoreti rilevate insieme ───
$attempts = foreach ($subnet in $subnets) {
    foreach ($i in 1..254) {
        $candidateIp = "$subnet.$i"
        if ($localIps -contains $candidateIp) { continue }
        $client = New-Object System.Net.Sockets.TcpClient
        [PSCustomObject]@{
            Ip     = $candidateIp
            Client = $client
            Task   = $client.ConnectAsync($candidateIp, $Port)
        }
    }
}

# Attesa "a poll" invece di un unico Start-Sleep fisso: su una vera rete
# WiFi, con centinaia di connect simultanee, il tempo di risposta varia
# parecchio (ARP, congestione, ecc.) — un timeout fisso troppo corto
# rischia di scartare host che avrebbero risposto un attimo dopo.
# Qui si controlla ogni 100ms e ci si ferma appena tutti i task sono
# completati, con un tetto massimo di sicurezza.
$maxWaitMs = [Math]::Max($ConnectTimeoutMs, 1500)
$elapsed = 0
while ($elapsed -lt $maxWaitMs) {
    Start-Sleep -Milliseconds 100
    $elapsed += 100
    $pending = ($attempts | Where-Object { -not $_.Task.IsCompleted }).Count
    if ($pending -eq 0) { break }
}

$openHosts = @()
foreach ($a in $attempts) {
    if ($a.Task.IsCompleted -and -not $a.Task.IsFaulted -and $a.Client.Connected) {
        $openHosts += $a.Ip
    }
    $a.Client.Close()
}

if ($openHosts.Count -eq 0) {
    Write-Host "Nessun dispositivo con la porta $Port aperta trovato su: $($subnets -join ', ')." -ForegroundColor Red
    Write-Host ""
    Write-Host "Suggerimento: prova a lanciare con l'IP che conosci gia':"
    Write-Host "  .\launch_ti59.ps1 -Ip 192.168.1.26"
    Write-Host "(mostrera' una diagnostica dettagliata se anche quello fallisce)"
    Write-Host ""
    Write-Host "Controlla anche che:"
    Write-Host "  - l'emulatore sia acceso"
    Write-Host "  - sia connesso alla stessa rete WiFi di questo PC"
    Write-Host "  - se e' la prima accensione, potrebbe essere ancora in modalita' setup"
    Write-Host "    (rete WiFi '$ApFallbackName', senza password) invece che sulla rete di casa"
    Read-Host "Premi INVIO per uscire"
    return
}

Write-Host "Host con porta $Port aperta: $($openHosts -join ', ')" -ForegroundColor Cyan
Write-Host "Verifica impronta TI-59 Zombie ($ProbePath)..." -ForegroundColor Cyan

foreach ($ip in $openHosts) {
    if (Test-Ti59Status $ip $false) {
        Open-Ti59 $ip
        return
    }
}

Write-Host "Trovati $($openHosts.Count) dispositivi sulla porta $Port, ma nessuno risponde" -ForegroundColor Red
Write-Host "come l'emulatore TI-59 Zombie (endpoint $ProbePath non trovato)."
Write-Host "Dispositivi controllati: $($openHosts -join ', ')"
Read-Host "Premi INVIO per uscire"