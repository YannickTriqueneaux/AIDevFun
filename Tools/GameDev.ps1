[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet('start', 'ping', 'build', 'reload', 'reload-wait', 'build-reload', 'logs', 'verify')]
    [string]$Command,
    [string]$Game,
    [string]$BuildDirectory,
    [ValidateSet('Launcher', 'GameHost', 'AssistantHost', 'All')]
    [string]$Process = 'All',
    [ValidateRange(1, 10000)][int]$Lines = 80,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 15,
    [switch]$Follow,
    [switch]$Json
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Resolve-DevelopmentRuntime {
    if ($BuildDirectory) {
        $buildRoot = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $BuildDirectory))
    } elseif ($Game) {
        $buildRoot = Join-Path $projectRoot "Games\$Game\build"
    } else {
        $buildRoot = Join-Path $projectRoot 'build'
    }
    $runtime = if ((Split-Path $buildRoot -Leaf) -eq 'Debug') { $buildRoot } else { Join-Path $buildRoot 'Debug' }
    $runtime = [System.IO.Path]::GetFullPath($runtime)
    if ($runtime -match '(?i)(^|[\\/])Release([\\/]|$)') {
        throw 'GameDev is disabled for Release runtimes.'
    }
    $runtime
}

function Resolve-GameName([string]$Runtime) {
    if ($Game) { return $Game }
    $cache = Join-Path (Split-Path -Parent $Runtime) 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $match = Select-String -LiteralPath $cache -Pattern '^GAME_PROJECT:STRING=(.+)$' | Select-Object -First 1
        if ($match) { return $match.Matches[0].Groups[1].Value }
    }
    throw 'Specify -Game, or point -BuildDirectory at a configured CMake build.'
}

function Read-Exactly([System.IO.Stream]$Stream, [byte[]]$Buffer, [int]$Count) {
    $offset = 0
    while ($offset -lt $Count) {
        $read = $Stream.Read($Buffer, $offset, $Count - $offset)
        if ($read -eq 0) { throw 'Game tools IPC closed unexpectedly.' }
        $offset += $read
    }
}

function Invoke-GamePipe([string]$GameName, [string]$RequestJson) {
    $pipeName = "MakeYourOwnGame.AI.$GameName.GameTools.v1"
    $pipe = [System.IO.Pipes.NamedPipeClientStream]::new('.', $pipeName, [System.IO.Pipes.PipeDirection]::InOut)
    try {
        $pipe.Connect($TimeoutSeconds * 1000)
        $payload = [System.Text.Encoding]::UTF8.GetBytes($RequestJson)
        if ($payload.Length -gt 4MB) { throw 'IPC request exceeds 4 MiB.' }
        $length = [System.BitConverter]::GetBytes([uint32]$payload.Length)
        $pipe.Write($length, 0, $length.Length)
        $pipe.Write($payload, 0, $payload.Length)
        $pipe.Flush()
        $lengthBytes = [byte[]]::new(4)
        Read-Exactly $pipe $lengthBytes 4
        $responseLength = [System.BitConverter]::ToUInt32($lengthBytes, 0)
        if ($responseLength -gt 4MB) { throw 'IPC response exceeds 4 MiB.' }
        $response = [byte[]]::new($responseLength)
        Read-Exactly $pipe $response $responseLength
        ([System.Text.Encoding]::UTF8.GetString($response) | ConvertFrom-Json)
    } finally {
        $pipe.Dispose()
    }
}

function Invoke-ToolCommand([string]$GameName, [string]$Name) {
    $request = @{ command = $Name; arguments = @{} } | ConvertTo-Json -Compress
    $response = Invoke-GamePipe $GameName $request
    if (-not $response.ok) { throw $response.error }
    $response
}

function Write-Response($Response) {
    if ($Json) { $Response | ConvertTo-Json -Depth 20 -Compress }
    else { $Response.result | ConvertTo-Json -Depth 20 }
}

function Get-LogPaths([string]$Runtime) {
    $names = if ($Process -eq 'All') { @('Launcher', 'GameHost', 'AssistantHost') } else { @($Process) }
    foreach ($name in $names) {
        $path = Join-Path $Runtime "Logs\$name.log"
        if (Test-Path -LiteralPath $path) { $path }
    }
}

$runtimeDirectory = Resolve-DevelopmentRuntime
$gameName = Resolve-GameName $runtimeDirectory

switch ($Command) {
    'start' {
        $launcher = Join-Path $runtimeDirectory 'Launcher.exe'
        if (-not (Test-Path -LiteralPath $launcher)) { throw "Launcher not found: $launcher" }
        $running = Get-CimInstance Win32_Process -Filter "Name = 'Launcher.exe'" | Where-Object { $_.ExecutablePath -eq $launcher }
        if ($running) {
            [pscustomobject]@{ status = 'already-running'; pid = $running.ProcessId; path = $launcher } | ConvertTo-Json -Compress
            break
        }
        $started = Start-Process -FilePath $launcher -WorkingDirectory $runtimeDirectory -PassThru
        [pscustomobject]@{ status = 'started'; pid = $started.Id; path = $launcher } | ConvertTo-Json -Compress
    }
    'ping' { Write-Response (Invoke-ToolCommand $gameName 'ping') }
    'build' { Write-Response (Invoke-ToolCommand $gameName 'build_game') }
    'reload' { Write-Response (Invoke-ToolCommand $gameName 'reload_game') }
    'reload-wait' {
        [void](Invoke-ToolCommand $gameName 'reload_game')
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        do {
            Start-Sleep -Milliseconds 100
            $response = Invoke-ToolCommand $gameName 'get_reload_status'
            $lastStatus = [string]$response.result.status
            if ($lastStatus -notmatch '(?i)requested|pending|loading') { break }
        } while ([DateTime]::UtcNow -lt $deadline)
        if ($lastStatus -match '(?i)failed') { throw $lastStatus }
        if ([DateTime]::UtcNow -ge $deadline) { throw "Reload timed out: $lastStatus" }
        Write-Response $response
    }
    'build-reload' {
        $build = Invoke-ToolCommand $gameName 'build_game'
        [void](Invoke-ToolCommand $gameName 'reload_game')
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        do {
            Start-Sleep -Milliseconds 100
            $reload = Invoke-ToolCommand $gameName 'get_reload_status'
            $status = [string]$reload.result.status
            if ($status -notmatch '(?i)requested|pending|loading') { break }
        } while ([DateTime]::UtcNow -lt $deadline)
        if ($status -match '(?i)failed') { throw $status }
        if ([DateTime]::UtcNow -ge $deadline) { throw "Reload timed out: $status" }
        [pscustomobject]@{
            build = $build.result
            reloadStatus = $status
        } | ConvertTo-Json -Depth 20
    }
    'logs' {
        $paths = @(Get-LogPaths $runtimeDirectory)
        if ($paths.Count -eq 0) { throw "No logs found under $runtimeDirectory\Logs" }
        if ($Follow) { Get-Content -LiteralPath $paths -Tail $Lines -Wait }
        else {
            foreach ($path in $paths) {
                "=== $([System.IO.Path]::GetFileName($path)) ==="
                Get-Content -LiteralPath $path -Tail $Lines
            }
        }
    }
    'verify' {
        $ping = Invoke-ToolCommand $gameName 'ping'
        [void](Invoke-ToolCommand $gameName 'reload_game')
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        do {
            Start-Sleep -Milliseconds 100
            $reload = Invoke-ToolCommand $gameName 'get_reload_status'
            $status = [string]$reload.result.status
            if ($status -notmatch '(?i)requested|pending|loading') { break }
        } while ([DateTime]::UtcNow -lt $deadline)
        if ($status -match '(?i)failed') { throw $status }
        if ([DateTime]::UtcNow -ge $deadline) { throw "Reload timed out: $status" }
        [pscustomobject]@{ game = $gameName; runtime = $runtimeDirectory; service = $ping.result.service; reloadStatus = $status; logs = @(Get-LogPaths $runtimeDirectory) } | ConvertTo-Json -Depth 5
    }
}
