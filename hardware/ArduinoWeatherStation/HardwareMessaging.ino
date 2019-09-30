//Message is:
// (Leading null)(Callsign)[Data]
size_t readMessage(byte* msgBuffer, size_t bufferSize)
{
  size_t messageLoc = 0;
  size_t readLoc = 0;
  int data;
  bool escaped = false;
  unsigned long lastReadMillis = millis();
  const unsigned long timeout = 1000;
  for (data = Serial.read(); data != -1 || millis() - lastReadMillis < timeout; data = Serial.read())
  {
    if (data == -1)
      continue;
    lastReadMillis = millis();
    //Serial.println(data);
    //Check if we're at the end of a message
    if (data == FEND) {
      if (messageLoc == 0) { //If we haven't started to received data, ignore the FEND
        continue;
      } else if (escaped) { //A FEND following a FESC is invalid, ignore it.
        escaped = false;
      } else {
        //Serial.print("Returning by FEND. ");
        //Serial.println(messageLoc + 1);
        //Serial.write(msgBuffer, messageLoc + 1);
        return messageLoc; //We got a FEND mid message, return.
      }
    }

    //Check if we should become escaped
    if (data == FESC) {
      escaped = !escaped; //Two FESC in a row are invalid. Discard them.
      continue;
    }
    
    if (escaped){
      escaped = false;
      if (data == TFEND) {
        data = FEND;
      } else if (data == TFESC) {
        data = FESC;
      } else {
        continue; //FESC followed by anything but TFEND or TFESC is invalid. Discard it.
      }
    }
    
    //messages have a null at the start, and bytes 1-6 contain the sender's callsign
    if (readLoc <= 6) {
      readLoc++;
      continue;
    }
    readLoc++;
    //If we overrun the buffer, fail:
    if (messageLoc == bufferSize)
      return 0;
    msgBuffer[messageLoc++] = data;
  }
  //If we got here, we got no terminating FEND so return a failure:
  //Serial.println("Returning by -1");
  //Serial.write(msgBuffer, messageLoc);
  //Serial.println();
  return 0;
}

void sendMessage(byte* msgBuffer, size_t& messageLength, size_t bufferLength)
{
  if (kissPackageMessage(msgBuffer, messageLength, bufferLength)) {
    Serial.write(msgBuffer, messageLength);

    #if DEBUG
    //sendTestMessage();
    //Serial.println();
    #endif
  }
  else {
    Serial.println("kissPackageMessage Failed.");
    Serial.println(bufferLength);
    Serial.println(messageLength);
  }
  
  /*//Ensure we start on a new line
  Serial.println();
  Serial.print('!');
  for (int i = 0; i < messageLength; i++)
  {
    if (msgBuffer[i + messageStartsAt] == '\n')
      msgBuffer[i + messageStartsAt]--;
  }
  Serial.write(msgBuffer + messageStartsAt, messageLength);
  Serial.println();*/
}

//Prepares a message to be sent over the KISS protocol.
//Escapes all special characters as necessary, and appends a CRC
//Parameters:
//msgBuffer: A buffer that contains the message to be sent. The data should start on msgBuffer[8]. This will be modified in place.
//msgLen: The length of the message. IN: The length of the data. OUT: The total length of the message.
//bufferSize: The maximum buffer size.
//Returns true if it can successfully encode the message, otherwise returns false.
bool kissPackageMessage(byte* msgBuffer, size_t& msgLen, const int bufferSize)
{  
  if (bufferSize < minMessageSize)
    return false;
  //Minimum 9 extra bytes: Start FEND, 0x00, CALLSIGN, end FEND.
  if (msgLen > bufferSize - extraMessageBytes)
    return false;

  //Put a FEND + 0x00 at the start.
  msgBuffer[0] = FEND;
  msgBuffer[1] = 0x00;
  msgLen += 2;

  //Put in our callsign
  memcpy(msgBuffer + 2, callSign, 6);
  msgLen += 6;

  //Escape everything:
  for (int i = 2; i < msgLen; i++)
  { 
    //If we encounter a special byte
    if (msgBuffer[i] == FEND || msgBuffer[i] == FESC)
    {
      msgLen++;
      if (msgLen > bufferSize - 1)
        return false;
        
      //Move everthing after the byte.
      memmove(msgBuffer + i + 1, msgBuffer + i + 2, msgLen - i);

      //Escape the byte
      if (msgBuffer[i] == FEND)
      {
        msgBuffer[i] = FESC;
        msgBuffer[i + 1] = TFEND;
      }
      else
        msgBuffer[i + 1] = TFESC;
      //make sure we don't check it again
      i++;
    }
  }
  //Ensure the message is large enough to make the Radioshield send it:
  if (msgLen < minMessageSize - 1) 
  {
    memset(msgBuffer + msgLen, 0, minMessageSize - msgLen);
    msgLen = minMessageSize - 1;
  }
  
  msgBuffer[msgLen] = FEND;
  msgLen++;
  return true;
}
