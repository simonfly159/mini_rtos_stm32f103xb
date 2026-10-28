# mini_rtos_stm32f103xb

一个参考 **FreeRTOS v10.3.1**、在 STM32F103C8T6 上从零手写的迷你 RTOS 内核学习项目。

仅移植 FreeRTOS 的核心基础功能，配套自写的启动文件与链接脚本，内核（`rtos/` + `lib/`）总计约 **3000 行**，相比完整 FreeRTOS 内核大幅缩减体积，便于逐行阅读、对照源码学习 RTOS 原理。

## 已实现功能

- **任务调度**：优先级抢占式调度，同优先级时间片轮转
- **系统节拍**：SysTick 1 kHz，基于 BASEPRI 的临界区
- **阻塞与延时**：双延时链表设计，自动处理 tick 计数溢出
- **队列**：任务间按值传递数据，阻塞等待队列按优先级排序
- **信号量**：二值信号量、计数信号量
- **互斥锁**：优先级继承、等待者超时后的优先级降级

## 设计取舍

- **仅静态内存分配**：任务 / 队列 / 信号量 / 互斥锁全部使用 `*Static` 接口创建，无堆、无动态内存管理
- **用临界区替代** `vTaskSuspendAll` / 队列锁机制，无 `xPendingReadyList`
- **未实现**：任务挂起/恢复、事件组、软件定时器、队列集、递归互斥锁（结构体字段已预留）
- `-nostdlib` 裸机环境，`memcpy` 等少量库函数自行实现（见 `lib/`）

## 适用平台

**仅支持 STM32F103xb 系列**（已在 STM32F103C8T6 上验证）。

启动文件 `startup_stm32f103xb.s`、链接脚本 `stm32f103xb.ld`、时钟初始化 `bsp/clock.c` 均按该系列编写。移植到其它 Cortex-M3/M4 芯片需替换启动文件与链接脚本，并按实际主频调整时钟配置。

## 目录结构

```
app/                            应用层（LED 演示：队列 → 信号量 → 互斥锁 全链路）
bsp/                            时钟与 GPIO 初始化
rtos/                           内核（list / minitask / queue）
rtos/include/                   内核头文件
rtos/portable/GCC/ARM_CM3/      Cortex-M3 移植层（PendSV / SVC / SysTick / BASEPRI）
lib/                            -nostdlib 环境下自实现的 memcpy
startup_stm32f103xb.s           启动文件
stm32f103xb.ld                  链接脚本
Makefile                        构建脚本
```

## 编译（Linux）

安装工具链后执行 `make`：

```bash
sudo apt install gcc-arm-none-eabi make     # Debian / Ubuntu
make
```

产物：`mini_rtos.elf` / `mini_rtos.bin` / `mini_rtos.hex`（附 `mini_rtos.map`）。

## 下载（Linux，ST-Link + OpenOCD）

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program mini_rtos.elf verify reset exit"
```

或直接执行 `make flash`（内部就是上面这条命令）。

## 演示程序说明

`app/main.c` 实现了一条完整的内核功能验证链路：

```
ledset 任务 --队列--> ledswitch 任务 --二值信号量--> ledon / ledoff 任务 --> LED
```

另有低优先级 `ledmutex` 任务周期性强占互斥锁约 1 秒，使 LED 呈现"**闪烁 1 秒 / 冻结 1 秒**"的交替现象；同时让互斥锁的阻塞、优先级继承与超时降级路径在运行中被反复执行。

## 参考

- [FreeRTOS Kernel v10.3.1](https://github.com/FreeRTOS/FreeRTOS-Kernel) —— 本项目的功能与代码结构参照对象
