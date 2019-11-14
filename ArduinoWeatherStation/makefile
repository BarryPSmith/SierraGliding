PORT=COM15
MCU=atmega328p
F_CPU=16000000L
DEFINES=-DF_CPU=$(F_CPU) -DARDUINO=100
ARDUINO_DIR=C:/Program Files (x86)/Arduino/hardware/arduino/avr/cores/arduino
INCLUDES=-I"C:\Program Files (x86)\Arduino\hardware\arduino\avr\cores\arduino" \
         -I"C:\Program Files (x86)\Arduino\hardware\arduino\avr\variants\standard" \
         -I"C:/Program Files (x86)/Arduino/hardware/Arduino/avr/libraries/SPI/src" \
         -I"C:/Program Files (x86)/Arduino/hardware/Arduino/avr/libraries/SoftwareSerial/src" \
         -I"lib/RadioLib/src" \
         -I"lib"
ARDUINO_BASE_LIBS="^/HardwareSerial.cpp" "^/HardwareSerial0.cpp" "^/wiring_analog.c" \
            "^/wiring_digital.c" "^/winterrupts.c" "^/wiring.c" "^/hooks.c" \
            "^/print.cpp" "^/new.cpp" "^/abi.cpp" "^/WMath.cpp"
ARDUINO_LIBS="^/SPI/src/spi.cpp"
ARDUINO_LIBSWDIRS = \
        $(ARDUINO_BASE_LIBS:^^/=C:/Program Files (x86^)/Arduino/hardware/arduino/avr/cores/arduino/) \
        $(ARDUINO_LIBS:^^/=C:/Program Files (x86^)/Arduino/hardware/arduino/avr/libraries/)
RADIO_LIB=lib/RadioLib/src/Module.cpp lib/RadioLib/src/modules/SX126x.cpp \
        lib/RadioLib/src/modules/SX1262.cpp lib/RadioLib/src/modules/RF69.cpp \
        lib/RadioLib/src/protocols/PhysicalLayer.cpp
LIBRARIES=lib/Low-Power-Master/LowPower.cpp lib/TimerOne/TimerOne.cpp $(RADIO_LIB) $(ARDUINO_LIBSWDIRS)
CFLAGS= -mcall-prologues -mmcu=$(MCU) -Os -fpermissive -fdata-sections -ffunction-sections
LDFLAGS=-Wl,--gc-sections -Wl,--relax
CC=avr-gcc
.SUFFIXES:.obj.hex

SRCS_C = ArduinoWeatherStation.cpp Commands.cpp HardwareMessaging.cpp MessageHandling.cpp \
         Messaging.cpp SleepyTime.cpp WeatherProcessing.cpp MessagingCommon.cpp \
         $(LIBRARIES)
SRCS_H = *.h
OBJS    = $(SRCS_C:.c=.o)

all: all.hex

clean:
	rm -f *.o *.hex *.obj *.hex
	
.obj.hex:
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
	avrdude -c arduino -p $(MCU) -P $(PORT) -b 57600 -U all.hex
	