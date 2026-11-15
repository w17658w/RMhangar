#include "servo.h"

static TIM_HandleTypeDef *s_htim = 0;

/* 通道号到下标记的映射，加通道时改这里 */
static const uint32_t s_chan[SERVO_CH_NUM] = { TIM_CHANNEL_1, TIM_CHANNEL_2 };

typedef struct
{
    uint16_t cur;       /* 当前下发脉宽 us，插值进行中的实时值 */
    uint16_t start;     /* 本次行程的起点脉宽 */
    uint16_t target;    /* 本次行程的目标脉宽 */
    uint32_t start_ms;  /* 本次行程开始时刻 */
    uint32_t dur_ms;    /* 本次行程总时长 */
    uint8_t  moving;    /* 1 = 占空比还在变化，0 = 已到目标 */
} servo_ctx_t;

static servo_ctx_t s_ctx[SERVO_CH_NUM];

/* ---------------------------- 内部工具 ---------------------------- */

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

static void servo_write(uint8_t ch, uint16_t pos)
{
    __HAL_TIM_SET_COMPARE(s_htim, s_chan[ch], pos);
}

/**
  * @brief  让某个通道在期望时长内走完到目标位置
  * @param  ch       通道号
  * @param  pos      目标脉宽 us
  * @param  want_ms  期望整段行程耗时 ms；短于物理极限则退化为阶跃
  * @retval 无
  */
static void servo_begin_move(uint8_t ch, uint16_t pos, uint32_t want_ms)
{
    servo_ctx_t *c;
    uint32_t travel;
    uint32_t t_min;

    if ((s_htim == 0) || (ch >= SERVO_CH_NUM))
    {
        return;
    }

    pos = servo_clamp_pos(pos);
    c = &s_ctx[ch];

    /* 已经在目标位置，不动 */
    if (c->cur == pos)
    {
        c->target = pos;
        c->moving = 0u;
        servo_write(ch, pos);
        return;
    }

    travel = (c->cur > pos) ? (uint32_t)(c->cur - pos) : (uint32_t)(pos - c->cur);

    /* 该行程的物理极限时长：travel µs ÷ SERVO_PHYS_SPEED µs/s，取整到 ms */
    t_min = (travel * 1000u) / SERVO_PHYS_SPEED;
    if (t_min < SERVO_MOVE_MS_MIN)
    {
        t_min = SERVO_MOVE_MS_MIN;
    }

    if (want_ms < t_min)
    {
        /* 要求得比舵机能做到的还快：不插值，一次性跳到目标 */
        want_ms = SERVO_MOVE_MS_MIN;
    }
    else if (want_ms > SERVO_MOVE_MS_MAX)
    {
        want_ms = SERVO_MOVE_MS_MAX;
    }

    c->start = c->cur;              /* 若正在移动中改目标，从当前实际脉宽重新起算 */
    c->target = pos;
    c->start_ms = HAL_GetTick();
    c->dur_ms = want_ms;
    c->moving = 1u;
}

/* ---------------------------- 对外接口 ---------------------------- */

/**
  * @brief  启动 PWM 并把所有通道压到初始位置
  * @param  htim     TIM 句柄，通常是 &htim1
  * @param  init_us  初始脉宽 us
  * @retval 无
  * @note   必须先写 CCR 再 HAL_TIM_PWM_Start。反过来的话，启动瞬间
  *         输出的是 MX_TIM1_Init 里 sConfigOC.Pulse 的默认值 2000，
  *         舱门会在上电那一刻弹开再回来。
  */
void Servo_Init(TIM_HandleTypeDef *htim, uint16_t init_us)
{
    uint8_t i;

    s_htim = htim;
    if (s_htim == 0)
    {
        return;
    }

    init_us = servo_clamp_pos(init_us);

    for (i = 0u; i < SERVO_CH_NUM; i++)
    {
        s_ctx[i].cur      = init_us;
        s_ctx[i].start    = init_us;
        s_ctx[i].target   = init_us;
        s_ctx[i].start_ms = HAL_GetTick();
        s_ctx[i].dur_ms   = 0u;
        s_ctx[i].moving   = 0u;
        servo_write(i, init_us);
    }

    for (i = 0u; i < SERVO_CH_NUM; i++)
    {
        (void)HAL_TIM_PWM_Start(s_htim, s_chan[i]);
    }
}

/**
  * @brief  多个通道共用同一总时长移动，双门同步用这个
  * @param  targets   目标数组
  * @param  count     个数，超过 SERVO_CH_NUM 会被截断
  * @param  total_ms  整段行程耗时 ms
  * @retval 无
  */
void Servo_MoveMulti(const servo_target_t *targets, uint8_t count, uint16_t total_ms)
{
    uint8_t i;

    if ((targets == 0) || (count == 0u) || (s_htim == 0))
    {
        return;
    }
    if (count > SERVO_CH_NUM)
    {
        count = (uint8_t)SERVO_CH_NUM;
    }

    for (i = 0u; i < count; i++)
    {
        servo_begin_move(targets[i].id, targets[i].pos, (uint32_t)total_ms);
    }
}

/**
  * @brief  推进插值，必须放在主循环里反复调用
  * @retval 无
  * @note   非阻塞。只有处于移动状态的通道才写寄存器，到位后自动停下。
  *         位置由 起点 + 距离 * 已流逝 / 总时长 推出，因此不依赖调用频率。
  */
void Servo_Update(void)
{
    uint32_t now;
    uint8_t i;

    if (s_htim == 0)
    {
        return;
    }

    now = HAL_GetTick();

    for (i = 0u; i < SERVO_CH_NUM; i++)
    {
        servo_ctx_t *c = &s_ctx[i];
        uint32_t el;
        int32_t delta;
        int32_t step;

        if (c->moving == 0u)
        {
            continue;
        }

        el = now - c->start_ms;

        if (el >= c->dur_ms)
        {
            c->cur = c->target;
            c->moving = 0u;
        }
        else
        {
            delta = (int32_t)c->target - (int32_t)c->start;
            /* 用 64 位中间量，避免 delta*el 在 32 位上溢出 */
            step = (int32_t)(((int64_t)delta * (int64_t)el) / (int64_t)c->dur_ms);
            c->cur = (uint16_t)((int32_t)c->start + step);
        }

        servo_write(i, c->cur);
    }
}
