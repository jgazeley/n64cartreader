param(
    [Parameter(Mandatory)][string]$Port,
    [Parameter(Mandatory)][string]$In,
    [string]$SaveType = "",
    [string]$CatalogPath = "",
    [switch]$Rescan,
    [switch]$Verify,
    [switch]$Quiet
)

. "$PSScriptRoot\lib\N64.ps1"

$session = $null
try {
    $session = Open-PpkSession -Port $Port -Quiet:$Quiet
    $result = Import-N64Save -Session $session -InPath $In -SaveType $SaveType -CatalogPath $CatalogPath -Rescan:$Rescan -Verify:$Verify
    Write-Host ("Imported N64 save ({0}, {1} bytes) from {2}, CRC16=0x{3:X4}" -f $result.SaveTypeName, $result.Size, $result.Path, $result.Crc16)
    if ($result.Verified) {
        Write-Host "Verification SUCCESS."
    }
} finally {
    if ($session) {
        Close-PpkSession -Session $session
    }
}
