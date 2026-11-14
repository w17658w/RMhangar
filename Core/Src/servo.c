/**
  ******************************************************************************
  * @file    servo.c
  * @brief   LSC 系列 16 路 PWM 舵机控制器驱动实现
  *
  * 舵机转动指令的帧结构（一帧控制 N 个舵机）：
  *   55 55 | 3N+5 | 03 | N | 时间低 时间高 | [ID 位置低 位置高] 重复 N 次
  *
  *   N = 1 时整帧 10 字节：55 55 08 03 01 E8 03 01 D0 07
  *   N = 2 时整帧 13 字节：55 55 0B 03 02 20 03 02 B0 04 09 FC 08
  ******************************************************************************
  */

#include "servo.h"
static UART_HandleTypeDef *s_huart = 0;

/* 位置限幅 */
static uint16_t servo_clamp_pos(uint16_t pos)
{
    if (pos < SERVO_POS_MIN)
    {
        return SERVO_POS_MIN;
    }
    if (pos > SERVO_POS_MAX)
    {
        return SERVO_POS_MAX;
    }
    return pos;
}

/* 时间限幅*/
static uint16_t servo_clamp_time(uint16_t time_ms)
{
    if (time_ms < SERVO_TIME_MIN)
    {
        return SERVO_TIME_MIN;
    }
    if (time_ms > SERVO_TIME_MAX)
    {
        return SERVO_TIME_MAX;
    }
    return time_ms;
}

/**
  * @brief  绑定舵机控制器所在的串口
  * @param  huart 串口句柄
  * @retval 无
  */
void Servo_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

/**
  * @brief  用一条帧让多个舵机同时转动
  * @param  targets 目标数组，每项含通道号与目标位置
  * @param  count   目标个数，1 到 SERVO_MAX_PER_FRAME
  * @param  time_ms 所有舵机共用的转动时间，1 到 9999
  * @retval 无
  */
void Servo_MoveMulti(const servo_target_t *targets, uint8_t count, uint16_t time_ms)
{
    uint8_t buf[7u + 3u * SERVO_MAX_PER_FRAME];
    uint8_t  i;
    uint8_t  k = 0u;
    uint16_t pos;

    if ((s_huart == 0) || (targets == 0) || (count == 0u))
    {
        return;
    }
    if (count > SERVO_MAX_PER_FRAME)
    {
        count = (uint8_t)SERVO_MAX_PER_FRAME;
    }
    time_ms = servo_clamp_time(time_ms);

    buf[k++] = SERVO_HEAD_BYTE;                              /* 帧头 0x55 */
    buf[k++] = SERVO_HEAD_BYTE;                              /* 帧头 0x55 */
    buf[k++] = (uint8_t)(((uint16_t)count * 3u) + 5u);       /* 数据长度 */
    buf[k++] = SERVO_CMD_MOVE;                               /* 指令 0x03 */
    buf[k++] = count;                                        /* 舵机个数 */
    buf[k++] = (uint8_t)(time_ms & 0xFFu);                   /* 时间低八位 */
    buf[k++] = (uint8_t)((time_ms >> 8) & 0xFFu);            /* 时间高八位 */

    for (i = 0u; i < count; i++)
    {
        pos = servo_clamp_pos(targets[i].pos);
        buf[k++] = targets[i].id;                            /* 舵机通道号 */
        buf[k++] = (uint8_t)(pos & 0xFFu);                   /* 位置低八位 */
        buf[k++] = (uint8_t)((pos >> 8) & 0xFFu);            /* 位置高八位 */
    }

    (void)HAL_UART_Transmit(s_huart, buf, (uint16_t)k, 150u);
}

/**
  * @brief  只转一个舵机
  * @param  id      舵机通道号，1 到 16
  * @param  pos     目标位置，500 到 2500（等于脉宽 us）
  * @param  time_ms 转动耗时，1 到 9999
  * @retval 无
  */
void Servo_Move(uint8_t id, uint16_t pos, uint16_t time_ms)
{
    servo_target_t one;

    one.id  = id;
    one.pos = pos;
    Servo_MoveMulti(&one, 1u, time_ms);
}
