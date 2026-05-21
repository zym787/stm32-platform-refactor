#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

typedef struct
{
    const uint8_t   width;
    uint8_t         height;
    const uint16_t *data;
} font_def_t;


extern font_def_t Font_7x10_t;
extern font_def_t Font_11x18_t;
extern font_def_t Font_16x26_t;

extern const uint16_t saber[][128];

#endif
