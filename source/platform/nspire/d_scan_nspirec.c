#include "../../nzportable_def.h"

#include "d_local.h"

extern int bbextents;
extern int bbextentt;


static inline void draw_span_nspire_fw_8( byte *pdest, byte *pbase, fixed16_t f16_rps, fixed16_t f16_sstep, fixed16_t f16_rpt, fixed16_t f16_tstep, int i_cachewidth )
{
	int i_tmp;
	__asm volatile(														\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #1										\n\t"	\
	"smlabt	%[_pbase], %[_i_cachewidth], %[_f16_rpt], %[_pbase]					\n\t"	\
	"ldrb	%[_f16_sstep], [%[_pbase], %[_f16_rps], asr #16]					\n\t"	\
	"strb	%[_f16_sstep], [%[_pdest], #0]										\n\t"	\
	: [_pdest] "+r" (pdest), [_pbase] "+r" (pbase), [_f16_rps] "+r" (f16_rps), [_f16_sstep] "+r" (f16_sstep), [_f16_rpt] "+r" (f16_rpt), [_f16_tstep] "+r" (f16_tstep), [_i_cachewidth] "+r" (i_cachewidth), [_i_tmp] "=r" (i_tmp) : : );
}


static inline void draw_span_nspire_bw_8( byte *pdest, byte *pbase, fixed16_t f16_rps, fixed16_t f16_sstep, fixed16_t f16_rpt, fixed16_t f16_tstep, int i_cachewidth )
{
	int i_tmp;
	__asm volatile (
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #-1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #-1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #-1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #-1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #-1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #-1										\n\t"	\
	"smlabt	%[_i_tmp], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_tmp], [%[_i_tmp], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_tmp], [%[_pdest]], #-1										\n\t"	\
	"smlabt	%[_pbase], %[_i_cachewidth], %[_f16_rpt], %[_pbase]					\n\t"	\
	"ldrb	%[_f16_sstep], [%[_pbase], %[_f16_rps], asr #16]					\n\t"	\
	"strb	%[_f16_sstep], [%[_pdest], #0]										\n\t"	\
	: [_pdest] "+r" (pdest), [_pbase] "+r" (pbase), [_f16_rps] "+r" (f16_rps), [_f16_sstep] "+r" (f16_sstep), [_f16_rpt] "+r" (f16_rpt), [_f16_tstep] "+r" (f16_tstep), [_i_cachewidth] "+r" (i_cachewidth), [_i_tmp] "=r" (i_tmp) : : );
}

static inline void draw_span_nspire_fw_s( byte *pdest, byte *pbase, fixed16_t f16_rps, fixed16_t f16_sstep, fixed16_t f16_rpt, fixed16_t f16_tstep, int i_cachewidth, int i_count )
{
	__asm volatile (
	"sub	%[_i_count], %[_i_count], #1										\n\t"	\
	"cmp	%[_i_count], #7														\n\t"	\
	"addls	pc, pc, %[_i_count], asl #2											\n\t"	\
	"b	.draw_span_nspire_fw_s0													\n\t"	\
	"b	.draw_span_nspire_fw_s1													\n\t"	\
	"b	.draw_span_nspire_fw_s2													\n\t"	\
	"b	.draw_span_nspire_fw_s3													\n\t"	\
	"b	.draw_span_nspire_fw_s4													\n\t"	\
	"b	.draw_span_nspire_fw_s5													\n\t"	\
	"b	.draw_span_nspire_fw_s6													\n\t"	\
	"b	.draw_span_nspire_fw_s7													\n\t"	\
	"b	.draw_span_nspire_fw_s8													\n\t"	\
".draw_span_nspire_fw_s8:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #1										\n\t"	\
".draw_span_nspire_fw_s7:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #1										\n\t"	\
".draw_span_nspire_fw_s6:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #1										\n\t"	\
".draw_span_nspire_fw_s5:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #1										\n\t"	\
".draw_span_nspire_fw_s4:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #1										\n\t"	\
".draw_span_nspire_fw_s3:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #1										\n\t"	\
".draw_span_nspire_fw_s2:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #1										\n\t"	\
".draw_span_nspire_fw_s1:														\n\t"	\
	"smlabt	%[_pbase], %[_i_cachewidth], %[_f16_rpt], %[_pbase]					\n\t"	\
	"ldrb	%[_f16_sstep], [%[_pbase], %[_f16_rps], asr #16]					\n\t"	\
	"strb	%[_f16_sstep], [%[_pdest], #0]										\n\t"	\
".draw_span_nspire_fw_s0:														\n\t"	\
	: [_pdest] "+r" (pdest), [_pbase] "+r" (pbase), [_f16_rps] "+r" (f16_rps), [_f16_sstep] "+r" (f16_sstep), [_f16_rpt] "+r" (f16_rpt), [_f16_tstep] "+r" (f16_tstep), [_i_cachewidth] "+r" (i_cachewidth), [_i_count] "+r" (i_count) : : );

}


static inline void draw_span_nspire_bw_s( byte *pdest, byte *pbase, fixed16_t f16_rps, fixed16_t f16_sstep, fixed16_t f16_rpt, fixed16_t f16_tstep, int i_cachewidth, int i_count )
{
	__asm volatile (
	"sub	%[_i_count], %[_i_count], #1										\n\t"	\
	"cmp	%[_i_count], #7														\n\t"	\
	"addls	pc, pc, %[_i_count], asl #2											\n\t"	\
	"b	.draw_span_nspire_bw_s0													\n\t"	\
	"b	.draw_span_nspire_bw_s1													\n\t"	\
	"b	.draw_span_nspire_bw_s2													\n\t"	\
	"b	.draw_span_nspire_bw_s3													\n\t"	\
	"b	.draw_span_nspire_bw_s4													\n\t"	\
	"b	.draw_span_nspire_bw_s5													\n\t"	\
	"b	.draw_span_nspire_bw_s6													\n\t"	\
	"b	.draw_span_nspire_bw_s7													\n\t"	\
	"b	.draw_span_nspire_bw_s8													\n\t"	\
".draw_span_nspire_bw_s8:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #-1										\n\t"	\
".draw_span_nspire_bw_s7:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #-1										\n\t"	\
".draw_span_nspire_bw_s6:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #-1										\n\t"	\
".draw_span_nspire_bw_s5:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #-1										\n\t"	\
".draw_span_nspire_bw_s4:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #-1										\n\t"	\
".draw_span_nspire_bw_s3:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #-1										\n\t"	\
".draw_span_nspire_bw_s2:														\n\t"	\
	"smlabt	%[_i_count], %[_i_cachewidth], %[_f16_rpt], %[_pbase]				\n\t"	\
	"add	%[_f16_rpt], %[_f16_rpt], %[_f16_tstep]								\n\t"	\
	"ldrb	%[_i_count], [%[_i_count], %[_f16_rps], asr #16]					\n\t"	\
	"add	%[_f16_rps], %[_f16_rps], %[_f16_sstep]								\n\t"	\
	"strb	%[_i_count], [%[_pdest]], #-1										\n\t"	\
".draw_span_nspire_bw_s1:														\n\t"	\
	"smlabt	%[_pbase], %[_i_cachewidth], %[_f16_rpt], %[_pbase]					\n\t"	\
	"ldrb	%[_f16_sstep], [%[_pbase], %[_f16_rps], asr #16]					\n\t"	\
	"strb	%[_f16_sstep], [%[_pdest], #0]										\n\t"	\
".draw_span_nspire_bw_s0:														\n\t"	\
	: [_pdest] "+r" (pdest), [_pbase] "+r" (pbase), [_f16_rps] "+r" (f16_rps), [_f16_sstep] "+r" (f16_sstep), [_f16_rpt] "+r" (f16_rpt), [_f16_tstep] "+r" (f16_tstep), [_i_cachewidth] "+r" (i_cachewidth), [_i_count] "+r" (i_count) : : );
}

#define NSPIRE_NOCLIP_FTW 1

void draw_span_nspire_fw_c( draw_span8_nspire_t *ps_dset )
{
	int spancount;
	fixed16_t f16_psnext;
	fixed16_t f16_ptnext;
	register int f16_rps, f16_rpt, f16_sstep, f16_tstep;

	ps_dset->f16_s = 0;
	ps_dset->f16_t = 0;
	do
	{
	// calculate s and t at the far end of the span
		if (ps_dset->count >= 8)
			spancount = 8;
		else
			spancount = ps_dset->count;

		ps_dset->count -= spancount;

		if ( ps_dset->count)
		{
			fixed16_t i_ipersp;
			ps_dset->f16_p = ( ps_dset->f16_p + ( ps_dset->f16_stepp ) );
			i_ipersp = udiv_fast_32_32_incorrect( ps_dset->f16_p, 0xffffffffU );

			ps_dset->f16_s = ( ps_dset->f16_s + ( ps_dset->f16_steps << 3 ) );
			f16_psnext = ps_dset->f16_sstart + llmull_s16( ps_dset->f16_s, i_ipersp );
#if !NSPIRE_NOCLIP_FTW
			if( f16_psnext > ps_dset->i_bbextents )
			{
				f16_psnext = ps_dset->i_bbextents;
			}
			if( f16_psnext < 8 )
			{
				f16_psnext = 8;
			}
#endif
			f16_sstep = ( f16_psnext - ps_dset->f16_ps ) >> 3;
			f16_rps = ps_dset->f16_ps;
			ps_dset->f16_ps = f16_psnext;


			ps_dset->f16_t = ( ps_dset->f16_t + ( ps_dset->f16_stept << 3 ) );
			f16_ptnext = ps_dset->f16_tstart + llmull_s16( ps_dset->f16_t, i_ipersp );
#if !NSPIRE_NOCLIP_FTW
			if( f16_ptnext > ps_dset->i_bbextentt )
			{
				f16_ptnext = ps_dset->i_bbextentt;
			}
			if( f16_ptnext < 8 )
			{
				f16_ptnext = 8;
			}
#endif
			f16_tstep = ( f16_ptnext - ps_dset->f16_pt ) >> 3;
			f16_rpt = ps_dset->f16_pt;
			ps_dset->f16_pt = f16_ptnext;

			draw_span_nspire_fw_8( ps_dset->pdest, ps_dset->pbase, f16_rps, f16_sstep, f16_rpt, f16_tstep, ps_dset->i_cachewidth );
			ps_dset->pdest += 8;
		}
		else
		{
			static const int rgi_mtab[ 8 ] = { 0, 65536, 32768, 21845, 16384, 13107, 10922, 9362 };

			if (spancount > 1 )
			{
				f16_psnext = ps_dset->f16_psend;
				f16_sstep = llmull_s16( f16_psnext - ps_dset->f16_ps, rgi_mtab[ spancount - 1 ] );

				f16_ptnext = ps_dset->f16_ptend;
				f16_tstep = llmull_s16( f16_ptnext - ps_dset->f16_pt, rgi_mtab[ spancount - 1 ] );
			}

			draw_span_nspire_fw_s( ps_dset->pdest, ps_dset->pbase, ps_dset->f16_ps, f16_sstep, ps_dset->f16_pt, f16_tstep, ps_dset->i_cachewidth, spancount );
		}
	} while (ps_dset->count > 0);
}


void draw_span_nspire_bw_c( draw_span8_nspire_t *ps_dset )
{
	int spancount;
	fixed16_t f16_psnext;
	fixed16_t f16_ptnext;
	register int f16_rps, f16_rpt, f16_sstep, f16_tstep;

	ps_dset->f16_s = 0;
	ps_dset->f16_t = 0;
	do
	{
	// calculate s and t at the far end of the span
		if (ps_dset->count >= 8)
			spancount = 8;
		else
			spancount = ps_dset->count;

		ps_dset->count -= spancount;

		if ( ps_dset->count)
		{
			fixed16_t i_ipersp;
			ps_dset->f16_p = ( ps_dset->f16_p + ( ps_dset->f16_stepp ) );
			i_ipersp = udiv_fast_32_32_incorrect( ps_dset->f16_p, 0xffffffffU );

			ps_dset->f16_s = ( ps_dset->f16_s + ( ps_dset->f16_steps << 3 ) );
			f16_psnext = ps_dset->f16_sstart + llmull_s16( ps_dset->f16_s, i_ipersp );
#if !NSPIRE_NOCLIP_FTW
			if( f16_psnext > ps_dset->i_bbextents )
			{
				f16_psnext = ps_dset->i_bbextents;
			}
			if( f16_psnext < 8 )
			{
				f16_psnext = 8;
			}
#endif
			f16_sstep = ( f16_psnext - ps_dset->f16_ps ) >> 3;
			f16_rps = ps_dset->f16_ps;
			ps_dset->f16_ps = f16_psnext;


			ps_dset->f16_t = ( ps_dset->f16_t + ( ps_dset->f16_stept << 3 ) );
			f16_ptnext = ps_dset->f16_tstart + llmull_s16( ps_dset->f16_t, i_ipersp );
#if !NSPIRE_NOCLIP_FTW
			if( f16_ptnext > ps_dset->i_bbextentt )
			{
				f16_ptnext = ps_dset->i_bbextentt;
			}
			if( f16_ptnext < 8 )
			{
				f16_ptnext = 8;
			}
#endif
			f16_tstep = ( f16_ptnext - ps_dset->f16_pt ) >> 3;
			f16_rpt = ps_dset->f16_pt;
			ps_dset->f16_pt = f16_ptnext;

			draw_span_nspire_bw_8( ps_dset->pdest, ps_dset->pbase, f16_rps, f16_sstep, f16_rpt, f16_tstep, ps_dset->i_cachewidth );
			ps_dset->pdest -= 8;
		}
		else
		{
			static const int rgi_mtab[ 8 ] = { 0, 65536, 32768, 21845, 16384, 13107, 10922, 9362 };

			if (spancount > 1 )
			{
				f16_psnext = ps_dset->f16_psend;
				f16_sstep = llmull_s16( f16_psnext - ps_dset->f16_ps, rgi_mtab[ spancount - 1 ] );

				f16_ptnext = ps_dset->f16_ptend;
				f16_tstep = llmull_s16( f16_ptnext - ps_dset->f16_pt, rgi_mtab[ spancount - 1 ] );
			}

			draw_span_nspire_bw_s( ps_dset->pdest, ps_dset->pbase, ps_dset->f16_ps, f16_sstep, ps_dset->f16_pt, f16_tstep, ps_dset->i_cachewidth, spancount );
		}
	} while (ps_dset->count > 0);
}
