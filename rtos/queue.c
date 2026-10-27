#include <string.h>
#include "queue.h"
#include "minirtos.h"
#include "portmacro.h"
#include "minitask.h"

/*
 * Uses a critical section to determine if there is any data in a queue.
 *
 * @return pdTRUE if the queue contains no items, otherwise pdFALSE.
 */
static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue );

/*
 * Uses a critical section to determine if there is any space in a queue.
 *
 * @return pdTRUE if there is no space, otherwise pdFALSE;
 */
static BaseType_t prvIsQueueFull( const Queue_t *pxQueue );

/*
 * Copies an item into the queue, either at the front of the queue or the
 * back of the queue.
 */
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition );

/*
 * Copies an item out of a queue.
 */
static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer );

/*
 * Called after a Queue_t structure has been allocated either statically or
 * dynamically to fill in the structure's members.
 */
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue );

/*
 * Mutexes are a special type of queue.  When a mutex is created, first the
 * queue is created, then prvInitialiseMutex() is called to configure the queue
 * as a mutex.
 */
static void prvInitialiseMutex( Queue_t *pxNewQueue );

/*
 * If a task waiting for a mutex causes the mutex holder to inherit a
 * priority, but the waiting task times out, then the holder should
 * disinherit the priority - but only down to the highest priority of any
 * other tasks that are waiting for the same mutex.  This function returns
 * that priority.
 */
static UBaseType_t prvGetDisinheritPriorityAfterTimeout( const Queue_t * const pxQueue );


static BaseType_t xQueueGenericReset( QueueHandle_t xQueue )
{
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );

	portENTER_CRITICAL();
	{
		pxQueue->u.xQueue.pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize ); /*lint !e9016 Pointer arithmetic allowed on char types, especially when it assists conveying intent. */
		pxQueue->uxMessagesWaiting = ( UBaseType_t ) 0U;
		pxQueue->pcWriteTo = pxQueue->pcHead;
		pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - 1U ) * pxQueue->uxItemSize ); /*lint !e9016 Pointer arithmetic allowed on char types, especially when it assists conveying intent. */

		{
			/* Ensure the event queues start in the correct state. */
			vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
			vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
		}
	}
	portEXIT_CRITICAL();

	/* A value is returned for calling semantic consistency with previous
	versions. */
	return pdTRUE;
}
/*-----------------------------------------------------------*/

QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType )
{
Queue_t *pxNewQueue;

	configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

	/* The StaticQueue_t structure and the queue storage area must be
	supplied. */
	configASSERT( pxStaticQueue != NULL );

	/* A queue storage area should be provided if the item size is not 0, and
	should not be provided if the item size is 0. */
	configASSERT( !( ( pucQueueStorage != NULL ) && ( uxItemSize == 0 ) ) );
	configASSERT( !( ( pucQueueStorage == NULL ) && ( uxItemSize != 0 ) ) );

	/* The address of a statically allocated queue was passed in, use it.
	The address of a statically allocated storage area was also passed in
	but is already set. */
	pxNewQueue = ( Queue_t * ) pxStaticQueue; /*lint !e740 !e9087 Unusual cast is ok as the structures are designed to have the same alignment, and the size is checked by an assert. */
	
    if( pxNewQueue != NULL )
	{
		prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
	return pxNewQueue;
}
/*-----------------------------------------------------------*/

QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue )
{
QueueHandle_t xHandle;

	configASSERT( uxMaxCount != 0 );
	configASSERT( uxInitialCount <= uxMaxCount );

	xHandle = xQueueGenericCreateStatic( uxMaxCount, queueSEMAPHORE_QUEUE_ITEM_LENGTH, NULL, pxStaticQueue, queueQUEUE_TYPE_COUNTING_SEMAPHORE );
	
    if( xHandle != NULL )
	{
		( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
	return xHandle;
}
/*-----------------------------------------------------------*/

QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue )
{
QueueHandle_t xNewQueue;
const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

	xNewQueue = xQueueGenericCreateStatic( uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType );
	prvInitialiseMutex( ( Queue_t * ) xNewQueue );

	return xNewQueue;
}
/*-----------------------------------------------------------*/

static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue )
{
	if( uxItemSize == ( UBaseType_t ) 0 )
	{
		/* No RAM was allocated for the queue storage area, but PC head cannot
		be set to NULL because NULL is used as a key to say the queue is used as
		a mutex.  Therefore just set pcHead to point to the queue as a benign
		value that is known to be within the memory map. */
		pxNewQueue->pcHead = ( int8_t * ) pxNewQueue;
	}
	else
	{
		/* Set the head to the start of the queue storage area. */
		pxNewQueue->pcHead = ( int8_t * ) pucQueueStorage;
	}

	/* Initialise the queue members as described where the queue type is
	defined. */
	pxNewQueue->uxLength = uxQueueLength;
	pxNewQueue->uxItemSize = uxItemSize;
	( void ) xQueueGenericReset( pxNewQueue );

	pxNewQueue->ucQueueType = ucQueueType;
}
/*-----------------------------------------------------------*/

static void prvInitialiseMutex( Queue_t *pxNewQueue )
{
	if( pxNewQueue != NULL )
	{
		/* The queue create function will set all the queue structure members
		correctly for a generic queue, but this function is creating a
		mutex.  Overwrite those members that need to be set differently -
		in particular the information required for priority inheritance. */
		pxNewQueue->u.xSemaphore.xMutexHolder = NULL;
		pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

		/* In case this is a recursive mutex. */
		pxNewQueue->u.xSemaphore.uxRecursiveCallCount = 0;

		/* Start with the semaphore in the expected state. */
		( void ) xQueueGenericSend( pxNewQueue, NULL, ( TickType_t ) 0U, queueSEND_TO_BACK );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}
/*-----------------------------------------------------------*/














BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )
{
BaseType_t xEntryTimeSet = pdFALSE, xYieldRequired;
TimeOut_t xTimeOut;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
	configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );



	/*lint -save -e904 This function relaxes the coding standard somewhat to
	allow return statements within the function itself.  This is done in the
	interest of execution time efficiency. */
	for( ;; )
	{
		portENTER_CRITICAL();
		{
			/* Is there room on the queue now?  The running task must be the
			highest priority task wanting to access the queue.  If the head item
			in the queue is to be overwritten then it does not matter if the
			queue is full. */
			if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
			{
				{
					xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

					/* If there was a task waiting for data to arrive on the
					queue then unblock it now. */
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
						{
							/* The unblocked task has a priority higher than
							our own so yield immediately.  Yes it is ok to do
							this from within the critical section - the kernel
							takes care of that. */
							queueYIELD_IF_USING_PREEMPTION();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else if( xYieldRequired != pdFALSE )//互斥锁保留逻辑
					{
						/* This path is a special case that will only get
						executed if the task was holding multiple mutexes and
						the mutexes were given back in an order that is
						different to that in which they were taken. */
						queueYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}

				portEXIT_CRITICAL();
				return pdTRUE;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* The queue was full and no block time is specified (or
					the block time has expired) so leave now. */
					portEXIT_CRITICAL();

					return errQUEUE_FULL;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* The queue was full and a block time was specified so
					configure the timeout structure. */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			if( prvIsQueueFull( pxQueue ) != pdFALSE )
			{
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );


				/* Resuming the scheduler will move tasks from the pending
				ready list into the ready list - so it is feasible that this
				task is already in a ready list before it yields - in which
				case the yield will not cause a context switch unless there
				is also a higher priority task in the pending ready list. */
                portEXIT_CRITICAL();

				queueYIELD_IF_USING_PREEMPTION();
			}
			else
			{
				/* Try again. */
                portEXIT_CRITICAL();
			}
		}
		else
		{
			/* The timeout has expired. */
            portEXIT_CRITICAL();

			return errQUEUE_FULL;
		}
	} /*lint -restore */
}
/*-----------------------------------------------------------*/


BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait )
{
BaseType_t xEntryTimeSet = pdFALSE;
TimeOut_t xTimeOut;
Queue_t * const pxQueue = xQueue;

	/* Check the pointer is not NULL. */
	configASSERT( ( pxQueue ) );

	/* The buffer into which data is received can only be NULL if the data size
	is zero (so no data is copied into the buffer. */
	configASSERT( !( ( ( pvBuffer ) == NULL ) && ( ( pxQueue )->uxItemSize != ( UBaseType_t ) 0U ) ) );


	/*lint -save -e904  This function relaxes the coding standard somewhat to
	allow return statements within the function itself.  This is done in the
	interest of execution time efficiency. */
	for( ;; )
	{
		portENTER_CRITICAL();
		{
			const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

			/* Is there data in the queue now?  To be running the calling task
			must be the highest priority task wanting to access the queue. */
			if( uxMessagesWaiting > ( UBaseType_t ) 0 )
			{
				/* Data available, remove one item. */
				prvCopyDataFromQueue( pxQueue, pvBuffer );
				pxQueue->uxMessagesWaiting = uxMessagesWaiting - ( UBaseType_t ) 1;

				/* There is now space in the queue, were any tasks waiting to
				post to the queue?  If so, unblock the highest priority waiting
				task. */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
					{
						queueYIELD_IF_USING_PREEMPTION();
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

				portEXIT_CRITICAL();
				return pdTRUE;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* The queue was empty and no block time is specified (or
					the block time has expired) so leave now. */
					portEXIT_CRITICAL();
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* The queue was empty and a block time was specified so
					configure the timeout structure. */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			/* The timeout has not expired.  If the queue is still empty place
			the task on the list of tasks waiting to receive from the queue. */
			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				portEXIT_CRITICAL();
                queueYIELD_IF_USING_PREEMPTION();
			}
			else
			{
				/* The queue contains data again.  Loop back to try and read the
				data. */
				portEXIT_CRITICAL();
			}
		}
		else
		{
			/* Timed out.  If there is no data in the queue exit, otherwise loop
			back and attempt to read the data. */
			portEXIT_CRITICAL();

			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	} /*lint -restore */
}
/*-----------------------------------------------------------*/

BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait )
{
BaseType_t xEntryTimeSet = pdFALSE;
TimeOut_t xTimeOut;
Queue_t * const pxQueue = xQueue;

BaseType_t xInheritanceOccurred = pdFALSE;

	/* Check the queue pointer is not NULL. */
	configASSERT( ( pxQueue ) );

	/* Check this really is a semaphore, in which case the item size will be
	0. */
	configASSERT( pxQueue->uxItemSize == 0 );

	/*lint -save -e904 This function relaxes the coding standard somewhat to allow return
	statements within the function itself.  This is done in the interest
	of execution time efficiency. */
	for( ;; )
	{
		portENTER_CRITICAL();
		{
			/* Semaphores are queues with an item size of 0, and where the
			number of messages in the queue is the semaphore's count value. */
			const UBaseType_t uxSemaphoreCount = pxQueue->uxMessagesWaiting;

			/* Is there data in the queue now?  To be running the calling task
			must be the highest priority task wanting to access the queue. */
			if( uxSemaphoreCount > ( UBaseType_t ) 0 )
			{
				/* Semaphores are queues with a data size of zero and where the
				messages waiting is the semaphore's count.  Reduce the count. */
				pxQueue->uxMessagesWaiting = uxSemaphoreCount - ( UBaseType_t ) 1;

				if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
				{
					/* Record the information required to implement
					priority inheritance should it become necessary. */
					pxQueue->u.xSemaphore.xMutexHolder = pvTaskIncrementMutexHeldCount();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* Check to see if other tasks are blocked waiting to give the
				semaphore, and if so, unblock the highest priority such task. */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
					{
						queueYIELD_IF_USING_PREEMPTION();
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

				portEXIT_CRITICAL();
				return pdTRUE;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* For inheritance to have occurred there must have been an
					initial timeout, and an adjusted timeout cannot become 0, as
					if it were 0 the function would have exited. */
                    configASSERT( xInheritanceOccurred == pdFALSE );

					/* The semaphore count was 0 and no block time is specified
					(or the block time has expired) so exit now. */
					portEXIT_CRITICAL();
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* The semaphore count was 0 and a block time was specified
					so configure the timeout structure ready to block. */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			/* A block time is specified and not expired.  If the semaphore
			count is 0 then enter the Blocked state to wait for a semaphore to
			become available.  As semaphores are implemented with queues the
			queue being empty is equivalent to the semaphore count being 0. */
			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
				{
                    //因为已经在临界区以不需要再进入临界区
					{
						xInheritanceOccurred = xTaskPriorityInherit( pxQueue->u.xSemaphore.xMutexHolder );
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				portEXIT_CRITICAL();
                queueYIELD_IF_USING_PREEMPTION();
			}
			else
			{
				/* There was no timeout and the semaphore count was not 0, so
				attempt to take the semaphore again. */
				portEXIT_CRITICAL();
			}
		}
		else
		{
			/* Timed out. */
			// portEXIT_CRITICAL();

			/* If the semaphore count is 0 exit now as the timeout has
			expired.  Otherwise return to attempt to take the semaphore that is
			known to be available.  As semaphores are implemented by queues the
			queue being empty is equivalent to the semaphore count being 0. */
			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
                /* xInheritanceOccurred could only have be set if
				pxQueue->uxQueueType == queueQUEUE_IS_MUTEX so no need to
				test the mutex type again to check it is actually a mutex. */
				if( xInheritanceOccurred != pdFALSE )
				{
					{
						UBaseType_t uxHighestWaitingPriority;

						/* This task blocking on the mutex caused another
						task to inherit this task's priority.  Now this task
						has timed out the priority should be disinherited
						again, but only as low as the next highest priority
						task that is waiting for the same mutex. */
						uxHighestWaitingPriority = prvGetDisinheritPriorityAfterTimeout( pxQueue );
						vTaskPriorityDisinheritAfterTimeout( pxQueue->u.xSemaphore.xMutexHolder, uxHighestWaitingPriority );
					}
				}
                //因为互斥锁的操作也要禁入临界区，为减少出入临界区操作次数，把临界区退出推后。
                portEXIT_CRITICAL();
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

            //因为互斥锁的操作也要禁入临界区，为减少出入临界区操作次数，把临界区退出推后。
            portEXIT_CRITICAL();
		}
	} /*lint -restore */
}
/*-----------------------------------------------------------*/


































static UBaseType_t prvGetDisinheritPriorityAfterTimeout( const Queue_t * const pxQueue )
{
UBaseType_t uxHighestPriorityOfWaitingTasks;

	/* If a task waiting for a mutex causes the mutex holder to inherit a
	priority, but the waiting task times out, then the holder should
	disinherit the priority - but only down to the highest priority of any
	other tasks that are waiting for the same mutex.  For this purpose,
	return the priority of the highest priority task that is waiting for the
	mutex. */
	if( listCURRENT_LIST_LENGTH( &( pxQueue->xTasksWaitingToReceive ) ) > 0U )
	{
		uxHighestPriorityOfWaitingTasks = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) listGET_ITEM_VALUE_OF_HEAD_ENTRY( &( pxQueue->xTasksWaitingToReceive ) );
	}
	else
	{
		uxHighestPriorityOfWaitingTasks = tskIDLE_PRIORITY;
	}

	return uxHighestPriorityOfWaitingTasks;
}
/*-----------------------------------------------------------*/

static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition )
{
BaseType_t xReturn = pdFALSE;
UBaseType_t uxMessagesWaiting;

	/* This function is called from a critical section. */

	uxMessagesWaiting = pxQueue->uxMessagesWaiting;

	if( pxQueue->uxItemSize == ( UBaseType_t ) 0 )
	{
		if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
		{
			/* The mutex is no longer being held. */
			xReturn = xTaskPriorityDisinherit( pxQueue->u.xSemaphore.xMutexHolder );
			pxQueue->u.xSemaphore.xMutexHolder = NULL;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else if( xPosition == queueSEND_TO_BACK )
	{
		( void ) memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 !e9087 MISRA exception as the casts are only redundant for some ports, plus previous logic ensures a null pointer can only be passed to memcpy() if the copy size is 0.  Cast to void required by function signature and safe as no alignment requirement and copy length specified in bytes. */
		pxQueue->pcWriteTo += pxQueue->uxItemSize; /*lint !e9016 Pointer arithmetic on char types ok, especially in this use case where it is the clearest way of conveying intent. */
		if( pxQueue->pcWriteTo >= pxQueue->u.xQueue.pcTail ) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
		{
			pxQueue->pcWriteTo = pxQueue->pcHead;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		( void ) memcpy( ( void * ) pxQueue->u.xQueue.pcReadFrom, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e9087 !e418 MISRA exception as the casts are only redundant for some ports.  Cast to void required by function signature and safe as no alignment requirement and copy length specified in bytes.  Assert checks null pointer only used when length is 0. */
		pxQueue->u.xQueue.pcReadFrom -= pxQueue->uxItemSize;
		if( pxQueue->u.xQueue.pcReadFrom < pxQueue->pcHead ) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
		{
			pxQueue->u.xQueue.pcReadFrom = ( pxQueue->u.xQueue.pcTail - pxQueue->uxItemSize );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		if( xPosition == queueOVERWRITE )
		{
			if( uxMessagesWaiting > ( UBaseType_t ) 0 )
			{
				/* An item is not being added but overwritten, so subtract
				one from the recorded number of items in the queue so when
				one is added again below the number of recorded items remains
				correct. */
				--uxMessagesWaiting;
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

	pxQueue->uxMessagesWaiting = uxMessagesWaiting + ( UBaseType_t ) 1;

	return xReturn;
}
/*-----------------------------------------------------------*/

static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer )
{
	if( pxQueue->uxItemSize != ( UBaseType_t ) 0 )
	{
		pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize; /*lint !e9016 Pointer arithmetic on char types ok, especially in this use case where it is the clearest way of conveying intent. */
		if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail ) /*lint !e946 MISRA exception justified as use of the relational operator is the cleanest solutions. */
		{
			pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
		( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 !e9087 MISRA exception as the casts are only redundant for some ports.  Also previous logic ensures a null pointer can only be passed to memcpy() when the count is 0.  Cast to void required by function signature and safe as no alignment requirement and copy length specified in bytes. */
	}
}
/*-----------------------------------------------------------*/

static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue )
{
BaseType_t xReturn;

	portENTER_CRITICAL();
	{
		if( pxQueue->uxMessagesWaiting == ( UBaseType_t )  0 )
		{
			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	portEXIT_CRITICAL();

	return xReturn;
}
/*-----------------------------------------------------------*/

static BaseType_t prvIsQueueFull( const Queue_t *pxQueue )
{
BaseType_t xReturn;

	portENTER_CRITICAL();
	{
		if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
		{
			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	portEXIT_CRITICAL();

	return xReturn;
}
/*-----------------------------------------------------------*/


