#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"


void vBSP_GPIOC13Init( void );
void vBSP_GPIOC13Set( void );
void vBSP_GPIOC13Reset( void );
void vBSP_GPIOC13Toggle( void );

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */