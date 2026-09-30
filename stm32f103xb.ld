/* ============================================================================
 * stm32f103xb.ld  —  STM32F103C8T6 的 GNU ld 链接脚本
 *
 * 写给读过 Keil .sct 的人：两种脚本做的是同一件事 —— 告诉链接器
 *   1) 芯片上有哪些存储区（Flash / RAM 的起始地址和大小）
 *   2) 每个段（.text / .data / .bss …）放到哪个区、按什么顺序
 *
 * 概念对照表：
 *   sct 的 Load Region（加载域）      ≈ ld 的 LMA / AT> 指定的区域
 *   sct 的 Execution Region（执行域） ≈ ld 的 VMA / > 指定的区域
 *   sct 的 *.o (RESET, +First)        ≈ ld 里把 .isr_vector 放在 SECTIONS 第一位
 *   sct 的 +RO / +RW / +ZI            ≈ ld 的 .text / .data / .bss 三个段
 *   sct 的 Image$$…$$Base/Limit       ≈ ld 里手工定义的 _sdata/_edata/_sbss…
 * ==========================================================================*/

/* 程序入口符号。写进 ELF 头的 e_entry，给调试器/仿真器看。
 * 注意：真正决定芯片复位后从哪执行的是向量表第 2 项，不是这里 */
ENTRY(Reset_Handler)

/* MEMORY：声明芯片的物理存储区（必须先定义，后面的 ORIGIN()/LENGTH() 才能用）
 * 格式：名字 (权限) : ORIGIN = 起始地址, LENGTH = 大小
 * 权限字母只是提示链接器：r=可读 w=可写 x=可执行，不起硬件保护作用 */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K   /* C8T6 内部 Flash */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K   /* C8T6 内部 SRAM */
}

/* 栈顶 = RAM 末尾。启动文件把它放在向量表第 1 项，
 * 复位时硬件自动装入 MSP。Cortex-M 栈向下生长，所以初始 SP 指到最高处 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  /* "." 叫位置计数器（location counter），表示"当前排到这儿的地址"，
   * 段每装入一点内容它就自动往前走 —— 类似 sct 执行域里地址自动累加 */

  /* ① 中断向量表：必须排第一（Flash 偏移 0 处），芯片复位就从这里取 SP/PC
   *    对应 sct 的 *.o (RESET, +First) */
  .isr_vector :
  {
    . = ALIGN(4);              /* 4 字节对齐：ARM 要求向量表至少按字对齐 */
    KEEP(*(.isr_vector))       /* KEEP：即使没有任何代码引用它，也不许
                                  --gc-sections 垃圾回收掉（向量表全靠硬件找） */
    . = ALIGN(4);
  } >FLASH                     /* ">FLASH"：这个段装进 FLASH 区 */

  /* ② 代码 + 只读数据，对应 sct 的 +RO
   *    启动文件里 Reset_Handler 所在的 .text.Reset_Handler、
   *    Default_Handler 所在的 .text.Default_Handler 都会被 *(.text*) 收进来 */
  .text :
  {
    . = ALIGN(4);
    *(.text*)                  /* 所有目标文件的代码段 */
    *(.rodata*)                /* 所有只读常量、字符串字面量 */
    . = ALIGN(4);
    _etext = .;                /* 记录代码段末尾地址（Flash 里），
                                  .data 的初值副本就紧跟在它后面 */
  } >FLASH

  /* ③ .data：已初始化的全局/静态变量，对应 sct 的 +RW
   *    ★ 它有两个地址，这是 ld 和 sct 共同的"加载域/执行域"概念：
   *      执行地址 VMA = RAM（程序运行时在这里读写它）
   *      加载地址 LMA = FLASH（初值存在 Flash，掉电不丢；
   *                             复位后由启动文件把它拷到 RAM） */
  _sidata = LOADADDR(.data);   /* LOADADDR() 问链接器要 .data 的加载地址(LMA)，
                                  启动文件拷贝时用它当"源地址" */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;                /* .data 执行域起点（RAM 地址）——拷贝的目标起点 */
    *(.data*)
    . = ALIGN(4);
    _edata = .;                /* .data 执行域终点 —— _sdata/_edata 界定了拷贝范围 */
  } >RAM AT> FLASH             /* 执行域(VMA)在 RAM，加载域(LMA)在 FLASH */

  /* ④ .bss：未初始化（或初始化为 0）的全局/静态变量，对应 sct 的 +ZI
   *    不占 Flash 空间（没有初值要存），只在 RAM 里留出位置，
   *    由启动文件清零。COMMON 是老 C 标准里"公用块"变量的段，一并收进来 */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;                 /* 清零范围起点 */
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;                 /* 清零范围终点 */
  } >RAM

  /* ⑤ 丢掉不想要的段（可省，但留着能让 bin 文件更干净） */
  /DISCARD/ :
  {
    *(.comment)                /* 编译器版本注释字符串 */
    *(.ARM.attributes)         /* ARM 编译属性记录 */
  }
}

/* 最终产出给启动文件的 6 个符号（在启动文件里被 ldr 取用）：
 *   _estack           —— 向量表第 1 项，初始栈顶
 *   _sidata           —— .data 初值在 Flash 中的地址（拷贝源）
 *   _sdata / _edata   —— .data 在 RAM 中的起止（拷贝目标）
 *   _sbss  / _ebss    —— .bss 在 RAM 中的起止（清零范围）
 */
