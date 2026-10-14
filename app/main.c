#include <stdint.h>
#include <stddef.h>
#include "minirtos.h"
#include "queue.h"
#include "clock.h"
#include "gpio.h"

#define LED_ON()    vBSP_GPIOC13Reset()   /* PC13 低 = 亮 */
#define LED_OFF()   vBSP_GPIOC13Set()     /* PC13 高 = 灭 */
#define LED_TOGGLE() vBSP_GPIOC13Toggle()
#define delay_ms_user(x) do{                             \
    for(volatile uint32_t  a = 0; a < (x); a++)          \
    {                                                    \
        for (volatile uint32_t i = 0; i < 8000; i++);    \
            __asm volatile("nop");                       \
    }                                                    \
}while(0)



TaskHandle_t ledon_TaskHandle;
char ledon_TaskName[10] = "on";
static StaticTask_t ledon_TCB;
static StackType_t  ledon_Stack[configMINIMAL_STACK_SIZE];
uint32_t ledon_TaskStackSize = (uint32_t)configMINIMAL_STACK_SIZE;

TaskHandle_t ledoff_TaskHandle;
char ledoff_TaskName[10] = "off";
static StaticTask_t ledoff_TCB;
static StackType_t  ledoff_Stack[configMINIMAL_STACK_SIZE];
uint32_t ledoff_TaskStackSize = (uint32_t)configMINIMAL_STACK_SIZE;

TaskHandle_t ledset_TaskHandle;
char ledset_TaskName[10] = "set";
static StaticTask_t ledset_TCB;
static StackType_t  ledset_Stack[configMINIMAL_STACK_SIZE];
uint32_t ledset_TaskStackSize = (uint32_t)configMINIMAL_STACK_SIZE;

TaskHandle_t ledswitch_TaskHandle;
char ledswitch_TaskName[10] = "switch";
static StaticTask_t ledswitch_TCB;
static StackType_t  ledswitch_Stack[configMINIMAL_STACK_SIZE];
uint32_t ledswitch_TaskStackSize = (uint32_t)configMINIMAL_STACK_SIZE;

QueueHandle_t ledstate_QueueHandle;
static StaticQueue_t ledstate_Queue;
static UBaseType_t ledstate_QueueLength = ( UBaseType_t ) 1;
uint8_t ledstate_QueueBuffer[1];
uint8_t ledstate_QueueType = queueQUEUE_TYPE_BASE;


uint8_t led_state_tmp = 0;


void ledon_Task(void *argument)
{
    ( void ) argument;

    for(;;)
    {
        if(led_state_tmp == 1)
        {
            LED_ON();
        }

        vTaskDelay(1);
    }
}

void ledoff_Task(void *argument)
{
    ( void ) argument;

    for(;;)
    {
        if(led_state_tmp == 0)
        {
            LED_OFF();
        }

        vTaskDelay(1);
    }
}

void ledset_Task(void *argument)
{
    ( void ) argument;

    uint8_t led_switch_tmp[1];

    for(;;)
    {
        led_switch_tmp[0] = 0;
        xQueueGenericSend( ledstate_QueueHandle, &led_switch_tmp[0], 0, queueSEND_TO_BACK );
        vTaskDelay(500);
        led_switch_tmp[0] = 1;
        xQueueGenericSend( ledstate_QueueHandle, &led_switch_tmp[0], 0, queueSEND_TO_BACK );
        vTaskDelay(500);
    }
}

void ledswitch_Task(void *argument)
{
    ( void ) argument;

    for(;;)
    {
        xQueueReceive( ledstate_QueueHandle, &led_state_tmp, ( TickType_t ) 1000 );
    }
}



int main(void)
{
    vBSP_ClockInit();
    vBSP_GPIOC13Init();

    ledoff_TaskHandle =  xTaskCreateStatic(	ledoff_Task,
								            ledoff_TaskName,
								            ledoff_TaskStackSize,
								            ( void * ) NULL,
								            ( UBaseType_t ) 0x01,
								            &ledoff_Stack[0],
								            &ledoff_TCB ) ;

    ledon_TaskHandle =  xTaskCreateStatic(	ledon_Task,
								            ledon_TaskName,
								            ledon_TaskStackSize,
								            ( void * ) NULL,
								            ( UBaseType_t ) 0x01,
								            &ledon_Stack[0],
								            &ledon_TCB ) ;

    ledset_TaskHandle =  xTaskCreateStatic(	ledset_Task,
								            ledset_TaskName,
								            ledset_TaskStackSize,
								            ( void * ) NULL,
								            ( UBaseType_t ) 0x02,
								            &ledset_Stack[0],
								            &ledset_TCB ) ;

    ledswitch_TaskHandle =  xTaskCreateStatic(	ledswitch_Task,
								                ledswitch_TaskName,
								                ledswitch_TaskStackSize,
								                ( void * ) NULL,
								                ( UBaseType_t ) 0x02,
								                &ledswitch_Stack[0],
								                &ledswitch_TCB ) ;

    ledstate_QueueHandle = xQueueGenericCreateStatic( ledstate_QueueLength,
                                                      sizeof(ledstate_QueueBuffer),
                                                      &ledstate_QueueBuffer[0],
                                                      &ledstate_Queue,
                                                      ledstate_QueueType ) ;

    vTaskStartScheduler();

    while (1)
    {
        

    }
}
