/**
 * NexaCtrl Library v.02 - Nexa/HomeEasy control library with absolute dim support.
 * Original author of this library is James Taylor (http://jtlog.wordpress.com/)
 *
 * Version 02 by Carl Gunnarsson - Refactoring and adding support for absolute dimming
 */

#include "NexaCtrl.h"

extern "C" {
    // AVR LibC Includes
#include <inttypes.h>
#include <avr/interrupt.h>
}

const int NexaCtrl::kPulseHigh = 275;
const int NexaCtrl::kPulseLow0 = 275;
const int NexaCtrl::kPulseLow1 = 1225;

const int NexaCtrl::kLowPulseLength = 64;

/*
 * The actual message is 32 bits of data:
 * bits 0-25: the group code - a 26bit number assigned to controllers.
 * bit 26: group flag
 * bit 27: on/off/dim flag
 * bits 28-31: the device code - 4bit number.
 * bits 32-35: the dim level - 4bit number.
 */
const int NexaCtrl::kControllerIdOffset = 0;
const int NexaCtrl::kControllerIdLength = 26;
const int NexaCtrl::kGroupFlagOffset = 26;
const int NexaCtrl::kOnFlagOffset = 27;
const int NexaCtrl::kDeviceIdOffset = 28;
const int NexaCtrl::kDeviceIdLength = 4;
const int NexaCtrl::kDimOffset = 32;
const int NexaCtrl::kDimLength = 4;

NexaCtrl::NexaCtrl(unsigned int tx_pin, unsigned int rx_pin, unsigned int led_pin)
{
    led_pin_ = led_pin;
    pinMode(led_pin_, OUTPUT);

    NexaCtrl(tx_pin, rx_pin);
}

NexaCtrl::NexaCtrl(unsigned int tx_pin, unsigned int rx_pin)
{
    tx_pin_ = tx_pin;
    rx_pin_ = rx_pin;
    pinMode(tx_pin_, OUTPUT);
    pinMode(rx_pin_, INPUT);

    // kLowPulseLength + kDimLength because we need room for dim bits if
    // we want to transmit a dim signal
    low_pulse_array = (int*)calloc((kLowPulseLength + (2 * kDimLength)), sizeof(int));
}

void NexaCtrl::DeviceOn(unsigned long controller_id, unsigned int device_id)
{
    SetControllerBits(controller_id);
    SetBit(kGroupFlagOffset, 0);
    SetBit(kOnFlagOffset, 1);
    SetDeviceBits(device_id);
    Transmit(kLowPulseLength);
}

void NexaCtrl::DeviceOff(unsigned long controller_id, unsigned int device_id)
{
    SetControllerBits(controller_id);
    SetBit(kGroupFlagOffset, 0);
    SetBit(kOnFlagOffset, 0);
    SetDeviceBits(device_id);
    Transmit(kLowPulseLength);
}

void NexaCtrl::DeviceDim(unsigned long controller_id, unsigned int device_id, unsigned int dim_level)
{
    SetControllerBits(controller_id);

    SetBit(kGroupFlagOffset, 0);

    // Specific for dim
    low_pulse_array[(kOnFlagOffset*2)] = kPulseLow0;
    low_pulse_array[(kOnFlagOffset*2) + 1] = kPulseLow0;

    SetDeviceBits(device_id);

    bool dim_bits[kDimLength];
    itob(dim_bits, dim_level, kDimLength);

    int bit;
    for (bit = 0; bit<kDimLength; bit++) {
        SetBit(kDimOffset+bit, dim_bits[bit]);
    }
    Transmit(kLowPulseLength + (kDimLength * 2));
}

void NexaCtrl::GroupOn(unsigned long controller_id)
{
    unsigned int device_id = 0;
    SetControllerBits(controller_id);
    SetDeviceBits(device_id);
    SetBit(kGroupFlagOffset, 1);
    SetBit(kOnFlagOffset, 1);
    Transmit(kLowPulseLength);
}

void NexaCtrl::GroupOff(unsigned long controller_id)
{
    unsigned int device_id = 0;
    SetControllerBits(controller_id);
    SetDeviceBits(device_id);
    SetBit(kGroupFlagOffset, 1);
    SetBit(kOnFlagOffset, 0);
    Transmit(kLowPulseLength);
}

// Private methods
void NexaCtrl::SetDeviceBits(unsigned int device_id)
{
    bool device[kDeviceIdLength];
    unsigned long ldevice_id = device_id;
    itob(device, ldevice_id, kDeviceIdLength);
    int bit;
    for (bit=0; bit < kDeviceIdLength; bit++) {
        SetBit(kDeviceIdOffset+bit, device[bit]);
    }
}

void NexaCtrl::SetControllerBits(unsigned long controller_id)
{
    bool controller[kControllerIdLength];
    itob(controller, controller_id, kControllerIdLength);

    int bit;
    for (bit=0; bit < kControllerIdLength; bit++) {
        SetBit(kControllerIdOffset+bit, controller[bit]);
    }
}

void NexaCtrl::SetBit(unsigned int bit_index, bool value)
{
    // Each actual bit of data is encoded as two bits on the wire...
    if (!value) {
        // Data 0 = Wire 01
        low_pulse_array[(bit_index*2)] = kPulseLow0;
        low_pulse_array[(bit_index*2) + 1] = kPulseLow1;
    } else {
        // Data 1 = Wire 10
        low_pulse_array[(bit_index*2)] = kPulseLow1;
        low_pulse_array[(bit_index*2) + 1] = kPulseLow0;
    }
}

void NexaCtrl::Transmit(int pulse_length)
{
    int pulse_count;
    int transmit_count;

    cli(); // disable interupts

    for (transmit_count = 0; transmit_count < 2; transmit_count++)
    {
        if (led_pin_ > 0) {
            digitalWrite(led_pin_, HIGH);
        }
        TransmitLatch1();
        TransmitLatch2();

        /*
         * Transmit the actual message
         * every wire bit starts with the same short high pulse, followed
         * by a short or long low pulse from an array of low pulse lengths
         */
        for (pulse_count = 0; pulse_count < pulse_length; pulse_count++)
        {
            digitalWrite(tx_pin_, HIGH);
            delayMicroseconds(kPulseHigh);
            digitalWrite(tx_pin_, LOW);
            delayMicroseconds(low_pulse_array[pulse_count]);
        }

        TransmitLatch2();

        if (led_pin_ > 0) {
            digitalWrite(led_pin_, LOW);
        }

        delayMicroseconds(10000);
    }

    sei(); // enable interupts
}

void NexaCtrl::TransmitLatch1(void)
{
    // bit of radio shouting before we start
    digitalWrite(tx_pin_, HIGH);
    delayMicroseconds(kPulseLow0);
    // low for 9900 for latch 1
    digitalWrite(tx_pin_, LOW);
    delayMicroseconds(9900);
}

void NexaCtrl::TransmitLatch2(void)
{
    // high for a moment 275
    digitalWrite(tx_pin_, HIGH);
    delayMicroseconds(kPulseLow0);
    // low for 2675 for latch 2
    digitalWrite(tx_pin_, LOW);
    delayMicroseconds(2675);
}

void itob(bool *bits, unsigned long integer, int length) {
    for (int i=0; i<length; i++){
        if ((integer / power2(length-1-i))==1){
            integer-=power2(length-1-i);
            bits[i]=1;
        }
        else bits[i]=0;
    }
}

unsigned long power2(int power){    //gives 2 to the (power)
    return (unsigned long) 1 << power;
}

unsigned long htoi (const char *ptr)
{
    unsigned long value = 0;
    char ch = *ptr;

    while (ch == ' ' || ch == '\t') {
        ch = *(++ptr);
    }

    for (;;) {
        if (ch >= '0' && ch <= '9') {
            value = (value << 4) + (ch - '0');
        } else if (ch >= 'A' && ch <= 'F') {
            value = (value << 4) + (ch - 'A' + 10);
        } else if (ch >= 'a' && ch <= 'f') {
            value = (value << 4) + (ch - 'a' + 10);
        } else {
            return value;
        }
        ch = *(++ptr);
    }
}
