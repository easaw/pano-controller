/*
 * Camera control
 *
 * Copyright (C)2016 Laurentiu Badea
 *
 * This file may be redistributed under the terms of the MIT license.
 * A copy of this license has been included with this distribution in the file LICENSE.
 */

#include "camera.h"
#include <Arduino.h>

// taken from https://en.wikipedia.org/wiki/Angle_of_view for now
const unsigned Camera::focal_lengths[] = {
        12, 14, 16, 20, 24, 35, 50, 70, 105, 200, 300, 400, 500, 640
};

Camera::Camera(int focus_pin, int shutter_pin)
:focus_pin(focus_pin),
 shutter_pin(shutter_pin)
{
    pinMode(focus_pin, OUTPUT);
    pinMode(shutter_pin, OUTPUT);
    digitalWrite(focus_pin, HIGH);
    digitalWrite(shutter_pin, HIGH);
    setFocalLength(24);
}

void Camera::shutter(int delay_ms, bool long_pulse){
    int shutter_pulse = (long_pulse) ? delay_ms : SHUTTER_PULSE;
    digitalWrite(focus_pin, LOW);
    digitalWrite(shutter_pin, LOW);
    delay(min(shutter_pulse, delay_ms));
    digitalWrite(focus_pin, HIGH);
    digitalWrite(shutter_pin, HIGH);
    if (delay_ms > shutter_pulse){
        delay(delay_ms - shutter_pulse);
    }
}

unsigned Camera::setFocalLength(unsigned focal_length){
    unsigned i;
    for (i=0; i<sizeof(focal_lengths) && focal_lengths[i] < focal_length; i++);
    lens_idx = i < sizeof(focal_lengths) ? i : sizeof(focal_lengths)-1;
    return lens_idx;
}

unsigned Camera::getFocalLength(void){
    return focal_lengths[lens_idx];
}

float Camera::getHorizFOV(void){
    float d;
    switch (aspect){
    case 32:
        d = 36;
        break;
    case 23:
        d = 24;
        break;
    default:
        d = 36;
    }
    return calcFOV(d);
}

float Camera::getVertFOV(void){
    float d;
    switch (aspect){
    case 32:
        d = 24;
        break;
    case 23:
        d = 36;
        break;
    default:
        d = 24;
    }
    return calcFOV(d);
}

/*
 * Set image aspect ratio
 *
 * @param aspect: number indicating WH, for example 32 for 3:2
 * allowable values are 32 (landscape) and 23 (portrait)
 */
void Camera::setAspect(int aspect){
    this->aspect = aspect;
}
