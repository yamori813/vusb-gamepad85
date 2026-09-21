# with Arduino IDE 0023
# http://www.clarestudio.org/elec/avr/gcc-1.html
#

CPU=attiny85

APPPATH=/Applications
TOOLPATH=Arduino.app/Contents/Resources/Java/hardware/tools/avr
CC=$(APPPATH)/$(TOOLPATH)/bin/avr-gcc
AS=$(APPPATH)/$(TOOLPATH)/bin/avr-as
CPP=$(APPPATH)/$(TOOLPATH)/bin/avr-cpp
CFLAGS=-Os -I$(APPPATH)/$(TOOLPATH)/avr/include/ -I. -Iusbdrv -DF_CPU=16500000 -DDEBUG_LEVEL=0
LDFLAGS=-Os -L$(APPPATH)/$(TOOLPATH)/avr/lib/
OBJCOPY=$(APPPATH)/$(TOOLPATH)/bin/avr-objcopy
AVRDUDE=$(APPPATH)/$(TOOLPATH)/bin/avrdude
AVRDUDECONF=$(APPPATH)/$(TOOLPATH)/etc/avrdude.conf 
ISP="arduino"
#ISP="usbtiny"

OBJ=main.o usbdrv/usbdrv.o usbdrv/usbdrvasm.o

main.hex: main.elf
	$(OBJCOPY) -I elf32-avr -O ihex main.elf main.hex

main.elf: $(OBJ)
	$(CC) -mmcu=$(CPU) $(LDFLAGS) -Wall -o main.elf $(OBJ)

usbdrv/usbdrvasm.o: usbdrv/usbdrvasm.S
	$(CPP) -mmcu=$(CPU) $(CFLAGS) -o usbdrv/usbdrvasm.tmp usbdrv/usbdrvasm.S
	$(AS) -mmcu=$(CPU) -o usbdrv/usbdrvasm.o usbdrv/usbdrvasm.tmp

usbdrv/usbdrv.o: usbdrv/usbdrv.c
	$(CC) -mmcu=$(CPU) $(CFLAGS) -c -o usbdrv/usbdrv.o usbdrv/usbdrv.c

main.o: main.c
	$(CC) -mmcu=$(CPU) $(CFLAGS) -c -o main.o main.c

flash85:
	$(AVRDUDE) -c $(ISP) -C $(AVRDUDECONF) -p t85 -U flash:w:main.hex

fuse85:
#	$(AVRDUDE) -c $(ISP) -C $(AVRDUDECONF) -p t85 -U hfuse:w:0xdd:m -U lfuse:w:0xe1:m
	$(AVRDUDE) -c $(ISP) -C $(AVRDUDECONF) -p t85 -U hfuse:w:0xdd:m -U lfuse:w:0xc1:m

check:	
	./checksize main.elf 8192 512

clean:
	rm -f *.o *.dump *.hex *.elf usbdrv/*.o usbdrv/*.tmp
