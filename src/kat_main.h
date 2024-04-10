#ifndef __KAT_MAIN_H__
#define __KAT_MAIN_H__
#include <stdbool.h>
#include <stdint.h>

/*
 * Internal state representation types.
 */

typedef enum __packed
{
    KAT_NONE = 0,
    KAT_DIR,
    KAT_LEFT,
    KAT_RIGHT,
    _KAT_MAX_DEVICE
} eKatDevice;

// Internal representation

typedef struct
{
    // Status
    uint8_t firmwareVersion;
    uint8_t chargeStatus;
    uint16_t chargeLevel;
    bool freshStatus;
    // TODO: add timestamp
    bool freshData;
    int64_t connect_ts;
} sKatDeviceStatus;

typedef struct
{
    // Latest data
    uint8_t status1;
    uint8_t status2;
    int16_t speed_x;
    int16_t speed_y;
    uint8_t color;
} sKatFootDevice;

typedef struct
{
    // Latest data
    uint16_t axis[7];
    uint8_t button;
} sKatDirDevice;

typedef struct
{
    eKatDevice deviceType;
    sKatDeviceStatus deviceStatus;
    union
    {
        sKatFootDevice footData;
        sKatDirDevice dirData;
    };
} sKatDeviceInfo;


/*
 * System state.
 */

extern sKatDeviceInfo KatDeviceInfo[];

#ifdef CONFIG_APP_FEET_ROTATION
// Angle to turn data from left and right sensors before sending to PC.
extern float KatCorrectLeft;
extern float KatCorrectRight;
#endif


/*
 * Settings saving export.
 */
void kat_settings_async_save(void);


/*
 * Macro helper to create function that will run with slight delay from worker thread.
 *
 * Usage:
 *    ASYNC_FUNC(myfunc, K_MSEC(100))
 *    {
 *       printk("myfunc is called");
 *    }
 * From any other code:
 *    myfunc();
 */
#define ASYNC_FUNC(name, delay)                                 \
    void _do_async_##name(struct k_work *);                     \
    K_WORK_DEFINE(_worker_##name, _do_async_##name);            \
    void _timer_exp_##name(struct k_timer *timer)               \
    {                                                           \
        k_work_submit(&_worker_##name);                         \
    }                                                           \
    K_TIMER_DEFINE(_timer_##name, _timer_exp_##name, NULL);     \
    void name(void)                                             \
    {                                                           \
        k_timer_start(&_timer_##name, delay, K_NO_WAIT);        \
    }                                                           \
    void _do_async_##name(struct k_work *)


// Enable configuration subsystem if any of the dynamic params enabled
#if defined(CONFIG_APP_FEET_ROTATION) || defined(CONFIG_APP_KAT_FREQ_PARAM)
#define __KAT_CONFIG_COMMANDS__
#endif

#endif // __KAT_MAIN_H__