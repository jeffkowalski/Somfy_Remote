/*
 * This library is based on the Arduino sketch by Nickduino: https://github.com/Nickduino/Somfy_Remote
 * Protocol documentation is at https://pushstack.wordpress.com/somfy-rts-protocol/
 */

#include "Somfy_Remote.h"
#include <EEPROM.h>
#include <assert.h>

#define FRAME_LENGTH 7
static int const transmitPin = D2;


// Constructor
SomfyRemote::SomfyRemote (String name, uint32_t remoteCode)
    : _name (name)
    , _remoteCode (remoteCode) {
    static uint8_t nextEepromAddress = 0;

    _eepromAddress = nextEepromAddress;
    nextEepromAddress += sizeof (_rollingCode);
    if (EEPROM.get (_eepromAddress, _rollingCode) < 1)
        _rollingCode = 1;

    pinMode (transmitPin, OUTPUT);
    digitalWrite (transmitPin, LOW);
}


static void printFrame (char const * message, uint8_t const * frame) {
    Serial.println (message);
    for (int ii = 0; ii < FRAME_LENGTH; ++ii) {
        if (frame[ii] >> 4 == 0)
            Serial.print ("0");
        Serial.print (frame[ii], HEX);
        Serial.print (" ");
    }
    Serial.println ("");
}


// Build frame according to Somfy RTS protocol
void SomfyRemote::buildFrame (uint8_t * frame, uint8_t command) {
    frame[0] = 0xA7;               // Encryption key
    frame[1] = command << 4;       // Selected command in 4 MSB; the 4 LSB are the checksum
    frame[2] = _rollingCode >> 8;  // Rolling code (big endian)
    frame[3] = _rollingCode;       // Rolling code
    frame[4] = _remoteCode >> 16;  // Remote address
    frame[5] = _remoteCode >> 8;   // Remote address
    frame[6] = _remoteCode;        // Remote address
    printFrame ("Frame:         ", frame);

    // Checksum calculation (XOR of all nibbles)
    uint8_t checksum = 0;
    for (int ii = 0; ii < FRAME_LENGTH; ++ii)
        checksum = checksum ^ frame[ii] ^ (frame[ii] >> 4);
    checksum &= 0b00001111;  // Keep the last 4 bits only
    // Checksum integration
    frame[1] |= checksum;  // If XOR of all the nibbles is 0, the blinds will consider the checksum ok
    printFrame ("  checksummed: ", frame);

    // Obfuscation (XOR of all bytes)
    for (int ii = 1; ii < FRAME_LENGTH; ++ii)
        frame[ii] ^= frame[ii - 1];
    printFrame ("  obfuscated:  ", frame);

    ++_rollingCode;
    EEPROM.put (_eepromAddress, _rollingCode);
    EEPROM.commit ();
}


inline void pulse (int level1, int duration1, int level2, int duration2) {
    digitalWrite (transmitPin, level1);
    delayMicroseconds (duration1);
    digitalWrite (transmitPin, level2);
    delayMicroseconds (duration2);
}


// Send frame according to Somfy RTS protocol
void SomfyRemote::sendCommand (uint8_t const * frame, uint8_t sync) const {
    assert (sync == 2 || sync == 7);  // 2 for first frame, or 7 subsequently

#define SYMBOL 604

    // Only with the first frame: wake-up pulse & silence
    if (sync == 2)
        pulse (HIGH, 9415, LOW, 89565);

    // Hardware sync: 2 sync for the first frame, 7 for the following ones
    for (int ii = 0; ii < sync; ++ii)
        pulse (HIGH, 4 * SYMBOL, LOW, 4 * SYMBOL);

    // Software sync
    pulse (HIGH, 4550, LOW, SYMBOL);

#define BITS 8
    // Data: bits are sent one by one, starting with the MSB
    for (int ii = 0; ii < FRAME_LENGTH * BITS; ++ii)
        // Somfy RTS bits are manchester encoded: 0 = high->low 1 = low->high
        if (((frame[ii / BITS] >> ((BITS - 1) - (ii % BITS))) & 1) == 1)
            pulse (LOW, SYMBOL, HIGH, SYMBOL);
        else
            pulse (HIGH, SYMBOL, LOW, SYMBOL);

    digitalWrite (transmitPin, LOW);
    delayMicroseconds (30415);  // Inter-frame silence
}


// Send a command to the blinds
void SomfyRemote::move (String command) {
    const uint8_t my   = 0x1;
    const uint8_t up   = 0x2;
    const uint8_t down = 0x4;
    const uint8_t prog = 0x8;

    // Build frame according to selected command
    uint8_t frame[FRAME_LENGTH];

    command.toUpperCase ();
    switch (command[0]) {
        case 'U':
            buildFrame (frame, up);
            break;
        case 'D':
            buildFrame (frame, down);
            break;
        case 'P':
            buildFrame (frame, prog);
            break;
        case 'M':
        default:
            buildFrame (frame, my);
            break;
    }

    // Send the frame according to Somfy RTS protocol
    sendCommand (frame, 2);
    for (int ii = 0; ii < 4; ++ii)
        sendCommand (frame, 7);
}
