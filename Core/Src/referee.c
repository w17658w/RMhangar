#include "referee.h"
#include <string.h>

/* 置 0 可关闭本文件对 HAL 回调的接管，改由用户自己在回调里调 Referee_OnRxCplt() */
#define REFEREE_USE_HAL_CALLBACK    1

/* =============================== 内部状态 =============================== */

referee_data_t g_referee;

static UART_HandleTypeDef *s_huart  = 0;
static uint8_t             s_rx_byte = 0;
static uint8_t             s_buf[REFEREE_BUF_SIZE];
static uint16_t            s_len    = 0u;   /* 已收字节数 */
static uint16_t            s_expect = 0u;   /* 预算整帧长度，0 表示尚未确定 */

/* =============================== CRC 校验 =============================== */

/**
 * @brief CRC8，反射多项式 0x8C，初值 0xFF
 */
static uint8_t referee_crc8(const uint8_t *p, uint16_t len)
{
    uint8_t  crc = 0xFFu;
    uint16_t i;
    uint8_t  b;

    for (i = 0u; i < len; i++)
    {
        crc = (uint8_t)(crc ^ p[i]);
        for (b = 0u; b < 8u; b++)
        {
            if ((crc & 0x01u) != 0u)
            {
                crc = (uint8_t)((crc >> 1) ^ 0x8Cu);
            }
            else
            {
                crc = (uint8_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/**
 * @brief CRC16，反射多项式 0x8408，初值 0xFFFF
 */
static uint16_t referee_crc16(const uint8_t *p, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t  b;

    for (i = 0u; i < len; i++)
    {
        crc = (uint16_t)(crc ^ (uint16_t)p[i]);
        for (b = 0u; b < 8u; b++)
        {
            if ((crc & 0x0001u) != 0u)
            {
                crc = (uint16_t)((crc >> 1) ^ 0x8408u);
            }
            else
            {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/**
 * @brief 校验帧头：CRC8 覆盖前 4 字节，校验值在第 5 字节
 */
static uint8_t referee_check_crc8(const uint8_t *frame, uint16_t len)
{
    uint8_t expect;

    if (len < REFEREE_HEADER_LEN)
    {
        return 0u;
    }
    /* len 为 5：算前 4 字节，与第 5 字节比较 */
    expect = referee_crc8(frame, (uint16_t)(len - 1u));
    return (expect == frame[len - 1u]) ? 1u : 0u;
}

/**
 * @brief 校验整包：CRC16 覆盖除末 2 字节外的全部内容
 * @note  协议中 CRC16 低字节在前
 */
static uint8_t referee_check_crc16(const uint8_t *frame, uint16_t len)
{
    uint16_t expect;

    if (len < REFEREE_FIX_LEN)
    {
        return 0u;
    }
    expect = referee_crc16(frame, (uint16_t)(len - REFEREE_TAIL_LEN));
    if ((uint8_t)(expect & 0x00FFu) != frame[len - 2u])
    {
        return 0u;
    }
    if ((uint8_t)((expect >> 8) & 0x00FFu) != frame[len - 1u])
    {
        return 0u;
    }
    return 1u;
}

/* =============================== 帧分发 =============================== */

/**
 * @brief 按 cmd_id 把 data 段拷贝进对应缓存
 * @param frame    完整帧起始地址
 * @param data_len 帧中 data 段的实际长度
 */
static void referee_dispatch(const uint8_t *frame, uint16_t data_len)
{
    uint16_t       cmd;
    const uint8_t *d = &frame[REFEREE_DATA_OFFSET];
    uint8_t        got = 0u;

    cmd = (uint16_t)((uint16_t)frame[5] | ((uint16_t)frame[6] << 8));

    switch (cmd)
    {
    case REFEREE_CMD_GAME_STATUS:
        if (data_len >= (uint16_t)sizeof(game_status_t))
        {
            (void)memcpy(&g_referee.game_status, d, sizeof(game_status_t));
            got = 1u;
        }
        break;

    case REFEREE_CMD_GAME_RESULT:
        if (data_len >= (uint16_t)sizeof(game_result_t))
        {
            (void)memcpy(&g_referee.game_result, d, sizeof(game_result_t));
            got = 1u;
        }
        break;

    case REFEREE_CMD_ROBOT_HP:
        if (data_len >= (uint16_t)sizeof(game_robot_HP_t))
        {
            (void)memcpy(&g_referee.robot_hp, d, sizeof(game_robot_HP_t));
            got = 1u;
        }
        break;

    case REFEREE_CMD_EVENT_DATA:
        if (data_len >= (uint16_t)sizeof(event_data_t))
        {
            (void)memcpy(&g_referee.event_data, d, sizeof(event_data_t));
            got = 1u;
        }
        break;

    case REFEREE_CMD_ROBOT_STATUS:
        if (data_len >= (uint16_t)sizeof(robot_status_t))
        {
            (void)memcpy(&g_referee.robot_status, d, sizeof(robot_status_t));
            got = 1u;
        }
        break;

    case REFEREE_CMD_POWER_HEAT:
        if (data_len >= (uint16_t)sizeof(power_heat_data_t))
        {
            (void)memcpy(&g_referee.power_heat, d, sizeof(power_heat_data_t));
            got = 1u;
        }
        break;

    case REFEREE_CMD_BUFF:
        if (data_len >= (uint16_t)sizeof(buff_t))
        {
            (void)memcpy(&g_referee.buff, d, sizeof(buff_t));
            got = 1u;
        }
        break;

    default:
        /* 未处理的命令码（哨兵/雷达/飞镖/图传链路等）：直接丢弃，不计错误 */
        break;
    }

    if (got != 0u)
    {
        g_referee.frame_ok++;
    }
}

/* =============================== 对外接口 =============================== */

void Referee_RxByte(uint8_t byte)
{
    /* 未同步到帧头：只接受 0xA5 作为起点 */
    if (s_len == 0u)
    {
        if (byte == REFEREE_SOF)
        {
            s_buf[s_len] = byte;
            s_len        = 1u;
            s_expect     = 0u;
        }
        return;
    }

    /* 防御：理论上先到 s_expect 就会复位，不会溢出 */
    if (s_len >= REFEREE_BUF_SIZE)
    {
        s_len    = 0u;
        s_expect = 0u;
        g_referee.err_len++;
        return;
    }

    s_buf[s_len] = byte;
    s_len++;

    /* 帧头收齐，先验 CRC8，通过后再信任 data_length */
    if (s_len == REFEREE_HEADER_LEN)
    {
        if (referee_check_crc8(s_buf, REFEREE_HEADER_LEN) == 0u)
        {
            s_len    = 0u;
            s_expect = 0u;
            g_referee.err_crc++;
            return;
        }

        s_expect = (uint16_t)(REFEREE_FIX_LEN
                            + (uint16_t)s_buf[1]
                            + ((uint16_t)s_buf[2] << 8));

        if ((s_expect < REFEREE_FIX_LEN) || (s_expect > REFEREE_BUF_SIZE))
        {
            s_len    = 0u;
            s_expect = 0u;
            g_referee.err_len++;
            return;
        }
    }

    /* 整帧收齐，验 CRC16 后分发 */
    if ((s_expect != 0u) && (s_len == s_expect))
    {
        if (referee_check_crc16(s_buf, s_expect) != 0u)
        {
            referee_dispatch(s_buf, (uint16_t)(s_expect - REFEREE_FIX_LEN));
        }
        else
        {
            g_referee.err_crc++;
        }

        s_len    = 0u;
        s_expect = 0u;
    }
}


void Referee_Init(UART_HandleTypeDef *huart)
{
    if (huart == 0)
    {
        return;
    }

    s_huart  = huart;
    s_len    = 0u;
    s_expect = 0u;

    (void)HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1u);
}

void Referee_OnRxCplt(void)
{
    Referee_RxByte(s_rx_byte);

    if (s_huart != 0)
    {
        (void)HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1u);
    }
}

uint8_t Referee_GetStage(void)
{
    return (uint8_t)g_referee.game_status.game_progress;
}

uint8_t Referee_IsInBattle(void)
{
    return (g_referee.game_status.game_progress == REFEREE_STAGE_IN_BATTLE) ? 1u : 0u;
}

uint16_t Referee_RemainTime(void)
{
    return g_referee.game_status.stage_remain_time;
}


#if (REFEREE_USE_HAL_CALLBACK != 0)

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == s_huart)
    {
        Referee_OnRxCplt();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == s_huart)
    {
        g_referee.err_len++;                      
        (void)HAL_UART_AbortReceive_IT(huart);
        (void)HAL_UART_Receive_IT(huart, &s_rx_byte, 1u);
    }
}

#endif /* REFEREE_USE_HAL_CALLBACK */
