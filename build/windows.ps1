param (
    [string]$Config = "Release",
    [string]$BuildDir = "build/output",
    [string]$JuceRoot = "../JUCE"
)

$ErrorActionPreference = "Stop"

if (-not ($IsWindows -or $env:OS -eq "Windows_NT")) {
    throw "build/windows.ps1 only supports Windows. Use build/macos.sh or build/linux.sh for other platforms."
}

$scriptDirectory = Split-Path -Parent $PSCommandPath
$repoRoot = Split-Path -Parent $scriptDirectory

function Resolve-RepoPath {
    param (
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
}

$knownConfigs = @{
    "debug"          = "Debug"
    "release"        = "Release"
    "minsizerel"     = "MinSizeRel"
    "relwithdebinfo" = "RelWithDebInfo"
}

$configKey = $Config.ToLowerInvariant()
if ($knownConfigs.ContainsKey($configKey)) {
    $Config = $knownConfigs[$configKey]
}

function Copy-BuildArtifact {
    param (
        [string]$Source,
        [string]$DestinationDirectory
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Expected build artifact was not found: $Source"
    }

    New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null

    $destination = Join-Path $DestinationDirectory (Split-Path $Source -Leaf)
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }

    Copy-Item -LiteralPath $Source -Destination $DestinationDirectory -Recurse -Force
    Write-Host "Copied: $destination" -ForegroundColor Green
}

$resolvedBuildDir = Resolve-RepoPath $BuildDir
$resolvedJuceRoot = Resolve-RepoPath $JuceRoot

$cmakeArgs = @(
    "-S", $repoRoot,
    "-B", $resolvedBuildDir,
    "-A", "x64",
    "-DJUCE_ROOT=$resolvedJuceRoot"
)

Write-Host "--- Configuring CMake ---" -ForegroundColor Cyan
& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "--- Building $Config with CMake ---" -ForegroundColor Cyan
& cmake --build $resolvedBuildDir --config $Config --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

if ($Config -ieq "Debug") {
    $outputConfig = "debug"
} elseif ($Config -ieq "Release") {
    $outputConfig = "release"
} else {
    $outputConfig = $Config.ToLowerInvariant()
}

$artifactRoot = Join-Path $resolvedBuildDir "Altar_artefacts/$Config"
$outputDirectory = Join-Path $repoRoot (Join-Path "binaries" $outputConfig)

$artifacts = @(
    @{
        Source = "Standalone/Altar.exe"
        DestinationDirectory = $outputDirectory
    },
    @{
        Source = "VST3/Altar.vst3"
        DestinationDirectory = (Join-Path $outputDirectory "VST3")
    },
    @{
        Source = "VST3/Altar.vst3/Contents/x86_64-win/Altar.vst3"
        DestinationDirectory = $outputDirectory
    }
)

Write-Host "--- Copying build artifacts to $outputDirectory ---" -ForegroundColor Cyan
foreach ($artifact in $artifacts) {
    Copy-BuildArtifact `
        -Source (Join-Path $artifactRoot $artifact.Source) `
        -DestinationDirectory $artifact.DestinationDirectory
}

Write-Host "--- CMake build successful! ---" -ForegroundColor Green
