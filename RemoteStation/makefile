#COM port for upload
PORT=COM24
# If we're using the ISP, it's handy to keep different COM ports. Also baud is not board dependent.
PORT_ISP=COM5
ISP_BAUD=115200
#ISP_TYPE of stk500v2 for Pololu v2.1
ISP_TYPE=stk500v2
MCU=atmega328p

uniq = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

#Boards:
# 0: Mt Tom Rev 1 (Not implemented)
# 1: Breadboard Modem -- BROKEN --
# 2: Moteino Modem
# 3: Flynns Rev 1 -- NO LONGER USED --
# 4: Flynns Rev 2 -- NO LONGER USED --
# 5: Dorji Modem
# 6: First attempt at a printed PCB configured for DavisWind (Flynns)
# 601: First attempt at a printed PCB configured for ADWind (old Mt Tom) -- NO LONGER USED --
# 7: Second printed PCB (includes battery PWM circuitry) -- NOT USED --
# 8: Second printed PCB configured for ALSWind (Mt Tom?)
# 9: Single board or second PCB with a Atmega328PB and 32768Hz crystal
# 10: Single board rev2
# 11: Single board rev2 no current sensor
# 12: Single board rev3
# 13: Single board rev3 FCC Compliant @ 917MHz
# 14: Dorji Modem @ 917MHz
BOARD = 10
include Boards.makefile

#Flags, etc.
DEFINES=$(BOARD_DEFINES) -DARDUINO=100 \
    -DTWI_BUFFER_LENGTH=10 \
	-DBOARD=$(BOARD)

ifeq ($(FCC_COMPLIANT), 1)
DEFINES += -DMIN_FREQ=903000000 -DMAX_FREQ=927000000 -DFCC_COMPLIANT
$(info FCC Complaint)
else
DEFINES += -DMIN_FREQ=420000000 -DMAX_FREQ=450000000
$(info not FCC Complaint)
endif
#-DRADIOLIB_DEBUG  -DRADIOLIB_VERBOSE
CFLAGS= -mcall-prologues -mmcu=$(MCU) -Os -fdata-sections -ffunction-sections -fno-exceptions -std=c++17 -g -flto
LDFLAGS=-Wl,--gc-sections -Wl,--relax
CC=avr-gcc

# External libraries
INCLUDES=-Ilib/ArduinoCore-avr/cores/arduino \
		 -Ilib/ArduinoCore-avr/variants/standard \
		 -Ilib/ArduinoCore-avr/libraries/SPI/src \
		 -Ilib/ArduinoCore-avr/libraries/SoftwareSerial/src \
		 -Ilib/ArduinoCore-avr/libraries/Wire/src \
		 -Ilib/RadioLib/src \
		 -Ilib/Low-Power \
		 -Ilib/RadioLib/src \
		 -Ilib/SPIFlash
ARDUINO_BASE_LIBS=^/wiring_analog.c \
			^/wiring_digital.c ^/hooks.c \
			^/Print.cpp ^/WMath.cpp

BOOTLOADER_HEX=Dualoptiboot_v5.0_atmega328_8mhz_57600baud_BlinkD9.hex

#Serial is included in modem and debug builds:
ifeq ($(DEBUG), 1)
ARDUINO_BASE_LIBS += ^/HardwareSerial.cpp ^/HardwareSerial0.cpp
DEFINES += -DDEBUG
else ifeq ($(MODEM), 1)
ARDUINO_BASE_LIBS += ^/HardwareSerial.cpp ^/HardwareSerial0.cpp
endif

ifeq ($(MODEM), 1)
DEFINES += -DMODEM -DDETAILED_LORA_CHECK
else
DEFINES += -DSERIAL_TX_BUFFER_SIZE=16 -DSERIAL_RX_BUFFER_SIZE=8
endif

ARDUINO_LIBS=^/SPI/src/SPI.cpp 
ARDUINO_LIBSWDIRS = \
				$(patsubst ^/%, lib/ArduinoCore-avr/cores/arduino/%, $(ARDUINO_BASE_LIBS)) \
				$(patsubst ^/%, lib/ArduinoCore-avr/libraries/%, $(ARDUINO_LIBS))

RADIO_LIB=lib/RadioLib/src/Module.cpp lib/RadioLib/src/modules/SX126x/SX126x.cpp \
		lib/RadioLib/src/modules/SX126x/SX1262.cpp lib/RadioLib/src/modules/RF69/RF69.cpp \

LIBRARIES=lib/Low-Power/LowPower.cpp lib/SPIFlash/SPIFlash.cpp $(RADIO_LIB) $(ARDUINO_LIBSWDIRS)

OBJFOLDER := $(OBJFOLDER)obj_$(BOARD)
ifeq ($(MODEM), 1)
SRCS_C = KissModem.cpp MessagingCommon.cpp LoraMessaging.cpp KissMessaging.cpp \
			../../ArduinoCore-avr/cores/arduino/wiring.c \
			PermanentStorage.cpp StackCanary.cpp \
			$(LIBRARIES)
OBJFOLDER := $(OBJFOLDER)_mod
else
SRCS_C = ArduinoWeatherStation.cpp Commands.cpp MessageHandling.cpp \
		 WeatherProcessing/WeatherProcessing.cpp WeatherProcessing/ADWind.cpp \
		 WeatherProcessing/DavisWind.cpp WeatherProcessing/ALSWind.cpp \
		 WeatherProcessing/TwoWire.cpp \
		 LoraMessaging.cpp MessagingCommon.cpp \
		 delay.c millis.cpp PermanentStorage.cpp StackCanary.cpp TimerTwo.cpp \
		 RemoteProgramming.cpp PWMSolar.cpp \
		 Flash.cpp Database.cpp \
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
	del /S $(OBJFOLDER)\\*.o $(OBJFOLDER)\\*.obj $(OBJFOLDER)\\*.elf $(OBJFOLDER)\\*.hex
	del /S $(OBJFOLDER)_debug\\*.o $(OBJFOLDER)_debug\\*.obj $(OBJFOLDER)_debug\\*.elf $(OBJFOLDER)_debug\\*.hex
#	del /S obj\\*.o obj\\*.hex obj\\*.obj obj\\*.elf
#	del /S obj_mod\\*.o obj_mod\\*.hex obj_mod\\*.obj obj_mod\\*.elf
#	del /S obj_debug\\*.o obj_debug\\*.hex obj_debug\\*.obj obj_debug\\*.elf
#	del /S obj_mod_debug\\*.o obj_mod_debug\\*.hex obj_mod_debug\\*.obj obj_mod_debug\\*.elf
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

$(OBJFOLDER)/all_with_bootloader.hex: $(OBJFOLDER)/all.hex
	srec_cat $< -intel $(BOOTLOADER_HEX) -intel -o $@ -intel

.PHONY: isp_program
isp_program: $(OBJFOLDER)/all_with_bootloader.hex $(OBJFOLDER)/all.s
	program_fuses.cmd $(PORT_ISP) $(ISP_BAUD) $(ISP_TYPE)
	avrdude -c $(ISP_TYPE) -p $(MCU) -P $(PORT_ISP) -b $(ISP_BAUD) -U eeprom:r:$(OBJFOLDER)/eeprom.hex:i
	avrdude -c $(ISP_TYPE) -p $(MCU) -P $(PORT_ISP) -b $(ISP_BAUD) -U $(OBJFOLDER)/all_with_bootloader.hex
	avrdude -c $(ISP_TYPE) -p $(MCU) -P $(PORT_ISP) -b $(ISP_BAUD) -U eeprom:w:$(OBJFOLDER)/eeprom.hex:i

.PHONY: program
program: $(OBJFOLDER)/all.hex $(OBJFOLDER)/all.s
	avrdude -c arduino -p $(MCU) -P $(PORT) -b $(PROG_BAUD) -U $(OBJFOLDER)/all.hex
	ECHO INFO : All done

.PHONY: save_outputs
save_outputs: $(OBJFOLDER)/all.hex $(OBJFOLDER)/all.s | saved
	save_outputs.cmd $(OBJFOLDER) $(BOARD)