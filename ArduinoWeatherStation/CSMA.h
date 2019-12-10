#ifndef _CSMA_H
#define _CSMA_H

//#include "lib/RadioLib/src/Radiolib.h"
#include "ArduinoWeatherStation.h"

inline uint8_t getLowNibble(const uint8_t input) { return input & 0x0F; }
inline uint8_t getHiNibble(const uint8_t input) { return (input >> 4) & 0xF0; }
inline void setLowNibble(uint8_t* dest, const uint8_t value) { *dest = getHiNibble(*dest) & getLowNibble(value); }
inline void setHiNibble(uint8_t* dest, const uint8_t value) { *dest = getLowNibble(*dest) & getHiNibble(value); }

#define NO_PACKET_AVAILABLE -1000
#define NOT_ENOUGH_SPACE -1001

enum class IdleStates {
  NotInitialised,
  ContinuousReceive,
  IntermittentReceive,
  Sleep
};

/*
*/
template<class T, uint8_t bufferSize = 255, uint8_t maxQueue = 8>
class CSMAWrapper : public T
{
  public:
    CSMAWrapper(Module* mod) : T(mod) {
      setP(100); //0.4
      setTimeSlot(20000); //20ms
    }

    static constexpr uint16_t averagingPeriod = 256;

    void initBuffer()
    {
      s_packetWaiting = false;
      this->setRxDoneAction(rxDoneActionStatic);
    }

    int16_t transmit(uint8_t* data, size_t len, uint16_t preambleLength, uint8_t addr = 0) {
      delayCSMA();
      
      auto ret = this->setPreambleLength(preambleLength);
      if (ret != ERR_NONE)
        return ret;
      ret = T::transmit(data, len, addr);
      enterIdleState();

      return ret;
    }

    // see http://www.ax25.net/kiss.aspx section 6 for CSMA implementation.
    // this is a blocking function that will queue any messages received while it waits.
    int16_t delayCSMA()
    {
      uint32_t entryMicros = micros();
      bool wasBusy = false;
      do
      {
        if(wasBusy && (random(0, 255)) > _p) {
          uint32_t startMicros = micros();
          while (micros() - startMicros < _timeSlot) {
            readIfPossible();
          }
        }
        int16_t state = this->isChannelBusy(true);
        wasBusy = (state == LORA_DETECTED);
        while(state == LORA_DETECTED) {
          readIfPossible();
          state = this->isChannelBusy(true);
        }
        if(state != CHANNEL_FREE) {
          return(state);
        }
      } while(wasBusy);

      uint32_t delayTime = micros() - entryMicros;
      _averageDelayTime = _averageDelayTime * (averagingPeriod - 1) / (averagingPeriod)
        +
        delayTime / averagingPeriod;

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

    void setPByte(const int8_t newValue) {
      _p = newValue;
    }

    // Note that buffer is possibly invalidateded by transmit, 
    // readIfPossible, and subsequent calls to dequeMessage.
    int16_t dequeueMessage(uint8_t** buffer, uint8_t* length) {
      // handle any available packet from the modem if we have space:
      // (this function also resets the buffer pointers to zero if it is empty).
      int16_t state = readIfPossible();

      if (_writeBufferLenPointer == 0) {
        *buffer = 0;
        *length = 0;
        
        return(state); //Receive error or NO_PACKET_AVAILABLE
      }

      *buffer = _buffer + getStartOfBuffer();
      *length = _messageLengths[_readBufferLenPointer];

      _readBufferLenPointer++;
      if (state != NO_PACKET_AVAILABLE)
        return(state);
      else
        return ERR_NONE;
    }

    int16_t readIfPossible() {
      if (!s_packetWaiting) {
        return(NO_PACKET_AVAILABLE);
      }

      AWS_DEBUG_PRINTLN("Got one in CSMA");

      if (_writeBufferLenPointer == maxQueue) {
        return(NOT_ENOUGH_SPACE);
      }
      uint8_t bufferEnd = getEndOfBuffer();
      uint8_t packetSize = this->getPacketLength(false);
      if (bufferSize - bufferEnd < packetSize) {
        return(NOT_ENOUGH_SPACE);
      }

      int16_t state = this->readData(_buffer + bufferEnd, packetSize);
      enterIdleState();

      //calculate some statistics:
      //crc error?
      uint16_t crcError = state == ERR_CRC_MISMATCH ? 0xFFFF / averagingPeriod : 0;
      _crcErrorRate = (uint32_t)_crcErrorRate * (averagingPeriod - 1) / averagingPeriod
        +
        crcError;

      //Did we drop any packets? This only counts packets dropped due to out of memory, not CRC or other errors.
      uint32_t droppedPackets = (uint32_t)(s_packetCounter - _lastPacketCounter - 1) * (0xFFFF / averagingPeriod);
      _lastPacketCounter = s_packetCounter;
      _droppedPacketRate = _droppedPacketRate * (averagingPeriod - 1) / averagingPeriod
        +
        droppedPackets;

      if (state != ERR_NONE) {
        return(state);
      }

      _messageLengths[_writeBufferLenPointer] = packetSize;
      _writeBufferLenPointer++;
      s_packetWaiting = false;
      return(ERR_NONE);
    }

    void clearBuffer() {
      _readBufferLenPointer = 0;
      _writeBufferLenPointer = 0;
    }

    int16_t setIdleState(IdleStates newState)
    {
      if (newState == _idleState)
        return ERR_NONE;
      _idleState = newState;
      if (newState == IdleStates::Sleep || this->isChannelBusy(false) == CHANNEL_FREE)
        return enterIdleState();
      else
        return LORA_DETECTED;
    }

    int16_t enterIdleState()
    {
      this->setPreambleLength(_senderPremableLength);
      switch (_idleState)
      {
      case IdleStates::IntermittentReceive:
        return this->startReceiveDutyCycleAuto();
      case IdleStates::ContinuousReceive:
        return this->startReceive();
      case IdleStates::Sleep:
        return this->sleep();
      default:
        return ERR_UNKNOWN;
      }
    }

    uint16_t _senderPremableLength = 0xFFFF;

    uint16_t _crcErrorRate;
    uint16_t _droppedPacketRate;
    uint32_t _averageDelayTime;
  private:
    uint8_t _buffer[bufferSize];
    uint8_t _messageLengths[maxQueue];
    uint8_t _readBufferLenPointer = 0;
    uint8_t _writeBufferLenPointer = 0;
    uint8_t _lastPacketCounter = 0;
    IdleStates _idleState = IdleStates::NotInitialised;
    
    uint8_t getEndOfBuffer() {
      if (_readBufferLenPointer == _writeBufferLenPointer) {
        clearBuffer();
      }
      uint8_t ret = 0;
      for (int i = 0; i < _writeBufferLenPointer; i++)
        ret += _messageLengths[i];
      return ret;
    }

    uint8_t getStartOfBuffer() {
      if (_readBufferLenPointer = _writeBufferLenPointer) {
        clearBuffer();
      }
      uint8_t ret = 0;
      for (int i = 0; i < _readBufferLenPointer; i++)
        ret += _messageLengths[i];
      return ret;
    }

    uint32_t _timeSlot;
    uint8_t _p;

    static volatile bool s_packetWaiting;
    static volatile uint8_t s_packetCounter;
    static void rxDoneActionStatic()
    {
      s_packetWaiting = true;
      s_packetCounter++;
    }
};

template<class T, uint8_t bs, uint8_t mp>
volatile bool CSMAWrapper<T, bs, mp>::s_packetWaiting = false;

template<class T, uint8_t bs, uint8_t mp>
volatile uint8_t CSMAWrapper<T, bs, mp>::s_packetCounter = 0;
#endif