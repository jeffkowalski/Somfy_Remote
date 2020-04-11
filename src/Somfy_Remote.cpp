/*
 * This library is based on the Arduino sketch by Nickduino: https://github.com/Nickduino/Somfy_Remote
 * Protocol documentation is at https://pushstack.wordpress.com/somfy-rts-protocol/
 */

#include "Somfy_Remote.h"
#include <EEPROM.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

static uint8_t currentEepromAddress = 0;

// Set pins according to module
#if defined __AVR_ATmega168__ || defined __AVR_ATmega328P__
static uint8_t const gdo0Pin = 2;
static uint8_t const gdo2Pin = 3;
#elif ESP32
static uint8_t const gdo0Pin = 2;
static uint8_t const gdo2Pin = 4;
#elif ESP8266
static uint8_t const gdo0Pin = D2;  // rx yellow (unused)
static uint8_t const gdo2Pin = D1;  // tx grey
#endif

// Constructor
SomfyRemote::SomfyRemote(String name, uint32_t remoteCode) {
    _name          = name;
    _remoteCode    = remoteCode;
    _rollingCode   = getRollingCode();
    _eepromAddress = getNextEepromAddress();

    ELECHOUSE_cc1101.setGDO(gdo0Pin, gdo2Pin);
    ELECHOUSE_cc1101.setSpiPin(D5, D6, D7, D8);  // SCK, MISO, MOSI, CSN
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.SetTx(433.3913);
}

// Getter for name
String SomfyRemote::getName() {
    return _name;
}

// Generates the next available EEPROM address
uint16_t SomfyRemote::getNextEepromAddress() {
    // Every address gets 4 bytes of space to save the rolling code
    currentEepromAddress = currentEepromAddress + 4;
    return currentEepromAddress;
}

// Generates the next available EEPROM address
uint32_t SomfyRemote::getRollingCode() {
    uint32_t rollingCode;

    // Set new rolling code if not already set
    if (EEPROM.get(_eepromAddress, rollingCode) < 1) {
        rollingCode = 1;
    }
    return rollingCode;
}

// Send a command to the blinds
void SomfyRemote::move(String command) {
    const uint8_t up   = 0x2;
    const uint8_t down = 0x4;
    const uint8_t my   = 0x1;
    const uint8_t prog = 0x8;

    uint8_t frame[7];

    // Build frame according to selected command
    command.toUpperCase();

    switch (command[0]) {
        case 'U':
            buildFrame(frame, up);
            break;
        case 'D':
            buildFrame(frame, down);
            break;
        case 'P':
            buildFrame(frame, prog);
            break;
        case 'M':
        default:
            buildFrame(frame, my);
            break;
    }

    // Send the frame according to Somfy RTS protocol
    sendCommand(frame, 2);
    for (int i = 0; i < 2; ++i) {
        sendCommand(frame, 7);
    }
}

// Build frame according to Somfy RTS protocol
void SomfyRemote::buildFrame(uint8_t * frame, uint8_t command) {
    frame[0] = 0xA7;               // Encryption key.
    frame[1] = command << 4;       // Selected command. The 4 LSB are the checksum
    frame[2] = _rollingCode >> 8;  // Rolling code (big endian)
    frame[3] = _rollingCode;       // Rolling code
    frame[4] = _remoteCode >> 16;  // Remote address
    frame[5] = _remoteCode >> 8;   // Remote address
    frame[6] = _remoteCode;        // Remote address

    // Checksum calculation (XOR of all nibbles)
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < 7; ++i) {
        checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
    }
    checksum &= 0b1111;  // Keep the last 4 bits only

    // Checksum integration
    frame[1] |= checksum;  // If XOR of all the nibbles is 0, the blinds will consider the checksum ok

    // Obfuscation (XOR of all bytes)
    for (uint8_t i = 1; i < 7; ++i) {
        frame[i] ^= frame[i - 1];
    }

    _rollingCode = _rollingCode + 1;

    EEPROM.put(_eepromAddress, _rollingCode);
}

// Send frame according to Somfy RTS protocol
void SomfyRemote::sendCommand(uint8_t * frame, uint8_t sync) {
    uint16_t const symbol = 604;

    if (sync == 2) {  // Only with the first frame.
        // Wake-up pulse & Silence
        digitalWrite(gdo2Pin, HIGH);
        delayMicroseconds(9415);
        digitalWrite(gdo2Pin, LOW);
        delayMicroseconds(89565);
    }

    // Hardware sync: two sync for the first frame, seven for the following ones.
    for (int i = 0; i < sync; ++i) {
        digitalWrite(gdo2Pin, HIGH);
        delayMicroseconds(4 * symbol);
        digitalWrite(gdo2Pin, LOW);
        delayMicroseconds(4 * symbol);
    }

    // Software sync
    digitalWrite(gdo2Pin, HIGH);
    delayMicroseconds(4550);
    digitalWrite(gdo2Pin, LOW);
    delayMicroseconds(symbol);

    // Data: bits are sent one by one, starting with the MSB.
    for (uint8_t i = 0; i < 56; ++i) {
        // Somfy RTS bits are manchester encoded: 0 = high->low 1 = low->high
        if (((frame[i / 8] >> (7 - (i % 8))) & 1) == 1) {
            digitalWrite(gdo2Pin, LOW);
            delayMicroseconds(symbol);
            digitalWrite(gdo2Pin, HIGH);
            delayMicroseconds(symbol);
        } else {
            digitalWrite(gdo2Pin, HIGH);
            delayMicroseconds(symbol);
            digitalWrite(gdo2Pin, LOW);
            delayMicroseconds(symbol);
        }
    }
    digitalWrite(gdo2Pin, LOW);
    delayMicroseconds(30415);  // Inter-frame silence
}
