#include "Database.h"
#include "Flash.h"
#include "TimerTwo.h"
#include "LoraMessaging.h"
#include "MessageHandling.h"

// Total available memory is 512kB
// At 256 bytes per message, could store 2k messages

// Fat table:
// Every 4k sector will have:
// MSG (Cur Cycle: 1 byte) - 4 bytes
// Message records (12b each) - 341 per sector
// With 6 sectors, we can store 2046 messages.
// (Maximum requirement of 523 776 bytes to store all those messages)

extern void* operator new(size_t size, void* ptr);

namespace Database
{
  struct MessageRecord
  {
    byte messageType; // 1
    byte stationID; // 1
    unsigned long address; // 4
    byte length; // 1
    unsigned long timestamp; // 4
    bool overwritten; // 1
  };

  constexpr unsigned long totalMemory = 512ul * 1024;

  constexpr short blockSize = 4096;
  constexpr unsigned long messageFatStart = 32768;
  constexpr unsigned short messageFatLen = blockSize * 6;
  constexpr unsigned long messageFatEnd = messageFatStart + messageFatLen;
  constexpr unsigned long messageDataStart = messageFatEnd;
  constexpr unsigned long messageDataLen = totalMemory - messageDataStart;
  constexpr unsigned long messageDataEnd = messageDataStart + messageDataLen;
  constexpr unsigned short maxProcessingTime = 1000;
  constexpr unsigned short minMessageInterval = 1000;

  const char MSG[] = { 'M', 'S', 'G' };

  constexpr byte maxMesagesToRead = 256 / sizeof(MessageRecord);
  constexpr byte recordBufferSize = maxMesagesToRead * sizeof(MessageRecord);
  
  unsigned long findCurrentBlock();
  bool checkEndsOfBlocks(byte byteCount);
  byte getHeaderChunk(void* buffer);
  MESSAGE_RESULT appendRecord(const MessageRecord& record, const unsigned short address);
  void doSearch();
  void startSearch(byte typeFilter, byte sourceFilter);
  void prepSearchMessage();
  void endSearchMessage();
  void endSearch();
  bool retrieveMessage(const unsigned short headerAddress,
    byte uniqueID);
  void sendSearchMessage();

  unsigned long _curHeaderAddress;
  unsigned long _curWriteAddress = messageDataStart;
  byte _curCycle = 0;
  bool _databaseOK = false;

  unsigned long _curSearchAddress;
  byte _messageTypeFilter;
  byte _messageSourceFilter;
  byte _outgoingMessageBuffer[sizeof(LoraMessageDestination<254>)];
  LoraMessageDestination<254>* _searchMessage;
  unsigned long _lastSearchMessage = 0;


  void initDatabase()
  {
    MessageRecord buffer[maxMesagesToRead];
    unsigned long blockStart = findCurrentBlock();
    unsigned long curAddress = blockStart + (sizeof(MSG) + 1);
    unsigned long blockEnd = blockStart + blockSize;
    for (; curAddress += sizeof(buffer); curAddress < blockEnd)
    {
      byte readSize = sizeof(buffer);
      unsigned long endAddress = curAddress + readSize;
      if (endAddress > blockEnd)
        readSize = blockEnd - curAddress;
      Flash::flash.readBytes(curAddress, buffer, readSize);
      MessageRecord* record = buffer;
      bool endFound = false;
      for (; record++; record < buffer + readSize / sizeof(MessageRecord)) 
      {
        if (record->messageType == 0)
        {
          endFound = true;
          break;
        }
      }
      byte recordsRead = record - buffer;
      if (recordsRead > 0)
      {
        record--;
        _curWriteAddress = record->address + record->length;
      }
      _curHeaderAddress = curAddress + recordsRead * sizeof(MessageRecord);
      if (endFound)
        break;
    }
    _databaseOK = true;
  }

  unsigned long findCurrentBlock()
  {
    bool lastBlockInitialised = true;
    byte buffer[sizeof(MSG) + 1];
    unsigned long blockStart = messageFatStart;
    for (; blockStart < messageFatEnd; blockStart += blockSize)
    {
      Flash::flash.readBytes(blockStart, buffer, sizeof(MSG) + 1);
      lastBlockInitialised = memcmp(buffer, MSG, sizeof(MSG)) == 0;
      if (!lastBlockInitialised)
      {
        //We've found an uninitialised block. The previous block was the active block.
        break;
      }
      byte blockCycle = buffer[sizeof(MSG)];
      if (blockStart != messageFatStart && blockCycle != _curCycle + 1)
      {
        //We've found a block that doesn't fit. The previous block was the active block
        break;
      }
      else
        _curCycle = blockCycle;
    }
    if (blockStart > messageFatStart)
      blockStart -= blockSize;
    else if (!lastBlockInitialised) // initialise the first block
    {
      byte initBuffer[] = {'M', 'S', 'G', 0 };
      Flash::flash.writeBytes(blockStart, initBuffer, sizeof(initBuffer));
    }
    return blockStart;
  }

  void storeMessage(byte messageType, byte stationID,
    MessageSource& msg)
  {
    byte byteCount = msg.getMessageLength() - msg.getCurrentLocation();

    if (!checkEndsOfBlocks(byteCount))
      return;

    MessageRecord headerRecord = {
      .messageType = messageType,
      .stationID = stationID,
      .address = _curWriteAddress,
      .length = byteCount,
      .timestamp = TimerTwo::seconds(),
      .overwritten = false
    };

    Flash::flash.writeBytes(_curHeaderAddress, &headerRecord, sizeof(headerRecord));
    _curHeaderAddress += sizeof(headerRecord);

    byte* buffer;
    msg.readBytes(&buffer, byteCount);
    Flash::flash.writeBytes(_curWriteAddress, buffer, byteCount);
    _curWriteAddress += byteCount;
  }

  bool checkEndsOfBlocks(byte messageSize)
  {
    //TODO: Implement some sort of circular buffer logic where it just starts overwriting once it's done.
    unsigned long endHeaderAddress = _curHeaderAddress + sizeof(MessageRecord);
    if (endHeaderAddress >= messageFatEnd)
      return false;
    unsigned long endMessageAddress = _curWriteAddress + messageSize;
    if (endMessageAddress >= messageDataEnd)
      return false;

    unsigned short endInBlock = endHeaderAddress % blockSize;
    if (endInBlock < sizeof(MessageRecord))
    {
      _curHeaderAddress = endHeaderAddress - endInBlock;
      byte initBuffer[] = {'M', 'S', 'G', ++_curCycle };
      Flash::flash.writeBytes(_curHeaderAddress, initBuffer, sizeof(initBuffer));
      _curHeaderAddress += sizeof(initBuffer);
    }

    endInBlock = endMessageAddress % blockSize;
    if (endInBlock < messageSize)
      _curWriteAddress = endMessageAddress - endInBlock;

    return true;
  }

  enum class ProcessingActions
  {
    Idle, Searching, Sending
  };
  ProcessingActions _currentAction = ProcessingActions::Idle;
  void doProcessing()
  {
    if (!_databaseOK)
      return;
    switch (_currentAction)
    {
    case ProcessingActions::Idle:
      return;
    case ProcessingActions::Searching:
      doSearch();
      break;
    case ProcessingActions::Sending:
      sendSearchMessage();
    }
  }

  void doSearch()
  {
    if (!_searchMessage)
      return;

    unsigned long entryMillis = millis();
    MessageRecord buffer[maxMesagesToRead];
    while (1)
    {
      if (millis() - entryMillis > maxProcessingTime)
        return;
      byte bytesRead = getHeaderChunk(buffer);
      unsigned short baseAddress = _curSearchAddress - bytesRead - messageFatStart;
      byte recordsRead = bytesRead / sizeof(MessageRecord);
      if (recordsRead == 0)
      {
        endSearch();
        return;
      }
      for (int i = 0; i < recordsRead; i++)
      {
        MessageRecord& record = buffer[i];
        if (record.messageType == 0)
        {
          endSearch();
          return;
        }
        if (_messageTypeFilter && record.messageType != _messageTypeFilter)
          continue;
        if (_messageSourceFilter && record.stationID != _messageSourceFilter)
          continue;
        if (record.overwritten)
          continue;
        unsigned short address = baseAddress + i * sizeof(MessageRecord);
        switch (appendRecord(record, address))
        {
        case MESSAGE_OK:
          continue;
        case MESSAGE_BUFFER_OVERRUN:
          _curSearchAddress = messageFatStart + address;
          _currentAction = ProcessingActions::Sending;
          return;
        default:
          SIGNALERROR();
          endSearch();
          return;
        }
      }
    }
  }

  MESSAGE_RESULT appendRecord(const MessageRecord& record, const unsigned short address)
  {
    MESSAGE_RESULT ok = _searchMessage->appendT(record.messageType); //1
    if (ok == MESSAGE_OK) ok = _searchMessage->appendT(record.stationID); //1
    if (ok == MESSAGE_OK) ok = _searchMessage->appendT(record.timestamp); //4
    if (ok == MESSAGE_OK) ok = _searchMessage->appendT(address); //2
    return ok;
  }

  byte getHeaderChunk(void* buffer)
  {
    unsigned short bytesToNextBlock = blockSize - _curSearchAddress % blockSize;
    if (bytesToNextBlock < sizeof(MessageRecord))
    {
      _curSearchAddress += bytesToNextBlock;
      if (_curSearchAddress >= messageFatEnd)
        return 0;
      byte blockHeaderBuffer[3];
      Flash::flash.readBytes(_curSearchAddress, blockHeaderBuffer, 3);
      if (memcmp(blockHeaderBuffer, MSG, 3))
        return 0;
      _curSearchAddress += 4;
      bytesToNextBlock = blockSize;
    }
    byte bytesToRead = recordBufferSize;
    if (bytesToRead > bytesToNextBlock)
      bytesToRead = (bytesToNextBlock / sizeof(MessageRecord)) * sizeof(MessageRecord);
    Flash::flash.readBytes(_curSearchAddress, buffer, bytesToRead);
    _curSearchAddress += bytesToRead;
    return bytesToRead;
  }

  void startSearch(byte typeFilter, byte sourceFilter)
  {
    _messageTypeFilter = typeFilter;
    _messageSourceFilter = sourceFilter;
    _curSearchAddress = messageFatStart + 4;
    _currentAction = ProcessingActions::Searching;
    prepSearchMessage();
    dbSleepEnabled = SleepModes::disabled;
  }

  void sendSearchMessage()
  {
    if (millis() - _lastSearchMessage < minMessageInterval)
    {
      dbSleepEnabled = SleepModes::powerSave;
      return;
    }
    endSearchMessage();
    prepSearchMessage();
    _lastSearchMessage = millis();
    _currentAction = ProcessingActions::Searching;
    dbSleepEnabled = SleepModes::disabled;
  }

  void prepSearchMessage()
  {
    _searchMessage = new (_outgoingMessageBuffer) LoraMessageDestination<254>(false);
    _searchMessage->appendByte('K');
    _searchMessage->appendByte(stationID);
    _searchMessage->appendByte(MessageHandling::getUniqueID());
    _searchMessage->appendByte('D');
    _searchMessage->appendByte('L');
  }

  void endSearchMessage()
  {
    if (!_searchMessage)
      return;
    _searchMessage->~LoraMessageDestination();
    _searchMessage = nullptr;
  }

  void endSearch()
  {
    endSearchMessage();
    _currentAction = ProcessingActions::Idle;
    dbSleepEnabled = SleepModes::powerSave;
  }

  bool handleDatabaseCommand(MessageSource& msg, const byte uniqueID,
    bool* ackRequired)
  {
    if (!_databaseOK)
      return false;
    byte cmdType;
    if (msg.readByte(cmdType))
      return false;
    switch (cmdType)
    {
    case 'L': //List
      {
        byte typeFilter = 0;
        byte sourceFilter = 0;
        if (msg.readByte(typeFilter) == MESSAGE_OK)
          msg.readByte(sourceFilter);
        startSearch(typeFilter, sourceFilter);
        return true;
      }
    case 'R': //Retrieve
      unsigned short address;
      if (msg.read(address))
        return false;
      *ackRequired = false;
      return retrieveMessage(address, uniqueID);
    }
    return false;
  }

  bool retrieveMessage(const unsigned short headerAddress,
    byte uniqueID)
  {
    unsigned long hAddress = messageFatStart + headerAddress;
    if (hAddress > messageFatEnd)
      return false;
    MessageRecord record;
    Flash::flash.readBytes(hAddress, &record, sizeof(record));
    if (record.messageType == 0 ||
        record.address < messageDataStart || record.address >= messageDataEnd)
      return false;
    LoraMessageDestination<254> dest(false);
    dest.appendByte('K');
    dest.appendByte(stationID);
    dest.appendByte(uniqueID);
    dest.appendByte('D');
    dest.appendByte('R');
    dest.appendByte(record.messageType);
    dest.appendByte(record.stationID);
    dest.appendByte(0);
    byte* buffer;
    byte maxSize = 254 - dest.getCurrentLocation();
    if (maxSize > record.length)
      maxSize = record.length;
    if (dest.getBuffer(&buffer, maxSize))
      return false;
    Flash::flash.readBytes(record.address, buffer, maxSize);
    return true;
  }
}