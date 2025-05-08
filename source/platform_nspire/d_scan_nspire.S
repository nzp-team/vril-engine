
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


.align 4
nr_table:
.byte 0xff, 0xfc, 0xf8, 0xf4, 0xf0, 0xed, 0xea, 0xe6
.byte 0xe3, 0xe0, 0xdd, 0xda, 0xd7, 0xd4, 0xd2, 0xcf
.byte 0xcc, 0xca, 0xc7, 0xc5, 0xc3, 0xc0, 0xbe, 0xbc
.byte 0xba, 0xb8, 0xb6, 0xb4, 0xb2, 0xb0, 0xae, 0xac
.byte 0xaa, 0xa8, 0xa7, 0xa5, 0xa3, 0xa2, 0xa0, 0x9f
.byte 0x9d, 0x9c, 0x9a, 0x99, 0x97, 0x96, 0x94, 0x93
.byte 0x92, 0x90, 0x8f, 0x8e, 0x8d, 0x8c, 0x8a, 0x89
.byte 0x88, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81

	.align	2
	.global	udiv_fast_32_32_incorrect
	.type	udiv_fast_32_32_incorrect, %function

udiv_fast_32_32_incorrect:
	stmfd	sp!, {r4, lr}
	clz r2, r0
	movs r4, r0, lsl r2
	add r4, pc, r4, lsr #25
	ldrneb r4, [r4, #nr_table-.nr_table_offset-64]
.nr_table_offset:
	subs r2, r2, #7
	rsb r3, r0, #0
	movpl r0, r4, lsl r2

	mulpl r4, r0, r3
	bmi .udiv_by_large_d
	smlawt r0, r0, r4, r0
	teq r3, r3, asr #1

	mulne r4, r0, r3
	movne r2, #0
	smlalne r2, r0, r4, r0
	beq .udiv_by_0_or_1

	umull r2, r0, r1, r0
	add r1, r1, r3
	mla r1, r0, r3, r1

	cmn r1, r3
	subcs r1, r1, r3
	addcc r0, r0, #1
	addpl r1, r1, r3, lsl #1
	addpl r0, r0, #2
	ldmfd	sp!, {r4, pc}
.udiv_by_large_d:
	sub r4, r4, #4
	rsb r2, r2, #0
	mov r0, r4, lsr r2
	umull r2, r0, r1, r0
	mla r1, r0, r3, r1
	cmn r3, r1, lsr #1
	addcs r1, r1, r3, lsl #1
	addcs r0, r0, #2
	cmn r3, r1
	addcs r1, r1, r3
	addcs r0, r0, #1
	ldmfd	sp!, {r4, pc}
.udiv_by_0_or_1:
	movcs r0, r1
	movcs r1, #0
	movcc r0, #-1
	movcc r1, #-1
	ldmfd	sp!, {r4, pc}



.align 4
nr_table2:
.byte 0xfe, 0xfa, 0xf6, 0xf2, 0xef, 0xeb, 0xe7, 0xe4
.byte 0xe0, 0xdd, 0xd9, 0xd6, 0xd2, 0xcf, 0xcc, 0xc9
.byte 0xc6, 0xc2, 0xbf, 0xbc, 0xb9, 0xb6, 0xb3, 0xb1
.byte 0xae, 0xab, 0xa8, 0xa5, 0xa3, 0xa0, 0x9d, 0x9b
.byte 0x98, 0x96, 0x93, 0x91, 0x8e, 0x8c, 0x8a, 0x87
.byte 0x85, 0x83, 0x80, 0x7e, 0x7c, 0x7a, 0x78, 0x75
.byte 0x73, 0x71, 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65
.byte 0x63, 0x61, 0x5f, 0x5e, 0x5c, 0x5a, 0x58, 0x56
.byte 0x54, 0x53, 0x51, 0x4f, 0x4e, 0x4c, 0x4a, 0x49
.byte 0x47, 0x45, 0x44, 0x42, 0x40, 0x3f, 0x3d, 0x3c
.byte 0x3a, 0x39, 0x37, 0x36, 0x34, 0x33, 0x32, 0x30
.byte 0x2f, 0x2d, 0x2c, 0x2b, 0x29, 0x28, 0x27, 0x25
.byte 0x24, 0x23, 0x21, 0x20, 0x1f, 0x1e, 0x1c, 0x1b
.byte 0x1a, 0x19, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12
.byte 0x10, 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09
.byte 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01

	.align	2
	.global	udiv_s31_32
	.type	udiv_s31_32, %function
udiv_s31_32:
	stmfd	sp!, {r4, lr}
	clz r2, r0
	cmp r1, r0
	movcc r3, r0, lsl r2
	addcc r0, pc, r3, lsr #24
	ldrccb r0, [r0, #nr_table2-.nr_table_offset2-128]
.nr_table_offset2:
	movcc r1, r1, lsl r2
	addcc r0, r0, #256
	smulbbcc r4, r0, r0
	movcs r0, #0x7fffffff
	umullcc r2, r4, r3, r4
	bxcs lr
	rsb r0, r4, r0, lsl #9
	umull r4, r2, r0, r0
	movs r4, r4, lsr #1
	adc r4, r4, r2, lsl #31
	movs r2, r2, lsl #30
	umull r2, r4, r3, r4
	addmi r4, r4, r3
	rsc r0, r4, r0, lsl #16
	umull r2, r0, r1, r0
	umull r2, r4, r3, r0
	rsbs r2, r2, #0
	rsc r1, r4, r1, lsr #1
	subs r2, r2, r3
	sbcs r1, r1, #0
	addpl r0, r0, #1
	subs r2, r2, r3
	sbcs r1, r1, #0
	addpl r0, r0, #1
	ldmfd	sp!, {r4, pc}


	.align	2
	.global	udiv_64_32
	.type	udiv_64_32, %function
udiv_64_32:
	cmp r1, r2
	bcs overflow_32
	rsb r2, r2, #0
	adds r3, r0, r0
	adcs r0, r2, r1, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r3, r3, r3
	adcs r0, r2, r0, lsl #1
	subcc r0, r0, r2
	adcs r2, r3, r3
	mov	r1, r0
	mov	r0, r2
	mov pc, lr
	overflow_32:
	mov r1, #-1
	mov r0, #-1
	mov pc, lr



.macro D_NSpirePolysetDrawSpanZStep zpass, lzi, zistepx, lpz, tmp1, tmp2, zpassor
	ldrsh	\tmp2, [ \lpz ]
	mov		\tmp1, \lzi, asr #16
	cmp		\tmp1, \tmp2
	strgeh	\tmp1, [ \lpz ]
	orrge	\zpass, \zpassor
	add		\lzi, \zistepx
	add		\lpz, \lpz, #2
.endm

.macro D_NSpirePolysetDrawSpanTStep zpass, lpdest, lptex, colormap, skinwidth, llight, llightstep, sfrac, sfracstep, tfrac, tfracstep, tmp1, tmp2, zpassand, labellizer
	tst	\zpass, \zpassand
	beq .nzpass_\labellizer
	mov		\tmp1, \sfrac, asr #16
	smlabt	\tmp1, \skinwidth, \tfrac, \tmp1
	ldrb	\tmp2, [ \lptex, \tmp1 ]
	and		\tmp1, \llight, #65280
	add		\tmp2, \tmp1, \tmp2
	ldrb	\tmp1, [ \colormap, \tmp2 ]
	strb	\tmp1, [ \lpdest ]
.nzpass_\labellizer:
	add		\lpdest, \lpdest, #1
	add		\sfrac, \sfrac, \sfracstep
	add		\tfrac, \tfrac, \tfracstep
	add		\llight, \llight, \llightstep
.endm
	
#if 0
	.align	2
	.global	D_NSpirePolysetDrawSpan
	.type	D_NSpirePolysetDrawSpan, %function
D_NSpirePolysetDrawSpan:
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, r10, r11, lr }

	ldr	r7, [ r0, #4 ]		@ lpdest
	ldr	r8, [ r0, #24 ]		@ lsfrac
	ldr	r9, [ r0, #28 ]		@ ltfrac
	ldr	r10, [ r0, #32 ]	@ llight
	ldr	r11, [ r0, #52 ]	@ llightstep

.D_NSpirePolysetDrawSpan_lcount_loop:
	ldr r1, [r0, #0]	@ lcount

	ldr r2, [r0, #12]	@ lpz
	ldr r3, [r0, #36]	@ lzi
	ldr r4, [r0, #48]	@ lzixstep
	mov	lr, #0			@ zpass

	cmp	r1, #7
	addls	pc, pc, r1, asl #2
	b	.D_NSpirePolysetDrawSpan_zspan8
@ ...
	b	.D_NSpirePolysetDrawSpan_zspan0
	b	.D_NSpirePolysetDrawSpan_zspan1
	b	.D_NSpirePolysetDrawSpan_zspan2
	b	.D_NSpirePolysetDrawSpan_zspan3
	b	.D_NSpirePolysetDrawSpan_zspan4
	b	.D_NSpirePolysetDrawSpan_zspan5
	b	.D_NSpirePolysetDrawSpan_zspan6
	b	.D_NSpirePolysetDrawSpan_zspan7
.D_NSpirePolysetDrawSpan_zspan8:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #1
.D_NSpirePolysetDrawSpan_zspan7:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #2
.D_NSpirePolysetDrawSpan_zspan6:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #4
.D_NSpirePolysetDrawSpan_zspan5:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #8
.D_NSpirePolysetDrawSpan_zspan4:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #16
.D_NSpirePolysetDrawSpan_zspan3:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #32
.D_NSpirePolysetDrawSpan_zspan2:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #64
.D_NSpirePolysetDrawSpan_zspan1:
	D_NSpirePolysetDrawSpanZStep lr, r3, r4, r2, r5, r6, #128
.D_NSpirePolysetDrawSpan_zspan0:
	str r2, [r0, #12]	@ lpz
	str r3, [r0, #36]	@ lzi

@ tex + l
	ldr	r2, [ r0, #8 ]		@ lptex
	ldr r3, [ r0, #16 ]		@ colormap
	ldr r4, [ r0, #20 ]		@ skinwidth
	ldr	r5, [ r0, #40 ]		@ sfracstep
	ldr	r6, [ r0, #44 ]		@ tfracstep

	cmp	r1, #7
	addls	pc, pc, r1, asl #2
	b	.D_NSpirePolysetDrawSpan_tspan8
@ ...
	b	.D_NSpirePolysetDrawSpan_tspan0
	b	.D_NSpirePolysetDrawSpan_tspan1
	b	.D_NSpirePolysetDrawSpan_tspan2
	b	.D_NSpirePolysetDrawSpan_tspan3
	b	.D_NSpirePolysetDrawSpan_tspan4
	b	.D_NSpirePolysetDrawSpan_tspan5
	b	.D_NSpirePolysetDrawSpan_tspan6
	b	.D_NSpirePolysetDrawSpan_tspan7
.D_NSpirePolysetDrawSpan_tspan8:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #1, s8
.D_NSpirePolysetDrawSpan_tspan7:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #2, s7
.D_NSpirePolysetDrawSpan_tspan6:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #4, s6
.D_NSpirePolysetDrawSpan_tspan5:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #8, s5
.D_NSpirePolysetDrawSpan_tspan4:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #16, s4
.D_NSpirePolysetDrawSpan_tspan3:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #32, s3
.D_NSpirePolysetDrawSpan_tspan2:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #64, s2
.D_NSpirePolysetDrawSpan_tspan1:
	D_NSpirePolysetDrawSpanTStep lr, r7, r2, r3, r4, r10, r11, r8, r5, r9, r6, r12, r1, #128, s1
.D_NSpirePolysetDrawSpan_tspan0:

	ldr	r1, [ r0, #0 ]		@ lcount
	sub	r1, r1, #8
	cmp	r1, #0
	str	r1, [ r0, #0 ]
	bgt	.D_NSpirePolysetDrawSpan_lcount_loop
	ldmfd	sp!, { r4, r5, r6, r7, r8, r9, r10, r11, pc }

#endif

.macro D_NSpirePolysetDrawSpanZTStep lpdest, lptex, lpz, colormap, sfrac, tfrac, llight, lzi, sfracstep, tfracstep, zistepx, llightstep, tmp1, tmp2, labellizer
	ldrsh	\tmp2, [ \lpz ]
	mov		\tmp1, \lzi, asr #16
	cmp		\tmp1, \tmp2
	ble .nzpass_\labellizer
	ldr		\tmp2, [ sp, #4 ]
	strh	\tmp1, [ \lpz ]
	mov		\tmp1, \sfrac, asr #16
	smlabt	\tmp1, \tmp2, \tfrac, \tmp1
	ldrb	\tmp2, [ \lptex, \tmp1 ]
	and		\tmp1, \llight, #65280
	add		\tmp2, \tmp1, \tmp2
	ldrb	\tmp1, [ \colormap, \tmp2 ]
	strb	\tmp1, [ \lpdest ]
.nzpass_\labellizer:
	add		\lzi, \zistepx
	add		\lpz, \lpz, #2
	add		\lpdest, \lpdest, #1
	add		\sfrac, \sfrac, \sfracstep
	add		\tfrac, \tfrac, \tfracstep
	add		\llight, \llight, \llightstep
.endm
	

	.align	2
	.global	D_NSpirePolysetDrawSpan
	.type	D_NSpirePolysetDrawSpan, %function
D_NSpirePolysetDrawSpan:
	stmfd	sp!, { r0, r1, r4, r5, r6, r7, r8, r9, r10, r11, lr }

	ldr r1, [ r0, #20 ]	@ skinwidth
	str	r1, [ sp, #4 ]

	ldr	r2, [ r0, #4 ]		@ lpdest
	ldr	r3, [ r0, #8 ]		@ lptex
	ldr r4, [ r0, #12]		@ lpz
	ldr r5, [ r0, #16 ]		@ colormap
	ldr	r6, [ r0, #24 ]		@ lsfrac
	ldr	r7, [ r0, #28 ]		@ ltfrac
	ldr	r8, [ r0, #32 ]		@ llight
	ldr r9, [ r0, #36]		@ lzi
	ldr	r10, [ r0, #40 ]	@ sfracstep
	ldr	r11, [ r0, #44 ]	@ tfracstep
	ldr r12, [ r0, #48 ]	@ lzixstep
	ldr	lr, [ r0, #52 ]		@ llightstep

.D_NSpirePolysetDrawSpan_lcount_loop:
	ldr	r0, [sp, #0]
	ldr r1, [r0, #0]	@ lcount

	cmp	r1, #7
	addls	pc, pc, r1, asl #2
	b	.D_NSpirePolysetDrawSpan_ztspan8
@ ...
	b	.D_NSpirePolysetDrawSpan_ztspan0
	b	.D_NSpirePolysetDrawSpan_ztspan1
	b	.D_NSpirePolysetDrawSpan_ztspan2
	b	.D_NSpirePolysetDrawSpan_ztspan3
	b	.D_NSpirePolysetDrawSpan_ztspan4
	b	.D_NSpirePolysetDrawSpan_ztspan5
	b	.D_NSpirePolysetDrawSpan_ztspan6
	b	.D_NSpirePolysetDrawSpan_ztspan7
.D_NSpirePolysetDrawSpan_ztspan8:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 8
.D_NSpirePolysetDrawSpan_ztspan7:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 7
.D_NSpirePolysetDrawSpan_ztspan6:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 6
.D_NSpirePolysetDrawSpan_ztspan5:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 5
.D_NSpirePolysetDrawSpan_ztspan4:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 4
.D_NSpirePolysetDrawSpan_ztspan3:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 3
.D_NSpirePolysetDrawSpan_ztspan2:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 2
.D_NSpirePolysetDrawSpan_ztspan1:
	D_NSpirePolysetDrawSpanZTStep r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, r0, r1, 1
.D_NSpirePolysetDrawSpan_ztspan0:

	ldr	r0, [sp, #0]
	ldr r1, [r0, #0]	@ lcount
	sub	r1, r1, #8
	cmp	r1, #0
	str	r1, [ r0, #0 ]
	bgt	.D_NSpirePolysetDrawSpan_lcount_loop
	ldmfd	sp!, { r0, r1, r4, r5, r6, r7, r8, r9, r10, r11, pc }




