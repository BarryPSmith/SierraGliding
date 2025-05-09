#ifndef _CSMA_H
#define _CSMA_H

#include "lib/RadioLib/src/Radiolib.h"
#include "ArduinoWeatherStation.h"
#include "StackCanary.h"

inline uint8_t getLowNibble(const uint8_t input) { return input & 0x0F; }
inline uint8_t getHiNibble(const uint8_t input) { return (input >> 4) & 0xF0; }
inline void setLowNibble(uint8_t* dest, const uint8_t value) { *dest = getHiNibble(*dest) & getLowNibble(value); }
inline void setHiNibble(uint8_t* dest, const uint8_t value) { *dest = getLowNibble(*dest) & getHiNibble(value); }



#ifdef MODEM
extern unsigned long lastFailMillis;
#endif

inline int16_t lora_check(const int16_t result, const __FlashStringHelper* msg) 
{
  if (result != ERR_NONE && result != NO_PACKET_AVAILABLE)
  {
    AWS_DEBUG_PRINT(msg);
    AWS_DEBUG_PRINTLN(result);
    SIGNALERROR(result);
#ifdef MODEM
    lastFailMillis = millis();
#endif
  }
  return result;
}
#ifdef DETAILED_LORA_CHECK
#define LORA_CHECK(A) lora_check(A, F("FAILED " #A ": "))
#else
#define LORA_CHECK(A) lora_check(A, F("LORA_CHECK FAILED: "))
#endif

enum class IdleStates : byte {
  NotInitialised,
  ContinuousReceive,
  IntermittentReceive,
  Sleep
};

/*
*/
template<class T, uint8_t bufferSize = 255, uint8_t maxQueue = 8>
class CSMAWrapper
{
  public:
    static constexpr uint16_t MaxDelay = 2500; // 2.5 seconds
    static constexpr uint16_t averagingPeriod = 256;

    CSMAWrapper(T* base) {
      _base = base;
      setP(100); //0.4
      setTimeSlot(20000); //20ms
    }

    T* _base;

    void initBuffer()
    {
      s_packetWaiting = false;
      _base->setRxDoneAction(rxDoneActionStatic);
    }

    int16_t transmit(uint8_t* data, byte len, uint16_t preambleLength,
        bool initialWait) {
      TX_DEBUG(auto entryMicros = micros());
      // Set this flag to stop CSMA flashing, the delay messes with the logic.
      silentSignal = true;
      LORA_CHECK(delayCSMA(initialWait));
      silentSignal = false;
      TX_DEBUG(auto delayMicros = micros() - entryMicros);
      auto ret = transmit2(data, len, preambleLength);
      enterIdleState();
      return ret;
    }

    //transmit and transmit2 are separated to foce the compiler to not allocate as much stack
    int16_t __attribute__ ((noinline)) transmit2(uint8_t* data, byte len, uint16_t preambleLength)
    { 
      auto ret = _base->setPreambleLength(preambleLength);
      TX_DEBUG(auto preambleMicros = micros() - entryMicros);
      if (ret != ERR_NONE)
        return ret;
      ret = _base->transmit(data, len, 0);
      
      TX_DEBUG(auto txMicros = micros() - entryMicros);
      TX_DEBUG(auto idleMicros = micros() - entryMicros);
      TX_PRINTVAR(delayMicros);
      TX_PRINTVAR(preambleMicros);
      TX_PRINTVAR(txMicros);
      TX_PRINTVAR(idleMicros);
      

      return ret;
    }

    void __attribute__ ((noinline)) updateAverageDelay(uint16_t entryMillis)
    {
      uint16_t delayTime = millis16() - entryMillis;
      _averageDelayTime = _averageDelayTime * (averagingPeriod - 1) / (averagingPeriod)
        +
        delayTime;
    }

    #define CHECK_TIMEOUT() do { if(millis16() - entryMillis > MaxDelay) return(ERR_RX_TIMEOUT); } while (0)
    // see http://www.ax25.net/kiss.aspx section 6 for CSMA implementation.
    // this is a blocking function that will queue any messages received while it waits.
    int16_t delayCSMA(bool initialWait)
    {
      uint16_t entryMillis = millis16();
      //uint32_t beforeIsChannelBusy, afterIsChannelBusy;
      bool wasBusy = initialWait;
      do
      {
        //Wait an random amount of time, exponentially distributed
        while (wasBusy && (rand() & 0xFF) > _p) {
          uint16_t startMillis = millis16();
          while (millis16() - startMillis < _timeSlot) {
            CHECK_TIMEOUT();
            readIfPossible();
          }
        }

        int16_t state = _base->isChannelBusy(_idleState != IdleStates::ContinuousReceive, false);
        wasBusy = (state == LORA_DETECTED);
        while(state == LORA_DETECTED) {
          delay(10); // If we spam the modem with detectCAD, it can never receieve a message.
                     // Also the delay lets our MCU sleep.
          CHECK_TIMEOUT();
          readIfPossible();
          //We use CAD in case the modem is halfway through receiving a message...
          state = _base->isChannelBusy(_idleState != IdleStates::ContinuousReceive, false);
        }
        if(state != CHANNEL_FREE) {
          return(state);
        }
      } while(wasBusy);

      updateAverageDelay(entryMillis);

      return ERR_NONE;
    }

    void setTimeSlot(const uint32_t newValue) {
      _timeSlot = newValue / 1000;
    }

    int16_t setP(const float newValue) {
      if (!((0 < newValue) && (newValue <= 1))) {
        return(ERR_UNKNOWN);
      }
      _p = newValue * 255;
      return(ERR_NONE);
    }

    void setPByte(const uint8_t newValue) {
      _p = newValue;
    }

    // Note that buffer is possibly invalidateded by transmit, 
    // readIfPossible, and subsequent calls to dequeMessage.
    int16_t dequeueMessage(uint8_t** buffer, uint8_t* length,
      uint16_t* timestamp
#ifdef GET_CRC_FAILURES
      ,bool* crcMismatch
#endif
      ) {
      // handle any available packet from the modem if we have space:
      // (this function also resets the buffer pointers to zero if it is empty).
      int16_t state = readIfPossible();

      if (_writeBufferLenIdx == 0) {
        *buffer = 0;
        *length = 0;
        
        return(state); //Receive error or NO_PACKET_AVAILABLE
      }
      if (_checkedOut)
      {
        *buffer = 0;
        *length = 0;
        return REENTRY_NOT_SUPPORTED;
      }

      *buffer = _buffer + getStartOfBuffer();
      *length = _messageLengths[_readBufferLenIdx];
      *timestamp = _messageTimestamps[_readBufferLenIdx];
#ifdef GET_CRC_FAILURES
      *crcMismatch = _crcMismatches[_readBufferLenIdx];
#endif
      _checkedOut = true;
      if (length == 0)
      {
        // We shouldn't end up here - but we have in the past,
        // and it caused re-entry problems because the calling code 
        // was using length to determine if it needed to call doneWithBuffer
        RX_PRINTLN(F("CSMA_UNEXPECTED_ZERO_LENGTH"));
        SIGNALERROR(CSMA_UNEXPECTED_ZERO_LENGTH);
        doneWithBuffer();
      }
      return(state);
    }

    void doneWithBuffer()
    {
      _checkedOut = false;
      _readBufferLenIdx++;
      if (_readBufferLenIdx > _writeBufferLenIdx) {
        RX_PRINTLN(F("ERROR: Read buffer ahead of write buffer!"));
        SIGNALERROR(CSMA_POINTER_INVERSION);
      }
      if (_readBufferLenIdx >= _writeBufferLenIdx) {
        clearBuffer();
      }
    }

    int16_t readIfPossible()
    {
      bool reenterRequired = false;
      auto ret = readIfPossibleInt(reenterRequired);
      if (reenterRequired)
        LORA_CHECK(enterIdleState());
      return ret;
    }

    int16_t __attribute__ ((noinline)) readIfPossibleInt(bool& reenterRequired) {
      LORA_CHECK(_base->processLoop());
      if (!s_packetWaiting) {
        return(NO_PACKET_AVAILABLE);
      }
      
      if (_writeBufferLenIdx == maxQueue) {
        return(NOT_ENOUGH_SPACE);
      }
      // Put the radio into standby to avoid a race condition
      // where we receive another packet between getPacketLength and readData
      LORA_CHECK(_base->standby());
      reenterRequired = true;
      uint8_t bufferWriteOffset = getBufferWriteOffset();
      uint8_t packetSize = _base->getPacketLength(false);
      if (bufferSize - bufferWriteOffset < packetSize) {
        return(NOT_ENOUGH_SPACE);
      }
      s_packetWaiting = false;
      if (packetSize == 0) {
        return NO_PACKET_AVAILABLE;
      }

      int16_t state = _base->readData(_buffer + bufferWriteOffset, packetSize);
      

      updateReadStats(state);

      if (state == ERR_NONE
#ifdef GET_CRC_FAILURES
        || state == ERR_CRC_MISMATCH
#endif
        )
      {
        RX_PRINTVAR(_writeBufferLenIdx);
        RX_PRINTVAR(packetSize);
        _messageLengths[_writeBufferLenIdx] = packetSize;
        _messageTimestamps[_writeBufferLenIdx] = millis16();
#ifdef GET_CRC_FAILURES
        _crcMismatches[_writeBufferLenIdx] = state == ERR_CRC_MISMATCH;
#endif
        _writeBufferLenIdx++;
      }

      return state;
    }

    void __attribute__ ((noinline)) updateReadStats(uint16_t state)
    {
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
    }

    void clearBuffer() {
      RX_PRINTLN(F("Buffer Cleared"));
      _readBufferLenIdx = 0;
      _writeBufferLenIdx = 0;
      _checkedOut = false;
    }

    int16_t setIdleState(IdleStates newState)
    {
      if (newState == _idleState &&
        _senderPremableLength == _actualPreambleLength)
        return ERR_NONE;

      _idleState = newState;

      if (newState == IdleStates::Sleep 
        || _base->_curStatus != SX126X_STATUS_MODE_RX
        || _base->isChannelBusy(false) == CHANNEL_FREE)
        return enterIdleState();
      else
        return LORA_DETECTED;
    }

    int16_t enterIdleState()
    {
      _base->setPreambleLength(_senderPremableLength);
      _actualPreambleLength = _senderPremableLength;
      switch (_idleState)
      {
      case IdleStates::IntermittentReceive:
        return _base->startReceiveDutyCycleAuto();
      case IdleStates::ContinuousReceive:
        return _base->startReceive();
      case IdleStates::Sleep:
        return _base->sleep();
      default:
        return ERR_UNKNOWN;
      }
    }

    uint16_t _senderPremableLength = 0xFFFF,
      _actualPreambleLength = 0;

    uint16_t _crcErrorRate = 0;
    uint16_t _droppedPacketRate = 0;
    uint32_t _averageDelayTime = 0;
  //private:
    uint8_t _buffer[bufferSize];
    uint8_t _messageLengths[maxQueue];
    uint16_t _messageTimestamps[maxQueue];
#ifdef GET_CRC_FAILURES
    bool _crcMismatches[maxQueue];
#endif
    bool _checkedOut = false;
    uint8_t _readBufferLenIdx = 0;
    uint8_t _writeBufferLenIdx = 0;
    uint8_t _lastPacketCounter = 0;
    IdleStates _idleState = IdleStates::NotInitialised;
    
    uint8_t getBufferWriteOffset() {
      if (_readBufferLenIdx == _writeBufferLenIdx && _readBufferLenIdx != 0) {
        RX_PRINTLN(F("ERR: Uncleared buffer on write"));
        clearBuffer();
      }
      else if (_readBufferLenIdx > _writeBufferLenIdx) {
        RX_PRINTLN(F("ERROR: Read buffer ahead of write buffer!"));
      }
      uint8_t ret = 0;
      for (int i = 0; i < _writeBufferLenIdx; i++)
        ret += _messageLengths[i];
      return ret;
    }

    uint8_t getStartOfBuffer() {
      uint8_t ret = 0;
      for (int i = 0; i < _readBufferLenIdx; i++)
        ret += _messageLengths[i];
      return ret;
    }

    uint16_t _timeSlot;
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