ifeq ($(BOARD), 0)
$(error Makefile for Mt Tom Rev1 doesn't exist (0))

else ifeq ($(BOARD), 1)
$(info Running Makefile for Breadboard Modem (1))
BOARD_DEFINES = \
    -DF_CPU=16000000L \
    -DSX_BUSY=4 \
    -DSX_DIO1=2 \
    -DSX_SELECT=9 \
    -DMODEM \
    -DWATCHDOG_LOOPS=5 \

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
    -DDAVIS_WIND \
    -DWIND_DIR_PIN=A0 \
    -DWIND_SPD_PIN=3 \
    -DSX_BUSY=4 \
    -DSX_DIO1=2 \
    -DSX_SELECT=9 \
    -DSOFT_WDT \

#	-DDISABLE_RFM60

PROG_BAUD = 57600

else ifeq ($(BOARD), 4)
$(info Running Makefile for Flynns Rev2 / 8MHz AtMega328P (4))
BOARD_DEFINES = \
    -DF_CPU=8000000L \
    -DSTATION_ID=\'2\' \
    -DREF_MV=3300 \
    -DBATTV_NUM=491 \
    -DBATTV_DEN=100 \
    -DBATT_PIN=A3 \
    -DDAVIS_WIND \
    -DWIND_DIR_PIN=A5 \
    -DWIND_SPD_PIN=2 \
    -DSX_BUSY=8 \
    -DSX_DIO1=3 \
    -DSX_SELECT=9 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=A1 \
    -DNO_INTERNAL_TEMP \

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
$(info Running Makefile for PCB Rev1 - Flynns (6))
BOARD_DEFINES = \
    -DF_CPU=8000000L \
    -DSERIAL_BAUD=9600 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=101 \
    -DBATTV_DEN=33 \
    -DWIND_DIR_PIN=A5 \
    -DWIND_SPD_PIN=2 \
    -DWIND_PWR_PIN=A2 \
    -DTEMP_SENSE=A4 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DDAVIS_WIND \
#   -DSTATION_ID=\'4\' \
    -DCLOCK_DIVIDER=1 \

PROG_BAUD=57600

else ifeq ($(BOARD), 601)
$(info Running Makefile for PCB Rev1 - Mt Tom (601))
BOARD_DEFINES = \
    -DF_CPU=8000000L \
    -DSERIAL_BAUD=9600 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=101 \
    -DBATTV_DEN=33 \
    -DWIND_DIR_PIN=A5 \
    -DWIND_SPD_PIN=2 \
    -DWIND_PWR_PIN=A2 \
    -DTEMP_SENSE=A4 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DARGENTDATA_WIND \
#   -DINVERSE_AD_WIND \
#   -DSTATION_ID=\'4\' \
    -DCLOCK_DIVIDER=1 \

PROG_BAUD=57600


else ifeq ($(BOARD), 7)
$(info Running Makefile for PCB Rev2 (7))
BOARD_DEFINES = \
    -DF_CPU=1000000L \
    -DSERIAL_BAUD=7200 \
    -DCLOCK_DIVIDER=8 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=2 \
    -DBATTV_DEN=1 \
    -DWIND_DIR_PIN=A5 \
    -DWIND_SPD_PIN=2 \
    -DWIND_PWR_PIN=A2 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DARGENTDATA_WIND \
    -DINVERSE_AD_WIND \
    -DSOLAR_PWM \
    -DTEMP_SENSE=A4 \
    -DSX_TCXO_STARTUP_US=2000

PROG_BAUD=57600

else ifeq ($(BOARD), 8)
$(info Running Makefile for PCB Rev2 & ALS Wind (8))
BOARD_DEFINES = \
    -DF_CPU=1000000L \
    -DSERIAL_BAUD=9600 \
    -DCLOCK_DIVIDER=8 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=2 \
    -DBATTV_DEN=1 \
    -DWIND_SPD_PIN=2 \
    -DTEMP_SENSE=A2 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DALS_WIND \
    -DSOLAR_PWM \
    -DSX_TCXO_STARTUP_US=2000
#   -DSTATION_ID=\'4\' \

PROG_BAUD=57600

else ifeq ($(BOARD), 9)
$(info Running Makefile for Single Board (9))
BOARD_DEFINES = \
    -DF_CPU=1000000L \
    -DSERIAL_BAUD=9600 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=2 \
    -DBATTV_DEN=1 \
    -DWIND_SPD_PIN=2 \
    -DTEMP_SENSE=A2 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DALS_WIND \
    -DSOLAR_PWM \
    -DSX_TCXO_STARTUP_US=2000 \
    -DATMEGA328PB \
    -DCRYSTAL_FREQ=32768 \
    -DCLOCK_DIVIDER=8 \
    #

PROG_BAUD=57600

else ifeq ($(BOARD), 10)
$(info Running Makefile for Single Board rev2 (10))
BOARD_DEFINES = \
    -DF_CPU=1000000L \
    -DSERIAL_BAUD=9600 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=2 \
    -DBATTV_DEN=1 \
    -DWIND_SPD_PIN=2 \
    -DTEMP_SENSE=A2 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_SWITCH=A3 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DALS_WIND \
    -DSOLAR_PWM \
    -DSX_TCXO_STARTUP_US=2000 \
    -DCRYSTAL_FREQ=32768 \
    -DCLOCK_DIVIDER=8 \
    -DCURRENT_SENSE=A1 \
    -DCURRENT_SENSE_PWR=7 \
    -DCURRENT_SENSE_GAIN=20 \
    -DSOLAR_IMPEDANCE_SWITCHING \
    -DTEMP_PWR_PIN=6 \
    #-DATMEGA328PB \

PROG_BAUD=57600

else ifeq ($(BOARD), 11)
$(info Running Makefile for Single Board rev2 without current sense (11))
BOARD_DEFINES = \
    -DF_CPU=1000000L \
    -DSERIAL_BAUD=9600 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=2 \
    -DBATTV_DEN=1 \
    -DWIND_SPD_PIN=2 \
    -DTEMP_SENSE=A2 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_SWITCH=A3 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DALS_WIND \
    -DSOLAR_PWM \
    -DSX_TCXO_STARTUP_US=2000 \
    -DCRYSTAL_FREQ=32768 \
    -DCLOCK_DIVIDER=8 \
    -DSOLAR_IMPEDANCE_SWITCHING \
    -DTEMP_PWR_PIN=6 \
    #-DATMEGA328PB \

PROG_BAUD=57600

else ifeq ($(BOARD), 12)
$(info Running Makefile for Single Board rev3 (12))
BOARD_DEFINES = \
    -DF_CPU=1000000L \
    -DSERIAL_BAUD=9600 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=2 \
    -DBATTV_DEN=1 \
    -DWIND_SPD_PIN=2 \
    -DTEMP_SENSE=A2 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_SWITCH=A3 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DALS_WIND \
    -DSOLAR_PWM \
    -DSX_TCXO_STARTUP_US=2000 \
    -DCRYSTAL_FREQ=32768 \
    -DCLOCK_DIVIDER=8 \
    -DCURRENT_SENSE=A1 \
    -DCURRENT_SENSE_PWR=7 \
    -DCURRENT_SENSE_GAIN=25 \
    -DSOLAR_INVERSE \
    -DTEMP_PWR_PIN=6 \
    #-DATMEGA328PB \

PROG_BAUD=57600

else ifeq ($(BOARD), 13)
$(info Running Makefile for Single Board rev3 915MHz (13))
BOARD_DEFINES = \
    -DF_CPU=1000000L \
    -DSERIAL_BAUD=9600 \
    -DREF_MV=3300 \
    -DBATT_PIN=A0 \
    -DBATTV_NUM=2 \
    -DBATTV_DEN=1 \
    -DWIND_SPD_PIN=2 \
    -DTEMP_SENSE=A2 \
    -DSX_BUSY=4 \
    -DSX_DIO1=3 \
    -DSX_SELECT=10 \
    -DSX_SWITCH=A3 \
    -DSX_TCXOV_X10=18 \
    -DSX_RESET=9 \
    -DFLASH_SELECT=8 \
    -DWIND_DIR_AVERAGING \
    -DALS_WIND \
    -DSOLAR_PWM \
    -DSX_TCXO_STARTUP_US=2000 \
    -DCRYSTAL_FREQ=32768 \
    -DCLOCK_DIVIDER=8 \
    -DCURRENT_SENSE=A1 \
    -DCURRENT_SENSE_PWR=7 \
    -DCURRENT_SENSE_GAIN=25 \
    -DSOLAR_INVERSE \
    -DTEMP_PWR_PIN=6 \
    #-DATMEGA328PB \

FCC_COMPLIANT=1

PROG_BAUD=57600

else ifeq ($(BOARD), 14)
$(info Running Makefile for 917MHz Dorji Test Board (14))
BOARD_DEFINES = \
    -DF_CPU=16000000L \
    -DSX_BUSY=3 \
    -DSX_SELECT=7 \
    -DSX_DIO1=5 \
    -DSX_RESET=A0 \
    -DSX_SWITCH=8 \
    -DWATCHDOG_LOOPS=15 \
    -DNO_DIO1_INTERRUPT \
    -DSX_TCXOV_X10=18 \
    -DDEBUG_PARAMETERS

PROG_BAUD=115200
MODEM=1
FCC_COMPLIANT=1

endif