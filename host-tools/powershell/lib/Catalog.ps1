. "$PSScriptRoot\Protocol.ps1"

function Get-N64DefaultCatalogPath {
    $path = Join-Path $PSScriptRoot "..\..\n64.txt"
    return ([System.IO.Path]::GetFullPath($path))
}

function Convert-N64TxtSaveCodeToHint {
    param([Parameter(Mandatory)][int]$SaveCode)

    switch ($SaveCode) {
        0 { return "none" }
        1 { return "sram" }
        2 { return "sram96k" }
        4 { return "flashram" }
        5 { return "eeprom4k" }
        6 { return "eeprom16k" }
        default { return $null }
    }
}

function Convert-N64TxtSmallInt {
    param([Parameter(Mandatory)][string]$Text)

    $stripped = $Text.Trim()
    if ($stripped -match '[A-Fa-f]') {
        return [Convert]::ToInt32($stripped, 16)
    }
    return [Convert]::ToInt32($stripped, 10)
}

function Import-N64TxtCatalog {
    param([string]$Path = "")

    if (-not $Path) {
        $Path = Get-N64DefaultCatalogPath
    }
    if (-not [System.IO.File]::Exists($Path)) {
        throw "n64.txt catalog not found: $Path"
    }

    $entriesByCrc1 = @{}
    $currentTitle = $null
    $metaRe = [regex]'^\s*([0-9A-Fa-f]{8})\s*,\s*([0-9A-Fa-f]{8})\s*,\s*([0-9A-Fa-f]{1,3})\s*,\s*([0-9A-Fa-f]{1,2})\s*$'
    $lineNo = 0

    foreach ($rawLine in [System.IO.File]::ReadLines($Path)) {
        $lineNo++
        $line = $rawLine.Trim()
        if (-not $line) {
            continue
        }

        $m = $metaRe.Match($line)
        if ($m.Success -and $currentTitle) {
            $romCrc32 = [Convert]::ToUInt32($m.Groups[1].Value, 16)
            $crc1 = [Convert]::ToUInt32($m.Groups[2].Value, 16)
            $sizeMib = Convert-N64TxtSmallInt -Text $m.Groups[3].Value
            $saveCode = Convert-N64TxtSmallInt -Text $m.Groups[4].Value
            $title = [regex]::Replace($currentTitle, '\.(z64|n64|v64)$', '', 'IgnoreCase').Trim()
            $saveHint = Convert-N64TxtSaveCodeToHint -SaveCode $saveCode
            $key = "{0:X8}" -f $crc1

            $entry = [pscustomobject]@{
                Title = $title
                Crc1 = $crc1
                Crc1Hex = $key
                RomCrc32 = $romCrc32
                RomSize = $sizeMib * 1024 * 1024
                RomSizeMiB = $sizeMib
                SaveCode = $saveCode
                SaveHint = $saveHint
                CatalogLine = $lineNo
                CatalogPath = $Path
            }

            if (-not $entriesByCrc1.ContainsKey($key)) {
                $entriesByCrc1[$key] = @()
            }
            $entriesByCrc1[$key] = @($entriesByCrc1[$key]) + $entry
            $currentTitle = $null
        } else {
            $currentTitle = $line
        }
    }

    [pscustomobject]@{
        Path = $Path
        EntriesByCrc1 = $entriesByCrc1
        Count = ($entriesByCrc1.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
    }
}

function Find-N64CatalogEntry {
    param(
        [Parameter(Mandatory)]$Catalog,
        [Parameter(Mandatory)][byte[]]$Header,
        [int]$RomSize = 0
    )

    $crc1 = Get-N64HeaderBe32 -Header $Header -Offset 0x10
    $key = "{0:X8}" -f $crc1
    if (-not $Catalog.EntriesByCrc1.ContainsKey($key)) {
        return $null
    }

    $candidates = @($Catalog.EntriesByCrc1[$key])
    if ($RomSize -gt 0) {
        $sizeMatches = @($candidates | Where-Object { $_.RomSize -eq $RomSize })
        if ($sizeMatches.Count -gt 0) {
            return $sizeMatches[0]
        }
    }

    return $candidates[0]
}
