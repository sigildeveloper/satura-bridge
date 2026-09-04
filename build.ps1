$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$FirmwareRoot = Join-Path $ProjectRoot "firmware"
$BuildRoot = Join-Path $ProjectRoot "build"

# ------------------------------------------------------------
# Arguments
#
#   .\build.ps1
#   .\build.ps1 build core2
#   .\build.ps1 build plus2
#   .\build.ps1 rebuild
#   .\build.ps1 clean
#   .\build.ps1 menuconfig core2
# ------------------------------------------------------------

$Action = "build"
$Board = "all"

if ($args.Count -ge 1) {
    $Action = $args[0].ToLower()
}

if ($args.Count -ge 2) {
    $Board = $args[1].ToLower()
}

# Allow:
#   .\build.ps1 core2
# as shorthand for:
#   .\build.ps1 build core2

if ($Action -in @("generic", "core2", "plus2")) {
    $Board = $Action
    $Action = "build"
}

# ------------------------------------------------------------
# Board definitions
# ------------------------------------------------------------

$Boards = @{
    generic = @{
        Name = "Generic ESP32 DevKit"
        Defaults = "sdkconfig.defaults.generic"
        BuildDir = "build\generic"
        Firmware = "firmware\generic\satura-bridge-generic-full.bin"
        ExpectedConfig = "CONFIG_SATURA_BOARD_GENERIC=y"
        ExpectedFlash = 'CONFIG_ESPTOOLPY_FLASHSIZE="2MB"'
        FlashSize = "2MB"
    }

    core2 = @{
        Name = "M5Stack Core2"
        Defaults = "sdkconfig.defaults.core2"
        BuildDir = "build\core2"
        Firmware = "firmware\core2\satura-bridge-core2-full.bin"
        ExpectedConfig = "CONFIG_SATURA_BOARD_CORE2=y"
        ExpectedFlash = 'CONFIG_ESPTOOLPY_FLASHSIZE="16MB"'
        FlashSize = "16MB"
    }

    plus2 = @{
        Name = "M5StickC Plus2"
        Defaults = "sdkconfig.defaults.stickc_plus2"
        BuildDir = "build\stickc_plus2"
        Firmware = "firmware\stickc_plus2\satura-bridge-stickc_plus2-full.bin"
        ExpectedConfig = "CONFIG_SATURA_BOARD_STICKC_PLUS2=y"
        ExpectedFlash = 'CONFIG_ESPTOOLPY_FLASHSIZE="8MB"'
        FlashSize = "8MB"
    }
}

# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------

function Write-Info {
    param([string]$Message)

    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

function Write-Ok {
    param([string]$Message)

    Write-Host "[ OK ] $Message" -ForegroundColor Green
}

function Write-Fail {
    param([string]$Message)

    Write-Host "[FAIL] $Message" -ForegroundColor Red
}

function Invoke-Idf {
    param(
        [string]$BuildDir,
        [string]$SdkConfig,
        [string]$Defaults,
        [string]$Action
    )

    $CommonDefaultsPath = Join-Path $ProjectRoot "sdkconfig.defaults"
    $BoardDefaultsPath = Join-Path $ProjectRoot $Defaults
    $SdkConfigPath = Join-Path $ProjectRoot $SdkConfig

    & idf.py `
        -B $BuildDir `
        -D "SDKCONFIG=$SdkConfigPath" `
        -D "SDKCONFIG_DEFAULTS=$CommonDefaultsPath;$BoardDefaultsPath" `
        $Action

    if ($LASTEXITCODE -ne 0) {
        throw "idf.py $Action failed"
    }
}

function Test-BoardConfig {
    param(
        [hashtable]$Config
    )

    $SdkConfigPath = Join-Path $ProjectRoot "$($Config.BuildDir)\sdkconfig"

    if (!(Test-Path $SdkConfigPath)) {
        throw "sdkconfig not found: $SdkConfigPath"
    }

    $Text = Get-Content $SdkConfigPath -Raw

    if ($Text -notmatch [regex]::Escape($Config.ExpectedConfig)) {
        throw @"
Wrong board configuration.

File:
$SdkConfigPath

Expected:
$($Config.ExpectedConfig)
"@
    }

    if ($Text -notmatch [regex]::Escape($Config.ExpectedFlash)) {
        throw @"
Wrong Flash configuration.

File:
$SdkConfigPath

Expected:
$($Config.ExpectedFlash)
"@
    }

    Write-Ok "$($Config.Name): board configuration OK"
    Write-Ok "$($Config.Name): Flash $($Config.FlashSize)"
}

function Build-Board {
    param(
        [string]$BoardName
    )

    $Config = $Boards[$BoardName]

    Write-Host ""
    Write-Host "============================================================" -ForegroundColor DarkGray
    Write-Host " BUILD: $($Config.Name)" -ForegroundColor Yellow
    Write-Host "============================================================" -ForegroundColor DarkGray

    $BuildDir = Join-Path $ProjectRoot $Config.BuildDir
    $SdkConfig = "$($Config.BuildDir)\sdkconfig"

    $FirmwarePath = Join-Path $ProjectRoot $Config.Firmware
    $FirmwareDir = Split-Path $FirmwarePath -Parent

    New-Item -ItemType Directory -Force -Path $FirmwareDir | Out-Null

    # --------------------------------------------------------
    # Configure
    # --------------------------------------------------------

    Write-Info "Configuring..."

    Invoke-Idf `
        -BuildDir $Config.BuildDir `
        -SdkConfig $SdkConfig `
        -Defaults $Config.Defaults `
        -Action "reconfigure"

    # --------------------------------------------------------
    # Verify configuration
    # --------------------------------------------------------

    Test-BoardConfig $Config

    # --------------------------------------------------------
    # Build
    # --------------------------------------------------------

    Write-Info "Building..."

    Invoke-Idf `
        -BuildDir $Config.BuildDir `
        -SdkConfig $SdkConfig `
        -Defaults $Config.Defaults `
        -Action "build"

    # --------------------------------------------------------
    # Check generated files
    # --------------------------------------------------------

    $Bootloader = Join-Path $BuildDir "bootloader\bootloader.bin"
    $Partition = Join-Path $BuildDir "partition_table\partition-table.bin"
    $Application = Join-Path $BuildDir "satura-bridge.bin"

    if (!(Test-Path $Bootloader)) {
        throw "Missing bootloader.bin"
    }

    if (!(Test-Path $Partition)) {
        throw "Missing partition-table.bin"
    }

    if (!(Test-Path $Application)) {
        throw "Missing satura-bridge.bin"
    }

    # --------------------------------------------------------
    # Merge full firmware
    # --------------------------------------------------------

    Write-Info "Creating full firmware image..."

    & python -m esptool `
        --chip esp32 `
        merge_bin `
        -o $FirmwarePath `
        --flash_mode dio `
        --flash_freq 40m `
        --flash_size $Config.FlashSize `
        0x1000 $Bootloader `
        0x8000 $Partition `
        0x10000 $Application

    if ($LASTEXITCODE -ne 0) {
        throw "esptool merge_bin failed"
    }

    if (!(Test-Path $FirmwarePath)) {
        throw "Full firmware was not created: $FirmwarePath"
    }

    $Size = (Get-Item $FirmwarePath).Length

    Write-Ok "$($Config.Name) complete"
    Write-Host "      Firmware: $FirmwarePath"
    Write-Host "      Size:     $Size bytes"
}

function Clean-Board {
    param(
        [string]$BoardName
    )

    $Config = $Boards[$BoardName]

    $BuildDir = Join-Path $ProjectRoot $Config.BuildDir
    $FirmwareDir = Join-Path $ProjectRoot (Split-Path $Config.Firmware -Parent)

    if (Test-Path $BuildDir) {
        Write-Info "Removing $BuildDir"
        Remove-Item -Recurse -Force $BuildDir
    }

    if (Test-Path $FirmwareDir) {
        Write-Info "Removing $FirmwareDir"
        Remove-Item -Recurse -Force $FirmwareDir
    }
}

function Open-Menuconfig {
    param(
        [string]$BoardName
    )

    $Config = $Boards[$BoardName]

    $SdkConfig = "$($Config.BuildDir)\sdkconfig"

    Write-Info "Opening menuconfig for $($Config.Name)..."

    Invoke-Idf `
        -BuildDir $Config.BuildDir `
        -SdkConfig $SdkConfig `
        -Defaults $Config.Defaults `
        -Action "menuconfig"
}

function Show-Help {
    Write-Host ""
    Write-Host "Satura Bridge build system" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "BUILD:"
    Write-Host "  .\build.ps1"
    Write-Host "      Build all boards"
    Write-Host ""
    Write-Host "  .\build.ps1 core2"
    Write-Host "      Build Core2"
    Write-Host ""
    Write-Host "  .\build.ps1 plus2"
    Write-Host "      Build M5StickC Plus2"
    Write-Host ""
    Write-Host "  .\build.ps1 generic"
    Write-Host "      Build Generic ESP32"
    Write-Host ""
    Write-Host "  .\build.ps1 build core2"
    Write-Host "      Build Core2"
    Write-Host ""
    Write-Host "REBUILD:"
    Write-Host "  .\build.ps1 rebuild"
    Write-Host "      Clean + build all boards"
    Write-Host ""
    Write-Host "  .\build.ps1 rebuild core2"
    Write-Host "      Clean + build Core2"
    Write-Host ""
    Write-Host "MENUCONFIG:"
    Write-Host "  .\build.ps1 menuconfig core2"
    Write-Host "  .\build.ps1 menuconfig plus2"
    Write-Host "  .\build.ps1 menuconfig generic"
    Write-Host ""
    Write-Host "CLEAN:"
    Write-Host "  .\build.ps1 clean"
    Write-Host "      Clean all boards"
    Write-Host ""
    Write-Host "  .\build.ps1 clean core2"
    Write-Host "      Clean Core2"
    Write-Host ""
}

# ------------------------------------------------------------
# Validate board
# ------------------------------------------------------------

if ($Action -in @("build", "rebuild", "clean", "menuconfig")) {

    if ($Board -ne "all" -and !$Boards.ContainsKey($Board)) {
        Write-Fail "Unknown board: $Board"
        Show-Help
        exit 1
    }
}

# ------------------------------------------------------------
# Execute
# ------------------------------------------------------------

try {

    switch ($Action) {

        "build" {

            if ($Board -eq "all") {

                foreach ($Name in @("generic", "core2", "plus2")) {
                    Build-Board $Name
                }

            } else {

                Build-Board $Board
            }
        }

        "rebuild" {

            if ($Board -eq "all") {

                foreach ($Name in @("generic", "core2", "plus2")) {
                    Clean-Board $Name
                }

                foreach ($Name in @("generic", "core2", "plus2")) {
                    Build-Board $Name
                }

            } else {

                Clean-Board $Board
                Build-Board $Board
            }
        }

        "clean" {

            if ($Board -eq "all") {

                foreach ($Name in @("generic", "core2", "plus2")) {
                    Clean-Board $Name
                }

            } else {

                Clean-Board $Board
            }
        }

        "menuconfig" {

            if ($Board -eq "all") {
                throw "menuconfig requires a board: core2, plus2 or generic"
            }

            Open-Menuconfig $Board
        }

        "help" {
            Show-Help
        }

        default {
            Show-Help
        }
    }

}
catch {

    Write-Host ""
    Write-Fail $_.Exception.Message
    Write-Host ""
    exit 1
}