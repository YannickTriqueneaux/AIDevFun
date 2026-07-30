param(
    [Parameter(Mandatory = $true)]
    [string] $SettingsPath
)

$ErrorActionPreference = "Stop"
$apiKeysUrl = "https://platform.openai.com/api-keys"
$defaultModel = "gpt-5.5"
$settings = $null

if (Test-Path -LiteralPath $SettingsPath) {
    try {
        $settings = Get-Content -LiteralPath $SettingsPath -Raw |
            ConvertFrom-Json
        $configuredKey = [string] $settings.openai.apiKey
        if (
            -not [string]::IsNullOrWhiteSpace($configuredKey) -and
            $configuredKey -ne "your-api-key"
        ) {
            exit 0
        }
    }
    catch {
        Write-Host "The existing OpenAI settings file is invalid and will be replaced."
    }
}

Write-Host ""
Write-Host "An OpenAI API key is required by AssistantHost."
Write-Host "Create or retrieve one here:"
Write-Host $apiKeysUrl
Write-Host ""

try {
    Start-Process $apiKeysUrl
}
catch {
    Write-Host "The API key page could not be opened automatically."
}

$secureKey = Read-Host "Paste your OpenAI API key" -AsSecureString
$keyPointer = [IntPtr]::Zero

try {
    $keyPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR(
        $secureKey)
    $apiKey = [Runtime.InteropServices.Marshal]::PtrToStringBSTR(
        $keyPointer)
}
finally {
    if ($keyPointer -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($keyPointer)
    }
}

if ([string]::IsNullOrWhiteSpace($apiKey)) {
    Write-Error "No API key was provided."
    exit 1
}

$model = $defaultModel
if (
    $null -ne $settings -and
    -not [string]::IsNullOrWhiteSpace([string] $settings.openai.model)
) {
    $model = [string] $settings.openai.model
}

$parentDirectory = Split-Path -Parent $SettingsPath
New-Item -ItemType Directory -Path $parentDirectory -Force | Out-Null

@{
    openai = @{
        apiKey = $apiKey
        model = $model
    }
} |
    ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath $SettingsPath -Encoding UTF8

Write-Host "OpenAI settings saved to AssistantHost/Config/settings.json."
