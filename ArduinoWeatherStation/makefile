#COM port for upload
PORT=COM11
PORT_MODEM=COM7
#Hardware spects
MCU=atmega328p
F_CPU=16000000L
PROG_BAUD=115200
#115200

#Flags, etc.
DEFINES=-DF_CPU=$(F_CPU) -DARDUINO=100
# We can put in these defines to save ~100 bytes of serial buffer memory if we need it.
#-DSERIAL_RX_BUFFER_SIZE=8 -DSERIAL_TX_BUFFER_SIZE=16
DEFINES_MODEM=-DF_CPU=$(F_CPU) -DARDUINO=100 -DMODEM -DMOTEINO_96 -DDEBUG
CFLAGS= -mcall-prologues -mmcu=$(MCU) -Os -fdata-sections -ffunction-sections -fno-exceptions -flto -std=c++17 -g
LDFLAGS=-Wl,--gc-sections -Wl,--relax
CC=avr-gcc

# External libraries
#ARDUINO_DIR=../../ArduinoCore-avr/cores/arduino
#C:/Program Files (x86)/Arduino/hardware/arduino/avr/cores/arduino
INCLUDES=-I"C:\Program Files (x86)\Arduino\hardware\arduino\avr\cores\arduino" \
         -I"C:\Program Files (x86)\Arduino\hardware\arduino\avr\variants\standard" \
         -I"C:/Program Files (x86)/Arduino/hardware/Arduino/avr/libraries/SPI/src" \
         -I"C:/Program Files (x86)/Arduino/hardware/Arduino/avr/libraries/SoftwareSerial/src" \
         -I"lib/RadioLib/src" \
         -I"lib/Low-Power" \
         -I"lib/RadioLib/src"
#         -I"lib/TimerOne" 
ARDUINO_BASE_LIBS="^/wiring_analog.c" \
            "^/wiring_digital.c" "^/winterrupts.c" "^/hooks.c" \
            "^/print.cpp" "^/WMath.cpp"
#"^/wiring.c" "^/new.cpp" "^/abi.cpp"

#Serial is included in modem and debug builds:
!IF DEFINED(DEBUG)
ARDUINO_BASE_LIBS = $(ARDUINO_BASE_LIBS) "^/HardwareSerial.cpp" "^/HardwareSerial0.cpp"
DEFINES = $(DEFINES) -DDEBUG
!ELSEIF DEFINED(MODEM)
ARDUINO_BASE_LIBS = $(ARDUINO_BASE_LIBS) "^/HardwareSerial.cpp" "^/HardwareSerial0.cpp"
!ENDIF

ARDUINO_LIBS="^/SPI/src/spi.cpp"
ARDUINO_LIBSWDIRS = \
                $(ARDUINO_BASE_LIBS:^^/=../../ArduinoCore-avr/cores/arduino/) \
                $(ARDUINO_LIBS:^^/=../../ArduinoCore-avr/libraries/)
# If they're in Program Files, use this.
#$(ARDUINO_BASE_LIBS:^^/=C:/Program Files (x86^)/Arduino/hardware/arduino/avr/cores/arduino/) \
#$(ARDUINO_LIBS:^^/=C:/Program Files (x86^)/Arduino/hardware/arduino/avr/libraries/)

RADIO_LIB=lib/RadioLib/src/Module.cpp lib/RadioLib/src/modules/SX126x/SX126x.cpp \
        lib/RadioLib/src/modules/SX126x/SX1262.cpp lib/RadioLib/src/modules/RF69/RF69.cpp \
#        lib/RadioLib/src/protocols/PhysicalLayer/PhysicalLayer.cpp

LIBRARIES=lib/Low-Power/LowPower.cpp $(RADIO_LIB) $(ARDUINO_LIBSWDIRS)

#Source files (Main)
SRCS_C = ArduinoWeatherStation.cpp Commands.cpp MessageHandling.cpp \
         LoraMessaging.cpp SleepyTime.cpp WeatherProcessing.cpp MessagingCommon.cpp \
		delay.c millis.cpp TimerTwo.cpp PermanentStorage.cpp stackCanary.cpp \
         $(LIBRARIES)
#Source files (modem)
SRCS_MOD = KissModem.cpp MessagingCommon.cpp LoraMessaging.cpp KissMessaging.cpp \
            "C:/Program Files (x86)/Arduino/hardware/arduino/avr/cores/arduino/wiring.c" \
            PermanentStorage.cpp stackCanary.cpp \
            lib/RadioLib/src/modules/SX127x/SX1278.cpp \
            lib/RadioLib/src/modules/SX127x/SX127x.cpp \
            $(LIBRARIES)
SRCS_H = *.h
OBJS    = $(SRCS_C:.c=.o)

.SUFFIXES : .elf .obj .hex

all: all.hex

clean:
	rm -f *.o *.hex *.obj *.elf
	
.obj.hex:
	avr-objcopy -R .eeprom -O ihex $< $@
    echo INFO : $@ Built

.elf.hex:
    avr-objcopy -R .eeprom -O ihex $< $@

checkSize: all.elf
	avr-size all.elf

all.elf: $(SRCS_C) $(SRCS_H)
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) $(SRCS_C) $(LDFLAGS) -o $@

revid:
	gitver.cmd

all.obj: $(SRCS_C) $(SRCS_H) revid
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) $(SRCS_C) $(LDFLAGS) -o $@

program: all.hex
	avrdude -c arduino -p $(MCU) -P $(PORT) -b $(PROG_BAUD) -U all.hex
    ECHO INFO : All done

#---------------------------------------
	
modem.obj: $(SRCS_MOD) $(SRCS_H) revid
    $(CC) $(INCLUDES) $(DEFINES_MODEM) $(CFLAGS) $(SRCS_MOD) $(LDFLAGS) -o $@

program_modem: modem.hex
    avrdude -c arduino -p $(MCU) -P $(PORT_MODEM) -b $(PROG_BAUD) -U modem.hex
    ECHO INFO : All done

modem.elf: $(SRCS_MOD) $(SRCS_H) revid
    $(CC) $(INCLUDES) $(DEFINES_MODEM) $(CFLAGS) $(SRCS_MOD) $(LDFLAGS) -o $@

check_modem_size: modem.elf
    avr-size modem.elf

modem_disassembly: $(SRCS_MOD) $(SRCS_H) revid
    $(CC) $(INCLUDES) $(DEFINES_MODEM) $(CFLAGS) $(SRCS_MOD) $(LDFLAGS) -o $@