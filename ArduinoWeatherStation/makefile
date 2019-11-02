PORT=COM15
MCU=atmega328p
F_CPU=16000000L
DEFINES=-DF_CPU=$(F_CPU) -DARDUINO=100
ARDUINO_DIR=C:/Program Files (x86)/Arduino/hardware/arduino/avr/cores/arduino
INCLUDES=-I"C:\Program Files (x86)\Arduino\hardware\arduino\avr\cores\arduino" -I"C:\Program Files (x86)\Arduino\hardware\arduino\avr\variants\standard" -I"lib"
ARDUINO_LIBS="^/HardwareSerial.cpp" "^/HardwareSerial0.cpp" "^/wiring_analog.c" "^/wiring_digital.c" "^/winterrupts.c" "^/wiring.c" "^/hooks.c" "^/print.cpp"
ARDUINO_LIBSWDIRS = $(ARDUINO_LIBS:^^/=C:/Program Files (x86^)/Arduino/hardware/arduino/avr/cores/arduino/)
LIBRARIES=lib/Low-Power-Master/LowPower.cpp lib/TimerOne/TimerOne.cpp $(ARDUINO_LIBSWDIRS)
CFLAGS=-Wall -mcall-prologues -mmcu=$(MCU) -Os -fpermissive
LDFLAGS=-Wl,-gc-sections -Wl,-relax
CC=avr-gcc
.SUFFIXES:.obj.hex

SRCS_C = ArduinoWeatherStation.cpp Commands.cpp HardwareMessaging.cpp MessageHandling.cpp Messaging.cpp SleepyTime.cpp WeatherProcessing.cpp $(LIBRARIES)
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
	