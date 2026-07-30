param(
    [Parameter(Mandatory = $true)]
    [string] $SettingsPath
)

$ErrorActionPreference = "Stop"
$apiKeysUrl = "https://platform.openai.com/api-keys"
$defaultModel = "gpt-5.5"
$defaultPricing = @{
    model = "gpt-5.5"
    inputUsdPerMillion = 5.0
    cachedInputUsdPerMillion = 0.5
    outputUsdPerMillion = 30.0
    longContextThreshold = 272000
    longContextInputMultiplier = 2.0
    longContextOutputMultiplier = 1.5
}
$settings = $null

function Save-OpenAISettings {
    param(
        [string] $ApiKey,
        [string] $Model,
        [object] $Pricing
    )

    $parentDirectory = Split-Path -Parent $SettingsPath
    New-Item -ItemType Directory -Path $parentDirectory -Force | Out-Null
    @{
        openai = @{
            apiKey = $ApiKey
            model = $Model
            pricing = $Pricing
        }
    } |
        ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath $SettingsPath -Encoding UTF8
}

if (Test-Path -LiteralPath $SettingsPath) {
    try {
        $settings = Get-Content -LiteralPath $SettingsPath -Raw |
            ConvertFrom-Json
        $configuredKey = [string] $settings.openai.apiKey
        if (
            -not [string]::IsNullOrWhiteSpace($configuredKey) -and
            $configuredKey -ne "your-api-key"
        ) {
            $configuredModel = [string] $settings.openai.model
            if ([string]::IsNullOrWhiteSpace($configuredModel)) {
                $configuredModel = $defaultModel
            }
            $configuredPricing = $settings.openai.pricing
            if ($null -eq $configuredPricing) {
                $configuredPricing = $defaultPricing
            }
            Save-OpenAISettings `
                -ApiKey $configuredKey `
                -Model $configuredModel `
                -Pricing $configuredPricing
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

Save-OpenAISettings `
    -ApiKey $apiKey `
    -Model $model `
    -Pricing $defaultPricing

Write-Host "OpenAI settings saved to AssistantHost/Config/settings.json."
