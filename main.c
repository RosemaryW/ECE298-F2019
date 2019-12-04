#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <msp430.h> 
#include <driverlib.h>
#include <hal_LCD.h>

#define keyPad_R1 GPIO_PORT_P1, GPIO_PIN1
#define keyPad_R2 GPIO_PORT_P2, GPIO_PIN5
#define keyPad_R3 GPIO_PORT_P5, GPIO_PIN1
#define keyPad_R4 GPIO_PORT_P2, GPIO_PIN7
#define keyPad_C1 GPIO_PORT_P1, GPIO_PIN0
#define keyPad_C2 GPIO_PORT_P8, GPIO_PIN1
#define keyPad_C3 GPIO_PORT_P8, GPIO_PIN0

#define motorA1 GPIO_PORT_P1, GPIO_PIN3
#define motorA2 GPIO_PORT_P5, GPIO_PIN3
#define motorB1 GPIO_PORT_P1, GPIO_PIN4
#define motorB2 GPIO_PORT_P1, GPIO_PIN5

#define pi_A1 GPIO_PORT_P1, GPIO_PIN7
#define pi_A2 GPIO_PORT_P1, GPIO_PIN6
#define pi_B1 GPIO_PORT_P5, GPIO_PIN0
#define pi_B2 GPIO_PORT_P5, GPIO_PIN2

#define mux_s0 GPIO_PORT_P8, GPIO_PIN2
#define mux_s1 GPIO_PORT_P8, GPIO_PIN3

#define distancePerRev (0.2041 * 5) // 0.065 * PI m  set faster by 1:5
#define tickPeriod 5 // seconds

/* declare variables */
char directionText[6][10] = {"ERROR", "FORWARD", "REVERSED", "LEFT", "RIGHT", "STOPPED"};
bool tick = false;
int tickCounter = 0;
char lcdPrint[200];
bool appRunning = 1;
int threshold = 100;
int direction = 5;
int motorA_rev_count = 0;
int totalDistance = 0;

int detectKey() {
    GPIO_setOutputHighOnPin(keyPad_R1);
    if (GPIO_getInputPinValue(keyPad_C1)) {
        GPIO_setOutputLowOnPin(keyPad_R1);
        return 1;
    } else if (GPIO_getInputPinValue(keyPad_C2)) {
        GPIO_setOutputLowOnPin(keyPad_R1);
        return 2;
    } else if (GPIO_getInputPinValue(keyPad_C3)) {
        GPIO_setOutputLowOnPin(keyPad_R1);
        return 3;
    }
    GPIO_setOutputLowOnPin(keyPad_R1);

    GPIO_setOutputHighOnPin(keyPad_R2);
    if (GPIO_getInputPinValue(keyPad_C1)) {
        GPIO_setOutputLowOnPin(keyPad_R2);
        return 4;
    } else if (GPIO_getInputPinValue(keyPad_C2)) {
        GPIO_setOutputLowOnPin(keyPad_R2);
        return 5;
    } else if (GPIO_getInputPinValue(keyPad_C3)) {
        GPIO_setOutputLowOnPin(keyPad_R2);
        return 6;
    }
    GPIO_setOutputLowOnPin(keyPad_R2);

    GPIO_setOutputHighOnPin(keyPad_R3);
    if (GPIO_getInputPinValue(keyPad_C1)) {
        GPIO_setOutputLowOnPin(keyPad_R3);
        return 7;
    } else if (GPIO_getInputPinValue(keyPad_C2)) {
        GPIO_setOutputLowOnPin(keyPad_R3);
        return 8;
    } else if (GPIO_getInputPinValue(keyPad_C3)) {
        GPIO_setOutputLowOnPin(keyPad_R3);
        return 9;
    }
    GPIO_setOutputLowOnPin(keyPad_R3);

    GPIO_setOutputHighOnPin(keyPad_R4);
    if (GPIO_getInputPinValue(keyPad_C1)) {
        GPIO_setOutputLowOnPin(keyPad_R4);
        return 10;
    } else if (GPIO_getInputPinValue(keyPad_C2)) {
        GPIO_setOutputLowOnPin(keyPad_R4);
        return 0;
    } else if (GPIO_getInputPinValue(keyPad_C3)) {
        GPIO_setOutputLowOnPin(keyPad_R4);
        return 11;
    }
    GPIO_setOutputLowOnPin(keyPad_R4);

    return -1;
}

void setWheels(int direction) {
    switch(direction) {
        case 1:
            GPIO_setOutputHighOnPin(motorA1);
            GPIO_setOutputLowOnPin(motorA2);
            GPIO_setOutputLowOnPin(motorB1);
            GPIO_setOutputHighOnPin(motorB2);
            break;
        case 2:
            GPIO_setOutputLowOnPin(motorA1);
            GPIO_setOutputHighOnPin(motorA2);
            GPIO_setOutputHighOnPin(motorB1);
            GPIO_setOutputLowOnPin(motorB2);
            break;
        case 3:
            GPIO_setOutputLowOnPin(motorA1);
            GPIO_setOutputHighOnPin(motorA2);
            GPIO_setOutputLowOnPin(motorB1);
            GPIO_setOutputHighOnPin(motorB2);
            __delay_cycles(1000000); // turn for a while to achieve 90 degrees. Exact number of cycles to delay needs testing
            GPIO_setOutputHighOnPin(motorA1);
            GPIO_setOutputLowOnPin(motorA2);
            break;
        case 4:
            GPIO_setOutputHighOnPin(motorA1);
            GPIO_setOutputLowOnPin(motorA2);
            GPIO_setOutputHighOnPin(motorB1);
            GPIO_setOutputLowOnPin(motorB2);
            __delay_cycles(1000000); // turn for a while to achieve 90 degrees. Exact number of cycles to delay needs testing
            GPIO_setOutputLowOnPin(motorB1);
            GPIO_setOutputHighOnPin(motorB2);
            break;
        default:
            GPIO_setOutputLowOnPin(motorA1);
            GPIO_setOutputLowOnPin(motorA2);
            GPIO_setOutputLowOnPin(motorB1);
            GPIO_setOutputLowOnPin(motorB2);
            break;
    }

    return;
}

void getActualWheelDirection() {
    unsigned long firstSection = 0;
    unsigned long secondSection = 0;
    while (GPIO_getInputPinValue(pi_A1) == 1);
    while (GPIO_getInputPinValue(pi_A2) == 1) {
        firstSection++;
    }
    while (GPIO_getInputPinValue(pi_A1) == 1) {
        secondSection++;
    }

    char *aDir = firstSection < secondSection ? "COUNTER" : "";
    char text[100];
    sprintf(text, "WHEEL A %s CLOCKWISE", aDir);
    displayScrollText(text);
}

/* 0-Green 1-Yellow 2-Orange 3-Red */
void updateLED() {
    int percentage = 100 - ((totalDistance * 100) / threshold);
    if (percentage > 75) {
        GPIO_setOutputLowOnPin(mux_s0);
        GPIO_setOutputLowOnPin(mux_s1);
    } else if (percentage > 50) {
        GPIO_setOutputHighOnPin(mux_s0);
        GPIO_setOutputLowOnPin(mux_s1);
    } else if (percentage > 25) {
        GPIO_setOutputLowOnPin(mux_s0);
        GPIO_setOutputHighOnPin(mux_s1);
    } else {
        GPIO_setOutputHighOnPin(mux_s0);
        GPIO_setOutputHighOnPin(mux_s1);
    }
}

int main(void)
{
    __disable_interrupt();

    WDT_A_hold(WDT_A_BASE);
	PMM_unlockLPM5();

	/* configure pins for keypad */
	GPIO_setAsInputPinWithPullDownResistor(keyPad_C1);
	GPIO_setAsInputPinWithPullDownResistor(keyPad_C2);
	GPIO_setAsInputPinWithPullDownResistor(keyPad_C3);

    GPIO_setAsOutputPin(keyPad_R1);
    GPIO_setAsOutputPin(keyPad_R2);
    GPIO_setAsOutputPin(keyPad_R3);
    GPIO_setAsOutputPin(keyPad_R4);

    GPIO_setOutputLowOnPin(keyPad_R1);
    GPIO_setOutputLowOnPin(keyPad_R2);
    GPIO_setOutputLowOnPin(keyPad_R3);
    GPIO_setOutputLowOnPin(keyPad_R4);

    /* configure pins for LED. LEDs are connected to mux. */
    GPIO_setAsOutputPin(mux_s0);
    GPIO_setAsOutputPin(mux_s1);

    GPIO_setOutputLowOnPin(mux_s0);
    GPIO_setOutputLowOnPin(mux_s1);

    /* configure pins for photo interrupters */
    GPIO_setAsInputPin(pi_A1);
    GPIO_setAsInputPin(pi_A2);
    GPIO_setAsInputPin(pi_B1);
    GPIO_setAsInputPin(pi_B2);

    /* configure pins for motor */
    GPIO_setAsOutputPin(motorA1);
    GPIO_setAsOutputPin(motorA2);
    GPIO_setAsOutputPin(motorB1);
    GPIO_setAsOutputPin(motorB2);
    setWheels(direction);

    /* init launchpad modules */
    Init_LCD();
//    Timer_A_initUpModeParam param;
//    param.clockSource           = TIMER_A_CLOCKSOURCE_SMCLK;
//    param.clockSourceDivider    = TIMER_A_CLOCKSOURCE_DIVIDER_1;
//    param.timerPeriod           = 65000; // 65ms
//    param.captureCompareInterruptEnable_CCR0_CCIE = TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE;
//    param.startTimer = false;
//    Timer_A_initUpMode(TIMER_A0_BASE, &param);

    /* get forward threshold */
    displayScrollText("ENTER THRESHOLD PRESS POUND OR ASTERISK WHEN DONE");
//    displayScrollText("T");
    int key = 0;
    char keyBuff[7];
    unsigned int idx = 0;
    memset(keyBuff, 0, 7);

    while (1) {
        key = detectKey();
        if (key == 11 || key == 10) { // # or *
            key = atoi(keyBuff);
            if (key != 0) { // if threshold has been changed at all, if not use default
                threshold = key;
            }
            break;
        } else if (key != -1) {
            keyBuff[idx] = (char) (key + 48);
            idx++;
            keyBuff[idx] = '\0';
            displayScrollText(keyBuff);
            if (idx == 7) {
                break;
            }
            __delay_cycles(10000); // debouncing
        }
    }

    sprintf(lcdPrint, "TRESHOLD IS %d M", threshold);
    displayScrollText(lcdPrint);


//    __enable_interrupt();
//    Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
//    __delay_cycles(1000000);

    while (appRunning) {
        while (direction != 5) {
            tickCounter++;
            if (detectKey() != -1 || totalDistance >= threshold) {
                direction = 5;
                setWheels(direction);
                motorA_rev_count = 0;
                tickCounter = 0;
//                tick = false;
//                Timer_A_stop(TIMER_A0_BASE);
//                Timer_A_clear(TIMER_A0_BASE);
                displayScrollText(directionText[direction]);
                __delay_cycles(10000);
                break;
            }

            if (tickCounter > 5000) {
//                displayScrollText("IN");
                int distance = motorA_rev_count * distancePerRev;
                totalDistance += distance;
                int speed = (distance * 100) / tickPeriod;
                updateLED();
                motorA_rev_count = 0;
                getActualWheelDirection();
                sprintf(lcdPrint, "TOTAL DISTANCE %d M AT %d CM PER S", totalDistance, speed);
                displayScrollText(lcdPrint);
//                tick = false;
                tickCounter = 0;
            }

            if (GPIO_getInputPinValue(pi_A1) == 0) {
                motorA_rev_count++;
                __delay_cycles(125000);
            }
        }

        if (totalDistance >= threshold) {
            displayScrollText("NEEDS CHARGING");
            while (1) {}
        }
        displayScrollText("ENTER DIRECTION PRESS 1 TO 4");
//        displayScrollText("DIRECTION");
        __delay_cycles(10000); // debouncing
        while (1) {
            direction = detectKey();
            if (direction < 5 && direction != -1) {
                break;
            }
        }

        setWheels(direction);
        sprintf(lcdPrint, "DIRECTION IS %s", directionText[direction]);
        displayScrollText(lcdPrint);
//        displayScrollText("PRESS ANY KEY TO STOP");
//        Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
        __delay_cycles(1000000);
    }

    displayScrollText("POWERING OFF");
    return 0;
}

//#pragma vector=TIMER0_A0_VECTOR
//__interrupt
//void timerAISR(void)
//{
//    displayScrollText("HHH");
////    if (tickCounter < 76) {
//        tickCounter++;
////    } else {
////        tickCounter = 0;
////        tick = true;
////    }
//
//}
