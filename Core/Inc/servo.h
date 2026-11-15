#ifndef SERVO_H
#define SERVO_H

#include "main.h"

#define SERVO_POS_MIN       500u
#define SERVO_POS_MID      1500u
#define SERVO_POS_MAX      2500u
#define SERVO_CH_DOOR_L     0u
#define SERVO_CH_DOOR_R     1u
#define SERVO_CH_NUM        2u
#define SERVO_PHYS_SPEED    6061u    //us/s
#define SERVO_MOVE_MS_MIN   1u       //ms   
#define SERVO_MOVE_MS_MAX   10000u   //ms

typedef struct
{
    uint8_t  id;    //通道号
    uint16_t pos;   //目标脉宽 us
} servo_target_t;

void Servo_Init(TIM_HandleTypeDef *htim, uint16_t init_us); //初始化到指定脉宽

void Servo_MoveMulti(const servo_target_t *targets, uint8_t count, uint16_t total_ms);//同步转动

void Servo_Update(void);

#endif 
