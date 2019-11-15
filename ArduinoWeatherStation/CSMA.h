#ifndef _CSMA_H
#define _CSMA_H

#include "lib/RadioLib/src/Radiolib.h"

/*
*/
template<class T>
class CSMAWrapper : public T
{
  public:
    CSMAWrapper(Module* mod) : T(mod) {
      setP(100); //0.4
      setTimeSlot(20000); //20ms
    }

    int16_t transmit(uint8_t* data, size_t len, uint8_t addr = 0) override {
      delayCSMA();

      return(T::transmit(data, len, addr));
    }

    // see http://www.ax25.net/kiss.aspx section 6
    int16_t delayCSMA()
    {
      bool wasBusy = false;
      do
      {
        if(wasBusy && (random(0, 255)) > _p) {
            delayMicroseconds(_timeSlot);
        }
        int16_t state = this->isChannelBusy(false);
        wasBusy = (state == LORA_DETECTED);
        while(state == LORA_DETECTED) {
          state = this->isChannelBusy(false);
        }
        if(state != CHANNEL_FREE) {
          return(state);
        }
      } while(wasBusy);

      return ERR_NONE;
    }

    void setTimeSlot(const uint32_t newValue) {
      _timeSlot = newValue;
    }

    int16_t setP(const float newValue) {
      if (!((0 < newValue) && (newValue <= 1))) {
        return(ERR_UNKNOWN);
      }
      _p = newValue * 255;
      return(ERR_NONE);
    }

    int16_t setPByte(const int8_t newValue) {
      _p = newValue;
      return(ERR_NONE);
    }

  private:
    uint32_t _timeSlot;
    uint8_t _p;
};

#endif