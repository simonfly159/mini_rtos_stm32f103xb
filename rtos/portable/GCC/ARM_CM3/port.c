
#include "minirtos.h"

/* Constants required to set up the initial stack. */
#define portINITIAL_XPSR			( 0x01000000 )

/* For strict compliance with the Cortex-M spec the task start address should
have bit-0 clear, as it is loaded into the PC on exit from an ISR. */
#define portSTART_ADDRESS_MASK				( ( StackType_t ) 0xfffffffeUL )

#define portNVIC_SVC_PRI      ( *( volatile uint8_t * ) 0xE000ED1F )
#define portNVIC_PENDSV_PRI   ( *( volatile uint8_t * ) 0xE000ED22 )
#define portNVIC_SYSTICK_PRI  ( *( volatile uint8_t * ) 0xE000ED23 )
#define portNVIC_SYSTICK_CTRL_REG       ( *( volatile uint32_t * ) 0xE000E010UL )
#define portNVIC_SYSTICK_LOAD_REG       ( *( volatile uint32_t * ) 0xE000E014UL )
#define portNVIC_SYSTICK_CURRENT_REG    ( *( volatile uint32_t * ) 0xE000E018UL )

#define portNVIC_SYSTICK_CLK_BIT        ( 1UL << 2UL )   /* CLKSOURCE：1 = 用内核时钟 */
#define portNVIC_SYSTICK_INT_BIT        ( 1UL << 1UL )   /* TICKINT：计到 0 触发异常 */
#define portNVIC_SYSTICK_ENABLE_BIT     ( 1UL << 0UL )   /* ENABLE */

/*
 * Setup the timer to generate the tick interrupts.  The implementation in this
 * file is weak to allow application writers to change the timer used to
 * generate the tick interrupt.
 */
void vPortSetupTimerInterrupt( void );

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void prvTaskExitError( void );

/* Each task maintains its own interrupt status in the critical nesting
variable. */
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;




void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;

	/* This is not the interrupt safe version of the enter critical function so
	assert() if it is being called from an interrupt context.  Only API
	functions that end in "FromISR" can be used in an interrupt.  Only assert if
	the critical nesting count is 1 to protect against recursive calls if the
	assert function also uses a critical section. */
	if( uxCriticalNesting == 1 )
	{
		configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
	}
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );
	uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}
/*-----------------------------------------------------------*/


StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	/* Simulate the stack frame as it would be created by a context switch
	interrupt. */
	pxTopOfStack--; /* Offset added to account for the way the MCU uses the stack on entry/exit of interrupts. */
	*pxTopOfStack = portINITIAL_XPSR;	/* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK;	/* PC */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) prvTaskExitError;	/* LR */

	pxTopOfStack -= 5;	/* R12, R3, R2 and R1. */
	*pxTopOfStack = ( StackType_t ) pvParameters;	/* R0 */
	pxTopOfStack -= 8;	/* R11, R10, R9, R8, R7, R6, R5 and R4. */

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
	/* A function that implements a task must not exit or attempt to return to
	its caller as there is nothing to return to.  If a task wants to exit it
	should instead call vTaskDelete( NULL ).

	Artificially force an assert() to be triggered if configASSERT() is
	defined, then stop here so application writers can catch the error. */
	configASSERT( uxCriticalNesting == ~0UL );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
    {
    volatile uint8_t * const pucIPR0 = ( volatile uint8_t * ) 0xE000E400;
    uint8_t ucOriginal = *pucIPR0;
    *pucIPR0 = 0xFF;                          /* 写全 1 */
    configASSERT( *pucIPR0 == 0xF0 );         /* F103 应读回 0xF0 = 4 位 */
    *pucIPR0 = ucOriginal;                    /* 恢复被实验的寄存器 */
    }

	/* Make PendSV and SysTick the lowest priority interrupts. */
    portNVIC_SVC_PRI     = 0x00;    /* SVC 最高：svc 永不被 BASEPRI 闷死 */
    portNVIC_PENDSV_PRI  = 0xFF;    /* PendSV 最低 */
    portNVIC_SYSTICK_PRI = 0xFF;    /* M2 才用，现在顺手写上无害 */

	/* Start the timer that generates the tick ISR.  Interrupts are disabled
	here already. */
	//vPortSetupTimerInterrupt();
    //先关闭测试任务切换

	/* Start the first task. */
	prvStartFirstTask();

	/* Should not get here! */
	return 0;
}
/*-----------------------------------------------------------*/

void vPortSetupTimerInterrupt( void )
{
    portNVIC_SYSTICK_CTRL_REG    = 0UL;                                   /* 先停 */
    portNVIC_SYSTICK_CURRENT_REG = 0UL;                                   /* 清当前值 */
    portNVIC_SYSTICK_LOAD_REG    = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
    portNVIC_SYSTICK_CTRL_REG    = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );
}
/*-----------------------------------------------------------*/
