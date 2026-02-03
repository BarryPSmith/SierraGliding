#ifdef WS80_WIND
#include "WeatherProcessing.h"
#include "../lib/RadioLib/src/RadioLib.h"
#include "../LoraMessaging.h"
#include "Wind.h"
#include <avr/wdt.h>

namespace WeatherProcessing
{
  extern volatile bool weatherRequired;
  bool Decode(unsigned char* b, WS80_Reading* reading);
  void InitialiseFSK();
  void InitialiseFSK2();

  constexpr int WS80_interval = 4200; // Actually ~4600, but we put a bit of a buffer in because we will only enter this perhaps every 250ms.

  unsigned int lastWS80_signal;
  WS80_Reading lastReading;

  void processWeather()
  {
    if (((unsigned int)millis()) - lastWS80_signal < WS80_interval)
      return;

    WX_PRINTLN(F("processWeather - passed millis check"));

    InitialiseFSK();

    WX_PRINTLN(F("processWeather - FSK Initialised"));

    LORA_CHECK(lora.startReceive(SX126X_RX_TIMEOUT_NONE));

    WX_PRINTLN(F("processWeather - start Receive"));


    /*LORA_CHECK(lora.setDioIrqParams(RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_TIMEOUT | RADIOLIB_SX126X_IRQ_CRC_ERR | RADIOLIB_SX126X_IRQ_HEADER_ERR, 
      RADIOLIB_SX126X_IRQ_RX_DONE));*/

#ifdef DEBUG_WEATHER
    unsigned int entryMillis = millis();
#endif
    // Check for our radio interrupt...
    while (!digitalRead(SX_DIO1)/*Check IRQ not signalled*/)
    {
      yield();
    }
#ifdef DEBUG_WEATHER
    unsigned int afterWaitMillis = millis();
    WX_PRINT(F("Wait: "));
    WX_PRINTLN(afterWaitMillis - entryMillis);
#endif


    WX_PRINTLN(F("processWeather - Data read"));

    byte data[32];
    LORA_CHECK(lora.readData(data, 32));
    initMessagingRequired = true;
    if (Decode(data, &lastReading))
    {
      WX_PRINTLN(F("Data Decoded"));
      lastWS80_signal = millis();
      weatherRequired = true;
    }
    else
      WX_PRINTLN(F("Decode failed."));
  }

  void InitialiseFSK2()
  {
    int state = lora.beginFSK();

    // if needed, you can switch between LoRa and FSK modes
    //
    // radio.begin()       start LoRa mode (and disable FSK)
    // radio.beginFSK()    start FSK mode (and disable LoRa)

    // the following settings can also
    // be modified at run-time
    // SX1262 already does FSK-2
    state = lora.setFrequency(915); //Modified
    state = lora.setFrequencyDeviation(47.6); //Modified
    // setCCMode appears to be CC1101 specific
    state = lora.setRxBandwidth(450.0); //Modified THIS FAILS

    state = lora.setBitRate(17.241); //CC1101 SetDRate?
    uint8_t syncWord[] = { 0x2D, 0xD4 };
    state = lora.setSyncWord(syncWord, 2); //Modified
    //Skipping address, whitening, pktFmt, pre, pqt, appendstatus
    state = lora.fixedPacketLengthMode(32); //Modified
    //state = radio.setOutputPower(10.0);
    state = lora.setCurrentLimit(100.0);
    //state = radio.setDataShaping(RADIOLIB_SHAPING_1_0);
    state = lora.setTCXO(1.8);
    state = lora.setCRC(0); //TODO later: Fix this
    lora.setWhitening(false);
  }

  void InitialiseFSK()
  {
    LORA_CHECK(lora.standby(SX126X_STANDBY_RC));
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
    uint16_t wind_avg_x10 = (((b[7] & 0x10) << 4) | (b[10]));
    short wind_dir = ((b[7] & 0x20) << 3) | (b[11]);
    uint16_t wind_max_x10 = (((b[7] & 0x40) << 2) | (b[12]));
    uint8_t uv_index_x10 = (b[13]);

    if (wind_avg_x10 > (wind_max_x10 + 1))
    {
      return false;
    }

    reading->battery_mv = battery_mv;
    reading->temp = temp;
    reading->humidity = humidity;
    reading->wind_avg_x10 = wind_avg_x10;
    reading->wind_max_x10 = wind_max_x10;
    reading->uv_index_x10 = uv_index_x10;
    reading->light = light;
    reading->wind_dir = (wind_dir * 255UL) / 360;

    WX_PRINT("wind_avg_x10: ");
    WX_PRINTLN(wind_avg_x10);

    return true;
  }
}
#endif