

extern unsigned int udiv_fast_32_32_incorrect(unsigned int d, unsigned int n); /* this one has a slight error */
extern unsigned int udiv_64_32( unsigned long long n, unsigned int d );
extern unsigned int udiv_s31_32(unsigned int d, unsigned int n);

static inline long long mul_64_32_r64( long long m0, int m1 )
{
	int __tmp;
	long long __ires;
	__asm volatile(															\
		"smull %Q[result], %R[result], %Q[_m0], %[_m1]	\n\t"				\
		"tst %Q[_m0], #0x80000000						\n\t"				\
		"addne %R[result], %R[result], %[_m1]			\n\t"				\
		"smlal %R[result], %[_tmp], %R[_m0], %[_m1]		\n\t"				\
		: [result] "=&r" (__ires), [_tmp] "=&r" (__tmp ) : [_m0] "r" (m0), [_m1] "r" (m1) : );
	return __ires;
}

static inline long long llmull_s0( int m0, int m1 )
{
	long long __ires;
	__asm volatile(														\
		"smull %Q[result], %R[result], %[_m0], %[_m1] \n"					\
		: [result] "=&r" (__ires) : [_m0] "r" (m0), [_m1] "r" (m1) : );
	return __ires;
}

static inline int llmull_s16( int m0, int m1 )
{
	int __ires;
	__asm volatile(														\
		"smull %[result], %[_m1], %[_m0], %[_m1] \n"					\
		"mov %[result], %[result], lsr #16 \n"							\
		"orr %[result], %[_m1], asl #16 \n"								\
		: [result] "=&r" (__ires), [_m1] "+r" (m1) : [_m0] "r" (m0) : );
	return __ires;
}

static inline int llmull_s21( int m0, int m1 )
{
	int __ires;
	__asm volatile(														\
		"smull %[result], %[_m1], %[_m0], %[_m1] \n"					\
		"mov %[result], %[result], lsr #21 \n"							\
		"orr %[result], %[_m1], asl #11 \n"								\
		: [result] "=&r" (__ires), [_m1] "+r" (m1) : [_m0] "r" (m0) : );
	return __ires;
}

static inline int llmull_s24( int m0, int m1 )
{
	int __ires;
	__asm volatile(														\
		"smull %[result], %[_m1], %[_m0], %[_m1] \n"					\
		"mov %[result], %[result], lsr #24 \n"							\
		"orr %[result], %[_m1], asl #8 \n"								\
		: [result] "=&r" (__ires), [_m1] "+r" (m1) : [_m0] "r" (m0) : );
	return __ires;
}

static inline int llmull_s27( int m0, int m1 )
{
	int __ires;
	__asm volatile(														\
		"smull %[result], %[_m1], %[_m0], %[_m1] \n"					\
		"mov %[result], %[result], lsr #27 \n"							\
		"orr %[result], %[_m1], asl #5 \n"								\
		: [result] "=&r" (__ires), [_m1] "+r" (m1) : [_m0] "r" (m0) : );
	return __ires;
}

static inline int llmull_s28( int m0, int m1 )
{
	int __ires;
	__asm volatile(														\
		"smull %[result], %[_m1], %[_m0], %[_m1] \n"					\
		"mov %[result], %[result], lsr #28 \n"							\
		"orr %[result], %[_m1], asl #4 \n"								\
		: [result] "=&r" (__ires), [_m1] "+r" (m1) : [_m0] "r" (m0) : );
	return __ires;
}

typedef	long long fixed32_t;
extern void test_float32_shift( float *f, int shift );
#define CALCG_FIXED_FTOF32( f, f32 ) \
do {								\
	float temp;						\
	temp = f;						\
	test_float32_shift( &temp, 32 );	\
	f32 = ( fixed32_t )( temp );		\
} while( 0 )

extern void test_float32_shift( float *f, int shift );

#define FIXED_FIXEDTOFLOAT( in_fixed, out_float, frac ) \
do {										\
	float temp;								\
	temp = ( float )in_fixed;				\
	test_float32_shift( &temp, -(frac) );	\
	out_float = temp;						\
} while( 0 )

#define FIXED_FLOATTOFIXED( in_float, out_fixed, frac ) \
do {									\
	float temp;							\
	temp = in_float;					\
	test_float32_shift( &temp, frac );	\
	out_fixed = ( fixed16_t) ( temp );	\
} while( 0 )

#define FIXED_SHIFT_FLOAT( in_float, out_float, shift ) \
do {									\
	out_float = in_float;				\
	test_float32_shift( &out_float, shift );	\
} while( 0 )
