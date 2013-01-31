#ifndef NexaCtrl_h
#define NexaCtrl_h

#include "Arduino.h"

class NexaCtrl
{
    public:
        NexaCtrl(unsigned int tx_pin, unsigned int rx_pin, unsigned int led_pin);
        NexaCtrl(unsigned int tx_pin, unsigned int rx_pin);

        void DeviceOn(unsigned long controller_id, unsigned int device_id);
        void DeviceOff(unsigned long controller_id, unsigned int device_id);

        void DeviceDim(unsigned long controller_id, unsigned int device_id, unsigned int dim_level);

        void GroupOn(unsigned long controller_id);
        void GroupOff(unsigned long controller_id);

    private:
        int tx_pin_;
        int rx_pin_;
        int led_pin_;
        unsigned long controller_id_;

        int* low_pulse_array;

        const static int kPulseHigh;
        const static int kPulseLow0;
        const static int kPulseLow1;

        const static int kLowPulseLength;

        const static int kControllerIdOffset;
        const static int kControllerIdLength;
        const static int kGroupFlagOffset;
        const static int kOnFlagOffset;
        const static int kDeviceIdOffset;
        const static int kDeviceIdLength;
        const static int kDimOffset;
        const static int kDimLength;

        void SetBit(unsigned int bit_index, bool value);
        void SetDeviceBits(unsigned int device_id);
        void SetControllerBits(unsigned long controller_id);
        void SendPair(bool on);

        void Transmit(int pulse_length);
        void TransmitLatch1(void);
        void TransmitLatch2(void);
};

void itob(bool *bits, unsigned long, int length);
unsigned long power2(int power);

#endif /* NexaCtrl_h */
