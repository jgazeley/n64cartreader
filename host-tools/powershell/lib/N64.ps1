. "$PSScriptRoot\Protocol.ps1"
. "$PSScriptRoot\Crc.ps1"
. "$PSScriptRoot\Transport.ps1"
. "$PSScriptRoot\Catalog.ps1"

function Invoke-N64Rescan {
    param([Parameter(Mandatory)]$Session)

    try {
        $null = Set-N64SaveConfig -Session $Session -SaveType 0 -SizeBytes 0
    } catch {
        # Match the Python host flow: stale save config should be cleared before
        # rescan, but first-connect/no-cart failures should not block rescan.
    }

    $null = Invoke-PpkCommand -Session $Session -Command $script:CMD_N64_RESCAN
}

function Get-N64Status {
    param(
        [Parameter(Mandatory)]$Session,
        [switch]$Rescan
    )

    if ($Rescan) {
        Invoke-N64Rescan -Session $Session
    }

    $rsp = Invoke-PpkCommand -Session $Session -Command $script:CMD_N64_STATUS
    return Decode-N64StatusMeta -Meta $rsp.Value
}

function Set-N64SaveConfig {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$SaveType,
        [Parameter(Mandatory)][int]$SizeBytes
    )

    if ($SaveType -lt 0 -or $SaveType -gt 4) {
        throw "Invalid save type $SaveType"
    }
    if ($SizeBytes -lt 0) {
        throw "Invalid save size $SizeBytes"
    }

    $units64 = 0
    if ($SizeBytes -gt 0) {
        $units64 = [Math]::Ceiling($SizeBytes / 64.0)
    }
    if ($units64 -gt 0xFFFF) {
        throw "Save size too large for protocol: $SizeBytes"
    }

    $rsp = Invoke-PpkCommand -Session $Session -Command $script:CMD_N64_SET_SAVE_CFG -Arg0 $SaveType -Arg1 $units64
    return Decode-N64StatusMeta -Meta $rsp.Value
}

function Set-N64SaveConfigByName {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][string]$SaveType
    )

    $cfg = Resolve-N64SaveConfig -SaveType $SaveType
    return Set-N64SaveConfig -Session $Session -SaveType $cfg["Type"] -SizeBytes $cfg["Size"]
}

function Get-N64RomSize {
    param(
        [Parameter(Mandatory)]$Session,
        [switch]$Rescan
    )

    if ($Rescan) {
        Invoke-N64Rescan -Session $Session
    }

    $rsp = Invoke-PpkCommand -Session $Session -Command $script:CMD_N64_ROM_INFO
    return (Decode-N64RomSize -SizeUnits2K $rsp.Value)
}

function Set-N64RomSize {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$SizeBytes
    )

    if ($SizeBytes -le 0 -or $SizeBytes -gt (64 * 1024 * 1024)) {
        throw "Invalid ROM size: $SizeBytes"
    }

    $units2k = [Math]::Ceiling($SizeBytes / 2048.0)
    if ($units2k -gt 0xFFFF) {
        throw "ROM size too large for protocol: $SizeBytes"
    }

    $rsp = Invoke-PpkCommand -Session $Session -Command $script:CMD_N64_SET_ROM_SIZE -Arg1 $units2k
    return (Decode-N64RomSize -SizeUnits2K $rsp.Value)
}

function Export-N64Header {
    param(
        [Parameter(Mandatory)]$Session,
        [switch]$Rescan
    )

    if ($Rescan) {
        Invoke-N64Rescan -Session $Session
    }

    Send-PpkCommand -Session $Session -Command $script:CMD_N64_EXPORT_HEADER
    $data = Receive-PpkReliable -Session $Session -TotalSize 64
    $rsp = Receive-PpkResponse -Session $Session -ExpectCommand $script:CMD_N64_EXPORT_HEADER
    Assert-PpkStatusOk -Command $rsp.Command -Status $rsp.Status -Value $rsp.Value

    $hostCrc = Get-Crc16 -Data $data
    if ($hostCrc -ne $rsp.Value) {
        throw ("CRC mismatch after header export: pico=0x{0:X4}, host=0x{1:X4}" -f $rsp.Value, $hostCrc)
    }

    return ,$data
}

function Get-N64HeaderSummary {
    param([Parameter(Mandatory)][byte[]]$Header)

    [pscustomobject]@{
        Title = Get-N64HeaderText -Header $Header -Offset 0x20 -Length 20
        GameId = Get-N64HeaderText -Header $Header -Offset 0x3B -Length 4
        Crc1 = Get-N64HeaderBe32 -Header $Header -Offset 0x10
        Crc2 = Get-N64HeaderBe32 -Header $Header -Offset 0x14
    }
}

function Set-N64SaveConfigFromCatalog {
    param(
        [Parameter(Mandatory)]$Session,
        [string]$CatalogPath = ""
    )

    $header = Export-N64Header -Session $Session
    $romSize = 0
    try {
        $romSize = Get-N64RomSize -Session $Session
    } catch {
        $romSize = 0
    }

    $catalog = Import-N64TxtCatalog -Path $CatalogPath
    $match = Find-N64CatalogEntry -Catalog $catalog -Header $header -RomSize $romSize
    if (-not $match) {
        $summary = Get-N64HeaderSummary -Header $header
        throw ("Cart not found in n64.txt catalog: title='{0}', game_id='{1}', CRC1=0x{2:X8}" -f $summary.Title, $summary.GameId, $summary.Crc1)
    }
    if (-not $match.SaveHint) {
        throw ("Catalog match '{0}' has unsupported save code {1}" -f $match.Title, $match.SaveCode)
    }

    $status = Set-N64SaveConfigByName -Session $Session -SaveType $match.SaveHint
    [pscustomobject]@{
        Status = $status
        Match = $match
        CatalogPath = $catalog.Path
        RomSize = $romSize
    }
}

function Set-N64RomSizeFromCatalog {
    param(
        [Parameter(Mandatory)]$Session,
        [string]$CatalogPath = ""
    )

    $header = Export-N64Header -Session $Session
    $romSize = 0
    try {
        $romSize = Get-N64RomSize -Session $Session
    } catch {
        $romSize = 0
    }

    $catalog = Import-N64TxtCatalog -Path $CatalogPath
    $match = Find-N64CatalogEntry -Catalog $catalog -Header $header -RomSize $romSize
    if (-not $match) {
        $summary = Get-N64HeaderSummary -Header $header
        throw ("Cart not found in n64.txt catalog: title='{0}', game_id='{1}', CRC1=0x{2:X8}" -f $summary.Title, $summary.GameId, $summary.Crc1)
    }
    if ($match.RomSize -le 0) {
        throw ("Catalog match '{0}' has invalid ROM size {1}" -f $match.Title, $match.RomSize)
    }

    $confirmedSize = Set-N64RomSize -Session $Session -SizeBytes $match.RomSize
    [pscustomobject]@{
        RomSize = $confirmedSize
        Match = $match
        CatalogPath = $catalog.Path
    }
}

function Export-N64Rom {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][string]$OutPath,
        [string]$CatalogPath = "",
        [switch]$Rescan
    )

    if ($Rescan) {
        Invoke-N64Rescan -Session $Session
    }

    try {
        $romSize = Get-N64RomSize -Session $Session
    } catch {
        $auto = Set-N64RomSizeFromCatalog -Session $Session -CatalogPath $CatalogPath
        $romSize = $auto.RomSize
    }

    if ($romSize -le 0) {
        throw "N64 ROM size reported as 0"
    }

    Send-PpkCommand -Session $Session -Command $script:CMD_N64_EXPORT_ROM
    $hostCrc = Receive-PpkReliableToFile -Session $Session -TotalSize $romSize -OutPath $OutPath
    $rsp = Receive-PpkResponse -Session $Session -ExpectCommand $script:CMD_N64_EXPORT_ROM
    Assert-PpkStatusOk -Command $rsp.Command -Status $rsp.Status -Value $rsp.Value
    if ($hostCrc -ne $rsp.Value) {
        throw ("CRC mismatch after ROM dump: pico=0x{0:X4}, host=0x{1:X4}" -f $rsp.Value, $hostCrc)
    }

    [pscustomobject]@{
        Path = $OutPath
        Size = $romSize
        Crc16 = $hostCrc
    }
}

function Export-N64Save {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][string]$OutPath,
        [string]$SaveType = "",
        [string]$CatalogPath = "",
        [switch]$Rescan
    )

    if ($Rescan) {
        Invoke-N64Rescan -Session $Session
    }
    if ($SaveType) {
        $status = Set-N64SaveConfigByName -Session $Session -SaveType $SaveType
    } else {
        $auto = Set-N64SaveConfigFromCatalog -Session $Session -CatalogPath $CatalogPath
        $status = $auto.Status
    }

    if ($status.SaveSize -le 0 -or $status.SaveType -eq 0 -or $status.SaveType -eq 5) {
        throw "N64 save not available. Use -SaveType if this first-pass PowerShell tool cannot infer the cart save type."
    }

    Send-PpkCommand -Session $Session -Command $script:CMD_N64_EXPORT_SAVE
    $data = Receive-PpkReliable -Session $Session -TotalSize $status.SaveSize
    $rsp = Receive-PpkResponse -Session $Session -ExpectCommand $script:CMD_N64_EXPORT_SAVE
    Assert-PpkStatusOk -Command $rsp.Command -Status $rsp.Status -Value $rsp.Value

    $hostCrc = Get-Crc16 -Data $data
    if ($hostCrc -ne $rsp.Value) {
        throw ("CRC mismatch after save export: pico=0x{0:X4}, host=0x{1:X4}" -f $rsp.Value, $hostCrc)
    }

    [System.IO.File]::WriteAllBytes($OutPath, $data)

    [pscustomobject]@{
        Path = $OutPath
        SaveType = $status.SaveType
        SaveTypeName = $status.SaveTypeName
        Size = $data.Length
        Crc16 = $hostCrc
    }
}

function Import-N64Save {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][string]$InPath,
        [string]$SaveType = "",
        [string]$CatalogPath = "",
        [switch]$Rescan,
        [switch]$Verify
    )

    if (-not [System.IO.File]::Exists($InPath)) {
        throw "Input file not found: $InPath"
    }

    if ($Rescan) {
        Invoke-N64Rescan -Session $Session
    }
    if ($SaveType) {
        $status = Set-N64SaveConfigByName -Session $Session -SaveType $SaveType
    } else {
        $auto = Set-N64SaveConfigFromCatalog -Session $Session -CatalogPath $CatalogPath
        $status = $auto.Status
    }

    if ($status.SaveSize -le 0 -or $status.SaveType -eq 0 -or $status.SaveType -eq 5) {
        throw "N64 save not available. Use -SaveType if this first-pass PowerShell tool cannot infer the cart save type."
    }

    $data = [System.IO.File]::ReadAllBytes($InPath)
    if ($data.Length -ne $status.SaveSize) {
        throw "Size mismatch: file=$($data.Length) bytes, cart expects $($status.SaveSize) bytes"
    }

    $oldTimeout = $Session.Serial.ReadTimeout
    $oldRetries = $Session.Retries
    $oldDelay = $Session.TxInterChunkDelayMs

    if ($status.SaveType -eq 2) {
        $Session.Serial.ReadTimeout = [Math]::Max($oldTimeout, 15000)
        $Session.Retries = [Math]::Max($oldRetries, 10)
        $Session.TxInterChunkDelayMs = [Math]::Max($oldDelay, 150)
    }

    try {
        Send-PpkCommand -Session $Session -Command $script:CMD_N64_IMPORT_SAVE
        Send-PpkReliable -Session $Session -Data $data
        $rsp = Receive-PpkResponse -Session $Session -ExpectCommand $script:CMD_N64_IMPORT_SAVE
        Assert-PpkStatusOk -Command $rsp.Command -Status $rsp.Status -Value $rsp.Value

        $hostCrc = Get-Crc16 -Data $data
        if ($hostCrc -ne $rsp.Value) {
            throw ("CRC mismatch after save import: pico=0x{0:X4}, host=0x{1:X4}" -f $rsp.Value, $hostCrc)
        }

        if ($Verify) {
            Send-PpkCommand -Session $Session -Command $script:CMD_N64_EXPORT_SAVE
            $verifyData = Receive-PpkReliable -Session $Session -TotalSize $status.SaveSize
            $verifyRsp = Receive-PpkResponse -Session $Session -ExpectCommand $script:CMD_N64_EXPORT_SAVE
            Assert-PpkStatusOk -Command $verifyRsp.Command -Status $verifyRsp.Status -Value $verifyRsp.Value
            $verifyCrc = Get-Crc16 -Data $verifyData
            if ($verifyCrc -ne $hostCrc -or $verifyData.Length -ne $data.Length) {
                throw "Verify failed: read-back CRC or size does not match source file"
            }
            for ($i = 0; $i -lt $data.Length; $i++) {
                if ($verifyData[$i] -ne $data[$i]) {
                    throw "Verify failed: read-back data differs at byte $i"
                }
            }
        }

        [pscustomobject]@{
            Path = $InPath
            SaveType = $status.SaveType
            SaveTypeName = $status.SaveTypeName
            Size = $data.Length
            Crc16 = $hostCrc
            Verified = [bool]$Verify
        }
    } finally {
        $Session.Serial.ReadTimeout = $oldTimeout
        $Session.Retries = $oldRetries
        $Session.TxInterChunkDelayMs = $oldDelay
    }
}
