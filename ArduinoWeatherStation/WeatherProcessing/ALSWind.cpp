#ifdef ALS_WIND
#include "WeatherProcessing.h"
#include "Wind.h"
#include <Wire.h>
namespace WeatherProcessing
{
  short SignExtendBitfield(unsigned short data, int width);
  void takeReading(short* sX, short* sY);
  constexpr byte alsAddress = 96;
  void read(byte regAddress, byte* data, byte count);
  void write(byte regAddress, byte* data, byte count);

#ifdef WIND_DIR_AVERAGING
  extern float curWindX, curWindY;
#endif

  void initWind()
  {
    Wire.begin();
    Wire.setClock(100000); //100kHz

    // enable writing to the chip:
    int customerAccessCode = 0x2C413534;
    write(0x35, &customerAccessCode, 4);

    //Set low power mode
    byte data[4];
    read(0x27, data, 4);
    data[3] = (data[3] & (~0x73)) | 0x42; //Low Power / 20 samples/sec
    write(0x27, data, 4);
    
    //Disable Z axis:
    read(0x02, data, 4);
    data[2] = (data[2] & (~0x01));
    write(0x02, data, 4);
  }

  void read(byte regAddress, byte* data, byte count)
  {
    Wire.beginTransmission(alsAddress);
    Wire.write(regAddress);
    Wire.endTransmission();
    Wire.requestFrom(alsAddress, count);
    for (byte i = 0; i < count; i++)
      data[i] = Wire.read();
  }

  void write(byte regAddress, byte* data, byte count)
  {
    Wire.beginTransmission(alsAddress);
    Wire.write(regAddress);
    for (byte i = 0; i < count; i++)
      Wire.write(data[i]);
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
    takeReading(&sX, &sY);
    curWindX += sX;
    curWindY += sY;
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