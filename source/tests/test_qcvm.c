#include "../nzportable_def.h"
#include "../qcvm/pr_vm.h"

#include <math.h>

static int Test_QCVM_Expect(const char *name, int actual, int expected)
{
	if (actual == expected)
	{
		Con_Printf("[PASS] QCVM: %s\n", name);
		return 0;
	}
	Con_Printf("[FAIL] QCVM: %s (got %i, expected %i)\n",
		name, actual, expected);
	return 1;
}

static int Test_QCVM_ExpectFloat(const char *name, float actual, float expected)
{
	if (fabsf(actual - expected) <= 0.00001f)
	{
		Con_Printf("[PASS] QCVM: %s\n", name);
		return 0;
	}
	Con_Printf("[FAIL] QCVM: %s (got %g, expected %g)\n", name, (double)actual, (double)expected);
	return 1;
}

static int Test_QCVM_FloatOpcode(const char *name, int opcode,
	float lhs, float rhs, float expected)
{
	eval_t a = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t b = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t c = {.vector = {0.0f, 0.0f, 0.0f}};

	a._float = lhs;
	b._float = rhs;
	if (!PR_VM_ExecuteCoreOpcode(opcode, &a, &b, &c))
		return Test_QCVM_Expect(name, 0, 1);
	return Test_QCVM_ExpectFloat(name, c._float, expected);
}

static int Test_QCVM_VectorOpcode(const char *name, int opcode,
	const vec3_t lhs, const vec3_t rhs, const vec3_t expected)
{
	eval_t a = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t b = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t c = {.vector = {0.0f, 0.0f, 0.0f}};
	int matches;

	VectorCopy(lhs, a.vector);
	VectorCopy(rhs, b.vector);
	if (!PR_VM_ExecuteCoreOpcode(opcode, &a, &b, &c))
		return Test_QCVM_Expect(name, 0, 1);
	matches = fabsf(c.vector[0] - expected[0]) <= 0.00001f
		&& fabsf(c.vector[1] - expected[1]) <= 0.00001f
		&& fabsf(c.vector[2] - expected[2]) <= 0.00001f;
	return Test_QCVM_Expect(name, matches, 1);
}

test_status_t Test_QCVM_Start(void)
{
	union
	{
		float f;
		uint32_t bits;
	} value;
	const uint32_t zero = 0;
	const uint32_t one = UINT32_C(0x3f800000);
	const uint32_t negative_three = UINT32_C(0xc0400000);
	const uint32_t entity = UINT32_C(0x00000420);
	const vec3_t vector_a = {1.0f, -2.0f, 3.5f};
	const vec3_t vector_b = {-4.0f, 5.0f, 0.5f};
	const vec3_t vector_add = {-3.0f, 3.0f, 4.0f};
	const vec3_t vector_sub = {5.0f, -7.0f, 3.0f};
	const vec3_t vector_scaled = {2.0f, -4.0f, 7.0f};
	const vec3_t vector_b_doubled = {-8.0f, 10.0f, 1.0f};
	eval_t a = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t b = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t c = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t target = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t result = {.vector = {0.0f, 0.0f, 0.0f}};
	eval_t array_slots[8] = {{0}};
	int array_words[16] = {0};
	int entity_words[8] = {0};
	int array_pointer;
	int failures = 0;

	// MARK: Id Ops

	failures += Test_QCVM_Expect("FTE OP_MULSTOREP_F ABI", OP_MULSTOREP_F, 68);
	failures += Test_QCVM_Expect("FTE OP_RAND0 ABI", OP_RAND0, 92);
	failures += Test_QCVM_Expect("FTE OP_STOREF_V ABI", OP_STOREF_V, 219);
	failures += Test_QCVM_Expect("FTE OP_STOREF_I ABI", OP_STOREF_I, 222);
	failures += Test_QCVM_Expect("FTE OP_CONV_FTOI ABI", OP_CONV_FTOI, 123);
	failures += Test_QCVM_Expect("FTE OP_GLOBALADDRESS ABI", OP_GLOBALADDRESS, 143);
	failures += Test_QCVM_Expect("FTE OP_BOUNDCHECK ABI", OP_BOUNDCHECK, 211);
	failures += Test_QCVM_Expect("FTE OP_GLOAD_V ABI", OP_GLOAD_V, 216);
	failures += Test_QCVM_Expect("FTE OP_ADD_I ABI", OP_ADD_I, 116);
	failures += Test_QCVM_Expect("FTE OP_CONV_ITOF ABI", OP_CONV_ITOF, 122);
	failures += Test_QCVM_Expect("FTE OP_MUL_I ABI", OP_MUL_I, 132);

	failures += Test_QCVM_Expect("+0 truth", PR_VM_TruthyBits(zero), 0);
	failures += Test_QCVM_Expect("-0 truth", PR_VM_TruthyBits(UINT32_C(0x80000000)), 0);
	failures += Test_QCVM_Expect("+subnormal truth", PR_VM_TruthyBits(UINT32_C(1)), 1);
	failures += Test_QCVM_Expect("-subnormal truth", PR_VM_TruthyBits(UINT32_C(0x80000001)), 1);
	failures += Test_QCVM_Expect("+3 truth", PR_VM_TruthyBits(UINT32_C(0x40400000)), 1);
	failures += Test_QCVM_Expect("-3 truth", PR_VM_TruthyBits(negative_three), 1);
	failures += Test_QCVM_Expect("entity truth", PR_VM_TruthyBits(entity), 1);
	failures += Test_QCVM_Expect("OP_AND true/true", PR_VM_LogicalAnd(one, entity), 1);
	failures += Test_QCVM_Expect("OP_AND true/false", PR_VM_LogicalAnd(entity, zero), 0);
	failures += Test_QCVM_Expect("OP_OR false/false", PR_VM_LogicalOr(zero, zero), 0);
	failures += Test_QCVM_Expect("OP_OR entity/false", PR_VM_LogicalOr(entity, zero), 1);
	failures += Test_QCVM_Expect("OP_NOT +subnormal", PR_VM_LogicalNot(UINT32_C(1)), 0);
	failures += Test_QCVM_Expect("OP_NOT -subnormal", PR_VM_LogicalNot(UINT32_C(0x80000001)), 0);

	failures += Test_QCVM_FloatOpcode("OP_ADD_F mixed signs", OP_ADD_F, 2.5f, -4.0f, -1.5f);
	failures += Test_QCVM_FloatOpcode("OP_SUB_F negative result", OP_SUB_F, -3.0f, 2.0f, -5.0f);
	failures += Test_QCVM_FloatOpcode("OP_MUL_F fraction", OP_MUL_F, -3.0f, 2.5f, -7.5f);
	failures += Test_QCVM_FloatOpcode("OP_MUL_F by zero", OP_MUL_F, -3.0f, 0.0f, 0.0f);
	failures += Test_QCVM_FloatOpcode("OP_DIV_F fraction", OP_DIV_F, 7.0f, 2.0f, 3.5f);
	failures += Test_QCVM_FloatOpcode("OP_DIV_F negative", OP_DIV_F, 9.0f, -3.0f, -3.0f);
	failures += Test_QCVM_FloatOpcode("OP_BITAND", OP_BITAND, 6.0f, 3.0f, 2.0f);
	failures += Test_QCVM_FloatOpcode("OP_BITOR", OP_BITOR, 6.0f, 3.0f, 7.0f);
	failures += Test_QCVM_FloatOpcode("OP_BITAND truncates", OP_BITAND, 6.9f, 3.2f, 2.0f);

	failures += Test_QCVM_FloatOpcode("OP_GE equal", OP_GE, -2.0f, -2.0f, 1.0f);
	failures += Test_QCVM_FloatOpcode("OP_GE false", OP_GE, -3.0f, -2.0f, 0.0f);
	failures += Test_QCVM_FloatOpcode("OP_LE equal", OP_LE, 4.0f, 4.0f, 1.0f);
	failures += Test_QCVM_FloatOpcode("OP_GT negative", OP_GT, -2.0f, -3.0f, 1.0f);
	failures += Test_QCVM_FloatOpcode("OP_LT negative", OP_LT, -3.0f, -2.0f, 1.0f);
	failures += Test_QCVM_FloatOpcode("OP_EQ_F signed zero", OP_EQ_F, 0.0f, -0.0f, 1.0f);
	failures += Test_QCVM_FloatOpcode("OP_NE_F signed zero", OP_NE_F, 0.0f, -0.0f, 0.0f);
	failures += Test_QCVM_FloatOpcode("OP_EQ_F distinct", OP_EQ_F, 1.0f, 1.0001f, 0.0f);
	failures += Test_QCVM_FloatOpcode("OP_NE_F distinct", OP_NE_F, 1.0f, 1.0001f, 1.0f);
	failures += Test_QCVM_FloatOpcode("OP_NOT_F zero", OP_NOT_F, 0.0f, 0.0f, 1.0f);
	failures += Test_QCVM_FloatOpcode("OP_NOT_F negative", OP_NOT_F, -3.0f, 0.0f, 0.0f);
	a._int = 1;
	PR_VM_ExecuteCoreOpcode(OP_NOT_F, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NOT_F +subnormal", c._float, 0.0f);
	a._int = (int)UINT32_C(0x80000001);
	PR_VM_ExecuteCoreOpcode(OP_NOT_F, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NOT_F -subnormal", c._float, 0.0f);

	// MARK: FTEQCC Extended ops

	a._int = 0x12345678;
	PR_VM_StoreScalar(&a, &target);
	failures += Test_QCVM_Expect("OP_STOREF scalar raw slot", target._int, a._int);
	VectorCopy(vector_a, a.vector);
	PR_VM_StoreVector(&a, &target);
	failures += Test_QCVM_Expect("OP_STOREF_V", VectorCompare(target.vector, vector_a), 1);
	a._float = 2.0f;
	target._float = 10.0f;
	PR_VM_AddStorePFloat(&a, &target, &result);
	failures += Test_QCVM_ExpectFloat("OP_ADDSTOREP_F target", target._float, 12.0f);
	failures += Test_QCVM_ExpectFloat("OP_ADDSTOREP_F result", result._float, 12.0f);
	PR_VM_SubStorePFloat(&a, &target, &result);
	failures += Test_QCVM_ExpectFloat("OP_SUBSTOREP_F", target._float, 10.0f);
	PR_VM_MulStorePFloat(&a, &target, &result);
	failures += Test_QCVM_ExpectFloat("OP_MULSTOREP_F", target._float, 20.0f);
	PR_VM_DivStorePFloat(&a, &target, &result);
	failures += Test_QCVM_ExpectFloat("OP_DIVSTOREP_F", target._float, 10.0f);
	VectorCopy(vector_a, a.vector);
	VectorCopy(vector_b, target.vector);
	PR_VM_AddStorePVector(&a, &target, &result);
	failures += Test_QCVM_Expect("OP_ADDSTOREP_V", VectorCompare(target.vector, vector_add), 1);
	PR_VM_SubStorePVector(&a, &target, &result);
	failures += Test_QCVM_Expect("OP_SUBSTOREP_V", VectorCompare(target.vector, vector_b), 1);
	a._float = 2.0f;
	PR_VM_MulStorePVectorFloat(&a, &target, &result);
	failures += Test_QCVM_Expect("OP_MULSTOREP_VF",
		VectorCompare(target.vector, vector_b_doubled), 1);
	target._float = 5.0f;
	a._float = 2.0f;
	PR_VM_BitSetStorePFloat(&a, &target);
	failures += Test_QCVM_ExpectFloat("OP_BITSETSTOREP_F", target._float, 7.0f);
	PR_VM_BitClearStorePFloat(&a, &target);
	failures += Test_QCVM_ExpectFloat("OP_BITCLRSTOREP_F", target._float, 5.0f);
	failures += Test_QCVM_ExpectFloat("OP_RAND0 minimum", PR_VM_RandomUnit(0), 0.0f);
	failures += Test_QCVM_Expect("OP_RAND0 excludes one", PR_VM_RandomUnit(0x7fff) < 1.0f, 1);
	failures += Test_QCVM_ExpectFloat("OP_RAND1 scaling", PR_VM_RandomScale(0x4000, 8.0f), 4.0f);
	failures += Test_QCVM_ExpectFloat("OP_RAND2 range", PR_VM_RandomRange(0x4000, -2.0f, 6.0f), 2.0f);

	a._int = 2000000000;
	b._int = 2000000000;
	PR_VM_ExecuteCoreOpcode(OP_ADD_I, &a, &b, &c);
	failures += Test_QCVM_Expect("OP_ADD_I wraps 32-bit", c._int, -294967296);
	a._int = -7;
	b._int = 6;
	PR_VM_ExecuteCoreOpcode(OP_MUL_I, &a, &b, &c);
	failures += Test_QCVM_Expect("OP_MUL_I signed operands", c._int, -42);
	a._int = -12345;
	PR_VM_ExecuteCoreOpcode(OP_CONV_ITOF, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_CONV_ITOF negative", c._float, -12345.0f);
	a._float = 3.75f;
	PR_VM_ExecuteCoreOpcode(OP_CONV_FTOI, &a, &b, &c);
	failures += Test_QCVM_Expect("OP_CONV_FTOI truncates positive", c._int, 3);
	a._float = -3.75f;
	PR_VM_ExecuteCoreOpcode(OP_CONV_FTOI, &a, &b, &c);
	failures += Test_QCVM_Expect("OP_CONV_FTOI truncates negative", c._int, -3);
	failures += Test_QCVM_Expect("OP_BOUNDCHECK lower inclusive",
		PR_VM_InBounds(2, 2, 6), 1);
	failures += Test_QCVM_Expect("OP_BOUNDCHECK upper exclusive",
		PR_VM_InBounds(6, 2, 6), 0);
	failures += Test_QCVM_Expect("OP_BOUNDCHECK rejects negative",
		PR_VM_InBounds(-1, 0, 6), 0);
	failures += Test_QCVM_Expect("OP_BOUNDCHECK 16-bit upper",
		PR_VM_InBounds(40000, 0, 50000), 1);
	array_pointer = PR_VM_GlobalPointer(2 * sizeof(int));
	failures += Test_QCVM_Expect("OP_GLOBALADDRESS tagged",
		PR_VM_IsGlobalPointer(array_pointer), 1);
	failures += Test_QCVM_Expect("OP_GLOBALADDRESS offset",
		PR_VM_GlobalPointerOffset(array_pointer), 2 * (int)sizeof(int));
	array_pointer = PR_VM_AddPointerWords(array_pointer, 3);
	failures += Test_QCVM_Expect("OP_ADD_PIW offset",
		PR_VM_GlobalPointerOffset(array_pointer), 5 * (int)sizeof(int));
	a._float = 42.5f;
	PR_VM_StoreScalar(&a, PR_VM_PointerAddress(array_pointer, array_words, NULL));
	failures += Test_QCVM_ExpectFloat("OP_LOADP_F tagged global",
		((float *)array_words)[5], 42.5f);
	array_pointer = PR_VM_AddPointerWords(0, 3);
	failures += Test_QCVM_Expect("OP_ADD_PIW entity pointer untagged",
		PR_VM_IsGlobalPointer(array_pointer), 0);
	a._int = 77;
	PR_VM_StoreScalar(&a, PR_VM_PointerAddress(array_pointer, array_words,
		entity_words));
	failures += Test_QCVM_Expect("OP_STOREP_I entity pointer",
		entity_words[3], 77);
	array_slots[4]._int = 0x10203040;
	PR_VM_StoreScalar(&array_slots[4], &result);
	failures += Test_QCVM_Expect("OP_LOADA scalar raw slot",
		result._int, 0x10203040);
	VectorCopy(vector_b, array_slots[3].vector);
	PR_VM_StoreVector(&array_slots[3], &result);
	failures += Test_QCVM_Expect("OP_LOADA_V",
		VectorCompare(result.vector, vector_b), 1);
	array_pointer = PR_VM_GlobalPointer(6 * sizeof(int));
	VectorCopy(vector_a, a.vector);
	PR_VM_StoreVector(&a, PR_VM_PointerAddress(array_pointer, array_words, NULL));
	PR_VM_StoreVector(PR_VM_PointerAddress(array_pointer, array_words, NULL),
		&result);
	failures += Test_QCVM_Expect("OP_LOADP_V tagged global",
		VectorCompare(result.vector, vector_a), 1);
	a._int = 31415;
	PR_VM_StoreScalar(&a, &array_slots[2]);
	PR_VM_StoreScalar(&array_slots[2], &result);
	failures += Test_QCVM_Expect("OP_GSTOREP scalar -> OP_GLOAD scalar",
		result._int, 31415);
	VectorCopy(vector_a, a.vector);
	PR_VM_StoreVector(&a, &array_slots[1]);
	PR_VM_StoreVector(&array_slots[1], &result);
	failures += Test_QCVM_Expect("OP_GSTOREP_V -> OP_GLOAD_V",
		VectorCompare(result.vector, vector_a), 1);

	failures += Test_QCVM_VectorOpcode("OP_ADD_V", OP_ADD_V, vector_a, vector_b, vector_add);
	failures += Test_QCVM_VectorOpcode("OP_SUB_V", OP_SUB_V, vector_a, vector_b, vector_sub);
	VectorCopy(vector_a, a.vector);
	VectorCopy(vector_b, b.vector);
	PR_VM_ExecuteCoreOpcode(OP_MUL_V, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_MUL_V dot product", c._float, -12.25f);
	a._float = 2.0f;
	VectorCopy(vector_a, b.vector);
	PR_VM_ExecuteCoreOpcode(OP_MUL_FV, &a, &b, &c);
	failures += Test_QCVM_Expect("OP_MUL_FV", VectorCompare(c.vector, vector_scaled), 1);
	VectorCopy(vector_a, a.vector);
	b._float = 2.0f;
	PR_VM_ExecuteCoreOpcode(OP_MUL_VF, &a, &b, &c);
	failures += Test_QCVM_Expect("OP_MUL_VF", VectorCompare(c.vector, vector_scaled), 1);
	VectorCopy(vector_a, a.vector);
	VectorCopy(vector_a, b.vector);
	PR_VM_ExecuteCoreOpcode(OP_EQ_V, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_EQ_V equal", c._float, 1.0f);
	b.vector[2] += 0.25f;
	PR_VM_ExecuteCoreOpcode(OP_NE_V, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NE_V one component", c._float, 1.0f);
	memset(&a, 0, sizeof(a));
	PR_VM_ExecuteCoreOpcode(OP_NOT_V, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NOT_V zero vector", c._float, 1.0f);
	a._int = 1;
	PR_VM_ExecuteCoreOpcode(OP_NOT_V, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NOT_V subnormal component", c._float, 0.0f);
	a.vector[1] = -0.5f;
	PR_VM_ExecuteCoreOpcode(OP_NOT_V, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NOT_V nonzero vector", c._float, 0.0f);

	a._int = 0x420;
	b._int = 0x420;
	PR_VM_ExecuteCoreOpcode(OP_EQ_E, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_EQ_E equal", c._float, 1.0f);
	b._int = 0x840;
	PR_VM_ExecuteCoreOpcode(OP_NE_E, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NE_E distinct", c._float, 1.0f);
	a.function = 7;
	b.function = 7;
	PR_VM_ExecuteCoreOpcode(OP_EQ_FNC, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_EQ_FNC equal", c._float, 1.0f);
	b.function = 8;
	PR_VM_ExecuteCoreOpcode(OP_NE_FNC, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NE_FNC distinct", c._float, 1.0f);
	a.function = 0;
	PR_VM_ExecuteCoreOpcode(OP_NOT_FNC, &a, &b, &c);
	failures += Test_QCVM_ExpectFloat("OP_NOT_FNC null", c._float, 1.0f);

	value.f = PR_VM_LogicalOr(entity, zero);
	failures += Test_QCVM_Expect("entity -> OP_OR -> OP_IF", value.bits != 0, 1);
	value.f = PR_VM_LogicalAnd(negative_three, entity);
	value.f = PR_VM_LogicalOr(value.bits, zero);
	failures += Test_QCVM_Expect("OP_AND -> OP_OR -> OP_IF", value.bits != 0, 1);
	a._float = 5.0f;
	b._float = 3.0f;
	PR_VM_ExecuteCoreOpcode(OP_SUB_F, &a, &b, &c);
	a = c;
	b._float = 2.0f;
	PR_VM_ExecuteCoreOpcode(OP_EQ_F, &a, &b, &c);
	value.f = PR_VM_LogicalAnd((uint32_t)c._int, entity);
	failures += Test_QCVM_Expect("OP_SUB_F -> OP_EQ_F -> OP_AND -> OP_IF", value.bits != 0, 1);

	if (failures)
	{
		Con_Printf("QCVM opcode tests failed: %i failure(s).\n", failures);
		return TEST_STATUS_FAILED;
	}
	Con_Printf("QCVM opcode tests passed.\n");
	return TEST_STATUS_PASSED;
}
