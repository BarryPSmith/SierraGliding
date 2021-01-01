#ifdef ALS_WIND
#include "WeatherProcessing.h"
#include "Wind.h"
#include <Wire.h>
namespace WeatherProcessing
{
  short SignExtendBitfield(unsigned short data, int width);
  void takeReading(short* sX, short* sY);
  constexpr byte alsAddress = 96;
  void read(byte regAddress, void* data, byte count);
  void write(byte regAddress, void* data, byte count);

#ifdef WIND_DIR_AVERAGING
  extern float curWindX, curWindY;
#endif

  constexpr byte GetWireClock(long desiredSpeed)
  {
    //FClock = F_CPU / (16 + 2 * TWBR * Prescaler)
    //TWBR = (F_CPU / FClock - 16) / 2
    auto test = (long)F_CPU / desiredSpeed - 16;
    if (test < 2)
      return 1;
    else if (test < 512)
      return test / 2;
    else
      return 255;
  }

  void initWind()
  {
    byte data[4];
    bool doWriteEeprom = false;

    Wire.begin();
    TWBR = GetWireClock(100000);
    
    //WX_PRINTVAR(TWBR);
    //WX_PRINTVAR(TWSR);
   
    if (doWriteEeprom)
    {     
      writeAlsEeprom();
    }

    //Set low power mode
    read(0x27, data, 4);
    data[3] = (data[3] & (~0x7F)) | 0x52; //Low Power / 10 samples/sec
    write(0x27, data, 4);

#if 0
    delay(100);

    while(1)
    {
    byte addresses[] = {2, 3, 0xD, 0xE, 0xF, 0x27, 0x28, 0x29};
    for (int i = 0; i < sizeof(addresses); i++)
    {
      auto address = addresses[i];
      PRINT_VARIABLE_HEX(address);
      read(address, data, 4);
      for (int i = 0; i < 4; i++)
      {
        AWS_DEBUG_PRINT(data[i], HEX);
        AWS_DEBUG_PRINT(' ');
      }
      AWS_DEBUG_PRINTLN();
    }
    AWS_DEBUG_PRINTLN();
    AWS_DEBUG_PRINTLN();
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
    data[1] = 0xC0; // Enable X and Y axes
    write(0x02, data, 4);
    return true;
  }

  void read(byte regAddress, void* data, byte count)
  {
    byte* dataB = (byte*)data;
    Wire.beginTransmission(alsAddress);
    Wire.write(regAddress);
    Wire.endTransmission(false);
    Wire.requestFrom(alsAddress, count);
    for (byte i = 0; i < count; i++)
      dataB[i] = Wire.read();
  }

  void write(byte regAddress, void* data, byte count)
  {
    byte* dataB = (byte*)data;
    Wire.beginTransmission(alsAddress);
    Wire.write(regAddress);
    for (byte i = 0; i < count; i++)
      Wire.write(dataB[i]);
    Wire.endTransmission();
  }
  
  byte getCurWindDirection()
  {
    short sX, sY;
    takeReading(&sX, &sY);
    return atan2ToByte(sX, sY);
  }
  
  void doSampleWind()
  {
#ifdef WIND_DIR_AVERAGING
    short sX, sY;
    //AWS_DEBUG(auto entryMicros = micros());
    takeReading(&sX, &sY);
    //AWS_DEBUG(auto postRead = micros());
    curWindX += sX;
    curWindY += sY;
    //AWS_DEBUG(auto endMicros = micros());
    //PRINT_VARIABLE(postRead - entryMicros);
    //PRINT_VARIABLE(endMicros - postRead);
#endif
  }

  //The ALS31313 hall effect sensor uses I2C to measure X, Y, and Z components of the magnetic field, 
  void takeReading(short* sX, short* sY)
  {
    byte data[8];
    read(0x28, data, 8);

    auto x = ((unsigned short)data[0] << 4) | (data[4] & 0x0F);
    auto y = ((unsigned short)data[1] << 4) | ((data[5] & 0xF0) >> 4);
  
    *sX = SignExtendBitfield(x, 12);
    *sY = SignExtendBitfield(y, 12);

#ifdef ALS_TEMP
    unsigned short t = (((unsigned short)data[3] & 0x3F) << 6) | (data[7] & 0x3F);
    externalTemperature = 25 + (t - 0x800) / 8.0;
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