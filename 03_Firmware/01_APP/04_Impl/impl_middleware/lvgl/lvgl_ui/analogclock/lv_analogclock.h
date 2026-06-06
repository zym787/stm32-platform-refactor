/**
 * @file lv_analogclock.h
 *
 */

/**
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LV_ANALOGCLOCK_H
#define LV_ANALOGCLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "board_types.h"
#include"lvgl.h"
 #define LV_USE_ANALOGCLOCK 1
#if LV_USE_ANALOGCLOCK != 0

/*Testing of dependencies*/
#if LV_DRAW_COMPLEX == 0
#error "lv_analogclock: Complex drawing is required. Enable it in lv_conf.h (LV_DRAW_COMPLEX 1)"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    lv_color_t tick_color;
    UINT16_t tick_cnt;
    UINT16_t tick_length;
    UINT16_t tick_width;

    lv_color_t tick_major_color;
    UINT16_t tick_major_nth;
    UINT16_t tick_major_length;
    UINT16_t tick_major_width;

    BOOL hide_label;
    INT16_t label_gap;
    INT16_t label_color;

    INT32_t min;
    INT32_t max;
    INT16_t r_mod;
    UINT16_t angle_range;
    INT16_t rotation;
} lv_analogclock_scale_t;

enum {
    LV_analogclock_INDICATOR_TYPE_NEEDLE_IMG,
    LV_analogclock_INDICATOR_TYPE_NEEDLE_LINE,
    LV_analogclock_INDICATOR_TYPE_SCALE_LINES,
    LV_analogclock_INDICATOR_TYPE_ARC,
};
typedef UINT8_t lv_analogclock_indicator_type_t;

typedef struct {
    lv_analogclock_scale_t * scale;
    lv_analogclock_indicator_type_t type;
    lv_opa_t opa;
    INT32_t start_value;
    INT32_t end_value;
    union {
        struct {
            const void * src;
            lv_point_t pivot;
        } needle_img;
        struct {
            UINT16_t width;
            INT16_t r_mod;
            lv_color_t color;
        } needle_line;
        struct {
            UINT16_t width;
            const void * src;
            lv_color_t color;
            INT16_t r_mod;
        } arc;
        struct {
            INT16_t width_mod;
            lv_color_t color_start;
            lv_color_t color_end;
            UINT8_t local_grad  : 1;
        } scale_lines;
    } type_data;
} lv_analogclock_indicator_t;

/*Data of line analogclock*/
typedef struct {
    lv_obj_t obj;
    lv_ll_t scale_ll;
    lv_analogclock_scale_t * scale;
    lv_ll_t indicator_ll;
    BOOL hide_point;
    lv_analogclock_indicator_t * hour_indic;
    lv_analogclock_indicator_t * min_indic;
    lv_analogclock_indicator_t * sec_indic;
} lv_analogclock_t;

extern const lv_obj_class_t lv_analogclock_class;

/**
 * `type` field in `lv_obj_draw_part_dsc_t` if `class_p = lv_analogclock_class`
 * Used in `LV_EVENT_DRAW_PART_BEGIN` and `LV_EVENT_DRAW_PART_END`
 */
typedef enum {
    LV_analogclock_DRAW_PART_ARC,             /**< The arc indicator*/
    LV_analogclock_DRAW_PART_NEEDLE_LINE,     /**< The needle lines*/
    LV_analogclock_DRAW_PART_NEEDLE_IMG,      /**< The needle images*/
    LV_analogclock_DRAW_PART_TICK,            /**< The tick lines and labels*/
} lv_analogclock_draw_part_type_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a analogclock object
 * @param parent pointer to an object, it will be the parent of the new bar.
 * @return pointer to the created analogclock
 */
lv_obj_t * lv_analogclock_create(lv_obj_t * parent);

/*=====================
 * Add scale
 *====================*/

/**
 * Add a new scale to the analogclock.
 * @param obj   pointer to a analogclock object
 * @return      the new scale
 * @note        Indicators can be attached to scales.
 */
lv_analogclock_scale_t * lv_analogclock_add_scale(lv_obj_t * obj);

/*=====================
 * Add scale
 *====================*/
/**
 * Set the properties of the ticks of a scale
 * @param obj       pointer to a analogclock object
 * @param scale     pointer to scale (added to `analogclock`)
 * @param cnt       number of tick lines
 * @param width     width of tick lines
 * @param len       length of tick lines
 * @param color     color of tick lines
 */
void lv_analogclock_set_ticks(lv_obj_t * obj, UINT16_t width, UINT16_t len, lv_color_t color);

/**
 * Make some "normal" ticks major ticks and set their attributes.
 * Texts with the current value are also added to the major ticks.
 * @param obj           pointer to a analogclock object
 * @param scale         pointer to scale (added to `analogclock`)
 * @param nth           make every Nth normal tick major tick. (start from the first on the left)
 * @param width         width of the major ticks
 * @param len           length of the major ticks
 * @param color         color of the major ticks
 * @param label_gap     gap between the major ticks and the labels
 */
void lv_analogclock_set_major_ticks(lv_obj_t * obj, UINT16_t width, UINT16_t len, lv_color_t color, INT16_t label_gap);


/**
 * Set the value and angular range of a scale.
 * @param obj           pointer to a analogclock object
 * @param scale         pointer to scale (added to `analogclock`)
 * @param min           the minimum value
 * @param max           the maximal value
 * @param angle_range   the angular range of the scale
 * @param rotation      the angular offset from the 3 o'clock position (clock-wise)
 */
void lv_analogclock_set_scale_range(lv_obj_t * obj, lv_analogclock_scale_t * scale, INT32_t min, INT32_t max,
                                    UINT32_t angle_range,
                                    UINT32_t rotation);

/*=====================
 * Hide digits / centerpoint
 *====================*/

/**
 * Hide the digits or not
 * @param obj           pointer to a analogclock object
 * @param hide_digits   set whether has digits
 */
void lv_analogclock_hide_digits(lv_obj_t * obj, BOOL hide_digits);

/**
 * Hide the center point or not
 * @param obj           pointer to a analogclock object
 * @param hide_point    set whether has center point
 */
void lv_analogclock_hide_point(lv_obj_t * obj, BOOL hide_point);

/*=====================
 * Add indicator
 *====================*/

/**
 * Add a needle line indicator the scale
 * @param obj           pointer to a analogclock object
 * @param scale         pointer to scale (added to `analogclock`)
 * @param width         width of the line
 * @param color         color of the line
 * @param r_mod         the radius modifier (added to the scale's radius) to get the lines length
 * @return              the new indicator
 */
lv_analogclock_indicator_t * lv_analogclock_add_needle_line(lv_obj_t * obj, lv_analogclock_scale_t * scale,
                                                            UINT16_t width,
                                                            lv_color_t color, INT16_t r_mod);
void lv_analogclock_set_hour_needle_line(lv_obj_t * obj, UINT16_t width,
                                         lv_color_t color, INT16_t r_mod);
void lv_analogclock_set_min_needle_line(lv_obj_t * obj, UINT16_t width,
                                        lv_color_t color, INT16_t r_mod);
void lv_analogclock_set_sec_needle_line(lv_obj_t * obj, UINT16_t width,
                                        lv_color_t color, INT16_t r_mod);

/**
 * Add a needle image indicator the scale
 * @param obj           pointer to a analogclock object
 * @param scale         pointer to scale (added to `analogclock`)
 * @param src           the image source of the indicator. path or pointer to ::lv_img_dsc_t
 * @param pivot_x       the X pivot point of the needle
 * @param pivot_y       the Y pivot point of the needle
 * @return              the new indicator
 * @note                the needle image should point to the right, like -O----->
 */
lv_analogclock_indicator_t * lv_analogclock_add_needle_img(lv_obj_t * obj, lv_analogclock_scale_t * scale,
                                                           const void * src,
                                                           lv_coord_t pivot_x, lv_coord_t pivot_y);
void lv_analogclock_set_hour_needle_img(lv_obj_t * obj, const void * src,
                                        lv_coord_t pivot_x, lv_coord_t pivot_y);
void lv_analogclock_set_min_needle_img(lv_obj_t * obj, const void * src,
                                       lv_coord_t pivot_x, lv_coord_t pivot_y);
void lv_analogclock_set_sec_needle_img(lv_obj_t * obj, const void * src,
                                       lv_coord_t pivot_x, lv_coord_t pivot_y);

/**
 * Add an arc indicator the scale
 * @param obj           pointer to a analogclock object
 * @param scale         pointer to scale (added to `analogclock`)
 * @param width         width of the arc
 * @param color         color of the arc
 * @param r_mod         the radius modifier (added to the scale's radius) to get the outer radius of the arc
 * @return              the new indicator
 */
lv_analogclock_indicator_t * lv_analogclock_add_arc(lv_obj_t * obj, UINT16_t width, lv_color_t color,
                                                    INT16_t r_mod);


/**
 * Add a scale line indicator the scale. It will modify the ticks.
 * @param obj           pointer to a analogclock object
 * @param scale         pointer to scale (added to `analogclock`)
 * @param color_start   the start color
 * @param color_end     the end color
 * @param local         tell how to map start and end color. true: the indicator's start and end_value; false: the scale's min max value
 * @param width_mod     add this the affected tick's width
 * @return              the new indicator
 */
lv_analogclock_indicator_t * lv_analogclock_add_scale_lines(lv_obj_t * obj, lv_color_t color_start,
                                                            lv_color_t color_end, BOOL local, INT16_t width_mod);

/*=====================
 * Set indicator value
 *====================*/

/**
 * Set the value of the indicator. It will set start and and value to the same value
 * @param obj           pointer to a analogclock object
 * @param indic         pointer to an indicator
 * @param value         the new value
 */
void lv_analogclock_set_indicator_value(lv_obj_t * obj, lv_analogclock_indicator_t * indic, INT32_t value);

/**
 * Set the start value of the indicator.
 * @param obj           pointer to a analogclock object
 * @param indic         pointer to an indicator
 * @param value         the new value
 */
void lv_analogclock_set_indicator_start_value(lv_obj_t * obj, lv_analogclock_indicator_t * indic, INT32_t value);

/**
 * Set the start value of the indicator.
 * @param obj           pointer to a analogclock object
 * @param indic         pointer to an indicator
 * @param value         the new value
 */
void lv_analogclock_set_indicator_end_value(lv_obj_t * obj, lv_analogclock_indicator_t * indic, INT32_t value);

/**
 * Set the time of clock.
 * @param obj           pointer to a analogclock object
 * @param hour          hour value
 * @param min           minute value
 * @param sec           second value
 */
void lv_analogclock_set_time(lv_obj_t * obj, INT32_t hour, INT32_t min, INT32_t sec);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_ANALOGCLOCK*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_ANALOGCLOCK_H*/
