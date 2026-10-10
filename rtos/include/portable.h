#ifndef PORTABLE_H
#define PORTABLE_H

#include "portmacro.h"
#include "minirtos.h"

#ifdef __cplusplus
extern "C" {
#endif

StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters );
BaseType_t xPortStartScheduler( void );




#ifdef __cplusplus
}
#endif

#endif /* PORTABLE_H */