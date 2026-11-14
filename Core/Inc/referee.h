#ifndef __REFEREE_H__
#define __REFEREE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

/* ============================ 协议常量 ============================ */

#define REFEREE_SOF             0xA5u   /* 数据帧起始字节 */
#define REFEREE_HEADER_LEN      5u      /* SOF(1) + data_length(2) + seq(1) + CRC8(1) */
#define REFEREE_CMD_LEN         2u      /* cmd_id 2 字节 */
#define REFEREE_TAIL_LEN        2u      /* CRC16 2 字节，整包校验 */
#define REFEREE_DATA_OFFSET     7u      /* 5 + 2，data 段起始偏移 */
#define REFEREE_FIX_LEN         9u      /* 不包含 data 的固定开销：5 + 2 + 2 */
#define REFEREE_MAX_DATA        300u    /* 协议中最大数据段长度（0x0310 图传链路） */
#define REFEREE_BUF_SIZE        (REFEREE_FIX_LEN + REFEREE_MAX_DATA)   /* 309 */

/* ============================ 命令码 ID ============================ */

#define REFEREE_CMD_GAME_STATUS     0x0001u  /* 比赛状态，服务器 1Hz */
#define REFEREE_CMD_GAME_RESULT     0x0002u  /* 比赛结果，结束触发 */
#define REFEREE_CMD_ROBOT_HP        0x0003u  /* 机器人血量，服务器 3Hz */
#define REFEREE_CMD_EVENT_DATA      0x0101u  /* 场地事件，己方 1Hz */
#define REFEREE_CMD_ROBOT_STATUS    0x0201u  /* 机器人性能体系，主控 10Hz */
#define REFEREE_CMD_POWER_HEAT      0x0202u  /* 缓冲能量与热量，主控 10Hz */
#define REFEREE_CMD_BUFF            0x0204u  /* 机器人增益，服务器 3Hz */

/* ======================= 比赛阶段（0x0001 bit4-7） ======================= */

#define REFEREE_STAGE_NOT_STARTED   0u   /* 未开始比赛 */
#define REFEREE_STAGE_PREPARE       1u   /* 准备阶段 */
#define REFEREE_STAGE_SELF_CHECK    2u   /* 十五秒裁判系统自检阶段 */
#define REFEREE_STAGE_COUNTDOWN     3u   /* 五秒倒计时 */
#define REFEREE_STAGE_IN_BATTLE     4u   /* 比赛中 */
#define REFEREE_STAGE_SETTLEMENT    5u   /* 比赛结算中 */

/* ============================ 数据结构 ============================ */
#if defined(__CC_ARM)
#pragma pack(1)        
#else
#pragma pack(push, 1)    
#endif

/* 0x0001  比赛状态*/
typedef struct
{
    uint8_t  game_type     : 4;   /* bit0-3 比赛类型：1 RMUC / 2 单项赛 / 3 ICRA / 4 RMUL 3V3 / 5 RMUL 步兵对抗 */
    uint8_t  game_progress : 4;   /* bit4-7 当前比赛阶段，见 REFEREE_STAGE_xxx */
    uint16_t stage_remain_time;   /* 当前阶段剩余时间，单位 s */
    uint64_t SyncTimeStamp;       /* UNIX 时间 */
} game_status_t;

/* 0x0002  比赛结果*/
typedef struct
{
    uint8_t winner;               /* 0 平局 / 1 红方胜 / 2 蓝方胜 */
} game_result_t;

/* 0x0003  机器人血量*/
typedef struct
{
    uint16_t ally_1_robot_HP;     /* 己方 1 号重装机器人血量 */
    uint16_t reserved;            /* 保留位 */
    uint16_t ally_3_robot_HP;     /* 己方 3 号步兵 */
    uint16_t ally_4_robot_HP;     /* 己方 4 号步兵 */
    int16_t  damage_difference;   /* 己方全队总伤害与对方之差 */
    uint16_t ally_7_robot_HP;     /* 己方 7 号哨兵 */
    uint16_t outpost_R_HP;        /* R 前哨血量 */
    uint16_t ally_base_HP;        /* 己方基地血量 */
    uint16_t outpost_B_HP;        /* B 前哨血量 */
    uint16_t enemy_base_HP;       /* 对方基地血量 */
    uint16_t ally_control_degree; /* 己方累计控制度 */
    uint16_t enemy_control_degree;/* 对方累计控制度 */
} game_robot_HP_t;

/* 0x0101  场地事件 */
typedef struct
{
    uint32_t event_data;
    uint32_t event_data_2;
} event_data_t;

/* 0x0201  机器人性能体系 */
typedef struct
{
    uint8_t  robot_id;                        /* 本机器人 ID */
    uint8_t  robot_level;                     /* 机器人等级 */
    uint16_t current_HP;                      /* 当前血量 */
    uint16_t maximum_HP;                      /* 血量上限 */
    uint16_t shooter_barrel_cooling_value;    /* 射击热量每秒冷却值 */
    uint16_t shooter_barrel_heat_limit;       /* 射击热量上限 */
    uint16_t chassis_power_limit;             /* 底盘功率上限 */
    float    bullet_speed_limit;              /* 射击初速度上限 */
    uint8_t  power_management_gimbal_output  : 1;  /* bit0 gimbal 口 24V 输出 */
    uint8_t  power_management_chassis_output : 1;  /* bit1 chassis 口 24V 输出 */
    uint8_t  power_management_shooter_output : 1;  /* bit2 shooter 口 24V 输出 */
} robot_status_t;

/* 0x0202  实时缓冲能量与射击热量 */
typedef struct
{
    uint16_t reserved_0;
    uint16_t reserved_1;
    float    reserved_2;
    uint16_t buffer_energy;              /* 缓冲能量，单位 J */
    uint16_t shooter_17mm_barrel_heat;   /* 17mm 发射机构射击热量 */
    uint16_t shooter_42mm_barrel_heat;   /* 42mm 发射机构射击热量 */
} power_heat_data_t;

/* 0x0204  机器人增益 */
typedef struct
{
    uint8_t  recovery_buff;      /* 回血增益百分比 */
    uint16_t cooling_buff;       /* 冷却增益，单位 /s */
    uint8_t  defence_buff;       /* 防御增益百分比 */
    uint8_t  vulnerability_buff; /* 负防御增益百分比 */
    uint16_t attack_buff;        /* 攻击增益百分比 */
} buff_t;

#if defined(__CC_ARM)
#pragma pack()         
#else
#pragma pack(pop)
#endif

/* ============================ 解析结果缓存 ============================ */

typedef struct
{
    game_status_t     game_status;
    game_result_t     game_result;
    game_robot_HP_t   robot_hp;
    event_data_t      event_data;
    robot_status_t    robot_status;
    power_heat_data_t power_heat;
    buff_t            buff;

    /* 链路统计，用于现场排查：收到多少帧、多少 CRC 错、多少长度非法 */
    volatile uint32_t frame_ok;
    volatile uint32_t err_crc;
    volatile uint32_t err_len;
} referee_data_t;

extern referee_data_t g_referee;


/**绑定串口并启动单字节中断接收。*/
void Referee_Init(UART_HandleTypeDef *huart);


/*在 HAL_UART_RxCpltCallback 中调用：取走接收到的字节并重新挂载中断*/
void Referee_OnRxCplt(void);

/* 取 0x0001 帧的 game_progress 阶段码（0-5），业务层判断阶段用 */
uint8_t Referee_GetStage(void);

/*1 比赛中；0 其它*/
uint8_t Referee_IsInBattle(void);

/*当前阶段剩余时间*/
uint16_t Referee_RemainTime(void);

/* 便捷宏：判断收到过某条命令后用对应字段 */
#define REFEREE_HAS_FRAME()     (g_referee.frame_ok > 0u)

#ifdef __cplusplus
}
#endif

#endif /* __REFEREE_H__ */
