

#ifndef MINIRTOS_H
#define MINIRTOS_H

#include "portmacro.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define pdFALSE			( ( BaseType_t ) 0 )
#define pdTRUE			( ( BaseType_t ) 1 )

#define portPOINTER_SIZE_TYPE uint32_t
#define portBYTE_ALIGNMENT_MASK ( 0x0007 )
#define configMAX_TASK_NAME_LEN                  ( 16 )
#define configMAX_PRIORITIES                     ( 8 )
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
#define portPRIVILEGE_BIT ( ( UBaseType_t ) 0x00 )

#define configCPU_CLOCK_HZ    ( 72000000UL )   /* ★ 必须和 main.c 实际配的频率一致 */
#define configTICK_RATE_HZ    ( 1000UL )       /* 1ms 心跳 */

#define tskIDLE_PRIORITY			( ( UBaseType_t ) 0U )

#define __WEAK                                 __attribute__((weak))

#define portYIELD_User() portYIELD()	

#define mtCOVERAGE_TEST_MARKER()

#define configASSERT( x ) if ((x) == 0) {portDISABLE_INTERRUPTS(); for( ;; );}


typedef void (*TaskFunction_t)( void * );

/*
 * Defines the memory ranges allocated to the task when an MPU is used.
 */
typedef struct xMEMORY_REGION
{
	void *pvBaseAddress;
	uint32_t ulLengthInBytes;
	uint32_t ulParameters;
} MemoryRegion_t;


/*
 * Task control block.  A task control block (TCB) is allocated for each task,
 * and stores task state information, including a pointer to the task's context
 * (the task's run time environment, including register values)
 */
typedef struct tskTaskControlBlock 			/* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
	volatile StackType_t	*pxTopOfStack;	/*< Points to the location of the last item placed on the tasks stack.  THIS MUST BE THE FIRST MEMBER OF THE TCB STRUCT. */

	ListItem_t			xStateListItem;	/*< The list that the state list item of a task is reference from denotes the state of that task (Ready, Blocked, Suspended ). */
	ListItem_t			xEventListItem;		/*< Used to reference a task from an event list. */
	UBaseType_t			uxPriority;			/*< The priority of the task.  0 is the lowest priority. */
	StackType_t			*pxStack;			/*< Points to the start of the stack. */
	char				pcTaskName[ configMAX_TASK_NAME_LEN ];/*< Descriptive name given to the task when created.  Facilitates debugging only. */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */



} TCB_t;

typedef TCB_t StaticTask_t;
typedef struct tskTaskControlBlock* TaskHandle_t;




TaskHandle_t xTaskCreateStatic(	TaskFunction_t pxTaskCode,
								const char * const pcName,		/*lint !e971 Unqualified char types are allowed for strings and single characters only. */
								const uint32_t ulStackDepth,
								void * const pvParameters,
								UBaseType_t uxPriority,
								StackType_t * const puxStackBuffer,
								StaticTask_t * const pxTaskBuffer ) ;
void vTaskStartScheduler( void ) ;


#ifdef __cplusplus
}
#endif

#endif /* MINIRTOS_H */