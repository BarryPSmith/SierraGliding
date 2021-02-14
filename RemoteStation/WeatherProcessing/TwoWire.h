typedef unsigned char byte;
byte Wire_read();
void Wire_endTransmission(bool close);
void Wire_requestFrom(byte address, byte count);
void Wire_beginTransmission(byte address);
void Wire_write(byte data);
void Wire_begin();
