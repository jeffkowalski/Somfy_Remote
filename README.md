# Somfy Remote

This is a library to emulate one or multiple Somfy remote controls via an inexpensive FS1000A transmitter.

If you want to learn more about the Somfy RTS protocol, check out [Pushtack](https://pushstack.wordpress.com/somfy-rts-protocol/).

All credit goes to [Nickduino and his Arduino sketch](https://github.com/Nickduino/Somfy_Remote) that built the foundation of my library.

## Hardware

You need:

- FS1000A transmitter module, widely available
- 433.42MHz resonator
- Arduino, ESP8266 or ESP32

Replace the original resonator on the transmitter with the new one at the correct frequency.

Attach a wire antenna to the transmitter.

Supply power and ground to the module, and connect the data pin to D2 on the controller.

Compile and upload this firmware.

## How To

For each blind you want to control individually:

- Choose a remote name (choose any name you like as it only serves as your personal identifier)
- Choose a remote code (make sure that you use each code only once across all remotes as it serves as identifier for the motors)
- Upload the sketch
- Long-press the program button of <b>your actual remote</b> until your blind goes up and down slightly
- Send 'PROGRAM' to the <b>simulated remote</b>

To control multiple blinds together:

- Repeat the last two steps with another blind (one by one)
