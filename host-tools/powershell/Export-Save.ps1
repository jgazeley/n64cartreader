param(
    [Parameter(Mandatory)][string]$Port,
    [Parameter(Mandatory)][string]$Out,
    [string]$SaveType = "",
    [string]$CatalogPath = "",
    [switch]$Rescan,
    [switch]$Quiet
)

. "$PSScriptRoot\lib\N64.ps1"

$session = $null
try {
    $session = Open-PpkSession -Port $Port -Quiet:$Quiet
    $result = Export-N64Save -Session $session -OutPath $Out -SaveType $SaveType -CatalogPath $CatalogPath -Rescan:$Rescan
    Write-Host ("Exported N64 save ({0}, {1} bytes) -> {2}, CRC16=0x{3:X4}" -f $result.SaveTypeName, $result.Size, $result.Path, $result.Crc16)
} finally {
    if ($session) {
        Close-PpkSession -Session $session
    }
}
