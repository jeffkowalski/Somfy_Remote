/*
 * This library is based on the Arduino sketch by Nickduino: https://github.com/Nickduino/Somfy_Remote
 * Protocol documentation is at https://pushstack.wordpress.com/somfy-rts-protocol/
 */

#include "Somfy_Remote.h"
#include <EEPROM.h>
#include <assert.h>
//#define CC1101  // comment this out to run FS1000A instead
#ifdef CC1101
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#define FREQ_OFFSET (-0.055)  // correction to align to 433,42MHz
#endif

#define EEPROM_SIZE 64
#define FRAME_LENGTH 7   // size of frame in bytes
#define TRANSMIT_PIN D2  // ESP8266 pin to CC1101 GDO0


SomfyRemote::SomfyRemote (String name, uint32_t remoteCode)
    : _name (name)
    , _remoteCode (remoteCode) {
    static uint8_t nextEepromAddress = 0;

    EEPROM.begin (EEPROM_SIZE);

    _eepromAddress = nextEepromAddress;
    EEPROM.get (_eepromAddress, _rollingCode);
    nextEepromAddress += sizeof (_rollingCode);

#ifdef CC1101
    // Choose pins before initializing radio
    ELECHOUSE_cc1101.setGDO (TRANSMIT_PIN, D1); // gdo0 (TX), gdo2 (unused)

    // Initialize radio chip
    ELECHOUSE_cc1101.Init ();

    // Enable transmission at 433.42 MHz
    ELECHOUSE_cc1101.SetTx (433.42 + FREQ_OFFSET);
#endif

    pinMode (TRANSMIT_PIN, OUTPUT);
    digitalWrite (TRANSMIT_PIN, LOW);
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
    uint8_t key = (_rollingCode - 8) & 0x0F;

    frame[0] = 0xA0 | key;         // Encryption key
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
    checksum &= 0x0F;  // Keep the last 4 bits only
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
    digitalWrite (TRANSMIT_PIN, level1);
    delayMicroseconds (duration1);
    digitalWrite (TRANSMIT_PIN, level2);
    delayMicroseconds (duration2);
}


// Send frame according to Somfy RTS protocol
void SomfyRemote::sendCommand (uint8_t const * frame, uint8_t sync) const {
    assert (sync == 2 || sync == 7);  // 2 for first frame, or 7 subsequently

#define SYMBOL 640

    // Wake-up pulse & silence, only with the first frame
    if (sync == 2)
        pulse (HIGH, 9986, LOW, 97087);  // (empirically determined)

    // Pre-sync + Sync = 15.44ms

    // Hardware pre-sync: 2 sync for the first frame, 7 for the following ones
    for (int ii = 0; ii < sync; ++ii)
        pulse (HIGH, 2750, LOW, 2250);

    // Software sync
    pulse (HIGH, 4800, LOW, SYMBOL);  // sync pulse, delimiter

    // Data: bits are sent one by one, starting with the MSB; 71.68ms
    for (uint8_t const * fb = frame; fb < &frame[FRAME_LENGTH]; ++fb)  // walk frame bytes
        for (uint8_t bit = 0b10000000; bit > 0; bit >>= 1)             // walk the 8 bits
            // Somfy RTS bits are MSB first, manchester encoded: 0 = high->low 1 = low->high
            if (*fb & bit)
                pulse (LOW, SYMBOL, HIGH, SYMBOL);
            else
                pulse (HIGH, SYMBOL, LOW, SYMBOL);

    digitalWrite (TRANSMIT_PIN, LOW);
    delayMicroseconds (32286);  // Inter-frame silence (empirically determined)
}


// Send a command to the blinds
void SomfyRemote::move (String command) {
    // Build frame according to selected command
    const uint8_t my   = 0x1;
    const uint8_t up   = 0x2;
    const uint8_t down = 0x4;
    const uint8_t prog = 0x8;
    uint8_t       frame[FRAME_LENGTH];

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
