#ifndef GF_LOG
#define GF_LOG

#include <linux/printk.h>
#define LOG_TAG "gf_"
#define GF_DRV_NAME "gf3626"

typedef enum {
    GF_LOG_LEVEL_ERR, /* Error   */
    GF_LOG_LEVEL_WRN, /* Warning */
    GF_LOG_LEVEL_INF, /* Info    */
    GF_LOG_LEVEL_DBG, /* Debug   */
    GF_LOG_LEVEL_VBS, /* Verbose */
    GF_LOG_LEVEL_ALL,
} gf_log_level;

static gf_log_level log_level =  GF_LOG_LEVEL_INF;

#define GF_LOGV(...)                                               \
    do {                                                           \
        if(log_level >= GF_LOG_LEVEL_VBS){                         \
            gf_log_printf(GF_LOG_LEVEL_VBS, LOG_TAG, __VA_ARGS__);      \
        };                                                         \
    } while (0)

#define GF_LOGD(...)                                               \
    do {                                                           \
        if(log_level >= GF_LOG_LEVEL_DBG){                         \
            gf_log_printf(GF_LOG_LEVEL_DBG, LOG_TAG, __VA_ARGS__);      \
        };                                                         \
    } while (0)

#define GF_LOGI(...)                                               \
    do {                                                           \
        if(log_level >= GF_LOG_LEVEL_INF){                         \
            gf_log_printf(GF_LOG_LEVEL_INF, LOG_TAG, __VA_ARGS__);       \
        };                                                         \
    } while (0)

#define GF_LOGW(...)                                               \
    do {                                                           \
        if(log_level >= GF_LOG_LEVEL_WRN){                         \
            gf_log_printf(GF_LOG_LEVEL_WRN, LOG_TAG, __VA_ARGS__);    \
        };                                                         \
    } while (0)

#define GF_LOGE(...)                                               \
    do {                                                           \
        if(log_level >= GF_LOG_LEVEL_ERR){                         \
            gf_log_printf(GF_LOG_LEVEL_ERR, LOG_TAG, __VA_ARGS__);        \
        };                                                         \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

void gf_log_printf(gf_log_level level, const char *tag, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif