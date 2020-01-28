ifeq ($(BOARD), 0)
$(error Makefile for Mt Tom Rev1 doesn't exist (0))

else ifeq ($(BOARD), 1)
$(info Running Makefile for Breadboard Modem (1))
BOARD_DEFINES = \
    -DF_CPU=16000000L \
    -DSX_BUSY=4 \
    -DSX_DIO1=2 \
    -DSX_SELECT=9 \
    -DWATCHDOG_LOOPS=15 \
    -DMODEM

PROG_BAUD=115200
MODEM=1

else ifeq ($(BOARD), 2)
$(info Running Makefile for Moteino Modem (2))
PROG_BAUD = 115200
MODEM = 1
BOARD_DEFINES = \
    -DF_CPU=16000000L \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_BUSY=5 \
    -DSX_RESET=4 \
    -DWATCHDOG_LOOPS=15 \
    -DSX_TCXOV_X10=18 \
    -DMODEM

else ifeq ($(BOARD), 3)
$(info Running Makefile for Flynns Rev1 / Arduino Nano (3))
BOARD_DEFINES = -DSTATION_ID=\'2\' \
    -DF_CPU=16000000L \
    -DREF_MV=3800 \
    -DBATTV_NUM=491 \
    -DBATTV_DEN=100 \
    -DBATT_PIN=A2 \
    -DWIND_TYPE=DAVIS_WIND \
    -DWIND_DIR_PIN=A0 \
    -DWIND_SPD_PIN=3 \
    -DSX_BUSY=4 \
    -DSX_DIO1=2 \
    -DSX_SELECT=9 \
    -DSOFT_WDT \
    -DDAVIS_WIND

#	-DDISABLE_RFM60

PROG_BAUD = 115200

else ifeq ($(BOARD), 4)
$(info Running Makefile for Flynns Rev2 / 8MHz AtMega328P (4))
BOARD_DEFINES = \
    -DF_CPU=8000000L \
    -DSTATION_ID=\'2\' \
    -DREF_MV=3300 \
    -DBATTV_NUM=491 \
    -DBATTV_DEN=100 \
    -DBATT_PIN=A3 \
    -DWIND_TYPE=DAVIS_WIND \
    -DWIND_DIR_PIN=A5 \
    -DWIND_SPD_PIN=2 \
    -DSX_BUSY=8 \
    -DSX_DIO1=3 \
    -DSX_SELECT=9 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=A1 \
    -DNO_INTERNAL_TEMP \
    -DDAVIS_WIND

PROG_BAUD=57600

else ifeq ($(BOARD), 5)
$(info Running Makefile for Dorji Test Board (5))
BOARD_DEFINES = \
    -DF_CPU=16000000L \
    -DSTATION_ID=\'4\' \
    -DSX_BUSY=3 \
    -DSX_SELECT=7 \
    -DSX_DIO1=5 \
    -DSX_RESET=A0 \
    -DSX_SWITCH=8 \
    -DWATCHDOG_LOOPS=15 \
    -DNO_DIO1_INTERRUPT \
    -DSX_TCXOV_X10=18

PROG_BAUD=115200
MODEM=1

else ifeq ($(BOARD), 6)
$(info Running Makefile for PCB Rev1 (6))
BOARD_DEFINES = \
    -DF_CPU=8000000L \
    -DSTATION_ID=\'3\' \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=101 \
    -DBATTV_DEN=33 \
    -DWIND_TYPE=DAVS_WIND \
    -DWIND_DIR_PIN=A5 \
    -DWIND_SPD_PIN=2 \
    -DWIND_PWR_PIN=A2 \
    -DTEMP_SENSE=A4 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DDAVIS_WIND

PROG_BAUD=57600

endif