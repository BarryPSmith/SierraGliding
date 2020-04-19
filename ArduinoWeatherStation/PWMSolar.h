namespace PwmSolar
{
  extern unsigned short chargeResponseRate;
  extern unsigned short safeFreezingChargeLevel_mV;
  extern byte safeFreezingPwm;
  extern unsigned short chargeVoltage_mV;

  void setupPwm();
  void doPwmLoop();
}