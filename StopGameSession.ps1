param(
    [Parameter(Mandatory = $true)]
    [string] $BuildDirectory
)

$ErrorActionPreference = "Stop"
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory).TrimEnd(
    [IO.Path]::DirectorySeparatorChar,
    [IO.Path]::AltDirectorySeparatorChar)
$buildPrefix = $buildRoot + [IO.Path]::DirectorySeparatorChar

$sessionProcesses = Get-CimInstance Win32_Process |
    Where-Object {
        -not [string]::IsNullOrWhiteSpace($_.ExecutablePath) -and
        (
            [IO.Path]::GetFullPath($_.ExecutablePath).Equals(
                $buildRoot,
                [StringComparison]::OrdinalIgnoreCase) -or
            [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith(
                $buildPrefix,
                [StringComparison]::OrdinalIgnoreCase)
        )
    }

foreach ($process in $sessionProcesses) {
    Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
}
