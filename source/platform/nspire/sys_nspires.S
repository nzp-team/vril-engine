	.cpu arm926ej-s
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 1
	.eabi_attribute 30, 4
	.eabi_attribute 18, 4
	.text

	.align	2
	.global	nspire_stack_redirect
	.type	nspire_stack_redirect, %function
nspire_stack_redirect:
	stmfd	sp!, {r1, r2, r3, r4, lr}
	mov	r4, sp
	mov sp, r1
	blx	r0
	mov sp, r4
	ldmfd	sp!, {r1, r2, r3, r4, pc}


	.align	2
	.global	nspire_stack_align
	.type	nspire_stack_align, %function
nspire_stack_align:
	stmfd	sp!, {r0, r1, r2, lr}
	mov	r4, sp
	tst sp, #4
	subne sp, sp, #4
	bl main_s(PLT)
	mov sp, r4
	ldmfd	sp!, {r1, r2, r3, pc}


