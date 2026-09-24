/* Name: main.c
 * Project: Thermostat based on AVR USB driver
 * Author: Christian Starkjohann
 * Creation Date: 2006-04-23
 * Tabsize: 4
 * Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: Proprietary, free under certain conditions. See Documentation.
 * This Revision: $Id: main.c 537 2008-02-28 21:13:01Z cs $
 */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "usbdrv.h"
#include "oddebug.h"

/*
Pin assignment:
PB1(6) = key input (active low with pull-up)
PB3(2) = led

PB0(5), PB2(7) = USB data lines
*/

#define BIT_KEY 1
#define LED_PIN   PB3

#define REVERS_KEY

#define UTIL_BIN4(x)        (uchar)((0##x & 01000)/64 + (0##x & 0100)/16 + (0##x & 010)/4 + (0##x & 1))
#define UTIL_BIN8(hi, lo)   (uchar)(UTIL_BIN4(hi) * 16 + UTIL_BIN4(lo))

#ifndef NULL
#define NULL    ((void *)0)
#endif

/* ------------------------------------------------------------------------- */

static uchar    reportBuffer[1];    /* buffer for HID reports */
static uchar    idleRate;           /* in 4 ms units */
static uchar    led;

/* ------------------------------------------------------------------------- */
// usbHidReportDescriptor size define in usbconfig.h
PROGMEM const char usbHidReportDescriptor[49] = {
// 6 byte
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Gamepad)
    0xA1, 0x01,        // Collection (Application)
    
    // --- ボタン入力 (Input: 1 Buttons) ---
// 22 byge
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x01,        //   Usage Maximum (Button 1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1 bit)
    0x95, 0x01,        //   Report Count (1 items)
    0x81, 0x02,        //   Input (Data, Var, Abs) -> ボタンの状態
    0x75, 0x07,        //   REPORT_SIZE (7 bits)
    0x95, 0x01,        //   REPORT_COUNT (1)
    0x81, 0x03,        //   INPUT (Cnst,Var,Abs)
    
    // --- LED出力 (Output: 1 LED) ---
// 20 byte
    0x05, 0x08,        //   Usage Page (LEDs)
    0x09, 0x01,        //   Usage (Num Lock)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1 bit)
    0x95, 0x01,        //   Report Count (1 item)
    0x91, 0x02,        //   Output (Data, Var, Abs) -> ホストからのLED制御
    0x75, 0x01,        //   Report Size (1 bit)
    0x95, 0x07,        //   Report Count (7 bits)
    0x91, 0x03,        //   Output (Cnst, Var, Abs)
    
    0xC0               // End Collection
};

/* ------------------------------------------------------------------------- */

static uchar    keyPressed(void)
{
	uchar   mask;
	
#ifdef REVERS_KEY
    if((PINB & (1 << BIT_KEY)) != 0)
#else
	if((PINB & (1 << BIT_KEY)) == 0)
#endif
		return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */

static void timerInit(void)
{
    TCCR1 = 0x0b;           /* select clock: 16.5M/1k -> overflow rate = 16.5M/256k = 62.94 Hz */
}

/* ------------------------------------------------------------------------- */
/* ------------------------ interface to USB driver ------------------------ */
/* ------------------------------------------------------------------------- */

uchar usbFunctionWrite(uchar *data, uchar len) {
    led = data[0];

//    PORTB ^= (1 << LED_PIN);
    if (led & 0x01) {
      PORTB |= (1 << LED_PIN);
    } else {
      PORTB &= ~(1 << LED_PIN);
    }

    return 1;
}

uchar	usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    usbMsgPtr = reportBuffer;
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            reportBuffer[0] = keyPressed();
            return sizeof(reportBuffer);
        }else if(rq->bRequest == USBRQ_HID_SET_REPORT){
             return USB_NO_MSG; 
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){
            usbMsgPtr = &idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }else{
        /* no vendor specific requests implemented */
    }
	return 0;
}

/* ------------------------------------------------------------------------- */
/* ------------------------ Oscillator Calibration ------------------------- */
/* ------------------------------------------------------------------------- */

/* Calibrate the RC oscillator to 8.25 MHz. The core clock of 16.5 MHz is
 * derived from the 66 MHz peripheral clock by dividing. Our timing reference
 * is the Start Of Frame signal (a single SE0 bit) available immediately after
 * a USB RESET. We first do a binary search for the OSCCAL value and then
 * optimize this value with a neighboorhod search.
 * This algorithm may also be used to calibrate the RC oscillator directly to
 * 12 MHz (no PLL involved, can therefore be used on almost ALL AVRs), but this
 * is wide outside the spec for the OSCCAL value and the required precision for
 * the 12 MHz clock! Use the RC oscillator calibrated to 12 MHz for
 * experimental purposes only!
 */
static void calibrateOscillator(void)
{
uchar       step = 128;
uchar       trialValue = 0, optimumValue;
int         x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);

    /* do a binary search: */
    do{
        OSCCAL = trialValue + step;
        x = usbMeasureFrameLength();    /* proportional to current real frequency */
        if(x < targetValue)             /* frequency still too low */
            trialValue += step;
        step >>= 1;
    }while(step > 0);
    /* We have a precision of +/- 1 for optimum OSCCAL here */
    /* now do a neighborhood search for optimum value */
    optimumValue = trialValue;
    optimumDev = x; /* this is certainly far away from optimum */
    for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++){
        x = usbMeasureFrameLength() - targetValue;
        if(x < 0)
            x = -x;
        if(x < optimumDev){
            optimumDev = x;
            optimumValue = OSCCAL;
        }
    }
    OSCCAL = optimumValue;
}
/*
Note: This calibration algorithm may try OSCCAL values of up to 192 even if
the optimum value is far below 192. It may therefore exceed the allowed clock
frequency of the CPU in low voltage designs!
You may replace this search algorithm with any other algorithm you like if
you have additional constraints such as a maximum CPU clock.
For version 5.x RC oscillators (those with a split range of 2x128 steps, e.g.
ATTiny25, ATTiny45, ATTiny85), it may be useful to search for the optimum in
both regions.
*/

void    usbEventResetReady(void)
{
    calibrateOscillator();
    eeprom_write_byte(0, OSCCAL);   /* store the calibrated value in EEPROM */
}

/* ------------------------------------------------------------------------- */
/* --------------------------------- main ---------------------------------- */
/* ------------------------------------------------------------------------- */

int main(void)
{
uchar   i;
uchar   calibrationValue;
uchar   idleCounter = 0;
uchar   key, lastKey = 0, keyDidChange = 0;

    led = 1;
	// this is work around at after wdt reset
    wdt_enable(WDTO_4S);
    calibrationValue = eeprom_read_byte(0); /* calibration value from last time */
    if(calibrationValue != 0xff){
        OSCCAL = calibrationValue;
    }
    odDebugInit();
    usbDeviceDisconnect();
    for(i=0;i<20;i++){  /* 300 ms disconnect */
        _delay_ms(15);
    }
    usbDeviceConnect();
    DDRB |= (1 << LED_PIN);
    PORTB |= 1 << BIT_KEY;  /* pull-up on key input */
    wdt_enable(WDTO_1S);
    timerInit();
    usbInit();
    sei();
    for(;;){    /* main event loop */
        wdt_reset();
        usbPoll();
		key = keyPressed();
        if(lastKey != key){
            lastKey = key;
            keyDidChange = 1;
        }
#if 0
		if(TIFR & (1<<TOV0)){   /* 22 ms timer */
            TIFR = 1<<TOV0;
            if(idleRate != 0){
                if(idleCounter > 4){
                    idleCounter -= 5;   /* 22 ms in units of 4 ms */
                }else{
                    idleCounter = idleRate;
                    keyDidChange = 1;
                }
            }
        }
#endif
        if(keyDidChange && usbInterruptIsReady()){
            keyDidChange = 0;
            /* use last key and not current key status in order to avoid lost
			 changes in key status. */
            reportBuffer[0] = lastKey;
            usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
        }
    }
    return 0;
}
