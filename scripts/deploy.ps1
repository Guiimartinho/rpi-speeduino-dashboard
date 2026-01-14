# deploy.ps1 - Deploy Speeduino UI to Raspberry Pi / Jetson
# Usage: .\scripts\deploy.ps1 [-Build] [-Test] [-Run] [-QmlOnly] [-Target rpi|jetson]
#
# ISO 26262 ASIL-B: Automated deployment for embedded targets

param(
    [switch]$Build,          # Build on target after sync
    [switch]$Test,           # Run tests after build
    [switch]$Run,            # Run app after build
    [switch]$QmlOnly,        # Sync only QML files (fast iteration)
    [string]$Target = "rpi", # Target platform: rpi or jetson
    [string]$RemoteHost      # Override default host
)

$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

# Set defaults based on target
switch ($Target) {
    "jetson" {
        $DefaultHost = "jetson"
        $RemoteUser = "nvidia"
    }
    default {
        $DefaultHost = "raspui"
        $RemoteUser = "raspui"
    }
}

if (-not $RemoteHost) {
    $RemoteHost = $DefaultHost
}
$RemotePath = "/home/$RemoteUser/speeduino-ui"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Speeduino UI - Deploy to $($Target.ToUpper())" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Calculate total steps
$TotalSteps = 3
if ($Build) { $TotalSteps++ }
if ($Test) { $TotalSteps++; if (-not $Build) { $Build = $true; $TotalSteps++ } }
if ($Run) { $TotalSteps++ }
$CurrentStep = 0

# Check SSH connection
$CurrentStep++
Write-Host "[$CurrentStep/$TotalSteps] Testing SSH connection..." -ForegroundColor Yellow
$sshTest = ssh -o BatchMode=yes -o ConnectTimeout=5 $RemoteHost "echo ok" 2>&1
if ($sshTest -ne "ok") {
    Write-Host "ERROR: Cannot connect to $RemoteHost" -ForegroundColor Red
    exit 1
}
Write-Host "  Connected to $RemoteHost" -ForegroundColor Green

# Create remote directory
$CurrentStep++
Write-Host "[$CurrentStep/$TotalSteps] Creating remote directory..." -ForegroundColor Yellow
ssh $RemoteHost "mkdir -p $RemotePath"

# Sync files
$CurrentStep++
Write-Host "[$CurrentStep/$TotalSteps] Syncing files..." -ForegroundColor Yellow

if ($QmlOnly) {
    # Fast QML-only sync for UI iteration
    Write-Host "  Syncing QML files only..." -ForegroundColor Cyan
    rsync -avz --progress `
        --include='*.qml' `
        --include='*/' `
        --exclude='*' `
        "$ProjectRoot/src/hmi_launcher/qml/" `
        "${RemoteHost}:${RemotePath}/src/hmi_launcher/qml/"
} else {
    # Full sync
    Write-Host "  Full project sync..." -ForegroundColor Cyan
    rsync -avz --progress `
        --exclude='.git' `
        --exclude='build' `
        --exclude='cmake-build-*' `
        --exclude='.claude' `
        --exclude='preview' `
        --exclude='*.exe' `
        --exclude='*.dll' `
        --exclude='nul' `
        "$ProjectRoot/" `
        "${RemoteHost}:${RemotePath}/"
}

Write-Host "  Sync complete!" -ForegroundColor Green

# Build if requested
if ($Build) {
    $CurrentStep++
    Write-Host "[$CurrentStep/$TotalSteps] Building on $($Target.ToUpper())..." -ForegroundColor Yellow

    $BuildJobs = if ($Target -eq "jetson") { 6 } else { 4 }

    ssh $RemoteHost @"
cd $RemotePath && \
mkdir -p build && \
cd build && \
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON .. && \
ninja -j$BuildJobs
"@

    if ($LASTEXITCODE -eq 0) {
        Write-Host "  Build successful!" -ForegroundColor Green
    } else {
        Write-Host "  Build failed!" -ForegroundColor Red
        exit 1
    }
}

# Run tests if requested
if ($Test) {
    $CurrentStep++
    Write-Host "[$CurrentStep/$TotalSteps] Running tests on $($Target.ToUpper())..." -ForegroundColor Yellow

    ssh $RemoteHost "cd $RemotePath/build && ctest --output-on-failure -j4"

    if ($LASTEXITCODE -eq 0) {
        Write-Host "  All tests passed!" -ForegroundColor Green
    } else {
        Write-Host "  Some tests failed!" -ForegroundColor Red
        # Don't exit on test failure - user may want to inspect
    }
}

# Run if requested
if ($Run) {
    $CurrentStep++
    Write-Host "[$CurrentStep/$TotalSteps] Launching application..." -ForegroundColor Yellow
    ssh $RemoteHost "$RemotePath/build/bin/hmi_launcher"
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Deploy complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Quick commands:" -ForegroundColor Cyan
Write-Host "  ssh $RemoteHost                          - Connect to target"
Write-Host "  ssh $RemoteHost 'cd ~/speeduino-ui && ls' - Check files"
Write-Host ""
Write-Host "Deploy options:" -ForegroundColor Cyan
Write-Host "  .\scripts\deploy.ps1                          - Sync only"
Write-Host "  .\scripts\deploy.ps1 -Build                   - Sync + build"
Write-Host "  .\scripts\deploy.ps1 -Test                    - Sync + build + test"
Write-Host "  .\scripts\deploy.ps1 -Build -Run              - Sync + build + run"
Write-Host "  .\scripts\deploy.ps1 -QmlOnly                 - Fast QML sync"
Write-Host "  .\scripts\deploy.ps1 -Target jetson -Test     - Deploy to Jetson + test"
Write-Host ""
