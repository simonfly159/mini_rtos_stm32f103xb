#include <stdint.h>

#define RCC_APB2ENR   (*(volatile uint32_t *)0x40021018)
#define GPIOC_CRH     (*(volatile uint32_t *)0x40011004)
#define GPIOC_ODR     (*(volatile uint32_t *)0x4001100C)

int main(void)
{
    RCC_APB2ENR |= (1 << 4);            /* 使能 GPIOC 时钟 */

    GPIOC_CRH &= ~(0xF << 20);          /* PC13: 推挽输出 2MHz */
    GPIOC_CRH |=  (0x2 << 20);

    while (1)
    {
        GPIOC_ODR ^= (1 << 13);         /* 翻转 PC13（Blue Pill 板载 LED） */
        for (volatile uint32_t i = 0; i < 500000; i++);
    }
}
