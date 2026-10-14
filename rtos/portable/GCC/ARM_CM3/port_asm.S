  .syntax unified
  .cpu cortex-m3
  .thumb


  .section .text.vPortSetBASEPRI
  .global vPortSetBASEPRI
  .type vPortSetBASEPRI, %function
vPortSetBASEPRI:
  msr   basepri, r0
  bx lr
  .size vPortSetBASEPRI, .-vPortSetBASEPRI


  .section .text.vPortRaiseBASEPRI
  .global vPortRaiseBASEPRI
  .type vPortRaiseBASEPRI, %function
vPortRaiseBASEPRI:
  movs  r1, #0x50
  msr   basepri, r1
  dsb
  isb
  bx lr
  .size vPortRaiseBASEPRI, .-vPortRaiseBASEPRI


  .section .text.vPortClearBASEPRIFromISR
  .global vPortClearBASEPRIFromISR
  .type vPortClearBASEPRIFromISR, %function
vPortClearBASEPRIFromISR:
  mov r0, #0
  msr basepri, r0
  bx lr
  .size vPortClearBASEPRIFromISR, .-vPortClearBASEPRIFromISR


  .section .text.ulPortRaiseBASEPRI
  .global ulPortRaiseBASEPRI
  .type ulPortRaiseBASEPRI, %function
ulPortRaiseBASEPRI:
  mrs r0, basepri
  mov r1, #0x50
  msr basepri, r1
  dsb
  isb
  bx lr
  .size ulPortRaiseBASEPRI, .-ulPortRaiseBASEPRI


  .section .text.prvStartFirstTask
  .global prvStartFirstTask
  .type prvStartFirstTask, %function
prvStartFirstTask:
	/* Use the NVIC offset register to locate the stack. */
	ldr r0, =0xE000ED08
	ldr r0, [r0]
	ldr r0, [r0]

	/* Set the msp back to the start of the stack. */
	msr msp, r0
	/* Globally enable interrupts. */
	cpsie i
	cpsie f
	dsb
	isb
	/* Call SVC to start the first task. */
	svc 0
	nop
	nop
  .size prvStartFirstTask, .-prvStartFirstTask


  .section .text.SVC_Handler
  .global SVC_Handler
  .extern pxCurrentTCB
  .type SVC_Handler, %function
SVC_Handler:
	ldr	r3, =pxCurrentTCB	/* Restore the context. */
	ldr r1, [r3]			/* Use pxCurrentTCBConst to get the pxCurrentTCB address. */
	ldr r0, [r1]			/* The first item in pxCurrentTCB is the task top of stack. */
	ldmia r0!, {r4-r11}		/* Pop the registers that are not automatically saved on exception entry and the critical nesting count. */
	msr psp, r0				/* Restore the task stack pointer. */
	isb
	mov r0, #0
	msr	basepri, r0
	orr r14, #0xd
	bx r14
  .size SVC_Handler, .-SVC_Handler


  .section .text.PendSV_Handler
  .global PendSV_Handler
  .extern pxCurrentTCB
  .extern vTaskSwitchContext
  .type PendSV_Handler, %function
PendSV_Handler:
	mrs r0, psp
	isb

	ldr	r3, =pxCurrentTCB		/* Get the location of the current TCB. */
	ldr	r2, [r3]

	stmdb r0!, {r4-r11}			/* Save the remaining registers. */
	str r0, [r2]				/* Save the new top of stack into the first member of the TCB. */

	stmdb sp!, {r3, r14}
	mov r0, #0x50 
	msr basepri, r0
	dsb
	isb
	bl vTaskSwitchContext
	mov r0, #0
	msr basepri, r0
	ldmia sp!, {r3, r14}

	ldr r1, [r3]
	ldr r0, [r1]				/* The first item in pxCurrentTCB is the task top of stack. */
	ldmia r0!, {r4-r11}			/* Pop the registers and the critical nesting count. */
	msr psp, r0
	isb
	bx r14
	nop
  .size PendSV_Handler, .-PendSV_Handler

  .section .text.vPortYield
  .global vPortYield
  .type vPortYield, %function
vPortYield:
  ldr r0, =0xE000ED04
  mov r1, #(1 << 28)
  str r1, [r0]
  dsb
  isb
  bx lr
  .size vPortYield, .-vPortYield



