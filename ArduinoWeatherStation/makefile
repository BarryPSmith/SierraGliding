#COM port for upload
PORT=COM11
PORT_MODEM=COM15
#Hardware spects
MCU=atmega328p
F_CPU=16000000L
PROG_BAUD=57600
#115200

#Flags, etc.
DEFINES=-DF_CPU=$(F_CPU) -DARDUINO=100 
#-DDEBUG 
#-DRADIOLIB_DEBUG -DRADIOLIB_VERBOSE
# We can put in these defines to save ~100 bytes of serial buffer memory if we need it.
#-DSERIAL_RX_BUFFER_SIZE=8 -DSERIAL_TX_BUFFER_SIZE=16
DEFINES_MODEM=-DF_CPU=$(F_CPU) -DARDUINO=100 -DDEBUG
#-DMOTEINO_96
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
            "^/print.cpp" "^/WMath.cpp" "^/abi.cpp"
#"^/wiring.c" "^/new.cpp" 
!IF DEFINED(DEBUG)
ARDUINO_BASE_LIBS += "^/HardwareSerial.cpp" "^/HardwareSerial0.cpp"
DEFINES += -DDEBUG
!ENDIF

ARDUINO_LIBS="^/SPI/src/spi.cpp"
ARDUINO_LIBSWDIRS = \
                $(ARDUINO_BASE_LIBS:^^/=../../ArduinoCore-avr/cores/arduino/) \
                $(ARDUINO_LIBS:^^/=../../ArduinoCore-avr/libraries/)

#$(ARDUINO_BASE_LIBS:^^/=C:/Program Files (x86^)/Arduino/hardware/arduino/avr/cores/arduino/) \
#$(ARDUINO_LIBS:^^/=C:/Program Files (x86^)/Arduino/hardware/arduino/avr/libraries/)
RADIO_LIB=lib/RadioLib/src/Module.cpp lib/RadioLib/src/modules/SX126x/SX126x.cpp \
        lib/RadioLib/src/modules/SX126x/SX1262.cpp lib/RadioLib/src/modules/RF69/RF69.cpp \

#        lib/RadioLib/src/protocols/PhysicalLayer/PhysicalLayer.cpp
LIBRARIES=lib/Low-Power/LowPower.cpp $(RADIO_LIB) $(ARDUINO_LIBSWDIRS)
#lib/TimerOne/TimerOne.cpp

#Source files (Main)
SRCS_C = ArduinoWeatherStation.cpp Commands.cpp MessageHandling.cpp \
         Messaging.cpp SleepyTime.cpp WeatherProcessing.cpp MessagingCommon.cpp \
		delay.c millis.cpp TimerTwo.cpp PermanentStorage.cpp \
         $(LIBRARIES)
#Source files (modem)
SRCS_MOD = KissModem.cpp MessagingCommon.cpp Messaging.cpp KissMessaging.cpp \
            "C:/Program Files (x86)/Arduino/hardware/arduino/avr/cores/arduino/wiring.c" \
            PermanentStorage.cpp \
            lib/RadioLib/src/modules/SX1278.cpp \
            lib/RadioLib/src/modules/SX127x.cpp \
            $(LIBRARIES)
SRCS_H = *.h
OBJS    = $(SRCS_C:.c=.o)

.SUFFIXES : .elf .obj .hex

!IFDEF DEBUG
    
!ENDIF


all: all.hex

clean:
	rm -f *.o *.hex *.obj *.elf
	
.obj.hex:
	avr-objcopy -R .eeprom -O ihex $< $@
    echo $@ Built

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

#---------------------------------------
	
modem.obj: $(SRCS_MOD) $(SRCS_H) revid
    $(CC) $(INCLUDES) $(DEFINES_MODEM) $(CFLAGS) $(SRCS_MOD) $(LDFLAGS) -o $@

program_modem: modem.hex
    avrdude -c arduino -p $(MCU) -P $(PORT_MODEM) -b $(PROG_BAUD) -U modem.hex

modem.elf: $(SRCS_MOD) $(SRCS_H) revid
    $(CC) $(INCLUDES) $(DEFINES_MODEM) $(CFLAGS) $(SRCS_MOD) $(LDFLAGS) -o $@

check_modem_size: modem.elf
    avr-size modem.elf

modem_disassembly: $(SRCS_MOD) $(SRCS_H) revid
    $(CC) $(INCLUDES) $(DEFINES_MODEM) $(CFLAGS) $(SRCS_MOD) $(LDFLAGS) -o $@