param(
    [Parameter(Mandatory = $true)]
    [string] $RepositoryRoot
)

$ErrorActionPreference = "Stop"
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$assistantSettingsPath = Join-Path $RepositoryRoot `
    "AssistantHost\Config\settings.json"
$assistantSettingsExamplePath = Join-Path $RepositoryRoot `
    "AssistantHost\Config\settings.example.json"
$openAISettingsPath = Join-Path $RepositoryRoot `
    "AssistantProviders\OpenAI\OpenAIProvider.settings.json"
$openAISettingsExamplePath = Join-Path $RepositoryRoot `
    "AssistantProviders\OpenAI\OpenAIProvider.settings.example.json"
$configureOpenAIPath = Join-Path $RepositoryRoot `
    "AssistantHost\Config\ConfigureOpenAI.ps1"

function Read-Confirmation {
    param([string] $Prompt)

    while ($true) {
        $answer = Read-Host "$Prompt [Y/n]"
        if ([string]::IsNullOrWhiteSpace($answer) -or $answer -match '^[Yy]') {
            return $true
        }
        if ($answer -match '^[Nn]') {
            return $false
        }
        Write-Host "Please answer Y or N."
    }
}

function Update-ProcessPath {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Find-CommandPath {
    param([string] $Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $command) {
        return $null
    }
    return $command.Source
}

function Require-Winget {
    if ($null -ne (Find-CommandPath "winget.exe")) {
        return
    }
    throw "Windows Package Manager (winget) is required to install missing development tools. Install App Installer from Microsoft, then run Start.bat again."
}

function Install-WingetPackage {
    param(
        [string] $DisplayName,
        [string] $PackageId,
        [string] $Override = ""
    )

    if (-not (Read-Confirmation "$DisplayName is missing. Install it now?")) {
        throw "$DisplayName is required to build the project."
    }
    Require-Winget
    Write-Host "Installing $DisplayName..."
    $arguments = @(
        "install", "--id", $PackageId, "-e",
        "--accept-package-agreements", "--accept-source-agreements"
    )
    if (-not [string]::IsNullOrWhiteSpace($Override)) {
        $arguments += @("--override", $Override)
    }
    & winget.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$DisplayName installation failed with exit code $LASTEXITCODE."
    }
    Update-ProcessPath
}

function Get-MsvcInstallation {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        return $null
    }
    $installation = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
        return $null
    }
    return [string] $installation
}

function Ensure-AssistantSettings {
    if (Test-Path -LiteralPath $assistantSettingsPath) {
        try {
            $settings = Get-Content -LiteralPath $assistantSettingsPath -Raw |
                ConvertFrom-Json
            $provider = [string] $settings.assistant.providerLibrary
            if ($provider -notin @(
                    "AssistantProviderCodex.dll",
                    "AssistantProviderOpenAI.dll")) {
                throw "Unsupported assistant provider: $provider"
            }
            Write-Host "Assistant provider already configured: $provider"
            return $settings
        }
        catch {
            throw "Assistant configuration is invalid: $($_.Exception.Message)"
        }
    }

    if (-not (Test-Path -LiteralPath $assistantSettingsExamplePath)) {
        throw "Assistant settings example is missing."
    }
    Write-Host ""
    Write-Host "Choose the AI assistant provider:"
    Write-Host "  1. Codex CLI with a ChatGPT account (recommended)"
    Write-Host "  2. OpenAI API with usage-based billing"
    $selection = Read-Host "Provider [1]"
    if ([string]::IsNullOrWhiteSpace($selection)) {
        $selection = "1"
    }
    switch ($selection) {
        "1" {
            $settings = @{
                assistant = @{
                    providerLibrary = "AssistantProviderCodex.dll"
                    providerSettings = "CodexProvider.settings.json"
                    promptConfig = "AssistantPrompts.json"
                }
            }
        }
        "2" {
            $settings = @{
                assistant = @{
                    providerLibrary = "AssistantProviderOpenAI.dll"
                    providerSettings = "OpenAIProvider.settings.json"
                    promptConfig = "AssistantPrompts.json"
                }
            }
        }
        default { throw "Invalid assistant provider selection." }
    }
    $settings | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $assistantSettingsPath -Encoding UTF8
    Write-Host "Assistant provider configuration created."
    return $settings
}

function Find-Codex {
    $command = Find-CommandPath "codex"
    if ($null -ne $command) {
        return $command
    }
    $candidates = @(
        (Join-Path $env:APPDATA "npm\codex.cmd"),
        (Join-Path $env:USERPROFILE ".local\bin\codex.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $null
}

function Test-CodexLogin {
    param([string] $CodexPath)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $CodexPath login status 2>&1 | Out-Null
        return $LASTEXITCODE -eq 0
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Invoke-CodexLogin {
    param([string] $CodexPath)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $CodexPath login 2>&1 | Out-Host
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Ensure-Codex {
    $codex = Find-Codex
    if ($null -eq $codex) {
        if (-not (Read-Confirmation "Codex CLI is missing. Install it using the official OpenAI installer?")) {
            throw "Codex CLI is required by the selected assistant provider."
        }
        Write-Host "Downloading and running the official Codex CLI installer..."
        $installer = Invoke-RestMethod -Uri `
            "https://chatgpt.com/codex/install.ps1"
        Invoke-Expression $installer
        Update-ProcessPath
        $codex = Find-Codex
        if ($null -eq $codex) {
            throw "Codex installation completed, but the executable could not be found."
        }
    } else {
        Write-Host "Codex CLI already installed: $codex"
    }

    & $codex --version
    if ($LASTEXITCODE -ne 0) {
        throw "Codex CLI is installed but could not be started."
    }
    if (-not (Test-CodexLogin -CodexPath $codex)) {
        Write-Host ""
        Write-Host "Codex needs a ChatGPT account. The sign-in page will open in your browser."
        $loginExitCode = Invoke-CodexLogin -CodexPath $codex
        if ($loginExitCode -ne 0) {
            throw "Codex sign-in was cancelled or failed."
        }
        if (-not (Test-CodexLogin -CodexPath $codex)) {
            throw "Codex did not report a valid authenticated session."
        }
    } else {
        Write-Host "Codex authentication already configured."
    }
}

Write-Host "Checking the development environment..."
Update-ProcessPath

if ($null -eq (Find-CommandPath "git.exe")) {
    Install-WingetPackage -DisplayName "Git" -PackageId "Git.Git"
} else {
    Write-Host "Git already installed."
}

if ($null -eq (Find-CommandPath "cmake.exe")) {
    Install-WingetPackage -DisplayName "CMake" -PackageId "Kitware.CMake"
} else {
    Write-Host "CMake already installed."
}

if ($null -eq (Get-MsvcInstallation)) {
    Install-WingetPackage `
        -DisplayName "Visual Studio 2022 Build Tools with C++ support" `
        -PackageId "Microsoft.VisualStudio.2022.BuildTools" `
        -Override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if ($null -eq (Get-MsvcInstallation)) {
        throw "Visual Studio Build Tools installed, but the C++ workload was not detected."
    }
} else {
    Write-Host "Visual Studio C++ Build Tools already installed."
}

$assistantSettings = Ensure-AssistantSettings

if (-not (Test-Path -LiteralPath $openAISettingsPath)) {
    if (-not (Test-Path -LiteralPath $openAISettingsExamplePath)) {
        throw "OpenAI provider settings example is missing."
    }
    Copy-Item -LiteralPath $openAISettingsExamplePath `
        -Destination $openAISettingsPath
    Write-Host "Created local OpenAI provider settings."
}

$selectedProvider = [string] $assistantSettings.assistant.providerLibrary
if ($selectedProvider -eq "AssistantProviderCodex.dll") {
    Ensure-Codex
} elseif ($selectedProvider -eq "AssistantProviderOpenAI.dll") {
    & $configureOpenAIPath -SettingsPath $assistantSettingsPath
    if ($LASTEXITCODE -ne 0) {
        throw "OpenAI API configuration was cancelled or failed."
    }
}

Write-Host "Development environment is ready."
