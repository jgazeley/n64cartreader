$script:SYNC_A = [byte]0xAA
$script:SYNC_B = [byte]0x55
$script:ACK = [byte]0x06
$script:NAK = [byte]0x15

$script:CMD_MAGIC = [byte[]](0x50, 0x50, 0x4B, 0x31) # PPK1
$script:RSP_MAGIC = [byte[]](0x52, 0x50, 0x4B, 0x31) # RPK1

$script:ST_OK = [byte]0x00
$script:ST_BAD_CMD = [byte]0x01
$script:ST_IO_ERR = [byte]0x02
$script:ST_BAD_ARG = [byte]0x03

$script:CMD_N64_RESCAN = [byte]0x40
$script:CMD_N64_STATUS = [byte]0x41
$script:CMD_N64_EXPORT_SAVE = [byte]0x42
$script:CMD_N64_IMPORT_SAVE = [byte]0x43
$script:CMD_N64_ROM_INFO = [byte]0x44
$script:CMD_N64_EXPORT_ROM = [byte]0x45
$script:CMD_N64_EXPORT_HEADER = [byte]0x46
$script:CMD_N64_SET_SAVE_CFG = [byte]0x4F
$script:CMD_N64_SET_ROM_SIZE = [byte]0x50

$script:DEFAULT_BAUD = 115200
$script:DEFAULT_TIMEOUT_MS = 5000
$script:DEFAULT_CHUNK_SIZE = 512
$script:DEFAULT_RETRIES = 5

$script:N64_SAVE_TYPE_NAMES = @{
    0 = "none"
    1 = "sram"
    2 = "flashram"
    3 = "eeprom4k"
    4 = "eeprom16k"
    5 = "unknown"
}

$script:N64_SAVE_CONFIGS = @{
    "none"      = @{ Type = 0; Size = 0 }
    "sram"      = @{ Type = 1; Size = 32768 }
    "sram96k"   = @{ Type = 1; Size = 98304 }
    "flashram"  = @{ Type = 2; Size = 131072 }
    "eeprom4k"  = @{ Type = 3; Size = 512 }
    "eeprom16k" = @{ Type = 4; Size = 2048 }
}

function Get-N64SaveTypeName {
    param([Parameter(Mandatory)][int]$SaveType)

    if ($script:N64_SAVE_TYPE_NAMES.ContainsKey($SaveType)) {
        return $script:N64_SAVE_TYPE_NAMES[$SaveType]
    }
    return "unknown"
}

function Resolve-N64SaveConfig {
    param([Parameter(Mandatory)][string]$SaveType)

    $key = $SaveType.ToLowerInvariant()
    if (-not $script:N64_SAVE_CONFIGS.ContainsKey($key)) {
        throw "Unknown save type '$SaveType'. Expected one of: $($script:N64_SAVE_CONFIGS.Keys -join ', ')"
    }

    return $script:N64_SAVE_CONFIGS[$key]
}

function Decode-N64StatusMeta {
    param([Parameter(Mandatory)][int]$Meta)

    $saveType = ($Meta -shr 12) -band 0x0F
    $saveSize = ($Meta -band 0x0FFF) * 64

    [pscustomobject]@{
        SaveType = $saveType
        SaveTypeName = Get-N64SaveTypeName -SaveType $saveType
        SaveSize = $saveSize
        Raw = $Meta
    }
}

function Decode-N64RomSize {
    param([Parameter(Mandatory)][int]$SizeUnits2K)

    return $SizeUnits2K * 2048
}

function Get-N64HeaderBe32 {
    param(
        [Parameter(Mandatory)][byte[]]$Header,
        [Parameter(Mandatory)][int]$Offset
    )

    if ($Header.Length -lt ($Offset + 4)) {
        throw "Header is too short to read BE32 at offset $Offset"
    }

    return ((([int]$Header[$Offset]) -shl 24) -bor
            (([int]$Header[$Offset + 1]) -shl 16) -bor
            (([int]$Header[$Offset + 2]) -shl 8) -bor
            ([int]$Header[$Offset + 3])) -band 0xFFFFFFFF
}

function Get-N64HeaderText {
    param(
        [Parameter(Mandatory)][byte[]]$Header,
        [Parameter(Mandatory)][int]$Offset,
        [Parameter(Mandatory)][int]$Length
    )

    if ($Header.Length -lt ($Offset + $Length)) {
        return ""
    }

    $bytes = [byte[]]::new($Length)
    [Array]::Copy($Header, $Offset, $bytes, 0, $Length)
    return ([System.Text.Encoding]::ASCII.GetString($bytes)).Trim([char]0, [char]32)
}

function Assert-PpkStatusOk {
    param(
        [Parameter(Mandatory)][int]$Command,
        [Parameter(Mandatory)][int]$Status,
        [Parameter(Mandatory)][int]$Value
    )

    if ($Status -eq $script:ST_OK) {
        return
    }

    $statusName = switch ($Status) {
        { $_ -eq $script:ST_BAD_CMD } { "BAD_CMD"; break }
        { $_ -eq $script:ST_IO_ERR } { "IO_ERR"; break }
        { $_ -eq $script:ST_BAD_ARG } { "BAD_ARG"; break }
        default { "UNKNOWN"; break }
    }

    if ($Status -eq $script:ST_BAD_CMD) {
        throw ("Pico does not support command 0x{0:X2}. Flash the latest n64-headless UF2." -f $Command)
    }

    throw ("Pico returned status 0x{0:X2} ({1}) for command 0x{2:X2} (value=0x{3:X4})" -f $Status, $statusName, $Command, $Value)
}
