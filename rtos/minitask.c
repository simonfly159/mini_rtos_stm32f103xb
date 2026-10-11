

#include "minirtos.h"
#include "portable.h"
#include "portmacro.h"
#include <stddef.h>

#define configIDLE_TASK_NAME "IDLE"

/* uxTopReadyPriority holds the priority of the highest priority ready
state task. */
#define taskRECORD_READY_PRIORITY( uxPriority )														\
{																									\
	if( ( uxPriority ) > uxTopReadyPriority )														\
	{																								\
		uxTopReadyPriority = ( uxPriority );														\
	}																								\
} /* taskRECORD_READY_PRIORITY */

/*
 * Place the task represented by pxTCB into the appropriate ready list for
 * the task.  It is inserted at the end of the list.
 */
#define prvAddTaskToReadyList( pxTCB )																\
	taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );												\
	vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) )
/*-----------------------------------------------------------*/

#define taskSELECT_HIGHEST_PRIORITY_TASK()															\
{																									\
UBaseType_t uxTopPriority = uxTopReadyPriority;														\
																									\
	/* Find the highest priority queue that contains ready tasks. */								\
	while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) )							\
	{																								\
		configASSERT( uxTopPriority );																\
		--uxTopPriority;																			\
	}																								\
																									\
	/* listGET_OWNER_OF_NEXT_ENTRY indexes through the list, so the tasks of						\
	the	same priority get an equal share of the processor time. */									\
	listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );			\
	uxTopReadyPriority = uxTopPriority;																\
} /* taskSELECT_HIGHEST_PRIORITY_TASK */
/*-----------------------------------------------------------*/

/* pxDelayedTaskList and pxOverflowDelayedTaskList are switched when the tick
count overflows. */
#define taskSWITCH_DELAYED_LISTS()																	\
{																									\
	List_t *pxTemp;																					\
																									\
	/* The delayed tasks list should be empty when the lists are switched. */						\
	configASSERT( ( listLIST_IS_EMPTY( pxDelayedTaskList ) ) );										\
																									\
	pxTemp = pxDelayedTaskList;																		\
	pxDelayedTaskList = pxOverflowDelayedTaskList;													\
	pxOverflowDelayedTaskList = pxTemp;																\
	xNumOfOverflows++;																				\
	prvResetNextTaskUnblockTime();																	\
}
/*-----------------------------------------------------------*/

#define taskYIELD_IF_USING_PREEMPTION() portYIELD()


static volatile UBaseType_t uxCurrentNumberOfTasks 	= ( UBaseType_t ) 0U;
static volatile TickType_t xTickCount 				= ( TickType_t ) configINITIAL_TICK_COUNT;
static volatile UBaseType_t uxTopReadyPriority 		= tskIDLE_PRIORITY;
static volatile BaseType_t xSchedulerRunning 		= pdFALSE;
static volatile BaseType_t xNumOfOverflows 			= ( BaseType_t ) 0;
static volatile TickType_t xNextTaskUnblockTime		= ( TickType_t ) 0U; /* Initialised to portMAX_DELAY before the scheduler starts. */
static TaskHandle_t xIdleTaskHandle					= NULL;			/*< Holds the handle of the idle task.  The idle task is created automatically when the scheduler is started. */


/*lint -save -e956 A manual analysis and inspection has been used to determine
which static variables must be declared volatile. */
TCB_t * volatile pxCurrentTCB = NULL;

/* Lists for ready and blocked tasks. --------------------
xDelayedTaskList1 and xDelayedTaskList2 could be move to function scople but
doing so breaks some kernel aware debuggers and debuggers that rely on removing
the static qualifier. */
static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];/*< Prioritised ready tasks. */
static List_t xDelayedTaskList1;						/*< Delayed tasks. */
static List_t xDelayedTaskList2;						/*< Delayed tasks (two lists are used - one for delays that have overflowed the current tick count. */
static List_t * volatile pxDelayedTaskList;				/*< Points to the delayed task list currently being used. */
static List_t * volatile pxOverflowDelayedTaskList;		/*< Points to the delayed task list currently being used to hold tasks that have overflowed the current tick count. */

/*
 * Called after a Task_t structure has been allocated either statically or
 * dynamically to fill in the structure's members.
 */
static void prvInitialiseNewTask( 	TaskFunction_t pxTaskCode,
									const char * const pcName, 		/*lint !e971 Unqualified char types are allowed for strings and single characters only. */
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions );

/*
 * Called after a new task has been created and initialised to place the task
 * under the control of the scheduler.
 */
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB );

/*
 * Utility to ready all the lists used by the scheduler.  This is called
 * automatically upon the creation of the first task.
 */
static void prvInitialiseTaskLists( void );

/*
 * Set xNextTaskUnblockTime to the time at which the next Blocked state task
 * will exit the Blocked state.
 */
static void prvResetNextTaskUnblockTime( void );

/*
 * The currently executing task is entering the Blocked state.  Add the task to
 * either the current or the overflow delayed task list.
 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely );


/*
 * The idle task, which as all tasks is implemented as a never ending loop.
 * The idle task is automatically created and added to the ready lists upon
 * creation of the first user task.
 *
 * The portTASK_FUNCTION_PROTO() macro is used to allow port/compiler specific
 * language extensions.  The equivalent prototype for this function is:
 *
 * void prvIdleTask( void *pvParameters );
 *
 */
static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters );                                    

void vApplicationGetIdleTaskMemory (StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) ;





TaskHandle_t xTaskCreateStatic(	TaskFunction_t pxTaskCode,
								const char * const pcName,		/*lint !e971 Unqualified char types are allowed for strings and single characters only. */
								const uint32_t ulStackDepth,
								void * const pvParameters,
								UBaseType_t uxPriority,
								StackType_t * const puxStackBuffer,
								StaticTask_t * const pxTaskBuffer )
{
TCB_t *pxNewTCB;
TaskHandle_t xReturn;

    configASSERT( puxStackBuffer != NULL );
    configASSERT( pxTaskBuffer != NULL );

    if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
    {
        pxNewTCB = ( TCB_t * ) pxTaskBuffer; /*lint !e740 !e9087 Unusual cast is ok as the structures are designed to have the same alignment, and the size is checked by an assert. */
        pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;
        
        prvInitialiseNewTask( pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, &xReturn, pxNewTCB, NULL );
        prvAddNewTaskToReadyList( pxNewTCB ); 
    }
    else
    {
        xReturn = NULL;
    }

    return xReturn;
}


static void prvInitialiseNewTask( 	TaskFunction_t pxTaskCode,
									const char * const pcName,		/*lint !e971 Unqualified char types are allowed for strings and single characters only. */
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions )
{
StackType_t *pxTopOfStack;
UBaseType_t x;
    
    pxTopOfStack = &( pxNewTCB->pxStack[ ulStackDepth - ( uint32_t ) 1 ] );
    pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 !e9033 !e9078 MISRA exception.  Avoiding casts between pointers and integers is not practical.  Size differences accounted for using portPOINTER_SIZE_TYPE type.  Checked by assert(). */

    /* Store the task name in the TCB. */
	if( pcName != NULL )
	{
		for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
		{
			pxNewTCB->pcTaskName[ x ] = pcName[ x ];

			/* Don't copy all configMAX_TASK_NAME_LEN if the string is shorter than
			configMAX_TASK_NAME_LEN characters just in case the memory after the
			string is not accessible (extremely unlikely). */
			if( pcName[ x ] == ( char ) 0x00 )
			{
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		/* Ensure the name string is terminated in the case that the string length
		was greater or equal to configMAX_TASK_NAME_LEN. */
		pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';
	}
	else
	{
		/* The task has not been given a name, so just ensure there is a NULL
		terminator when it is read out. */
		pxNewTCB->pcTaskName[ 0 ] = 0x00;
	}

    /* This is used as an array index so must ensure it's not too large.  First
	remove the privilege bit if one is present. */
	if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
	{
		uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

    pxNewTCB->uxPriority = uxPriority;

	vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
	vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

	/* Set the pxNewTCB as a link back from the ListItem_t.  This is so we can get
	back to	the containing TCB from a generic item in a list. */
	listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

	/* Event lists are always in priority order. */
	listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
	listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

    pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );

	if( pxCreatedTask != NULL )
	{
		/* Pass the handle out in an anonymous way.  The handle can be used to
		change the created task's priority, delete the created task, etc.*/
		*pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

    /* Avoid compiler warning about unreferenced parameter. */
	( void ) xRegions;
}
/*-----------------------------------------------------------*/

static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
	/* Ensure interrupts don't access the task lists while the lists are being
	updated. */
	portENTER_CRITICAL();
	{
		uxCurrentNumberOfTasks++;
		if( pxCurrentTCB == NULL )
		{
			/* There are no other tasks, or all the other tasks are in
			the suspended state - make this the current task. */
			pxCurrentTCB = pxNewTCB;

			if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
			{
				/* This is the first task to be created so do the preliminary
				initialisation required.  We will not recover if this call
				fails, but we will report the failure. */
				prvInitialiseTaskLists();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			/* If the scheduler is not already running, make this task the
			current task if it is the highest priority task to be created
			so far. */
			if( xSchedulerRunning == pdFALSE )
			{
				if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
				{
					pxCurrentTCB = pxNewTCB;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}


		prvAddTaskToReadyList( pxNewTCB );

	}
	portEXIT_CRITICAL();

	if( xSchedulerRunning != pdFALSE )
	{
		/* If the created task is of a higher priority than the current task
		then it should run now. */
		if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
		{
			taskYIELD_IF_USING_PREEMPTION();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}
/*-----------------------------------------------------------*/


void vTaskStartScheduler( void )
{
BaseType_t xReturn;


	StaticTask_t *pxIdleTaskTCBBuffer = NULL;
	StackType_t *pxIdleTaskStackBuffer = NULL;
	uint32_t ulIdleTaskStackSize;

	/* The Idle task is created using user provided RAM - obtain the
	address of the RAM then create the idle task. */
	vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &ulIdleTaskStackSize );
	xIdleTaskHandle = xTaskCreateStatic(	prvIdleTask,
											configIDLE_TASK_NAME,
											ulIdleTaskStackSize,
											( void * ) NULL, /*lint !e961.  The cast is not redundant for all compilers. */
											portPRIVILEGE_BIT, /* In effect ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ), but tskIDLE_PRIORITY is zero. */
											pxIdleTaskStackBuffer,
											pxIdleTaskTCBBuffer ); /*lint !e961 MISRA exception, justified as it is not a redundant explicit cast to all supported compilers. */

	if( xIdleTaskHandle != NULL )
	{
		xReturn = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}


	if( xReturn == pdTRUE )
	{
		/* Interrupts are turned off here, to ensure a tick does not occur
		before or during the call to xPortStartScheduler().  The stacks of
		the created tasks contain a status word with interrupts switched on
		so interrupts will automatically get re-enabled when the first task
		starts to run. */
		portDISABLE_INTERRUPTS();

        xNextTaskUnblockTime = portMAX_DELAY;
		xSchedulerRunning = pdTRUE;
        xTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;

		/* Setting up the timer tick is hardware specific and thus in the
		portable interface. */
		if( xPortStartScheduler() != pdFALSE )
		{
			/* Should not reach here as if the scheduler is running the
			function will not return. */
		}
		else
		{
			/* Should only reach here if a task calls xTaskEndScheduler(). */
		}
	}
	else
	{
		configASSERT( xReturn != pdTRUE );
	}

	/* Prevent compiler warnings if INCLUDE_xTaskGetIdleTaskHandle is set to 0,
	meaning xIdleTaskHandle is not used anywhere else. */
	( void ) xIdleTaskHandle;
}
/*-----------------------------------------------------------*/

BaseType_t xTaskIncrementTick( void )
{
TCB_t * pxTCB;
TickType_t xItemValue;
BaseType_t xSwitchRequired = pdFALSE;


	{
		/* Minor optimisation.  The tick count cannot change in this
		block. */
		const TickType_t xConstTickCount = xTickCount + ( TickType_t ) 1;

		/* Increment the RTOS tick, switching the delayed and overflowed
		delayed lists if it wraps to 0. */
		xTickCount = xConstTickCount;

		if( xConstTickCount == ( TickType_t ) 0U ) /*lint !e774 'if' does not always evaluate to false as it is looking for an overflow. */
		{
			taskSWITCH_DELAYED_LISTS();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* See if this tick has made a timeout expire.  Tasks are stored in
		the	queue in the order of their wake time - meaning once one task
		has been found whose block time has not expired there is no need to
		look any further down the list. */
		if( xConstTickCount >= xNextTaskUnblockTime )
		{
			for( ;; )
			{
				if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
				{
					/* The delayed list is empty.  Set xNextTaskUnblockTime
					to the maximum possible value so it is extremely
					unlikely that the
					if( xTickCount >= xNextTaskUnblockTime ) test will pass
					next time through. */
					xNextTaskUnblockTime = portMAX_DELAY; /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
					break;
				}
				else
				{
					/* The delayed list is not empty, get the value of the
					item at the head of the delayed list.  This is the time
					at which the task at the head of the delayed list must
					be removed from the Blocked state. */
					pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList ); /*lint !e9079 void * is used as this macro is used with timers and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */
					xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

					if( xConstTickCount < xItemValue )
					{
						/* It is not time to unblock this item yet, but the
						item value is the time at which the task at the head
						of the blocked list must be removed from the Blocked
						state -	so record the item value in
						xNextTaskUnblockTime. */
						xNextTaskUnblockTime = xItemValue;
						break; /*lint !e9011 Code structure here is deedmed easier to understand with multiple breaks. */
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* It is time to remove the item from the Blocked state. */
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );

					/* Is the task waiting on an event also?  If so remove
					it from the event list. */
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
					{
						( void ) uxListRemove( &( pxTCB->xEventListItem ) );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* Place the unblocked task into the appropriate ready
					list. */
					prvAddTaskToReadyList( pxTCB );

					/* A task being unblocked cannot cause an immediate
					context switch if preemption is turned off. */
					{
						/* Preemption is on, but a context switch should
						only be performed if the unblocked task has a
						priority that is equal to or higher than the
						currently executing task. */
						if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
						{
							xSwitchRequired = pdTRUE;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
				}
			}
		}

		/* Tasks of equal priority to the currently running task will share
		processing time (time slice) if preemption is on, and the application
		writer has not explicitly turned time slicing off. */
		{
			if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
			{
				xSwitchRequired = pdTRUE;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	}

	return xSwitchRequired;
}
/*-----------------------------------------------------------*/

void vTaskSwitchContext( void )
{
	/* Select a new task to run using either the generic C or port
	optimised asm code. */
	taskSELECT_HIGHEST_PRIORITY_TASK(); /*lint !e9079 void * is used as this macro is used with timers and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */

}
/*-----------------------------------------------------------*/

void vTaskDelay( const TickType_t xTicksToDelay )
{
	if( xTicksToDelay > ( TickType_t ) 0U )
	{
        portENTER_CRITICAL();

		/* A task that is removed from the event list while the
		scheduler is suspended will not get placed in the ready
		list or removed from the blocked list until the scheduler
		is resumed.

		This task cannot be in an event list as it is the currently
		executing task. */
		prvAddCurrentTaskToDelayedList( xTicksToDelay, pdFALSE );

        portEXIT_CRITICAL();
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

    taskYIELD_IF_USING_PREEMPTION();
}
/*-----------------------------------------------------------*/










static void prvInitialiseTaskLists( void )
{
UBaseType_t uxPriority;

	for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
	{
		vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
	}

    vListInitialise( &xDelayedTaskList1 );
	vListInitialise( &xDelayedTaskList2 );

    /* Start with pxDelayedTaskList using list1 and the pxOverflowDelayedTaskList
	using list2. */
	pxDelayedTaskList = &xDelayedTaskList1;
	pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
/*-----------------------------------------------------------*/

static void prvResetNextTaskUnblockTime( void )
{
TCB_t *pxTCB;

	if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
	{
		/* The new current delayed list is empty.  Set xNextTaskUnblockTime to
		the maximum possible value so it is	extremely unlikely that the
		if( xTickCount >= xNextTaskUnblockTime ) test will pass until
		there is an item in the delayed list. */
		xNextTaskUnblockTime = portMAX_DELAY;
	}
	else
	{
		/* The new current delayed list is not empty, get the value of
		the item at the head of the delayed list.  This is the time at
		which the task at the head of the delayed list should be removed
		from the Blocked state. */
		( pxTCB ) = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList ); /*lint !e9079 void * is used as this macro is used with timers and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same. */
		xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( ( pxTCB )->xStateListItem ) );
	}
}
/*-----------------------------------------------------------*/

static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely )
{
TickType_t xTimeToWake;
const TickType_t xConstTickCount = xTickCount;

	/* Remove the task from the ready list before adding it to the blocked list
	as the same list item is used for both lists. */
	uxListRemove( &( pxCurrentTCB->xStateListItem ) );

	/* Calculate the time at which the task should be woken if the event
	does not occur.  This may overflow but this doesn't matter, the kernel
	will manage it correctly. */
	xTimeToWake = xConstTickCount + xTicksToWait;

	/* The list item will be inserted in wake time order. */
	listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

	if( xTimeToWake < xConstTickCount )
	{
		/* Wake time has overflowed.  Place this item in the overflow list. */
		vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
	}
	else
	{
		/* The wake time has not overflowed, so the current block list is used. */
		vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

		/* If the task entering the blocked state was placed at the head of the
		list of blocked tasks then xNextTaskUnblockTime needs to be updated
		too. */
		if( xTimeToWake < xNextTaskUnblockTime )
		{
			xNextTaskUnblockTime = xTimeToWake;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

	/* Avoid compiler warning when INCLUDE_vTaskSuspend is not 1. */
	( void ) xCanBlockIndefinitely;
}
/*-----------------------------------------------------------*/

/*
  vApplicationGetIdleTaskMemory gets called when configSUPPORT_STATIC_ALLOCATION
  equals to 1 and is required for static memory allocation support.
*/
void vApplicationGetIdleTaskMemory (StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Idle task control block and stack */
  static StaticTask_t Idle_TCB;
  static StackType_t  Idle_Stack[configMINIMAL_STACK_SIZE];

  *ppxIdleTaskTCBBuffer   = &Idle_TCB;
  *ppxIdleTaskStackBuffer = &Idle_Stack[0];
  *pulIdleTaskStackSize   = (uint32_t)configMINIMAL_STACK_SIZE;
}



static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
	/* Stop warnings. */
	( void ) pvParameters;

	/** THIS IS THE RTOS IDLE TASK - WHICH IS CREATED AUTOMATICALLY WHEN THE
	SCHEDULER IS STARTED. **/

	for( ;; )
    {


    }
}