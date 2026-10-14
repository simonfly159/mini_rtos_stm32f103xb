#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include "list.h"
#include "portmacro.h"
#include "minirtos.h"

#ifdef __cplusplus
extern "C" {
#endif


#define errQUEUE_EMPTY	( ( BaseType_t ) 0 )
#define errQUEUE_FULL	( ( BaseType_t ) 0 )


/* For internal use only. */
#define	queueSEND_TO_BACK		( ( BaseType_t ) 0 )
#define	queueSEND_TO_FRONT		( ( BaseType_t ) 1 )
#define queueOVERWRITE			( ( BaseType_t ) 2 )

#define queueYIELD_IF_USING_PREEMPTION() portYIELD()


typedef struct QueuePointers
{
	int8_t *pcTail;					/*< Points to the byte at the end of the queue storage area.  Once more byte is allocated than necessary to store the queue items, this is used as a marker. */
	int8_t *pcReadFrom;				/*< Points to the last place that a queued item was read from when the structure is used as a queue. */
} QueuePointers_t;

typedef struct SemaphoreData
{
	TaskHandle_t xMutexHolder;		 /*< The handle of the task that holds the mutex. */
	UBaseType_t uxRecursiveCallCount;/*< Maintains a count of the number of times a recursive mutex has been recursively 'taken' when the structure is used as a mutex. */
} SemaphoreData_t;

/*
 * Definition of the queue used by the scheduler.
 * Items are queued by copy, not reference.  See the following link for the
 * rationale: https://www.freertos.org/Embedded-RTOS-Queues.html
 */
typedef struct QueueDefinition 		/* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
	int8_t *pcHead;					/*< Points to the beginning of the queue storage area. */
	int8_t *pcWriteTo;				/*< Points to the free next place in the storage area. */

	union
	{
		QueuePointers_t xQueue;		/*< Data required exclusively when this structure is used as a queue. */
		SemaphoreData_t xSemaphore; /*< Data required exclusively when this structure is used as a semaphore. */
	} u;

	List_t xTasksWaitingToSend;		/*< List of tasks that are blocked waiting to post onto this queue.  Stored in priority order. */
	List_t xTasksWaitingToReceive;	/*< List of tasks that are blocked waiting to read from this queue.  Stored in priority order. */

	volatile UBaseType_t uxMessagesWaiting;/*< The number of items currently in the queue. */
	UBaseType_t uxLength;			/*< The length of the queue defined as the number of items it will hold, not the number of bytes. */
	UBaseType_t uxItemSize;			/*< The size of each items that the queue will hold. */

	// volatile int8_t cRxLock;		/*< Stores the number of items received from the queue (removed from the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */
	// volatile int8_t cTxLock;		/*< Stores the number of items transmitted to the queue (added to the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */
    /*计划把所有的tasksuspend替换为临界区，这样在进行队列操作时就确保不会有中断进入，
    这样就不用再进行unlock的检查操作，这样虽然会提高中断的关闭时长，但缩小了代码量，简化了逻辑，并且
    我的minirtos不计划设计susupend功能。*/

	uint8_t ucQueueType;

} Queue_t;

typedef Queue_t StaticQueue_t;
typedef struct QueueDefinition * QueueHandle_t;


QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType );
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition );
BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait );


#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */