/* bsp/clock.c —— HSE 8MHz → PLL ×9 → SYSCLK 72MHz */
#include "clock.h"

#define RCC_BASE          ( 0x40021000UL )
#define FLASH_R_BASE      ( 0x40022000UL )

#define RCC_CR            ( *( volatile uint32_t * ) ( RCC_BASE + 0x00UL ) )
#define RCC_CFGR          ( *( volatile uint32_t * ) ( RCC_BASE + 0x04UL ) )
#define FLASH_ACR         ( *( volatile uint32_t * ) ( FLASH_R_BASE + 0x00UL ) )

/* RCC_CR 位 */
#define RCC_CR_HSEON      ( 1UL << 16 )
#define RCC_CR_HSERDY     ( 1UL << 17 )
#define RCC_CR_PLLON      ( 1UL << 24 )
#define RCC_CR_PLLRDY     ( 1UL << 25 )

/* RCC_CFGR 位 */
#define RCC_CFGR_SW_MASK     ( 0x3UL << 0  )
#define RCC_CFGR_SW_PLL      ( 0x2UL << 0  )   /* SW=10：选 PLL 作系统时钟 */
#define RCC_CFGR_SWS_MASK    ( 0x3UL << 2  )
#define RCC_CFGR_SWS_PLL     ( 0x2UL << 2  )   /* SWS=10：已在用 PLL */
#define RCC_CFGR_PPRE1_DIV2  ( 0x4UL << 8  )   /* APB1 2 分频 → 36MHz（上限 36M） */
#define RCC_CFGR_PLLSRC_HSE  ( 1UL << 16   )   /* PLL 时钟源 = HSE */
#define RCC_CFGR_PLLMULL9    ( 0x7UL << 18 )   /* 8MHz × 9 = 72MHz */
/* HPRE（AHB）和 PPRE2（APB2）复位值就是不分频，不用写 */

/* FLASH_ACR 位 */
#define FLASH_ACR_LATENCY_2  ( 0x2UL << 0  )   /* 72MHz 必须 2 个等待周期 */
#define FLASH_ACR_PRFTBE     ( 1UL << 4    )   /* 开预取指缓冲 */

void vBSP_ClockInit( void )
{
    /* ① 先给 Flash 加等待周期 —— 顺序绝不能反：
       0 等待状态下直接跑 72MHz，取指当场错乱 */
    FLASH_ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    /* ② 开 HSE，等就绪 */
    RCC_CR |= RCC_CR_HSEON;
    while( ( RCC_CR & RCC_CR_HSERDY ) == 0 ) { }

    /* ③ 配 PLL 来源/倍频和 APB1 分频（PLL 关闭状态下配） */
    RCC_CFGR = RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2;

    /* ④ 开 PLL，等锁定 */
    RCC_CR |= RCC_CR_PLLON;
    while( ( RCC_CR & RCC_CR_PLLRDY ) == 0 ) { }

    /* ⑤ 切换系统时钟到 PLL，等切换完成 */
    RCC_CFGR = ( RCC_CFGR & ~RCC_CFGR_SW_MASK ) | RCC_CFGR_SW_PLL;
    while( ( RCC_CFGR & RCC_CFGR_SWS_MASK ) != RCC_CFGR_SWS_PLL ) { }
}
