/*
 * Pano engine
 *
 * Copyright (C)2016 Laurentiu Badea
 *
 * This file may be redistributed under the terms of the MIT license.
 * A copy of this license has been included with this distribution in the file LICENSE.
 */
#ifndef PANO_H_
#define PANO_H_
#include <BasicStepperDriver.h>
#include "camera.h"
#include "mpu.h"

// Calculate maximum allowed movement at given focal length and shutter
// =angular velocity that would cause a pixel to overlap next one within shot time
#define STEADY_TARGET(fov, shutter, resolution) (100*1000/shutter*fov/resolution)
// how long to wait for steady camera before giving up (ms)
#define STEADY_TIMEOUT 10000
#define CAMERA_RESOLUTION 4000

#define Motor BasicStepperDriver
// can do 60 @ 0.8A, 20 @ 0.3A & 1:16
#define HORIZ_MOTOR_RPM 20
// can do 180 @ 0.8A. 60 @ 0.3A & 1:16
#define VERT_MOTOR_RPM 60

#define MIN_OVERLAP 20

// formula used to lower speed for short movements, to reduce shake
#define DYNAMIC_RPM(rpm, angle) ((abs(angle)>10) ? rpm : rpm/4)
#define DYNAMIC_HORIZ_RPM(angle) DYNAMIC_RPM(HORIZ_MOTOR_RPM, angle)
#define DYNAMIC_VERT_RPM(angle) DYNAMIC_RPM(VERT_MOTOR_RPM, angle)

class Pano {
protected:
    Motor& horiz_motor;
    Motor& vert_motor;
    Camera& camera;
    MPU& mpu;
    int motors_pin;
    float horiz_move;
    float vert_move;
    int horiz_count;
    int vert_count;
    unsigned shots_per_position = 1;
    unsigned shutter_delay = 1000/250;
    unsigned pre_shutter_delay = 0;
    unsigned post_shutter_delay = 100;
    bool shutter_long_pulse = false;
    void gridFit(int total_size, int overlap, float& block_size, int& count);
    // state information
public:
    const int horiz_gear_ratio = 5; // 1:5
    const int vert_gear_ratio = 15; // 1:15
    int horiz_fov;
    int vert_fov;
    unsigned position;

    float horiz_home_offset = 0;
    float vert_home_offset = 0;

    unsigned steady_delay_avg = 100;

    // configuration
    Pano(Motor& horiz_motor, Motor& vert_motor, Camera& camera, MPU& mpu, int motors_pin);
    void setFOV(int horiz_angle, int vert_angle);
    void setShutter(unsigned shutter_delay, unsigned pre_delay, unsigned post_wait, bool long_pulse);
    void setShots(unsigned shots);
    void setMode(unsigned mode);
    void computeGrid(void);
    unsigned getHorizShots(void);
    unsigned getVertShots(void);

    // pano info
    int getCurRow(void);
    int getCurCol(void);
    unsigned getTimeLeft(void);

    // pano execution
    void start(void);
    bool next(void);
    bool prev(void);
    void end(void);
    void run(void);
    void shutter(void);
    void motorsEnable(bool on);

    // positioning within pano
    bool moveTo(int new_position);
    bool moveTo(int new_row, int new_col);

    // pre-positioning (fine grained)
    void setMotorsHomePosition(void);
    void setZeroElevation(void);
    void moveMotorsHome(void);
    void moveMotors(float h, float v);
    void moveMotorsAdaptive(float h, float v);
};

#endif /* PANO_H_ */
