#pragma once

#include <cstddef>
#include <string>

namespace CameraSerial
{

bool GetNumSerialPorts(unsigned int& numPorts, std::string& error);

// Lists the serial ports exposed by the Euresys Camera Link driver.
bool GetPortIdentifier(unsigned int index, std::string& identifier,
std::string& error);

// Opens a port and configures its baud rate (for example, 57600).
bool Open(unsigned int portIndex, unsigned int baudRate,
          std::string& error);

// Sends a command followed by carriage return.
bool WriteCommand(const char* command, std::string& error);

// Reads currently available response bytes.
bool ReadAvailable(char* buffer, std::size_t bufferSize,
                   std::size_t& bytesRead, std::string& error);
                   
bool FlushPort();

// Closes the serial connection.
void Close();


}
