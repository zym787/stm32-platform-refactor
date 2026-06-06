#ifndef __FONT_H
#define __FONT_H

#include "board_types.h"

typedef struct
{
    const UINT8_t   width;
    UINT8_t         height;
    const UINT16_t *data;
} font_def_t;


extern font_def_t Font_7x10_t;
extern font_def_t Font_11x18_t;
extern font_def_t Font_16x26_t;

extern const UINT16_t saber[][128];

#endif
