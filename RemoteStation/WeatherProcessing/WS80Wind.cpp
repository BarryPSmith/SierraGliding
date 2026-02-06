#ifdef WS80_WIND
#include "WeatherProcessing.h"
#include "../lib/RadioLib/src/RadioLib.h"
#include "../LoraMessaging.h"
#include "Wind.h"
#include "../TimerTwo.h"
#include <avr/wdt.h>

namespace WeatherProcessing
{
  //constexpr uint16_t WS80_interval = 4720;
  constexpr uint16_t WS80_preSwitch = 70; // Minimum pre-switch is 30: 10ms for initialisation, 20ms for receive. We choose 70ms to give ourselves a larger buffer.

  extern volatile bool weatherRequired;
  bool Decode(unsigned char* b, WS80_Reading* reading);
  void InitialiseFSK();
  void InitialiseFskDirect();

  uint32_t lastWS80_signal;
  WS80_Reading lastReading;

  void processWeather()
  {
    uint32_t entryMillis = millis();
    uint32_t millisSinceLastSignal = entryMillis - lastWS80_signal;
    // If we need to switch to listening for the WS80 before the next timer cycle, ensure that we don't sleep the MCU.
    if (millisSinceLastSignal > weatherInterval - WS80_preSwitch - TimerTwo::MillisPerTick)
      weatherSleepEnabled = SleepModes::disabled;
    else
      weatherSleepEnabled = SleepModes::powerSave;
    if (millisSinceLastSignal < weatherInterval - WS80_preSwitch)
      return;

    InitialiseFskDirect();

    WX_DEBUG(uint32_t intialiseMillis = millis() - entryMillis);

    LORA_CHECK(lora.startReceive(SX126X_RX_TIMEOUT_NONE));

    WX_DEBUG(uint32_t startReceiveMillis = millis() - entryMillis);

    WX_DEBUG(uint32_t beforeReadMillis = millis());
    WX_DEBUG(uint32_t beforeReadDelay = beforeReadMillis - entryMillis);
#ifndef DEBUG
    digitalWrite(LED_PIN1, LED_ON);
#endif
    // Check for our radio interrupt...
    while (!digitalRead(SX_DIO1)/*Check IRQ not signalled*/)
    {
      yield();
    }
#ifndef  DEBUG
    digitalWrite(LED_PIN1, LED_OFF);
#endif

    WX_DEBUG(uint32_t afterWaitMillis = millis());
    WX_DEBUG(uint32_t WS80_measuredInterval = afterWaitMillis - lastWS80_signal);


    byte data[32];
    LORA_CHECK(lora.readData(data, 32));
    noHardReset = true;
    initMessagingRequired = true;
    if (Decode(data, &lastReading))
    {
      lastWS80_signal = millis();
      WX_PRINTLN(F("Data Decoded"));
      weatherRequired = true;
    }
    else
      WX_PRINTLN(F("Decode failed."));

    WX_PRINT(F("processWeather - FSK Initialised "));
    WX_PRINTLN(intialiseMillis);

    WX_PRINT(F("processWeather - start Receive "));
    WX_PRINTLN(startReceiveMillis);

    WX_PRINT(F("Init time: "));
    WX_PRINTLN(beforeReadDelay);

    WX_PRINT(F("Measured WS 80 interval: "));
    WX_PRINTLN(WS80_measuredInterval);
    WX_PRINT(F("Wait: "));
    WX_PRINTLN(afterWaitMillis - beforeReadMillis);
  }

  void InitialiseFskDirect()
  {
    // Radiolib is excessively chatty, calling setPacketParams and other options over and over.
    // Also, beginFSK_i calls a bunch of generic stuff that we don't need to do every time we change packet type.
    // This methos takes 8ms; using the public Radiolib API takes 28ms.
    // Speeding this method up => less likely to miss packets, lower power consumption.
    // Also this uses less code space.
    uint8_t modem = SX126X_PACKET_TYPE_GFSK;
    LORA_CHECK(lora.standby(SX126X_STANDBY_RC));
    LORA_CHECK(lora.SPIwriteCommand(SX126X_CMD_SET_PACKET_TYPE, &modem, 1));

    // Calculate all the modulation params, then set them:
    constexpr uint32_t br_bps = 17241;
    uint32_t brRaw = (32 * SX126X_CRYSTAL_FREQ * SX126x::MHz) / br_bps;
    lora._br = brRaw;
    constexpr uint32_t freqDev_Hz = 47600;
    uint32_t freqDevRaw = lora.getRfFreq(freqDev_Hz);
    lora._freqDev = freqDevRaw;
    lora._rxBwKhz_x10 = SX126X_GFSK_RX_BW_156_2;
    lora._pulseShape = SX126X_GFSK_FILTER_GAUSS_0_5;
    LORA_CHECK(lora.setModulationParamsFSK(lora._br, lora._rxBwKhz_x10, lora._pulseShape, lora._freqDev));

    uint8_t syncWord[] = { 0x2D, 0xD4 };
    LORA_CHECK(lora.writeRegister(SX126X_REG_SYNC_WORD_0, syncWord, 2));

    // Set the packet params:
    lora._preambleLengthFSK = 16;
    lora._syncWordLength = 2 * 8;
    lora._whitening = SX126X_GFSK_WHITENING_OFF;
    lora._packetType = SX126X_GFSK_PACKET_FIXED;
    lora._packetLen = 32;
    lora._crcTypeFSK = SX126X_GFSK_CRC_OFF;
    lora._addrComp = SX126X_GFSK_ADDRESS_FILT_OFF;
    LORA_CHECK(lora.setPacketParamsFSK(
      lora._preambleLengthFSK, lora._crcTypeFSK, lora._syncWordLength, lora._addrComp,
      lora._whitening, lora._packetType, lora._packetLen, SX126X_GFSK_PREAMBLE_DETECT_16));

    LORA_CHECK(lora.setFrequency_i(915000000));
  }

  void InitialiseFSK()
  {
    // We don't need to call standby because beginFSK_i calls it.
    // LORA_CHECK(lora.standby(SX126X_STANDBY_RC));
    LORA_CHECK(lora.beginFSK_i(
      915000000, //Frequency
      17241, //br_bpds
      47600, // freq dev_Hz
      1562, // rxBw_kHz_x10
      22, // power
      40, // currentLimit_mA_div2_5
      16,// preamble length
      5,// datashaping,
      SX_TCXOV_X10 //tcxoVoltage_x10
    ));

    //LORA_CHECK(lora.setFrequency_i(915));
    //LORA_CHECK(lora.setFrequencyDeviation_i(47600));
    //LORA_CHECK(lora.setRxBandwidth_i(4500));
    //LORA_CHECK(lora.setBitRate_i(17241)); //CC1101 SetDRate?
    uint8_t syncWord[] = { 0x2D, 0xD4 };
    LORA_CHECK(lora.setSyncWord(syncWord, 2));
    //Skipping address, whitening, pktFmt, pre, pqt, appendstatus
    LORA_CHECK(lora.fixedPacketLengthMode(32));
    //LORA_CHECK(lora.setCurrentLimit_i(40));
#ifdef SX_TCXO_STARTUP_US
    LORA_CHECK(lora.setTCXO_i(SX_TCXOV_X10, SX_TCXO_STARTUP_US));
#endif


    // FSK modem allows advanced CRC configuration
    // Default is CCIT CRC16 (2 bytes, initial 0x1D0F, polynomial 0x1021, inverted)
    // Set CRC to IBM CRC (2 bytes, initial 0xFFFF, polynomial 0x8005, non-inverted)
    // state = radio.setCRC(2, 0xFFFF, 0x8005, false);
    // set CRC length to 0 to disable CRC
    LORA_CHECK(lora.setCRC(0)); //TODO later: Fix this
    LORA_CHECK(lora.setWhitening(false));
  }

  uint8_t crc8(uint8_t const message[], unsigned nBytes, uint8_t polynomial, uint8_t init)
  {
    uint8_t remainder = init;
    unsigned byte, bit;

    for (byte = 0; byte < nBytes; ++byte) {
      remainder ^= message[byte];
      for (bit = 0; bit < 8; ++bit) {
        if (remainder & 0x80) {
          remainder = (remainder << 1) ^ polynomial;
        }
        else {
          remainder = (remainder << 1);
        }
      }
    }
    return remainder;
  }

  int add_bytes(uint8_t const message[], unsigned num_bytes)
  {
    int result = 0;
    for (unsigned i = 0; i < num_bytes; ++i) {
      result += message[i];
    }
    return result;
  }


  bool Decode(unsigned char* b, WS80_Reading* reading)
  {
    for (int i = 0; i < 32; i++)
    {
      WX_PRINT(b[i], HEX);
      WX_PRINT(" ");
    }
    WX_PRINTLN();
    //Verify checksum and CRC
    uint8_t crc = crc8(b, 17, 0x31, 0x00);
    uint8_t chk = add_bytes(b, 17);
    if (crc != 0 || chk != b[17])
    {
      WX_PRINTLN(crc);
      WX_PRINTLN(chk);
      WX_PRINTLN(b[17]);
      return false;
    }

    int light = (b[4] << 8) | (b[5]);
    // float light_wm2 = light_raw * 0.078925f; // W/m2
    int battery_mv = (b[6] * 20); // mV
    int temp_raw = ((b[7] & 0x03) << 8) | (b[8]);
    float temp = (temp_raw - 400) * 0.1f;
    int humidity = (b[9]);
    uint16_t wind_avg_mps_x10 = (((b[7] & 0x10) << 4) | (b[10]));
    short wind_dir = ((b[7] & 0x20) << 3) | (b[11]);
    uint16_t wind_max_mps_x10 = (((b[7] & 0x40) << 2) | (b[12]));
    uint8_t uv_index_x10 = (b[13]);

    if (wind_avg_mps_x10 > (wind_max_mps_x10 + 1))
    {
      return false;
    }

    reading->battery_mv = battery_mv;
    reading->temp = temp;
    reading->humidity = humidity;
    reading->wind_avg_x50 = wind_avg_mps_x10 * 18; // (* 3.6) = (*18 / 5)
    reading->wind_max_x50 = wind_max_mps_x10 * 18; // (* 3.6) = (*18 / 5)
    reading->uv_index_x10 = uv_index_x10;
    reading->light = light;
    reading->wind_dir = (wind_dir * 255UL) / 360;

    WX_PRINT("wind_avg_x10: ");
    WX_PRINTLN(wind_avg_mps_x10);

    return true;
  }
}
#endif