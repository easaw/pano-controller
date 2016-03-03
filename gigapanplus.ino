/*
 * GigapanPlus for Arduino project
 *
 * Copyright (C)2015 Laurentiu Badea
 *
 * This file may be redistributed under the terms of the MIT license.
 * A copy of this license has been included with this distribution in the file LICENSE.
 */
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <DRV8834.h>
#include "pano.h"
#include "camera.h"
#include "joystick.h"
#include "menu.h"

// Address of I2C OLED display. If screen looks scaled edit Adafruit_SSD1306.h
// and pick SSD1306_128_64 or SSD1306_128_32 that matches display type.
#define DISPLAY_I2C_ADDRESS 0x3C
#define OLED_RESET 12
#define TEXT_SIZE 1
#define DISPLAY_ROWS SSD1306_LCDHEIGHT/8/TEXT_SIZE

#define CAMERA_FOCUS 0
#define CAMERA_SHUTTER 1

#define JOYSTICK_X A3
#define JOYSTICK_Y A2
#define JOYSTICK_SW 2

#define MOTOR_STEPS 200
#define MOTORS_ON 13
#define VERT_DIR 5
#define VERT_STEP 6
#define HORIZ_DIR 8
#define HORIZ_STEP 9

#define DRV_M0 10
#define DRV_M1 11

static DRV8834 horiz_motor(MOTOR_STEPS, HORIZ_DIR, HORIZ_STEP, DRV_M0, DRV_M1);
static DRV8834 vert_motor(MOTOR_STEPS, VERT_DIR, VERT_STEP, DRV_M0, DRV_M1);
static Adafruit_SSD1306 display(OLED_RESET);
static Camera camera(CAMERA_FOCUS, CAMERA_SHUTTER);
static Joystick joystick(JOYSTICK_SW, JOYSTICK_X, JOYSTICK_Y);
static Pano pano(horiz_motor, vert_motor, camera, MOTORS_ON);

// these variables are modified by the menu
volatile int focal, fov, shutter, pre_shutter, orientation, aspect, shots, motors_enable, display_invert;
volatile int running;
int horiz, vert;


void setup() {
    Serial.begin(38400);
    // temporary turn on EN on both motors
    pinMode(4, OUTPUT); digitalWrite(4, LOW);
    pinMode(7, OUTPUT); digitalWrite(7, LOW);

    horiz_motor.setMicrostep(32);
    vert_motor.setMicrostep(32);
    delay(1000); // wait for serial
    delay(100);  // give time for display to init; if display blank increase delay
    display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDRESS);
    display.setRotation(2);
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextColor(WHITE);
    display.setTextSize(TEXT_SIZE);
    Serial.println(F("Ready\n"));
}

/*
 * Display current panorama status (photo index, etc)
 */
void displayPanoStatus(void){
    display.clearDisplay();
    display.setCursor(0,0);

    display.print(F("# "));
    display.print(pano.position+1);
    display.print(F("/"));
    display.println(pano.getHorizShots()*pano.getVertShots());
    display.print(F("@ "));
    display.print(1+pano.position / pano.getHorizShots());
    display.print(F("x"));
    display.println(1+pano.position % pano.getHorizShots());
    displayPanoSize();

#if DISPLAY_ROWS > 4
    display.print(F("Focal Length "));
    display.print(focal);
    display.println(F("mm"));
    display.print(F("Lens FOV "));
    display.print(camera.getHorizFOV());
    display.print(F("x"));
    display.println(camera.getVertFOV());
    display.print(F("Pano FOV "));
    display.print(pano.horiz_fov);
    display.print(F("x"));
    display.println(pano.vert_fov);
#endif
    display.display();
}

/*
 * Display the panorama grid size
 */
void displayPanoSize(){
    display.print(F("\xb0 "));
    display.print(pano.getVertShots());
    display.print(F("x"));
    display.print(pano.getHorizShots());
    display.println();
}

/*
 * Let user move the camera while optionally recording position
 * @param msg:   title string
 * @param horiz: pointer to store horizontal movement
 * @param vert:  pointer to store vertical movement
 */
void positionCamera(const char *msg, int *horiz, int *vert){
    int pos_x, pos_y;
    int h = 0, v = 0;
    int when_to_display = 0;

    display.clearDisplay();
    display.setCursor(0,0);
    display.println(msg);
    display.println(F("  \x1e  \n"
                      "\x11 x \x10\n"
                      "  \x1f"));
    display.display();
    pano.motorsEnable(true);
    while (!Joystick::isEventClick(joystick.read())){
        pos_x = joystick.getPositionX();
        pos_y = joystick.getPositionY();
        if (pos_x){
            horiz_motor.setRPM(40*abs(pos_x)/joystick.range);
            if (!horiz || h + pos_x/abs(pos_x) > 0){
                horiz_motor.rotate(pos_x/abs(pos_x));
                h += pos_x/abs(pos_x);
            }
        }
        if (pos_y){
            vert_motor.setRPM(120*abs(pos_y)/joystick.range);
            if (!vert || v - pos_y/abs(pos_y) > 0){
                v += -pos_y/abs(pos_y);
                vert_motor.rotate(pos_y/abs(pos_y));
            }
        }
        if (horiz && h > 0 && v > 0){
            pano.setFOV(h / pano.horiz_gear_ratio + camera.getHorizFOV(),
                        v / pano.vert_gear_ratio + camera.getVertFOV());
            pano.computeGrid();
            if (pano.getVertShots()+pano.getHorizShots() != when_to_display){
                display.setCursor(0,8*6);
                display.print("          ");
                display.setCursor(0,8*6);
                displayPanoSize();
                display.display();
                when_to_display = pano.getVertShots() + pano.getHorizShots();
            }
        }
    }
    if (horiz){
        *horiz = h / pano.horiz_gear_ratio + camera.getHorizFOV();
        horiz_motor.setRPM(40);
        horiz_motor.rotate(-h);
    }
    if (vert){
        *vert = v / pano.vert_gear_ratio + camera.getVertFOV();
        vert_motor.setRPM(120);
        vert_motor.rotate(v);
    }
}

/*
 * Display and navigate main menu
 * Only way to exit is by starting the panorama which sets running=true
 */
void displayMenu(void){
    int event;

    menu.open();
    display.clearDisplay();
    display.setCursor(0,0);
    menu.render(display, DISPLAY_ROWS);
    display.display();

    while (!running){
        event = joystick.read();
        if (!event) continue;

        if (Joystick::isEventLeft(event)) menu.cancel();
        else if (Joystick::isEventRight(event) || Joystick::isEventClick(event)) menu.select();
        else if (Joystick::isEventDown(event)) menu.next();
        else if (Joystick::isEventUp(event)) menu.prev();

        Serial.println();
        display.clearDisplay();
        display.setCursor(0,0);
        menu.render(display, DISPLAY_ROWS);
        display.display();
        delay(100);

        display.invertDisplay(display_invert);

        pano.motorsEnable(motors_enable);
    }
    menu.cancel(); // go back to main menu to avoid re-triggering
}

/*
 * Interrupt handler triggered by button click
 */
volatile static int button_clicked = false;
void button_click(){
    button_clicked = true;
}
/*
 * Execute panorama from start to finish.
 * Button click interrupts.
 */
void executePano(void){


    button_clicked = false;
    while (joystick.getButtonState()) delay(20);
    pano.start();
    attachInterrupt(digitalPinToInterrupt(JOYSTICK_SW), button_click, FALLING);

    while (running){
        displayPanoStatus();
        pano.shutter();
        running = pano.next();

        if (button_clicked){
            // button was clicked mid-pano, go in manual mode
            int event;
            while (running){
                event=joystick.read();
                if (Joystick::isEventLeft(event)) pano.prev();
                else if (Joystick::isEventRight(event)) pano.next();
                else if (Joystick::isEventClick(event)) break;
                displayPanoStatus();
                display.println("\x11 [ok] \x10\n");
                display.display();
            }
            button_clicked = false;
        }
    };

    // clean up
    detachInterrupt(digitalPinToInterrupt(JOYSTICK_SW));
    running = false;

    pano.end();

    Serial.println((button_clicked) ? F("Canceled") : F("Finished"));
    display.println((button_clicked) ? F("Canceled") : F("Finished"));
    display.display();
    while (!joystick.read()) delay(20);
}

/*
 * This is a callback invoked by selecting "Start"
 */
int onStart(int __){
    menu.cancel();
    camera.setAspect(aspect);
    camera.setFocalLength(focal);
    pano.setShutter(shutter, pre_shutter);
    pano.setShots(shots);
    running = true;

    // set panorama FOV
    positionCamera("Top Left", NULL, NULL);
    positionCamera("Bot Right", &horiz, &vert);
    pano.setFOV(horiz, vert);
    menu.sync();
    positionCamera("Adj start", NULL, NULL);
    executePano();
}

/*
 * This is a callback invoked by selecting "Repeat"
 */
int onRepeat(int __){
    menu.cancel();
    camera.setAspect(aspect);
    camera.setFocalLength(focal);
    pano.setShutter(shutter, pre_shutter);
    pano.setShots(shots);
    running = true;
    pano.setFOV(horiz, vert);
    positionCamera("Adj start", NULL, NULL);
    executePano();
}

/*
 * This is a callback invoked by selecting "Repeat"
 */
int on360(int __){
    menu.cancel();
    camera.setAspect(aspect);
    camera.setFocalLength(focal);
    pano.setShutter(shutter, pre_shutter);
    pano.setShots(shots);
    running = true;
    pano.setFOV(360, vert);
    positionCamera("Adj start", NULL, NULL);
    executePano();
}

void loop() {
    displayMenu();
    menu.sync();
}
