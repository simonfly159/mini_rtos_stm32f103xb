#include "gpio.h"

#define RCC_APB2ENR   (*(volatile uint32_t *)0x40021018)
#define GPIOC_CRH     (*(volatile uint32_t *)0x40011004)
#define GPIOC_ODR     (*(volatile uint32_t *)0x4001100C)
#define GPIOC_BSRR    (*(volatile uint32_t *)0x40011010)
#define GPIOC_PIN13   (1UL << 13)

void vBSP_GPIOC13Init( void )
{
    RCC_APB2ENR |= (1 << 4);            /* 使能 GPIOC 时钟 */

    GPIOC_CRH &= ~(0xF << 20);          /* PC13: 推挽输出 2MHz */
    GPIOC_CRH |=  (0x2 << 20);
}

/* 高电平：BSRR 低 16 位 = 置位 */
void vBSP_GPIOC13Set( void )
{
    GPIOC_BSRR = GPIOC_PIN13;
}

/* 低电平：BSRR 高 16 位 = 复位 */
void vBSP_GPIOC13Reset( void )
{
    GPIOC_BSRR = ( GPIOC_PIN13 << 16 );
}

/* 翻转：读 ODR 看现状，反向写 */
void vBSP_GPIOC13Toggle( void )
{
    if( GPIOC_ODR & GPIOC_PIN13 )
        vBSP_GPIOC13Reset();
    else
        vBSP_GPIOC13Set();
}

