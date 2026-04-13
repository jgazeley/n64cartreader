function Update-Crc16 {
    param(
        [Parameter(Mandatory)][int]$Crc,
        [Parameter(Mandatory)][byte[]]$Data,
        [int]$Offset = 0,
        [int]$Count = -1
    )

    if ($Count -lt 0) {
        $Count = $Data.Length - $Offset
    }

    for ($i = 0; $i -lt $Count; $i++) {
        $crc = ($Crc -bxor ([int]$Data[$Offset + $i] -shl 8)) -band 0xFFFF
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 0x8000) -ne 0) {
                $crc = (($crc -shl 1) -bxor 0x8005) -band 0xFFFF
            } else {
                $crc = ($crc -shl 1) -band 0xFFFF
            }
        }
        $Crc = $crc
    }

    return ($Crc -band 0xFFFF)
}

function Get-Crc16 {
    param([Parameter(Mandatory)][byte[]]$Data)

    return Update-Crc16 -Crc 0 -Data $Data
}

