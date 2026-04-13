param(
    [string]$Port = ""
)

. "$PSScriptRoot\lib\N64.ps1"

if (-not $Port) {
    $Port = Read-Host "Serial port, e.g. COM5"
}

:menuLoop while ($true) {
    Write-Host ""
    Write-Host "N64 Cart Reader"
    Write-Host "1. Dump ROM"
    Write-Host "2. Export save"
    Write-Host "3. Import save"
    Write-Host "Q. Quit"
    $input = Read-Host "Select"
    if ($null -eq $input) { break }
    $choice = $input.Trim().ToUpperInvariant()

    switch ($choice) {
        "1" {
            $out = Read-Host "Output ROM path"
            & "$PSScriptRoot\Dump-Rom.ps1" -Port $Port -Out $out
        }
        "2" {
            $out = Read-Host "Output save path"
            $saveType = Read-Host "Optional save override (sram, sram96k, flashram, eeprom4k, eeprom16k) or blank for auto"
            if ($saveType) {
                & "$PSScriptRoot\Export-Save.ps1" -Port $Port -Out $out -SaveType $saveType
            } else {
                & "$PSScriptRoot\Export-Save.ps1" -Port $Port -Out $out
            }
        }
        "3" {
            $inPath = Read-Host "Input save path"
            $saveType = Read-Host "Optional save override (sram, sram96k, flashram, eeprom4k, eeprom16k) or blank for auto"
            $verifyText = (Read-Host "Verify after import? y/N").Trim().ToUpperInvariant()
            $verify = $verifyText -eq "Y" -or $verifyText -eq "YES"
            if ($saveType) {
                & "$PSScriptRoot\Import-Save.ps1" -Port $Port -In $inPath -SaveType $saveType -Verify:$verify
            } else {
                & "$PSScriptRoot\Import-Save.ps1" -Port $Port -In $inPath -Verify:$verify
            }
        }
        "Q" {
            break menuLoop
        }
        default {
            if ($choice) {
                Write-Host "Unknown selection: $choice"
            }
        }
    }
}
