#ifndef _ERRORCODES_H
#define _ERRORCODES_H

// Radiolib uses -1 to -1000

//Used by CSMA:
constexpr short NO_PACKET_AVAILABLE = -1000;
constexpr short NOT_ENOUGH_SPACE = -1001;
constexpr short REENTRY_NOT_SUPPORTED = -1002;
constexpr short CSMA_POINTER_INVERSION = -1003;
constexpr short CSMA_UNEXPECTED_ZERO_LENGTH = -1004;

constexpr short WX_CALIBRATION_FAILED = 0x11;
constexpr short WX_CALIBRATION_FAILED_BAD_VOLTAGE = 0x12;

constexpr short REMOTE_PROGRAM_INITALISATION_FAILURE = 0x21;

constexpr short RESTARTING = 0x31;

constexpr short DATABASE_UNKNOWN_CUR_ACTION = 0x41;

constexpr short RADIO_STARTUP_ERROR = 0x555;

constexpr short ALTERNATE_FLASH = 0xAAA;
constexpr short SYNC_FLASH = 0x555;
constexpr short SOLID = 0xFFF;
constexpr short OFF = 0x000;

constexpr short MESSAGING_CANNOT_READ_HEADER = 0x61;
constexpr short MESSAGING_UNKNOWN_TYPE = 0x62;
constexpr short MESSAGING_CANT_READ_CALLSIGN = 0x63;
constexpr short MESSAGING_CALLSIGN_MISMATCH = 0x64;
#endif