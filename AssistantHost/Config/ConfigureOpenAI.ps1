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
$launcherSettings = Get-Content -LiteralPath $SettingsPath -Raw | ConvertFrom-Json
if ($launcherSettings.assistant.providerLibrary -ne "AssistantProviderOpenAI.dll") {
    exit 0
}
$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path (Split-Path -Parent $SettingsPath) "..\.."))
$SettingsPath = Join-Path $repositoryRoot `
    "AssistantProviders\OpenAI\OpenAIProvider.settings.json"

function Save-OpenAISettings {
    param(
        [string] $ApiKey,
        [string] $Model,
        [object] $Pricing
    )

    $parentDirectory = Split-Path -Parent $SettingsPath
    New-Item -ItemType Directory -Path $parentDirectory -Force | Out-Null
    @{
        apiKey = $ApiKey
        model = $Model
        pricing = $Pricing
    } |
        ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath $SettingsPath -Encoding UTF8
}

if (Test-Path -LiteralPath $SettingsPath) {
    try {
        $settings = Get-Content -LiteralPath $SettingsPath -Raw |
            ConvertFrom-Json
        $configuredKey = [string] $settings.apiKey
        if (
            -not [string]::IsNullOrWhiteSpace($configuredKey) -and
            $configuredKey -ne "your-api-key"
        ) {
            $configuredModel = [string] $settings.model
            if ([string]::IsNullOrWhiteSpace($configuredModel)) {
                $configuredModel = $defaultModel
            }
            $configuredPricing = $settings.pricing
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
    -not [string]::IsNullOrWhiteSpace([string] $settings.model)
) {
    $model = [string] $settings.model
}

Save-OpenAISettings `
    -ApiKey $apiKey `
    -Model $model `
    -Pricing $defaultPricing

Write-Host "OpenAI provider settings saved."
