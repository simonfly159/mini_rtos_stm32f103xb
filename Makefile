# ============================================================================
# Makefile —— mini_rtos (STM32F103C8T6)
#
# 【语法导读：Makefile 只有两大类语法】
#   一、变量赋值
#         名字 = 值              定义变量
#         $(名字)                使用变量（读到时才会展开成值）
#   二、规则（告诉 make "某个文件怎么造出来"）
#         目标: 依赖1 依赖2      ← 想生成什么 : 需要哪些原材料
#         <TAB>命令1             ← 怎么生成。★ 行首必须是 Tab，不能是空格！
#         <TAB>命令2
#
# 【make 的工作流程】
#   1. 从"默认目标"（文件里第一条规则的目标）出发
#   2. 检查依赖：依赖不存在、或比目标新 → 先递归地去构建依赖
#   3. 依赖就绪后，目标不存在或已过期 → 执行本规则命令（命令前会回显）
#   4. 目标已是最新 → 什么都不做  ← 这就是"增量编译"，只重编改过的部分
#
# 用法：make          构建默认目标 all（生成 elf/bin/hex）
#       make clean    构建指定目标 clean（清理所有产物）
#       make main.o   也可以单独构建任何一个目标试试
# Linux 下安装工具链：sudo apt install gcc-arm-none-eabi
# ============================================================================

# ========================== 第一部分：变量定义区 ==========================

# ---- 工具链（交叉编译器前缀） ----
# 语法：变量名 = 值。等号两侧空格随意；# 之后到行尾是注释
# CC      C 编译器（同时可当汇编器、链接器的前端用）
# OBJCOPY 格式转换工具：elf → bin / hex
# SIZE    打印 text/data/bss 各段占用大小
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# ---- 芯片相关选项：Cortex-M3 内核、Thumb 指令集 ----
MCU_FLAGS = -mcpu=cortex-m3 -mthumb

# ---- 编译选项 ----
# 变量值里可以引用其它变量：$(MCU_FLAGS) 在使用时会被替换成上面定义的那串
# -Wall                    打开常用警告
# -O0                      不优化，便于学习时单步对照源码
# -g3                      带最详细的调试信息（含宏定义）
# -ffunction/data-sections 每个函数/变量单独成段，配合 --gc-sections 删未用代码
CFLAGS  = $(MCU_FLAGS) -Wall -O0 -g3 -ffunction-sections -fdata-sections \
		  -Irtos/include -Irtos/portable/GCC/ARM_CM3 -Ibsp

# ---- 链接选项 ----
# -T                指定我们的链接脚本
# -nostdlib         裸机程序，不使用 C 标准库（启动文件里也没调 __libc_init_array）
# -Wl,xxx           不是 gcc 的选项，表示"把 xxx 透传给链接器 ld"
# --gc-sections     删掉没被引用的段（.isr_vector 靠脚本里的 KEEP 保命）
# -Map              生成 map 文件，可查看每个段/符号的最终地址
LDFLAGS = $(MCU_FLAGS) -T stm32f103xb.ld -nostdlib \
          -Wl,--gc-sections -Wl,-Map=$(TARGET).map
# ★ 行尾反斜杠 \ 是续行符：两行会拼成一行读，纯粹为了排版
# ★ 注意上面引用了下面才定义的 $(TARGET) 却不会出错：
#   用 = 定义的变量是"惰性展开"——值在使用时才展开，那时 TARGET 已有值。
#   （若改用 := 立即展开，写在这种顺序下就展开成空串了，初学先记住用 = 即可）

# TARGET 产物文件名（不含后缀）
TARGET  = mini_rtos
OBJS    = app/main.o startup_stm32f103xb.o \
		  rtos/minitask.o \
		  rtos/portable/GCC/ARM_CM3/port.o \
		  rtos/portable/GCC/ARM_CM3/port_asm.o \
		  bsp/clock.o \
		  bsp/gpio.o

# OBJS 是所有目标文件。以后新增 xxx.c，记得在这里加对应的 xxx.o
# ★ 注意：变量值后面不要再写行内注释！make 会把 # 前的空格也保留进值里，
#   导致 $(TARGET).elf 展开出中间带空格的多个单词，规则直接错乱。

# =========================== 第二部分：规则区 ===========================

# 【默认目标】make 不带参数时，构建文件中第一条规则的目标（即 all）
# all 本身不生成文件，只是把三个产物列为依赖 → 充当"全部构建"的入口
all: $(TARGET).elf $(TARGET).bin $(TARGET).hex

# 【链接规则】把 .o 链接成 elf
#   $@  自动变量，代表"目标"，这里展开为 mini_rtos.elf
#   依赖里写上链接脚本：脚本改动后 make 发现依赖变新，会自动重新链接
# 这条规则实际展开的命令是：
#   arm-none-eabi-gcc main.o startup_stm32f103xb.o -mcpu=cortex-m3 -mthumb \
#       -T stm32f103xb.ld -nostdlib -Wl,--gc-sections -Wl,-Map=mini_rtos.map \
#       -o mini_rtos.elf
$(TARGET).elf: $(OBJS) stm32f103xb.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

# 【模式规则】%.o: %.c —— "任何 xxx.o 都可以由同名 xxx.c 编译而来"
#   %   通配一段非空字符串（叫"词干"），目标和依赖里的 % 代表同一段
#   $<  自动变量 = 第一个依赖（即 xxx.c）
#   $@  自动变量 = 目标（即 xxx.o）
# 有了它，OBJS 里每加一个 .o 都不用再手写规则
# gcc 选项：-c 只编译不链接；-o 指定输出文件名
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 大写 .S：先经 C 预处理器（可用 #define/#include），再汇编
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# 【模式规则】汇编启动文件 → .o（gcc 也能直接当汇编器前端用）
# 和上一条并存不冲突：make 会选用"依赖文件实际存在"的那条
%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

# 【格式转换】elf → 裸二进制 / Intel Hex（烧录用）
# 模式规则可以"链式推导"：要 bin → 需要 elf → 自动触发上面的链接规则
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

# 【伪目标】clean 不生成任何文件，只是执行清理命令的"快捷方式"
clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).map

# 【烧录】通过 OpenOCD + ST-Link 把 elf 写入芯片并复位运行
# program ... verify reset exit：烧写 → 校验 → 复位运行 → 退出 openocd
flash: $(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
	        -c "program $(TARGET).elf verify reset exit"

# 把 all / clean / flash 登记为"伪目标"：防止目录里恰好存在同名文件时，
# make 误以为"目标已存在且没依赖，已是最新"而拒绝执行命令
.PHONY: all clean flash
