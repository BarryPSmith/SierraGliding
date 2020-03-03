#COM port for upload
PORT=COM4
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
# 5: Dorji Modem
# 6: First attempt at a printed PCB
DEBUG = 1
BOARD = 1
include Boards.makefile

#Flags, etc.
DEFINES=$(BOARD_DEFINES) -DARDUINO=100 #-DRADIOLIB_DEBUG 
#-DRADIOLIB_VERBOSE
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
		 -Ilib/RadioLib/src \
		 -Ilib/SPIFlash
#		 -Ilib/TimerOne 
ARDUINO_BASE_LIBS=^/wiring_analog.c \
			^/wiring_digital.c ^/WInterrupts.c ^/hooks.c \
			^/Print.cpp ^/WMath.cpp

#Serial is included in modem and debug builds:
ifeq ($(DEBUG), 1)
ARDUINO_BASE_LIBS += ^/HardwareSerial.cpp ^/HardwareSerial0.cpp
DEFINES += -DDEBUG
else ifeq ($(MODEM), 1)
ARDUINO_BASE_LIBS += ^/HardwareSerial.cpp ^/HardwareSerial0.cpp
endif

ifeq ($(MODEM), 1)
DEFINES += -DMODEM
endif

ARDUINO_LIBS=^/SPI/src/SPI.cpp
ARDUINO_LIBSWDIRS = \
				$(patsubst ^/%, ../../ArduinoCore-avr/cores/arduino/%, $(ARDUINO_BASE_LIBS)) \
				$(patsubst ^/%, ../../ArduinoCore-avr/libraries/%, $(ARDUINO_LIBS))

RADIO_LIB=lib/RadioLib/src/Module.cpp lib/RadioLib/src/modules/SX126x/SX126x.cpp \
		lib/RadioLib/src/modules/SX126x/SX1262.cpp lib/RadioLib/src/modules/RF69/RF69.cpp \

LIBRARIES=lib/Low-Power/LowPower.cpp lib/SPIFlash/SPIFlash.cpp $(RADIO_LIB) $(ARDUINO_LIBSWDIRS)

OBJFOLDER := $(OBJFOLDER)obj
ifeq ($(MODEM), 1)
SRCS_C = KissModem.cpp MessagingCommon.cpp LoraMessaging.cpp KissMessaging.cpp \
			../../ArduinoCore-avr/cores/arduino/wiring.c \
			PermanentStorage.cpp StackCanary.cpp \
			$(LIBRARIES)
OBJFOLDER := $(OBJFOLDER)_mod
else
SRCS_C = ArduinoWeatherStation.cpp Commands.cpp MessageHandling.cpp \
		 LoraMessaging.cpp WeatherProcessing.cpp MessagingCommon.cpp \
		 delay.c millis.cpp TimerTwo.cpp PermanentStorage.cpp StackCanary.cpp \
		 RemoteProgramming.cpp \
		 $(LIBRARIES)
endif

OBJS1	= $(SRCS_C:.cpp=.o)

ifeq ($(DEBUG), 1)
OBJFOLDER := $(OBJFOLDER)_debug
endif

OBJS2	= $(OBJS1:.c=.o)
OBJS	= $(addprefix $(OBJFOLDER)/, $(notdir $(OBJS2)))
FOLDERS = $(call uniq, $(dir $(SRCS_C)))
$(info Folders: $(FOLDERS))

VPATH = $(FOLDERS)

.SUFFIXES:


all: $(OBJFOLDER)/all.hex

clean:
	del /S obj/*.o obj/*.hex obj/*.obj obj/*.elf
	del /S obj_mod/*.o obj_mod/*.hex obj_mod/*.obj obj_mod/*.elf
	del /S obj_debug/*.o obj_debug/*.hex obj_debug/*.obj obj_debug/*.elf
	del /S obj_mod_debug/*.o obj_mod_debug/*.hex obj_mod_debug/*.obj obj_mod_debug/*.elf
#rm -rf *.o *.hex *.obj *.elf

$(OBJFOLDER):
	mkdir $(OBJFOLDER)

saved:
	mkdir saved
	
%.hex: %.obj
	avr-objcopy -R .eeprom -O ihex $< $@

%.s: %.obj
	avr-objdump -S $< > $@

checkSize: $(OBJFOLDER)/all.elf
	avr-size $(OBJFOLDER)/all.elf

$(OBJFOLDER)/all.elf: $(OBJS)
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

revid:
	gitver.cmd

#TODO: Dependency bullshit.

$(OBJFOLDER)/%.o: %.c *.h | $(OBJFOLDER)
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) -c $< -o $(OBJFOLDER)/$(notdir $@)

$(OBJFOLDER)/%.o: %.cpp *.h | $(OBJFOLDER)
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) -c $< -o $(OBJFOLDER)/$(notdir $@)

$(OBJFOLDER)/all.obj: $(OBJS)
	$(CC) $(INCLUDES) $(DEFINES) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@


program: $(OBJFOLDER)/all.hex $(OBJFOLDER)/all.s
	avrdude -c arduino -p $(MCU) -P $(PORT) -b $(PROG_BAUD) -U $(OBJFOLDER)/all.hex
	ECHO INFO : All done

.PHONY: save_outputs
save_outputs: $(OBJFOLDER)/all.hex $(OBJFOLDER)/all.s | saved
	save_outputs.cmd $(OBJFOLDER)