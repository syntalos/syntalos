/*
 * Firmata is a generic protocol for communicating with microcontrollers
 * from software on a host computer. It is intended to work with
 * any host computer software package.
 *
 * To download a host software package, please click on the following link
 * to open the list of Firmata client libraries in your default browser.
 *
 * https://github.com/firmata/arduino#firmata-client-libraries
 */

/* Experimental Firmata interface to the Mux Shield II.
 * Currently, only digital I/O is supported.
 */

#define _BOARD_MUXSHIELD2

#include "Firmata.h"
#include "MuxShield.h"

MuxShield muxShield;

// the minimum interval for sampling analog input
#define MINIMUM_SAMPLING_INTERVAL   1

/* digital input ports */
byte reportPORT[TOTAL_PORTS];        // 1 = report this port, 0 = silence
byte muxMode[3];                     // mode the multiplexers should be configured in

byte previousPORT[TOTAL_PORTS];      // previous byte sent per port
byte previousPORTValue[TOTAL_PORTS]; // previous bytes set for port

unsigned int samplingInterval = 19; // how often to run the main loop (in ms)
boolean isResetting = false;


inline unsigned char writeMXPin(byte pin, byte value)
{
  if (pin > 47)
    return 0;
  int mux = pin / 16;

  muxShield.digitalWriteMS(mux, pin - (16 * mux), value);
  return 1;
}

inline int readMXPinDigital(byte pin)
{
  if (pin > 47)
    return 0;
  int mux = pin / 16;

  return muxShield.digitalReadMS(mux, pin - (16 * mux));
}

inline int readMXPinAnalog(byte pin)
{
  if (pin > 47)
    return 0;
  int mux = pin / 16;

  return muxShield.analogReadMS(mux, pin - (16 * mux));
}

inline byte readMXPort(byte port, byte bitmask)
{
  byte out = 0;
  byte pin = port * 8;

  if (IS_PIN_DIGITAL(pin + 0) && (bitmask & 0x01) && readMXPinDigital(PIN_TO_DIGITAL(pin + 0))) out |= 0x01;
  if (IS_PIN_DIGITAL(pin + 1) && (bitmask & 0x02) && readMXPinDigital(PIN_TO_DIGITAL(pin + 1))) out |= 0x02;
  if (IS_PIN_DIGITAL(pin + 2) && (bitmask & 0x04) && readMXPinDigital(PIN_TO_DIGITAL(pin + 2))) out |= 0x04;
  if (IS_PIN_DIGITAL(pin + 3) && (bitmask & 0x08) && readMXPinDigital(PIN_TO_DIGITAL(pin + 3))) out |= 0x08;
  if (IS_PIN_DIGITAL(pin + 4) && (bitmask & 0x10) && readMXPinDigital(PIN_TO_DIGITAL(pin + 4))) out |= 0x10;
  if (IS_PIN_DIGITAL(pin + 5) && (bitmask & 0x20) && readMXPinDigital(PIN_TO_DIGITAL(pin + 5))) out |= 0x20;
  if (IS_PIN_DIGITAL(pin + 6) && (bitmask & 0x40) && readMXPinDigital(PIN_TO_DIGITAL(pin + 6))) out |= 0x40;
  if (IS_PIN_DIGITAL(pin + 7) && (bitmask & 0x80) && readMXPinDigital(PIN_TO_DIGITAL(pin + 7))) out |= 0x80;

  return out;
}

inline byte writeMXPort(byte port, byte value, byte bitmask)
{
  byte pin = port * 8;
  if ((bitmask & 0x01)) writeMXPin(PIN_TO_DIGITAL(pin + 0), (value & 0x01));
  if ((bitmask & 0x02)) writeMXPin(PIN_TO_DIGITAL(pin + 1), (value & 0x02));
  if ((bitmask & 0x04)) writeMXPin(PIN_TO_DIGITAL(pin + 2), (value & 0x04));
  if ((bitmask & 0x08)) writeMXPin(PIN_TO_DIGITAL(pin + 3), (value & 0x08));
  if ((bitmask & 0x10)) writeMXPin(PIN_TO_DIGITAL(pin + 4), (value & 0x10));
  if ((bitmask & 0x20)) writeMXPin(PIN_TO_DIGITAL(pin + 5), (value & 0x20));
  if ((bitmask & 0x40)) writeMXPin(PIN_TO_DIGITAL(pin + 6), (value & 0x40));
  if ((bitmask & 0x80)) writeMXPin(PIN_TO_DIGITAL(pin + 7), (value & 0x80));

  return 1;
}

void outputPort(byte portNumber, byte portValue, byte forceSend)
{
  // only send the data when it changes, otherwise you get too many messages!
  if (forceSend || previousPORT[portNumber] != portValue) {
    Firmata.sendDigitalPort(portNumber, portValue);
    previousPORT[portNumber] = portValue;
  }
}

void setPinModeCallback(byte pin, int mode) {
  if (pin >= TOTAL_PINS)
    return;

  if (IS_PIN_DIGITAL(pin)) {
    Firmata.setPinMode(pin, mode);

    byte port = pin / 8;
    byte mux = pin / 16;

    reportPORT[port] = (mode == INPUT || mode == PIN_MODE_PULLUP);
    if (mode == INPUT) {
      muxShield.setMode(mux, DIGITAL_IN);
      muxMode[mux] = DIGITAL_IN;
    } else if (mode == PIN_MODE_PULLUP) {
      muxShield.setMode(mux, DIGITAL_IN_PULLUP);
      muxMode[mux] = DIGITAL_IN_PULLUP;
    } else {
      muxShield.setMode(mux, DIGITAL_OUT);
      muxMode[mux] = DIGITAL_OUT;

      Firmata.setPinMode(pin, OUTPUT);
    }
  }
}

void digitalWriteCallback(byte port, int value)
{
  byte i;
  byte currentPinValue, previousPinValue;

  if (port < TOTAL_PORTS && value != previousPORTValue[port]) {
    for (i = 0; i < 8; i++) {
      currentPinValue = (byte) value & (1 << i);
      previousPinValue = previousPORTValue[port] & (1 << i);
      if (currentPinValue != previousPinValue) {
        writeMXPin(i + (port * 8), currentPinValue);
      }
    }
    previousPORTValue[port] = value;
  }
}

void sysexCallback(byte command, byte argc, byte *argv)
{
  byte mode;
  byte stopTX;
  byte slaveAddress;
  byte data;
  int slaveRegister;
  unsigned int delayTime;

  switch (command) {
    case SAMPLING_INTERVAL:
      if (argc > 1) {
        samplingInterval = argv[0] + (argv[1] << 7);
        if (samplingInterval < MINIMUM_SAMPLING_INTERVAL) {
          samplingInterval = MINIMUM_SAMPLING_INTERVAL;
        }
      } else {
        //Firmata.sendString("Not enough data");
      }
      break;
    case CAPABILITY_QUERY:
      Firmata.write(START_SYSEX);
      Firmata.write(CAPABILITY_RESPONSE);
      for (byte pin = 0; pin < TOTAL_PINS; pin++) {
        if (IS_PIN_DIGITAL(pin)) {
          Firmata.write((byte)INPUT);
          Firmata.write(1);
          Firmata.write((byte)PIN_MODE_PULLUP);
          Firmata.write(1);
          Firmata.write((byte)OUTPUT);
          Firmata.write(1);
        }
        if (IS_PIN_ANALOG(pin)) {
          Firmata.write(PIN_MODE_ANALOG);
          Firmata.write(10); // 10 = 10-bit resolution
        }
        if (IS_PIN_PWM(pin)) {
          Firmata.write(PIN_MODE_PWM);
          Firmata.write(DEFAULT_PWM_RESOLUTION);
        }
        if (IS_PIN_DIGITAL(pin)) {
          Firmata.write(PIN_MODE_SERVO);
          Firmata.write(14);
        }
        Firmata.write(127);
      }
      Firmata.write(END_SYSEX);
      break;
    case PIN_STATE_QUERY:
      if (argc > 0) {
        byte pin = argv[0];
        Firmata.write(START_SYSEX);
        Firmata.write(PIN_STATE_RESPONSE);
        Firmata.write(pin);
        if (pin < TOTAL_PINS) {
          Firmata.write(Firmata.getPinMode(pin));
          Firmata.write((byte)Firmata.getPinState(pin) & 0x7F);
          if (Firmata.getPinState(pin) & 0xFF80) Firmata.write((byte)(Firmata.getPinState(pin) >> 7) & 0x7F);
          if (Firmata.getPinState(pin) & 0xC000) Firmata.write((byte)(Firmata.getPinState(pin) >> 14) & 0x7F);
        }
        Firmata.write(END_SYSEX);
      }
      break;
  }
}

void checkDigitalInputs(void)
{
  for (byte i = 0; i < TOTAL_PORTS; i++) {
    if (reportPORT[i])
      outputPort(i, readMXPort(i, 0xFF), false);
  }
}

/*
 * Sets the value of an individual pin. Useful if you want to set a pin value but
 * are not tracking the digital port state.
 * Can only be used on pins configured as OUTPUT.
 * Cannot be used to enable pull-ups on Digital INPUT pins.
 */
void setPinValueCallback(byte pin, int value)
{
  if (pin < TOTAL_PINS && IS_PIN_DIGITAL(pin)) {
    if (Firmata.getPinMode(pin) == OUTPUT) {
      Firmata.setPinState(pin, value);
      writeMXPin(pin, value);
    }
  }
}

void systemResetCallback()
{
  isResetting = true;

  // initialize default state

  for (byte i = 0; i < TOTAL_PORTS; i++) {
    reportPORT[i] = false;    // by default, reporting off
    previousPORT[i] = 0;
    previousPORTValue[i] = 0;
  }

  for (byte i = 0; i < TOTAL_PINS; i++) {
    // pins with analog capability default to analog input
    // otherwise, pins default to digital output
    if (IS_PIN_ANALOG(i)) {
      // turns off pullup, configures everything
      setPinModeCallback(i, PIN_MODE_ANALOG);
    } else if (IS_PIN_DIGITAL(i)) {
      // sets the output to 0, configures portConfigInputs
      setPinModeCallback(i, INPUT);
    }
  }

  isResetting = false;
}

void setup()
{
  Firmata.setFirmwareVersion(FIRMATA_FIRMWARE_MAJOR_VERSION, FIRMATA_FIRMWARE_MINOR_VERSION);
  Firmata.attach(DIGITAL_MESSAGE, digitalWriteCallback);
  Firmata.attach(SET_PIN_MODE, setPinModeCallback);
  Firmata.attach(SET_DIGITAL_PIN_VALUE, setPinValueCallback);
  Firmata.attach(START_SYSEX, sysexCallback);
  Firmata.attach(SYSTEM_RESET, systemResetCallback);

  systemResetCallback();
  Firmata.begin(57600);
}

void loop()
{
  checkDigitalInputs();
  while (Firmata.available()) {
    Firmata.processInput();
  }
}
