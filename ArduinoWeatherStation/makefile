#COM port for upload
PORT=COM4
PORT_MODEM=COM15
#Hardware spects
MCU=atmega328p
#115200

uniq = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

#Boards:
# 0: Mt Tom Rev 1 (Not implemented)
# 1: Breadboard Modem
# 2: Moteino Modem
# 3: Flynns Rev 1
# 4: Flynns Rev 2
BOARD = 1
#make / nmake: !INCLUDE
include Boards.makefile

#Flags, etc.
DEFINES=$(BOARD_DEFINES) -DARDUINO=100
CFLAGS= -mcall-prologues -mmcu=$(MCU) -Os -fdata-sections -ffunction-sections -fno-exceptions -flto -std=c++17 -g
LDFLAGS=-Wl,--gc-sections -Wl,--relax
CC=avr-gcc

# External libraries
#ARDUINO_DIR=../../ArduinoCore-avr/cores/arduino
#/Program Files (x86)/Arduino/hardware/arduino/avr/cores/arduino
INCLUDES=-I../../ArduinoCore-avr/cores/arduino \
		 -I../../ArduinoCore-avr/variants/standard \
		 -I../../ArduinoCore-avr/libraries/SPI/src \
		 -I../../ArduinoCore-avr/libraries/SoftwareSerial/src \
		 -Ilib/RadioLib/src \
		 -Ilib/Low-Power \
		 -Ilib/RadioLib/src
#		 -Ilib/TimerOne 
ARDUINO_BASE_LIBS=^/wiring_analog.c \
			^/wiring_digital.c ^/winterrupts.c ^/hooks.c \
			^/print.cpp ^/WMath.cpp

#Serial is included in modem and debug builds:
ifeq ($(or $(DEBUG), $(MODEM)), 1) 
ARDUINO_BASE_LIBS += ^/HardwareSerial.cpp ^/HardwareSerial0.cpp
endif

ifeq ($(DEBUG), 1)
DEFINES += -DDEBUG
endif
ifeq ($(MODEM), 1)
DEFINES += -DMODEM
endif

ARDUINO_LIBS=^/SPI/src/spi.cpp
ARDUINO_LIBSWDIRS = \
				$(patsubst ^/%, ../../ArduinoCore-avr/cores/arduino/%, $(ARDUINO_BASE_LIBS)) \
				$(patsubst ^/%, ../../ArduinoCore-avr/libraries/%, $(ARDUINO_LIBS))

RADIO_LIB=lib/RadioLib/src/Module.cpp lib/RadioLib/src/modules/SX126x/SX126x.cpp \
		lib/RadioLib/src/modules/SX126x/SX1262.cpp lib/RadioLib/src/modules/RF69/RF69.cpp \

LIBRARIES=lib/Low-Power/LowPower.cpp $(RADIO_LIB) $(ARDUINO_LIBSWDIRS)

OBJFOLDER = obj
ifeq ($(MODEM), 1)
SRCS_C = KissModem.cpp MessagingCommon.cpp LoraMessaging.cpp KissMessaging.cpp \
			../../ArduinoCore-avr/cores/arduino/wiring.c \
			PermanentStorage.cpp stackCanary.cpp \
			lib/RadioLib/src/modules/SX127x/SX1278.cpp \
			lib/RadioLib/src/modules/SX127x/SX127x.cpp \
			$(LIBRARIES)
OBJFOLDER := $(OBJFOLDER)_mod
else
SRCS_C = ArduinoWeatherStation.cpp Commands.cpp MessageHandling.cpp \
		 LoraMessaging.cpp WeatherProcessing.cpp MessagingCommon.cpp \
		delay.c millis.cpp TimerTwo.cpp PermanentStorage.cpp stackCanary.cpp \
		 $(LIBRARIES)
endif

SRCS_H = *.h
OBJS1	= $(SRCS_C:.cpp=.o)

ifeq ($(DEBUG), 1)
OBJFOLDER := $(OBJFOLDER)_debug
endif

OBJS2	= $(OBJS1:.c=.o)
OBJS	= $(addprefix $(OBJFOLDER)/, $(notdir $(OBJS2)))
FOLDERS = $(call uniq, $(dir $(SRCS_C)))
$(info Folders: $(FOLDERS))

VPATH = $(FOLDERS)


all: $(OBJFOLDER)/all.hex

clean:
	rm -rf *.o *.hex *.obj *.elf

obj:
	mkdir $(OBJFOLDER)
	
%.hex: %.obj
	avr-objcopy -R .eeprom -O ihex $< $@
	echo INFO : $@ Built

checkSize: all.elf
	avr-size all.elf
	avr-size all.elf

all.elf: $(SRCS_C) $(SRCS_H)
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

revid:
	gitver.cmd

$(OBJFOLDER)/%.o: %.c $(SRCS_H) | obj
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) -c $< -o $(OBJFOLDER)/$(notdir $@)

$(OBJFOLDER)/%.o: %.cpp $(SRCS_H) | obj
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) -c $< -o $(OBJFOLDER)/$(notdir $@)

$(OBJFOLDER)/all.obj: $(OBJS)
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

program: $(OBJFOLDER)/all.hex
	avrdude -c arduino -p $(MCU) -P $(PORT) -b $(PROG_BAUD) -U all.hex
	ECHO INFO : All done
	
