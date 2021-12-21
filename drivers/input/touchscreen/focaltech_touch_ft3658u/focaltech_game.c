#ifdef CONFIG_TCT_TOUCH_GAME_MODE
#include "focaltech_core.h"

#define GAME_ENABLE_REG 0xC3
#define REPORT_RATE_REG 0x88
#define SENSITIVITY_REG 0x9D
#define DRAW_ALPHA      0xBC

#ifdef CONFIG_TCT_LITO_CHICAGO
#define DRAW_ALPHA_LEVEL_REGULAR  0x50
#define DRAW_ALPHA_LEVEL_HIGH     0x50
#define DRAW_ALPHA_LEVEL_HIGHEST  0x28
#else
#define DRAW_ALPHA_LEVEL_REGULAR  0xA0
#define DRAW_ALPHA_LEVEL_HIGH     0x50
#define DRAW_ALPHA_LEVEL_HIGHEST  0x28
#endif


extern void (*tct_game_mode_enable)(bool enable);
extern void (*tct_game_touch_report_rate_set)(unsigned int rate);
extern void (*tct_game_touch_smooth_set)(unsigned int level);
extern void (*tct_game_touch_sensitivity_set)(unsigned int level);
extern void (*tct_game_accidental_touch_set)(unsigned int level);

#if 0
static u8 accidental_touch_max[60] = {
0x0A, 0x00, 0x00, 0x04, 0x38, 0x0A, 0x00, 0x00, 0x04, 0x38,
0x0A, 0x00, 0x00, 0x09, 0x60, 0x0A, 0x00, 0x00, 0x09, 0x60,
0x28, 0x00, 0x00, 0x04, 0x38, 0x28, 0x00, 0x00, 0x04, 0x38,
0x28, 0x00, 0x00, 0x09, 0x60, 0x28, 0x00, 0x00, 0x09, 0x60,
0x00, 0xC8, 0x00, 0xC8, 0x00, 0xC8, 0x00, 0xC8, 0x00, 0xC8,
0x00, 0xC8, 0x00, 0xC8, 0x00, 0xC8, 0x00, 0x00, 0x00, 0x00
};

static u8 accidental_touch_middle[60] = {
0x08, 0x00, 0x00, 0x04, 0x38, 0x08, 0x00, 0x00, 0x04, 0x38, 
0x08, 0x00, 0x00, 0x09, 0x60, 0x08, 0x00, 0x00, 0x09, 0x60, 
0x20, 0x00, 0x00, 0x04, 0x38, 0x20, 0x00, 0x00, 0x04, 0x38, 
0x20, 0x00, 0x00, 0x09, 0x60, 0x20, 0x00, 0x00, 0x09, 0x60, 
0x00, 0x96, 0x00, 0x64, 0x00, 0x96, 0x00, 0x96, 0x00, 0x96, 
0x00, 0x96, 0x00, 0x96, 0x00, 0x96, 0x00, 0x00, 0x00, 0x00
};

static u8 accidental_touch_min[60] = {
0x04, 0x00, 0x00, 0x04, 0x38, 0x04, 0x00, 0x00, 0x04, 0x38, 
0x04, 0x00, 0x00, 0x09, 0x60, 0x04, 0x00, 0x00, 0x09, 0x60, 
0x14, 0x00, 0x00, 0x04, 0x38, 0x14, 0x00, 0x00, 0x04, 0x38, 
0x14, 0x00, 0x00, 0x09, 0x60, 0x14, 0x00, 0x00, 0x09, 0x60, 
0x00, 0x64, 0x00, 0x64, 0x00, 0x64, 0x00, 0x64, 0x00, 0x64, 
0x00, 0x64, 0x00, 0x64, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00
};
#endif

static void fts_game_mode_enable(bool enable)
{
    if (enable)
        fts_write_reg(GAME_ENABLE_REG, 0x01);
    else {
        fts_write_reg(GAME_ENABLE_REG, 0x00);
        tct_grip_mode_set(fts_data->tct_grip_mode);
    }
}

static void fts_report_rate_set(unsigned int rate)
{
    switch (rate) {
        case 1:
            fts_write_reg(REPORT_RATE_REG, 0x0F);
            break;
        case 2:
        case 3:
            fts_write_reg(REPORT_RATE_REG, 0x1E);
            break;
        default:
            fts_write_reg(REPORT_RATE_REG, 0x0F);
            break;
    }
}

static void fts_sensitivity_set(unsigned int level)
{
#ifdef CONFIG_TCT_LITO_CHICAGO
    switch (level) {
        case 1:
            fts_write_reg(SENSITIVITY_REG, 0x02);
            break;
        case 2:
        case 3:
        case 4:
        case 5:
            fts_write_reg(SENSITIVITY_REG, 0x03);
            break;
    }
#else
    if (level>=1 && level<=5)
        fts_write_reg(SENSITIVITY_REG, level);
    else
        fts_write_reg(SENSITIVITY_REG, 0x03);
#endif
}

static void fts_smooth_set(unsigned int level)
{
    switch (level) {
        case 1:
            fts_write_reg(DRAW_ALPHA, DRAW_ALPHA_LEVEL_HIGHEST);
            break;
        case 2:
            fts_write_reg(DRAW_ALPHA, DRAW_ALPHA_LEVEL_HIGH);
            break;
        case 3:
            fts_write_reg(DRAW_ALPHA, DRAW_ALPHA_LEVEL_REGULAR);
            break;
        default:
            fts_write_reg(DRAW_ALPHA, DRAW_ALPHA_LEVEL_HIGH);
            break;
    }
}

#if 0
void fts_accidental_touch_set(unsigned int level)
{
    u8 buf[61] = { 0 };

    buf[0] = 0xC8;
    switch (level) {
        case 1:
            memcpy(&buf[1], accidental_touch_min, 60);
            break;
        case 2:
            memcpy(&buf[1], accidental_touch_middle, 60);
            break;
        case 3:
            memcpy(&buf[1], accidental_touch_max, 60);
            break;
	}

    fts_write(buf, 61);
}
#else
void fts_accidental_touch_set(unsigned int level)
{
    struct fts_ts_data *ts_data = fts_data;

    pr_info("%s: level=%d", __func__, level);
    if (level >=0 && level <=3)
        ts_data->game_grip_level = level;
}
#endif

void fts_game_init(void)
{
    tct_game_touch_report_rate_set = fts_report_rate_set;
    tct_game_mode_enable = fts_game_mode_enable;
    tct_game_touch_smooth_set = fts_smooth_set;
    tct_game_touch_sensitivity_set = fts_sensitivity_set;
    tct_game_accidental_touch_set = fts_accidental_touch_set;
}
#endif