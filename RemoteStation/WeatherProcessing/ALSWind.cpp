#ifdef ALS_WIND
#include "WeatherProcessing.h"
#include "Wind.h"
//#include <Wire.h>
#include "TwoWire.h"
#include "../PermanentStorage.h"
#include <avr/wdt.h>

#define DEBUG_ALS
#ifdef DEBUG_ALS
#define ALS_PRINT AWS_DEBUG_PRINT
#define ALS_PRINTLN AWS_DEBUG_PRINTLN
#define ALS_PRINTVAR PRINT_VARIABLE
#define ALS_PRINTVAR_HEX PRINT_VARIABLE_HEX
#define ALS_DEBUG AWS_DEBUG
#define ALS_DEBUG_WRITE AWS_DEBUG_WRITE
#else
#define ALS_PRINT(...) do {} while (0)
#define ALS_PRINTLN(...) do {} while (0)
#define ALS_PRINTVAR(...) do {} while (0)
#define ALS_DEBUG(...) do {} while (0)
#define ALS_DEBUG_WRITE(...) do {} while (0)
#define ALS_PRINTVAR_HEX(...) do {} while (0)
#endif

namespace WeatherProcessing
{
  short SignExtendBitfield(unsigned short data, int width);
  void takeReading(short* sX, short* sY);
  constexpr byte alsAddress = 96;
  void read(byte regAddress, void* data, byte count);
  void write(byte regAddress, void* data, byte count);

  struct wdCalibStruct
  {
    char x, y;
  };
  wdCalibStruct wdCalibOffset;

#ifdef WIND_DIR_AVERAGING
  extern float curWindX, curWindY;
#endif


  void initWind()
  {
    byte data[4];
    bool doWriteEeprom = false;

    ALS_PRINTLN(F("Beginning wire..."));
    Wire_begin();
    ALS_PRINTLN(F("Wire Begun!"));

    if (doWriteEeprom)
    {     
      writeAlsEeprom();
    }

    //Set low power mode
    read(0x27, data, 4);
    data[3] = (data[3] & (~0x7F)) | 0x52; //Low Power / 10 samples/sec
    write(0x27, data, 4);

    ALS_PRINTLN(F("ALS Low power set"));

    GET_PERMANENT2(&wdCalibOffset, wdCalib1);
#if 0 // DEBUG_ALS
    byte zeroData[10] = { 0 };
    while (1)
    {
      read(0x02, zeroData, 4);
      short sX, sY;
      takeReading(&sX, &sY);
      ALS_DEBUG_WRITE((byte*)&sX, 2);
      ALS_DEBUG_WRITE((byte*)&sY, 2);
      ALS_DEBUG_WRITE(zeroData, 10);
      wdt_reset();
    }
#endif
#if 0
    delay(100);

    while(1)
    {
    byte addresses[] = {2, 3, 0xD, 0xE, 0xF, 0x27, 0x28, 0x29};
    for (int i = 0; i < sizeof(addresses); i++)
    {
      auto address = addresses[i];
      ALS_PRINTVAR(address);
      read(address, data, 4);
      for (int i = 0; i < 4; i++)
      {
        ALS_PRINT(data[i], HEX);
        ALS_PRINT(' ');
      }
      ALS_PRINTLN();
    }
    ALS_PRINTLN();
    ALS_PRINTLN();
    }
#endif
  }

  bool writeAlsEeprom()
  {
    byte data[4];
    // enable writing to the chip: 
    unsigned long customerAccessCode = 0x2C413534;
    write(0x35, &customerAccessCode, 4);
    read(0x02, data, 4);
    data[2] = 0x02; // Disable Z axis, 3V I2C
    data[3] = 0xC0; // Enable X and Y axes
    write(0x02, data, 4);
    return true;
  }

  void read(byte regAddress, void* data, byte count)
  {
    byte* dataB = (byte*)data;
    Wire_beginTransmission(alsAddress);
    ALS_PRINTLN(F("begun tx"));
    Wire_write(regAddress);
    ALS_PRINTLN(F("wrote"));
    Wire_endTransmission(false);
    ALS_PRINTLN(F("ended false"));
    Wire_requestFrom(alsAddress, count);
    ALS_PRINTLN(F("requested"));
    for (byte i = 0; i < count; i++)
    {
      dataB[i] = Wire_read();
      ALS_PRINTVAR_HEX(dataB[i]);
    }
    ALS_PRINTLN(F("read"));
    Wire_endTransmission(true);
    ALS_PRINTLN(F("ended true"));
  }

  void write(byte regAddress, void* data, byte count)
  {
    byte* dataB = (byte*)data;
    Wire_beginTransmission(alsAddress);
    ALS_PRINTLN(F("begun write tx"));
    Wire_write(regAddress);
    ALS_PRINTLN(F("register written"));
    for (byte i = 0; i < count; i++)
      Wire_write(dataB[i]);
    ALS_PRINTLN(F("data written"));
    Wire_endTransmission(true);
    ALS_PRINTLN(F("ended true"));
  }
  
  byte getCurWindDirection()
  {
    short sX, sY;
    takeReading(&sX, &sY);
    return atan2ToByte(sX - wdCalibOffset.x, sY - wdCalibOffset.y);
  }
  
  void doSampleWind()
  {
#ifdef WIND_DIR_AVERAGING
    short sX, sY;
    //AWS_DEBUG(auto entryMicros = micros());
    takeReading(&sX, &sY);
    //AWS_DEBUG(auto postRead = micros());
    curWindX += sX - wdCalibOffset.x;
    curWindY += sY - wdCalibOffset.y;
    //AWS_DEBUG(auto endMicros = micros());
    //PRINT_VARIABLE(postRead - entryMicros);
    //PRINT_VARIABLE(endMicros - postRead);
#endif
  }

  void calibrateWindDirection()
  {
    short sX, sY;
    long tX = 0,
      tY = 0;
    constexpr short sampleCount = 1000;

    //Set full power mode
    /*byte data[4];
    read(0x27, data, 4);
    data[3] = (data[3] & (~0x7F)); //Full active mode
    write(0x27, data, 4);*/

    for (short i = 0; i < sampleCount; i++)
    {
      takeReading(&sX, &sY);
      tX += sX;
      tY += sY;
      delay(1); 
    }

    //Set low power mode
    /*read(0x27, data, 4);
    data[3] = (data[3] & (~0x7F)) | 0x52; //Low Power / 10 samples/sec
    write(0x27, data, 4);*/

    tX /= sampleCount;
    tY /= sampleCount;
    if (tX > 127 || tX < -128 || tY > 127 || tY < -128)
    {
      SIGNALERROR();
      WX_PRINTLN("Calibration Failed!");
      return;
    }
    wdCalibOffset.x = tX;
    wdCalibOffset.y = tY;
    SET_PERMANENT2(&wdCalibOffset, wdCalib1);
  }

  //The ALS31313 hall effect sensor uses I2C to measure X, Y, and Z components of the magnetic field, 
  void takeReading(short* sX, short* sY)
  {
    byte data[8];
    read(0x28, data, 8);
    ALS_DEBUG_WRITE(data, 8);

    auto x = ((unsigned short)data[0] << 4) | (data[5] & 0x0F);
    auto y = ((unsigned short)data[1] << 4) | ((data[6] & 0xF0) >> 4);
  
    *sX = SignExtendBitfield(x, 12);
    *sY = SignExtendBitfield(y, 12);

    //ALS_TEMP should not be used because the processor doesn't properly report temperature in low power mode
#ifdef ALS_TEMP
    short t = (((unsigned short)data[3] & 0x3F) << 6) | (data[7] & 0x3F);
    ALS_DEBUG_WRITE((byte*)&t, 2);
    internalTemperature_x2 = 50 + (t - 0x800) / 4;
#endif
  }

  short SignExtendBitfield(unsigned short data, int width)
  {
    short x = (short)data;
    short mask = 1L << (width - 1);
    if (width < 16)
    {
        x = x & ((1 << width) - 1); // make sure the upper bits are zero
    }
    return ((x ^ mask) - mask);
  }
}
#endif