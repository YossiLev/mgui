#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>

#include "CameraSerial.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#pragma comment(lib, "Advapi32.lib")

namespace
{
using U32 = std::uint32_t;
using I32 = std::int32_t;
using HSerRef = void*;


constexpr I32 CL_OK = 0;
constexpr I32 CL_BUFFER_TOO_SMALL = -10001;
constexpr U32 CL_BAUDRATE_57600 = 8;

struct SerialApi
{
    HMODULE dll = nullptr;
    HSerRef serialRef = nullptr;

    void(__cdecl* serialClose)(HSerRef) = nullptr;
    I32(__cdecl* serialInit)(U32, HSerRef*) = nullptr;
    I32(__cdecl* serialRead)(HSerRef, char*, U32*, U32) = nullptr;
    I32(__cdecl* serialWrite)(HSerRef, char*, U32*, U32) = nullptr;
    // I32(__cdecl* getPortIdentifier)(U32, char*, U32*) = nullptr;
    I32(__cdecl* getSupportedBaudRates)(HSerRef, U32*) = nullptr;
    I32(__cdecl* setBaudRate)(HSerRef, U32) = nullptr;

    I32(__cdecl* getNumBytesAvail)(HSerRef, U32*) = nullptr;
    I32(__cdecl* flushPort)(HSerRef) = nullptr;
    I32(__cdecl* getNumSerialPorts)(U32*) = nullptr;

    INT32 (__cdecl* getSerialPortIdentifier)(UINT32 serialIndex, char* portIdentifier, UINT32* bufferSize) = nullptr;

    ~SerialApi() {
        if (serialRef && serialClose) serialClose(serialRef);
        serialRef = nullptr;
        if (dll) FreeLibrary(dll);
        dll = nullptr;
    }
};

SerialApi api;


template <typename T>
bool LoadFunction(HMODULE dll, T& function, const char* symbol, std::string& error)
{
    function = reinterpret_cast<T>(GetProcAddress(dll, symbol));

    if (!function)
    {
        error = std::string("Could not load function: ") + symbol;
        printf(error.c_str());
        fflush(stdout);
        return false;
    }

    return true;
}

// bool LoadFunction(void** target, const char* name, std::string& error)
// {
//     *target = reinterpret_cast<void*>(GetProcAddress(api.dll, name));

//     if (!*target) {
//         error = std::string("Missing DLL function: ") + name;
//         return false;
//     }

//     return true;
// }

bool LoadApi(std::string& error)
{
    if (api.dll)
        return true;

    // // Prefer the installation's registered Camera Link serial path.
    // char path[MAX_PATH] = {};
    // DWORD type = 0;
    // DWORD size = sizeof(path);

    // LONG rc = RegGetValueA(
    //     HKEY_LOCAL_MACHINE,
    //     "SOFTWARE\\cameralink",
    //     "CLSERIALPATH",
    //     RRF_RT_REG_SZ,
    //     &type,
    //     path,
    //     &size);

    // if (rc != ERROR_SUCCESS)
    // {
    //     error = "Cannot read CLSERIALPATH from the registry.";
    //     return false;
    // }

    // std::string dllPath(path);
    // if (!dllPath.empty() && dllPath.back() != '\\')
    //     dllPath += '\\';
    // dllPath += "clseremc.dll";

    // dllPath = "C:\\Users\\levinsa2\\Documents\\projects\\CamGrab\\clseremc.dll";
    std::string dllPath("C:\\Program Files (x86)\\Euresys\\MultiCam\\Drivers\\x86_64\\clseremc.dll");

    printf("================================ %s\n", dllPath.c_str());

    api.dll = LoadLibraryA(dllPath.c_str());

    if (!api.dll)
    {
        error = "Cannot load " + dllPath + ". Check the Euresys installation and DLL architecture.";
        return false;
    }


    if (!LoadFunction(api.dll, api.serialClose, "clSerialClose", error)) return false;
    if (!LoadFunction(api.dll, api.serialInit, "clSerialInit", error)) return false;
    if (!LoadFunction(api.dll, api.serialRead, "clSerialRead", error)) return false;
    if (!LoadFunction(api.dll, api.serialWrite, "clSerialWrite", error)) return false;
    if (!LoadFunction(api.dll, api.getSerialPortIdentifier, "clGetSerialPortIdentifier", error)) return false;
    if (!LoadFunction(api.dll, api.getSupportedBaudRates, "clGetSupportedBaudRates", error)) return false;
    if (!LoadFunction(api.dll, api.setBaudRate, "clSetBaudRate", error)) return false;
    if (!LoadFunction(api.dll, api.getNumBytesAvail, "clGetNumBytesAvail", error)) return false;
    if (!LoadFunction(api.dll, api.flushPort, "clFlushPort", error)) return false;
    if (!LoadFunction(api.dll, api.getNumSerialPorts, "clGetNumSerialPorts", error)) return false;

    // This assumes your `SerialApi` struct has members with those exact names and that `api.dll` is an `HMODULE`. If your member names differ, use the names from your struct.

    //**Important:** this fixes the syntax issue, but it does not yet confirm that the DLL loads or that the serial connection works. If you get another compiler error, send me your current `SerialApi` struct and `LoadApi()` function, and I'll adapt the code to match them exactly.

    #define CHECK_API(fn) printf("%-28s %p\n", #fn, reinterpret_cast<void*>(api.fn))

    CHECK_API(serialInit);
    CHECK_API(serialRead);
    CHECK_API(serialWrite);
    CHECK_API(serialClose);
    CHECK_API(getNumSerialPorts);
    CHECK_API(getSerialPortIdentifier);
    CHECK_API(getNumBytesAvail);
    CHECK_API(flushPort);
    CHECK_API(getSupportedBaudRates);
    CHECK_API(setBaudRate);

#undef CHECK_API

    return true;
}

bool CheckStatus(I32 status, const char* operation, std::string& error)
{
    printf("chack status %s %d\n", operation, status); fflush(stdout);
    if (status == CL_OK)
        return true;

    error = std::string(operation) + " failed with Euresys serial status " + std::to_string(status);
    return false;
}


}

bool CameraSerial::GetNumSerialPorts(unsigned int& numPorts, std::string& error)
{
    if (!LoadApi(error))
        return false;

    U32 count = 0;
    I32 status = api.getNumSerialPorts(&count);

    if (status != 0)
    {
        error = "GetNumSerialPorts failed with status "
              + std::to_string(status);
        return false;
    }

    numPorts = count;
    return true;
}

bool CameraSerial::GetPortIdentifier(unsigned int index, std::string& identifier, std::string& error)
{
    if (!LoadApi(error))
        return false;

    char buffer[256] = {};
    UINT32 size = sizeof(buffer);

    printf("Calling getPortIdentifier: index=%u, size=%u\n", index, size);

    printf("api.getSerialPortIdentifier %p\n", api.getSerialPortIdentifier);

    INT32 status = api.getSerialPortIdentifier(static_cast<UINT32>(index),  buffer, &size);

    printf("Returned: status=%d, size=%u\n", status, size);

    if (status != 0) {
        error = "GetSerialPortIdentifier failed with status " + std::to_string(status);
        return false;
    }

    identifier.assign(buffer);

    return true;
}

bool CameraSerial::Open(unsigned int portIndex, unsigned int baudRate, std::string& error)
{
    printf("====== open\n");
    if (!LoadApi(error))
        return false;


    if (api.serialRef) {
        error = "A Camera Link serial port is already open.";
        return false;
    }
    printf("====== api.serialRef\n");

    U32 portCount = 0;
    if (!CheckStatus(api.getNumSerialPorts(&portCount), "GetNumSerialPorts", error))
        return false;

    if (portIndex >= portCount) {
        error = "Port index is outside the enumerated port range.";
        return false;
    }

    printf("====== ok %d / %d\n", portIndex, portCount);
    if (!CheckStatus(api.serialInit(portIndex, &api.serialRef), "SerialInit", error))
        return false;

    U32 supportedRates = 0;
    if (!CheckStatus(api.getSupportedBaudRates(api.serialRef, &supportedRates), "GetSupportedBaudRates", error))
    {
        Close();
        return false;
    }
    printf("supported rates = %d\n", supportedRates);

    U32 rateFlag = 0;
    switch (baudRate) {
        case 9600:   rateFlag = 1; break;
        case 19200:  rateFlag = 2; break;
        case 38400:  rateFlag = 4; break;
        case 57600:  rateFlag = 8; break;
        case 115200: rateFlag = 16; break;
        case 230400: rateFlag = 32; break;
        case 460800: rateFlag = 64; break;
        case 921600: rateFlag = 128; break;
        default: error = "Unsupported baud rate value."; Close(); return false;
    }

    if ((supportedRates & rateFlag) == 0) {
        error = "The selected port does not support this baud rate.";
        Close();
        return false;
    }

    printf("set rate = %d\n", rateFlag);
    printf("set rate = %d\n", rateFlag);

    if (!CheckStatus(api.setBaudRate(api.serialRef, rateFlag), "SetBaudRate", error)) {
        printf("set rate = %d was ERROR\n", rateFlag);  fflush(stdout);

        Close();
        return false;
    }
    printf("set rate = %d was OK\n", rateFlag); fflush(stdout);

    return true;
}

bool CameraSerial::WriteCommand(const char* command, std::string& error)
{
    if (!api.serialRef) {
        error = "No Camera Link serial port is open.";
        return false;
    }

    if (!command) {
        error = "Command pointer is null.";
        return false;
    }

    std::vector<char> bytes(command, command + std::strlen(command));
    if (bytes.empty() || bytes.back() != '\r') {
        bytes.push_back('\r');
    }

    if (bytes.size() > (std::numeric_limits<U32>::max)()) {
        error = "Command is too long.";
        return false;
    }

    U32 count = static_cast<U32>(bytes.size());
    printf("Sending %u bytes: ", count);
    for (U32 i = 0; i < count; ++i) {
        printf("%02X ", static_cast<unsigned char>(bytes[i]));
    }
    printf("\n");

    return CheckStatus(api.serialWrite(api.serialRef, bytes.data(), &count, 5000), "SerialWrite", error);
}

bool CameraSerial::ReadAvailable(char* buffer, std::size_t bufferSize, std::size_t& bytesRead, std::string& error) {
    bytesRead = 0;

    if (!api.serialRef) {
        error = "No Camera Link serial port is open.";
        return false;
    }

    if (!buffer || bufferSize == 0) {
        error = "Read buffer is empty.";
        return false;
    }

    U32 available = 0;
    if (!CheckStatus(api.getNumBytesAvail(api.serialRef, &available), "GetNumBytesAvail", error))
        return false;

    if (available == 0)
        return true;

    printf("available to read %d\n", available);

    U32 count = static_cast<U32>(
        (std::min)(bufferSize, static_cast<std::size_t>(available)));

    if (!CheckStatus(api.serialRead(api.serialRef, buffer, &count, 5000), "SerialRead", error))
        return false;

    bytesRead = count;

    return true;
}

bool CameraSerial::FlushPort() {

    int rc = api.flushPort(api.serialRef);

    if (rc != 0) {
        printf("FlushPort PROB %d\n", rc);
        return false;
    }
    printf("FlushPort OK\n");

    return true;
}

void CameraSerial::Close()
{
    if (api.serialRef && api.serialClose)  {
        api.serialClose(api.serialRef);
        api.serialRef = nullptr;
    }
}


