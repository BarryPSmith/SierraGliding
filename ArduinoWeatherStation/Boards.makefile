!IF $(BOARD) == 0
!ERROR Makefile for Mt Tom Rev1 doesn't exist (0)

!ELSE IF $(BOARD) == 1
!MESSAGE Running Makefile for Breadboard Modem (1)
BOARD_DEFINES = \
    -DF_CPU=16000000L \
    -DSX_BUSY=4 \
    -DSX_DIO1=2 \
    -DSX_SELECT=9

PROG_BAUD=57600 \
MODEM = 1
#TODO: Change the target?

!ELSE IF $(BOARD) == 2
!ERROR Haven't done makefile for Moteino / RFM96 (2)
!MESSAGE Running Makefile for Moteino / RMF96 Modem (2)
F_CPU = 16000000L
PROG_BAUD = 115200
MODEM = 1

!ELSE IF $(BOARD) == 3
!MESSAGE Running Makefile for Flynns Rev1 / Arduino Nano (3)
BOARD_DEFINES = -DSTATION_ID='2' \
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
    -DSX_SELECT=9

PROG_BAUD = 57600

!ELSE IF $(BOARD) == 4
!MESSAGE Running Makefile for Flynns Rev2 / 8MHz AtMega328P (4)
BOARD_DEFINES = \
    -DF_CPU=8000000L \
    -DSTATION_ID='2' \
    -DREF_MV=3300 \
    -DBATTV_NUM=391 \
    -DBATTV_DEN=100 \
    -DBATT_PIN=A3 \
    -DWIND_TYPE=DAVIS_WIND \
    -DWIND_DIR_PIN=A5 \
    -DWIND_SPD_PIN=2 \
    -DSX_BUSY=8 \
    -DSX_DIO1=3 \
    -DSX_SELECT=9 \
    -DSX_RESET=A1 \
    -DUSE_WDT=1 \

PROG_BAUD=57600

!ENDIF