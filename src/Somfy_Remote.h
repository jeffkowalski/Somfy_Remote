/*
This library is based on the Arduino sketch by Nickduino: https://github.com/Nickduino/Somfy_Remote
*/

#ifndef SOMFY_REMOTE
#define SOMFY_REMOTE

#include <Arduino.h>

class SomfyRemote {
  private:
    String   _name;
    uint32_t _remoteCode;
    uint32_t _rollingCode;
    uint16_t _eepromAddress;

    void buildFrame (uint8_t * frame, uint8_t command);
    void sendCommand (uint8_t const * frame, uint8_t sync) const;

  public:
    SomfyRemote (String name, uint32_t remoteCode);  // Constructor requires name and remote code
    String getName () const { return _name; }
    void   move (String command);  // Method to send a command (Possible inputs: UP, DOWN, MY, PROGRAM)
};

#endif  // SOMFY_REMOTE
