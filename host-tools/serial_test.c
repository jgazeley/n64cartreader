/*
 * File: recv_serial.c
 *
 * Build (MSVC): cl /Od /Zi /EHsc recv_serial.c
 * Run, then use HxD: Extras > Open process > select this exe > go to printed address.
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define TOTAL_BYTES (8 * 1024 * 1024)  // 1 MiB
static uint8_t buffer[TOTAL_BYTES];

int main(int argc, char **argv) {
    // Pick up the COM port from argv[1], or default to COM3
    const char *userPort = (argc > 1 ? argv[1] : "COM3");
    // Windows needs \\.\ prefix for COM10+
    static char portName[32];
    if (strncmp(userPort, "\\\\.\\", 4) == 0) {
        snprintf(portName, sizeof portName, "%s", userPort);
    } else {
        snprintf(portName, sizeof portName, "\\\\.\\%s", userPort);
    }

    HANDLE hComm = CreateFileA(
        portName,
        GENERIC_READ | GENERIC_WRITE,
        0,              // exclusive access
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (hComm == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open %s (err %lu)\n", portName, GetLastError());
        return 1;
    }

    // SetupComm(hComm, 64*1024, 8192);

    // Configure serial: 115200, 8N1
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hComm, &dcb)) {
        fprintf(stderr, "GetCommState failed (err %lu)\n", GetLastError());
        return 1;
    }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;
    if (!SetCommState(hComm, &dcb)) {
        fprintf(stderr, "SetCommState failed (err %lu)\n", GetLastError());
        return 1;
    }

    // Set timeouts
    COMMTIMEOUTS to = {0};
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutConstant = 5000;
    to.ReadTotalTimeoutMultiplier = 10;
    to.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(hComm, &to);

    // Flush buffers
    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);

    // Send handshake
    printf("→ Sending handshake 'G'...\n");
    DWORD written;
    char cmd = 'G';
    if (!WriteFile(hComm, &cmd, 1, &written, NULL) || written != 1) {
        fprintf(stderr, "WriteFile failed (err %lu)\n", GetLastError());
        return 1;
    }

    // Read data
    printf("← Reading %d bytes...\n", TOTAL_BYTES);
    DWORD totalRead = 0;
    DWORD chunk;
    while (totalRead < TOTAL_BYTES) {
        if (!ReadFile(hComm, buffer + totalRead, TOTAL_BYTES - totalRead, &chunk, NULL)) {
            fprintf(stderr, "ReadFile failed (err %lu) at %lu bytes\n", GetLastError(), totalRead);
            return 1;
        }
        totalRead += chunk;
    }

    // Report buffer address
    printf("✔ Received %lu bytes. Buffer at address: %p\n", totalRead, (void*)buffer);
    printf("Press Enter to exit, then attach HxD to inspect memory.\n");
    getchar();

    CloseHandle(hComm);
    return 0;
}
