#ifndef MINITASK_H
#define MINITASK_H

#include "portmacro.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif


#define configIDLE_TASK_NAME "IDLE"

#define taskYIELD_IF_USING_PREEMPTION() portYIELD()


/*
 * Used internally only.
 */
typedef struct xTIME_OUT
{
	BaseType_t xOverflowCount;
	TickType_t xTimeOnEntering;
} TimeOut_t;


BaseType_t xTaskIncrementTick( void );

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList );
void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut );
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait );
void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait );
BaseType_t xTaskPriorityInherit( TaskHandle_t const pxMutexHolder );
BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder );
void vTaskPriorityDisinheritAfterTimeout( TaskHandle_t const pxMutexHolder, UBaseType_t uxHighestPriorityWaitingTask );
TaskHandle_t pvTaskIncrementMutexHeldCount( void );


#ifdef __cplusplus
}
#endif

#endif /* MINITASK_H */