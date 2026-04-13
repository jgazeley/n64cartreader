. "$PSScriptRoot\Protocol.ps1"
. "$PSScriptRoot\Crc.ps1"

try {
    Add-Type -AssemblyName System.IO.Ports -ErrorAction Stop
} catch {
    # Windows PowerShell normally has this loaded already. If neither path works,
    # SerialPort construction below will fail with the useful platform error.
}

$script:PpkNativeAvailable = $false
$script:PpkNativeLoadError = $null
try {
    if (-not ("PpkSerialNative" -as [type])) {
        Add-Type -ReferencedAssemblies @("System.IO.Ports", "System.ComponentModel.Primitives") -TypeDefinition @'
using System;
using System.IO;
using System.IO.Ports;

public static class PpkSerialNative
{
    private const byte SyncA = 0xAA;
    private const byte SyncB = 0x55;
    private const byte Ack = 0x06;
    private const byte Nak = 0x15;

    private static readonly byte[] AckBytes = new byte[] { Ack };
    private static readonly byte[] NakBytes = new byte[] { Nak };

    private static int ReadByteOrThrow(Stream stream)
    {
        int value = stream.ReadByte();
        if (value < 0)
        {
            throw new TimeoutException("Timeout waiting for serial byte");
        }
        return value;
    }

    private static void ReadExact(Stream stream, byte[] buffer, int offset, int count)
    {
        int read = 0;
        while (read < count)
        {
            int n = stream.Read(buffer, offset + read, count - read);
            if (n <= 0)
            {
                throw new TimeoutException("Timeout while reading serial payload");
            }
            read += n;
        }
    }

    public static ushort Crc16Update(ushort crc, byte[] data, int offset, int count)
    {
        for (int i = 0; i < count; i++)
        {
            crc = (ushort)(crc ^ (data[offset + i] << 8));
            for (int bit = 0; bit < 8; bit++)
            {
                if ((crc & 0x8000) != 0)
                {
                    crc = (ushort)(((crc << 1) ^ 0x8005) & 0xFFFF);
                }
                else
                {
                    crc = (ushort)((crc << 1) & 0xFFFF);
                }
            }
        }
        return crc;
    }

    public static ushort ReceiveReliableToStream(SerialPort serial, int totalSize, Stream output, int chunkSize)
    {
        Stream serialStream = serial.BaseStream;
        int expectedSeq = 0;
        int written = 0;
        ushort runningCrc = 0;

        byte[] header = new byte[3];
        byte[] crcBytes = new byte[2];
        byte[] tail = new byte[7];

        while (written < totalSize)
        {
            int b1 = ReadByteOrThrow(serialStream);

            if (b1 == 0x52) // 'R'
            {
                ReadExact(serialStream, tail, 0, tail.Length);
                if (tail[0] == 0x50 && tail[1] == 0x4B && tail[2] == 0x31) // "PK1"
                {
                    int cmd = tail[3];
                    int status = tail[4];
                    int value = (tail[5] << 8) | tail[6];
                    throw new InvalidOperationException(
                        string.Format("Unexpected response 0x{0:X2}/0x{1:X2} while waiting for data stream (value=0x{2:X4})", cmd, status, value)
                    );
                }
            }

            if (b1 != SyncA)
            {
                continue;
            }

            int b2 = ReadByteOrThrow(serialStream);
            if (b2 != SyncB)
            {
                continue;
            }

            ReadExact(serialStream, header, 0, header.Length);
            int seq = header[0];
            int payloadLen = (header[1] << 8) | header[2];
            if (payloadLen < 0 || payloadLen > 65535)
            {
                serialStream.Write(NakBytes, 0, 1);
                continue;
            }

            byte[] payload = new byte[payloadLen];
            ReadExact(serialStream, payload, 0, payloadLen);
            ReadExact(serialStream, crcBytes, 0, crcBytes.Length);
            ushort rxCrc = (ushort)((crcBytes[0] << 8) | crcBytes[1]);
            ushort calcCrc = Crc16Update(0, payload, 0, payloadLen);

            if (rxCrc != calcCrc)
            {
                serialStream.Write(NakBytes, 0, 1);
                continue;
            }

            if (seq == expectedSeq)
            {
                int remain = totalSize - written;
                int blockLen = Math.Min(payloadLen, remain);
                if (blockLen > 0)
                {
                    output.Write(payload, 0, blockLen);
                    runningCrc = Crc16Update(runningCrc, payload, 0, blockLen);
                    written += blockLen;
                }

                serialStream.Write(AckBytes, 0, 1);
                expectedSeq = (expectedSeq + 1) & 0xFF;
            }
            else if (seq == ((expectedSeq - 1) & 0xFF))
            {
                serialStream.Write(AckBytes, 0, 1);
            }
            else
            {
                serialStream.Write(NakBytes, 0, 1);
            }
        }

        return runningCrc;
    }
}
'@
    }
    $script:PpkNativeAvailable = $true
} catch {
    $script:PpkNativeAvailable = $false
    $script:PpkNativeLoadError = $_.Exception.Message
}

function Open-PpkSession {
    param(
        [Parameter(Mandatory)][string]$Port,
        [int]$Baud = $script:DEFAULT_BAUD,
        [int]$TimeoutMs = $script:DEFAULT_TIMEOUT_MS,
        [int]$ChunkSize = $script:DEFAULT_CHUNK_SIZE,
        [int]$Retries = $script:DEFAULT_RETRIES,
        [switch]$Quiet
    )

    $serial = [System.IO.Ports.SerialPort]::new($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.ReadTimeout = $TimeoutMs
    $serial.WriteTimeout = $TimeoutMs
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true
    $serial.Open()

    Start-Sleep -Milliseconds 600
    try { $serial.DiscardInBuffer() } catch {}

    [pscustomobject]@{
        Serial = $serial
        ChunkSize = $ChunkSize
        Retries = $Retries
        Quiet = [bool]$Quiet
        TxInterChunkDelayMs = 0
    }
}

function Close-PpkSession {
    param([Parameter(Mandatory)]$Session)

    if ($Session.Serial -and $Session.Serial.IsOpen) {
        $Session.Serial.Close()
    }
}

function Read-PpkExact {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$Count
    )

    $buf = [byte[]]::new($Count)
    $offset = 0

    while ($offset -lt $Count) {
        $n = $Session.Serial.Read($buf, $offset, $Count - $offset)
        if ($n -le 0) {
            throw "Timeout while reading $Count bytes"
        }
        $offset += $n
    }

    return ,$buf
}

function Read-PpkByte {
    param([Parameter(Mandatory)]$Session)

    try {
        $value = $Session.Serial.ReadByte()
    } catch [System.TimeoutException] {
        throw "Timeout waiting for serial byte"
    }

    if ($value -lt 0) {
        throw "Timeout waiting for serial byte"
    }

    return ([byte]$value)
}

function Write-PpkBytes {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][byte[]]$Bytes
    )

    $Session.Serial.Write($Bytes, 0, $Bytes.Length)
}

function Write-PpkProgress {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][string]$Activity,
        [Parameter(Mandatory)][int]$Done,
        [Parameter(Mandatory)][int]$Total
    )

    if ($Session.Quiet -or $Total -le 0) {
        return
    }

    $percent = [int](($Done * 100) / $Total)
    Write-Progress -Activity $Activity -Status "$Done/$Total chunks" -PercentComplete $percent
}

function Complete-PpkProgress {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][string]$Activity
    )

    if (-not $Session.Quiet) {
        Write-Progress -Activity $Activity -Completed
    }
}

function Send-PpkCommand {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$Command,
        [int]$Arg0 = 0,
        [int]$Arg1 = 0
    )

    if ($Command -lt 0 -or $Command -gt 0xFF -or $Arg0 -lt 0 -or $Arg0 -gt 0xFF -or $Arg1 -lt 0 -or $Arg1 -gt 0xFFFF) {
        throw "Command arguments out of range"
    }

    $frame = [byte[]]::new(8)
    [Array]::Copy($script:CMD_MAGIC, 0, $frame, 0, 4)
    $frame[4] = [byte]$Command
    $frame[5] = [byte]$Arg0
    $frame[6] = [byte](($Arg1 -shr 8) -band 0xFF)
    $frame[7] = [byte]($Arg1 -band 0xFF)
    Write-PpkBytes -Session $Session -Bytes $frame
}

function Receive-PpkResponse {
    param(
        [Parameter(Mandatory)]$Session,
        [int]$ExpectCommand = -1
    )

    while ($true) {
        $first = Read-PpkByte -Session $Session
        if ($first -ne $script:RSP_MAGIC[0]) {
            continue
        }

        $tail = Read-PpkExact -Session $Session -Count 7
        $frame = [byte[]]::new(8)
        $frame[0] = $first
        [Array]::Copy($tail, 0, $frame, 1, 7)

        $matched = $true
        for ($i = 0; $i -lt 4; $i++) {
            if ($frame[$i] -ne $script:RSP_MAGIC[$i]) {
                $matched = $false
                break
            }
        }
        if (-not $matched) {
            continue
        }

        $cmd = [int]$frame[4]
        $status = [int]$frame[5]
        $value = (([int]$frame[6] -shl 8) -bor [int]$frame[7]) -band 0xFFFF

        if ($ExpectCommand -ge 0 -and $cmd -ne $ExpectCommand) {
            throw ("Response command mismatch: got 0x{0:X2}, expected 0x{1:X2}" -f $cmd, $ExpectCommand)
        }

        return [pscustomobject]@{
            Command = $cmd
            Status = $status
            Value = $value
        }
    }
}

function Invoke-PpkCommand {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$Command,
        [int]$Arg0 = 0,
        [int]$Arg1 = 0,
        [int]$ExpectCommand = -1
    )

    Send-PpkCommand -Session $Session -Command $Command -Arg0 $Arg0 -Arg1 $Arg1
    if ($ExpectCommand -lt 0) {
        $ExpectCommand = $Command
    }
    $rsp = Receive-PpkResponse -Session $Session -ExpectCommand $ExpectCommand
    Assert-PpkStatusOk -Command $rsp.Command -Status $rsp.Status -Value $rsp.Value
    return $rsp
}

function Send-PpkReliable {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][byte[]]$Data
    )

    $seq = 0
    $totalChunks = [Math]::Ceiling($Data.Length / [double]$Session.ChunkSize)

    for ($idx = 0; $idx -lt $totalChunks; $idx++) {
        $start = $idx * $Session.ChunkSize
        $len = [Math]::Min($Session.ChunkSize, $Data.Length - $start)
        $chunk = [byte[]]::new($len)
        [Array]::Copy($Data, $start, $chunk, 0, $len)
        $crc = Get-Crc16 -Data $chunk

        $frame = [byte[]]::new(5 + $len + 2)
        $frame[0] = $script:SYNC_A
        $frame[1] = $script:SYNC_B
        $frame[2] = [byte]$seq
        $frame[3] = [byte](($len -shr 8) -band 0xFF)
        $frame[4] = [byte]($len -band 0xFF)
        [Array]::Copy($chunk, 0, $frame, 5, $len)
        $frame[5 + $len] = [byte](($crc -shr 8) -band 0xFF)
        $frame[5 + $len + 1] = [byte]($crc -band 0xFF)

        $sent = $false
        for ($try = 0; $try -lt $Session.Retries; $try++) {
            Write-PpkBytes -Session $Session -Bytes $frame
            try {
                $resp = Read-PpkByte -Session $Session
            } catch {
                Start-Sleep -Milliseconds 40
                continue
            }

            if ($resp -eq $script:ACK) {
                $sent = $true
                break
            }

            if ($resp -eq $script:RSP_MAGIC[0]) {
                $tail = Read-PpkExact -Session $Session -Count 7
                $full = [byte[]]::new(8)
                $full[0] = $resp
                [Array]::Copy($tail, 0, $full, 1, 7)
                if ($full[0] -eq $script:RSP_MAGIC[0] -and $full[1] -eq $script:RSP_MAGIC[1] -and $full[2] -eq $script:RSP_MAGIC[2] -and $full[3] -eq $script:RSP_MAGIC[3]) {
                    $cmd = [int]$full[4]
                    $status = [int]$full[5]
                    $value = (([int]$full[6] -shl 8) -bor [int]$full[7]) -band 0xFFFF
                    Assert-PpkStatusOk -Command $cmd -Status $status -Value $value
                    throw ("Unexpected response 0x{0:X2}/0x{1:X2} while sending data stream" -f $cmd, $status)
                }
            }

            Start-Sleep -Milliseconds 40
        }

        if (-not $sent) {
            throw "Chunk $($idx + 1)/$totalChunks failed after retries"
        }

        $seq = ($seq + 1) -band 0xFF
        Write-PpkProgress -Session $Session -Activity "TX" -Done ($idx + 1) -Total $totalChunks

        if ($Session.TxInterChunkDelayMs -gt 0) {
            Start-Sleep -Milliseconds $Session.TxInterChunkDelayMs
        }
    }

    Complete-PpkProgress -Session $Session -Activity "TX"
}

function Receive-PpkReliable {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$TotalSize
    )

    $stream = [System.IO.MemoryStream]::new()
    try {
        $null = Receive-PpkReliableToStream -Session $Session -TotalSize $TotalSize -Stream $stream
        return ,$stream.ToArray()
    } finally {
        $stream.Dispose()
    }
}

function Receive-PpkReliableToFile {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$TotalSize,
        [Parameter(Mandatory)][string]$OutPath
    )

    $file = [System.IO.File]::Open($OutPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        return Receive-PpkReliableToStream -Session $Session -TotalSize $TotalSize -Stream $file
    } finally {
        $file.Dispose()
    }
}

function Receive-PpkReliableToStream {
    param(
        [Parameter(Mandatory)]$Session,
        [Parameter(Mandatory)][int]$TotalSize,
        [Parameter(Mandatory)][System.IO.Stream]$Stream
    )

    if ($script:PpkNativeAvailable) {
        return [PpkSerialNative]::ReceiveReliableToStream($Session.Serial, $TotalSize, $Stream, $Session.ChunkSize)
    }

    $expectedSeq = 0
    $written = 0
    $runningCrc = 0
    $totalChunks = [Math]::Ceiling($TotalSize / [double]$Session.ChunkSize)
    $chunks = 0

    while ($written -lt $TotalSize) {
        $b1 = Read-PpkByte -Session $Session

        if ($b1 -eq $script:RSP_MAGIC[0]) {
            $tail = Read-PpkExact -Session $Session -Count 7
            $frame = [byte[]]::new(8)
            $frame[0] = $b1
            [Array]::Copy($tail, 0, $frame, 1, 7)
            if ($frame[0] -eq $script:RSP_MAGIC[0] -and $frame[1] -eq $script:RSP_MAGIC[1] -and $frame[2] -eq $script:RSP_MAGIC[2] -and $frame[3] -eq $script:RSP_MAGIC[3]) {
                $cmd = [int]$frame[4]
                $status = [int]$frame[5]
                $value = (([int]$frame[6] -shl 8) -bor [int]$frame[7]) -band 0xFFFF
                Assert-PpkStatusOk -Command $cmd -Status $status -Value $value
                throw ("Unexpected response 0x{0:X2}/0x{1:X2} while waiting for data stream" -f $cmd, $status)
            }
        }

        if ($b1 -ne $script:SYNC_A) {
            continue
        }

        $b2 = Read-PpkByte -Session $Session
        if ($b2 -ne $script:SYNC_B) {
            continue
        }

        $header = Read-PpkExact -Session $Session -Count 3
        $seq = [int]$header[0]
        $payloadLen = (([int]$header[1] -shl 8) -bor [int]$header[2]) -band 0xFFFF
        $payload = Read-PpkExact -Session $Session -Count $payloadLen
        $crcBytes = Read-PpkExact -Session $Session -Count 2
        $rxCrc = (([int]$crcBytes[0] -shl 8) -bor [int]$crcBytes[1]) -band 0xFFFF

        if ($rxCrc -ne (Get-Crc16 -Data $payload)) {
            Write-PpkBytes -Session $Session -Bytes ([byte[]]($script:NAK))
            continue
        }

        if ($seq -eq $expectedSeq) {
            $remain = $TotalSize - $written
            $blockLen = [Math]::Min($payloadLen, $remain)
            if ($blockLen -gt 0) {
                $Stream.Write($payload, 0, $blockLen)
                $runningCrc = Update-Crc16 -Crc $runningCrc -Data $payload -Offset 0 -Count $blockLen
                $written += $blockLen
            }

            Write-PpkBytes -Session $Session -Bytes ([byte[]]($script:ACK))
            $expectedSeq = ($expectedSeq + 1) -band 0xFF
            $chunks++
            Write-PpkProgress -Session $Session -Activity "RX" -Done $chunks -Total $totalChunks
        } elseif ($seq -eq (($expectedSeq - 1) -band 0xFF)) {
            Write-PpkBytes -Session $Session -Bytes ([byte[]]($script:ACK))
        } else {
            Write-PpkBytes -Session $Session -Bytes ([byte[]]($script:NAK))
        }
    }

    Complete-PpkProgress -Session $Session -Activity "RX"
    return ($runningCrc -band 0xFFFF)
}
