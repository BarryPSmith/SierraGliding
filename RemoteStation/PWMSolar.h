namespace PwmSolar
{
  extern unsigned short chargeResponseRate;
  extern unsigned short safeFreezingChargeLevel_mV;
  extern byte safeFreezingPwm;
  extern unsigned short chargeVoltage_mV;
  extern short curCurrent_mA_x6;
  extern byte solarPwmValue;

  void setupPwm();
  void doPwmLoop();
}