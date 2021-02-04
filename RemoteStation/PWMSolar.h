namespace PwmSolar
{
  extern unsigned short chargeResponseRate;
  extern unsigned short safeFreezingChargeLevel_mV;
  extern byte safeFreezingPwm;
  extern unsigned short chargeVoltage_mV;
  extern short curCurrent_mA_x6;
  extern byte solarPwmValue;
  extern unsigned short lastCurrentCheckMillis;

  extern short debug_desired;
  extern bool debug_applyLimits;
  extern short debug_change;
  extern unsigned short debug_curMillis;

  void setupPwm();
  void doPwmLoop();
}