/**
  ******************************************************************************
  * @file    servo.h
  * @brief   LSC 系列 16 路 PWM 舵机控制器驱动（串口二次开发接口）
  *
  * 协议依据：LSC 系列控制板通信协议 V1.1，串口 9600 8N1
  *   帧格式：0x55 0x55 | Length | Cmd | Prm1 ... PrmN
  *   Length 等于参数个数 N 加 2
  *
  *   舵机转动指令 CMD_SERVO_MOVE = 0x03，Length = 舵机个数 * 3 + 5
  *     参数1        ：要控制的舵机个数
  *     参数2、3     ：转动时间，低八位在前、高八位在后（单位 ms）
  *     参数4、5、6  ：舵机 ID、位置低八位、位置高八位
  *     参数7、8、9  ：第二个舵机，格式同 4、5、6，按个数重复
  *
  * 舵机 HPS-3518SG：脉宽 500~2500 us，1500 为中位
  *   500 -> 0 度，1500 -> 90 度，2500 -> 180 度
  ******************************************************************************
  */

#ifndef SERVO_H
#define SERVO_H

#include "main.h"

/* ---------- 位置端点---------- */
#define SERVO_POS_MIN       500u        /* 0 度 */
#define SERVO_POS_MID       1500u       /* 90 度，中位 */
#define SERVO_POS_MAX       2500u       /* 180 度 */

/* ---------- 舵机通道 ---------- */
#define SERVO_CH_DOOR_L     1u          /* 舱门左，接控制板 1 号口 */
#define SERVO_CH_DOOR_R     2u          /* 舱门右，接控制板 2 号口 */

/* ---------- 时间参数 ---------- */
#define SERVO_TIME_MIN      1u          /* 最短转动时间 ms */
#define SERVO_TIME_MAX      9999u       /* 最长转动时间 ms */

/* ---------- 一帧最多能带多少个舵机 ---------- */
#define SERVO_MAX_PER_FRAME 16u

/* ---------- 协议常量 ---------- */
#define SERVO_HEAD_BYTE     0x55u
#define SERVO_CMD_MOVE      0x03u      


typedef struct
{
    uint8_t  id;
    uint16_t pos;
} servo_target_t;

void Servo_Init(UART_HandleTypeDef *huart);

/* 单通道，适合临时点动一个舵机 */
void Servo_Move(uint8_t id, uint16_t pos, uint16_t time_ms);

/* 一条帧同时控制 1 到 16 个通道。
 * 同一帧内的舵机是同一起点、同一转动时间，这正是双门需要的同步效果。 */
void Servo_MoveMulti(const servo_target_t *targets, uint8_t count, uint16_t time_ms);

#endif /* SERVO_H */
