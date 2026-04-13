param(
    [Parameter(Mandatory)][string]$Port,
    [Parameter(Mandatory)][string]$Out,
    [string]$CatalogPath = "",
    [switch]$Rescan,
    [switch]$Quiet
)

. "$PSScriptRoot\lib\N64.ps1"

$session = $null
try {
    $session = Open-PpkSession -Port $Port -Quiet:$Quiet
    $result = Export-N64Rom -Session $session -OutPath $Out -CatalogPath $CatalogPath -Rescan:$Rescan
    Write-Host ("Dumped N64 ROM ({0} bytes) -> {1}, CRC16=0x{2:X4}" -f $result.Size, $result.Path, $result.Crc16)
} finally {
    if ($session) {
        Close-PpkSession -Session $session
    }
}
