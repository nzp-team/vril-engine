/*
matrixlib.c - internal matrixlib
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "quakedef.h"


const matrix3x4 matrix3x4_identity =
{
{ 1, 0, 0, 0 },	// PITCH	[forward], org[0]
{ 0, 1, 0, 0 },	// YAW	[right]  , org[1]
{ 0, 0, 1, 0 },	// ROLL	[up]     , org[2]
};

/*
========================================================================

		Matrix3x4 operations

========================================================================
*/
void Matrix3x4_VectorTransform( const matrix3x4 in, const float v[3], float out[3] )
{
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2] + in[0][3];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2] + in[1][3];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2] + in[2][3];
}

void Matrix3x4_VectorITransform( const matrix3x4 in, const float v[3], float out[3] )
{
	vec3_t	dir;

	dir[0] = v[0] - in[0][3];
	dir[1] = v[1] - in[1][3];
	dir[2] = v[2] - in[2][3];

	out[0] = dir[0] * in[0][0] + dir[1] * in[1][0] + dir[2] * in[2][0];
	out[1] = dir[0] * in[0][1] + dir[1] * in[1][1] + dir[2] * in[2][1];
	out[2] = dir[0] * in[0][2] + dir[1] * in[1][2] + dir[2] * in[2][2];
}

void Matrix3x4_VectorRotate( const matrix3x4 in, const float v[3], float out[3] )
{
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2];
}

void Matrix3x4_VectorIRotate( const matrix3x4 in, const float v[3], float out[3] )
{
	out[0] = v[0] * in[0][0] + v[1] * in[1][0] + v[2] * in[2][0];
	out[1] = v[0] * in[0][1] + v[1] * in[1][1] + v[2] * in[2][1];
	out[2] = v[0] * in[0][2] + v[1] * in[1][2] + v[2] * in[2][2];
}

void Matrix3x4_ConcatTransforms( matrix3x4 out, const matrix3x4 in1, const matrix3x4 in2 )
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

void Matrix3x4_SetOrigin( matrix3x4 out, float x, float y, float z )
{
	out[0][3] = x;
	out[1][3] = y;
	out[2][3] = z;
}

void Matrix3x4_OriginFromMatrix( const matrix3x4 in, float *out )
{
	out[0] = in[0][3];
	out[1] = in[1][3];
	out[2] = in[2][3];
}

void Matrix3x4_FromOriginQuat( matrix3x4 out, const vec4_t quaternion, const vec3_t origin )
{
	out[0][0] = 1.0f - 2.0f * quaternion[1] * quaternion[1] - 2.0f * quaternion[2] * quaternion[2];
	out[1][0] = 2.0f * quaternion[0] * quaternion[1] + 2.0f * quaternion[3] * quaternion[2];
	out[2][0] = 2.0f * quaternion[0] * quaternion[2] - 2.0f * quaternion[3] * quaternion[1];

	out[0][1] = 2.0f * quaternion[0] * quaternion[1] - 2.0f * quaternion[3] * quaternion[2];
	out[1][1] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[2] * quaternion[2];
	out[2][1] = 2.0f * quaternion[1] * quaternion[2] + 2.0f * quaternion[3] * quaternion[0];

	out[0][2] = 2.0f * quaternion[0] * quaternion[2] + 2.0f * quaternion[3] * quaternion[1];
	out[1][2] = 2.0f * quaternion[1] * quaternion[2] - 2.0f * quaternion[3] * quaternion[0];
	out[2][2] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[1] * quaternion[1];

	out[0][3] = origin[0];
	out[1][3] = origin[1];
	out[2][3] = origin[2];
}

void Matrix3x4_CreateFromEntity( matrix3x4 out, const vec3_t angles, const vec3_t origin, float scale )
{
	float	angle, sr, sp, sy, cr, cp, cy;

	if( angles[ROLL] )
	{
		angle = angles[YAW] * (M_PI*2 / 360);
		SinCos( angle, &sy, &cy );
		angle = angles[PITCH] * (M_PI*2 / 360);
		SinCos( angle, &sp, &cp );
		angle = angles[ROLL] * (M_PI*2 / 360);
		SinCos( angle, &sr, &cr );

		out[0][0] = (cp*cy) * scale;
		out[0][1] = (sr*sp*cy+cr*-sy) * scale;
		out[0][2] = (cr*sp*cy+-sr*-sy) * scale;
		out[0][3] = origin[0];
		out[1][0] = (cp*sy) * scale;
		out[1][1] = (sr*sp*sy+cr*cy) * scale;
		out[1][2] = (cr*sp*sy+-sr*cy) * scale;
		out[1][3] = origin[1];
		out[2][0] = (-sp) * scale;
		out[2][1] = (sr*cp) * scale;
		out[2][2] = (cr*cp) * scale;
		out[2][3] = origin[2];
	}
	else if( angles[PITCH] )
	{
		angle = angles[YAW] * (M_PI*2 / 360);
		SinCos( angle, &sy, &cy );
		angle = angles[PITCH] * (M_PI*2 / 360);
		SinCos( angle, &sp, &cp );

		out[0][0] = (cp*cy) * scale;
		out[0][1] = (-sy) * scale;
		out[0][2] = (sp*cy) * scale;
		out[0][3] = origin[0];
		out[1][0] = (cp*sy) * scale;
		out[1][1] = (cy) * scale;
		out[1][2] = (sp*sy) * scale;
		out[1][3] = origin[1];
		out[2][0] = (-sp) * scale;
		out[2][1] = 0;
		out[2][2] = (cp) * scale;
		out[2][3] = origin[2];
	}
	else if( angles[YAW] )
	{
		angle = angles[YAW] * (M_PI*2 / 360);
		SinCos( angle, &sy, &cy );

		out[0][0] = (cy) * scale;
		out[0][1] = (-sy) * scale;
		out[0][2] = 0;
		out[0][3] = origin[0];
		out[1][0] = (sy) * scale;
		out[1][1] = (cy) * scale;
		out[1][2] = 0;
		out[1][3] = origin[1];
		out[2][0] = 0;
		out[2][1] = 0;
		out[2][2] = scale;
		out[2][3] = origin[2];
	}
	else
	{
		out[0][0] = scale;
		out[0][1] = 0;
		out[0][2] = 0;
		out[0][3] = origin[0];
		out[1][0] = 0;
		out[1][1] = scale;
		out[1][2] = 0;
		out[1][3] = origin[1];
		out[2][0] = 0;
		out[2][1] = 0;
		out[2][2] = scale;
		out[2][3] = origin[2];
	}
}

void Matrix3x4_TransformPositivePlane( const matrix3x4 in, const vec3_t normal, float d, vec3_t out, float *dist )
{
	float	scale = sqrt( in[0][0] * in[0][0] + in[0][1] * in[0][1] + in[0][2] * in[0][2] );
	float	iscale = 1.0f / scale;

	out[0] = (normal[0] * in[0][0] + normal[1] * in[0][1] + normal[2] * in[0][2]) * iscale;
	out[1] = (normal[0] * in[1][0] + normal[1] * in[1][1] + normal[2] * in[1][2]) * iscale;
	out[2] = (normal[0] * in[2][0] + normal[1] * in[2][1] + normal[2] * in[2][2]) * iscale;
	*dist = d * scale + ( out[0] * in[0][3] + out[1] * in[1][3] + out[2] * in[2][3] );
}

void Matrix3x4_Invert_Simple( matrix3x4 out, const matrix3x4 in1 )
{
	// we only support uniform scaling, so assume the first row is enough
	// (note the lack of sqrt here, because we're trying to undo the scaling,
	// this means multiplying by the inverse scale twice - squaring it, which
	// makes the sqrt a waste of time)
	float	scale = 1.0 / (in1[0][0] * in1[0][0] + in1[0][1] * in1[0][1] + in1[0][2] * in1[0][2]);

	// invert the rotation by transposing and multiplying by the squared
	// recipricol of the input matrix scale as described above
	out[0][0] = in1[0][0] * scale;
	out[0][1] = in1[1][0] * scale;
	out[0][2] = in1[2][0] * scale;
	out[1][0] = in1[0][1] * scale;
	out[1][1] = in1[1][1] * scale;
	out[1][2] = in1[2][1] * scale;
	out[2][0] = in1[0][2] * scale;
	out[2][1] = in1[1][2] * scale;
	out[2][2] = in1[2][2] * scale;

	// invert the translate
	out[0][3] = -(in1[0][3] * out[0][0] + in1[1][3] * out[0][1] + in1[2][3] * out[0][2]);
	out[1][3] = -(in1[0][3] * out[1][0] + in1[1][3] * out[1][1] + in1[2][3] * out[1][2]);
	out[2][3] = -(in1[0][3] * out[2][0] + in1[1][3] * out[2][1] + in1[2][3] * out[2][2]);
}

const matrix4x4 matrix4x4_identity =
{
{ 1, 0, 0, 0 },	// PITCH
{ 0, 1, 0, 0 },	// YAW
{ 0, 0, 1, 0 },	// ROLL
{ 0, 0, 0, 1 },	// ORIGIN
};

/*
========================================================================

		Matrix4x4 operations

========================================================================
*/
void Matrix4x4_VectorTransform( const matrix4x4 in, const float v[3], float out[3] )
{
#ifdef __PSP__
	__asm__ (
		".set			push\n"					// save assembler option
		".set			noreorder\n"			// suppress reordering
		"lv.q			C100,  0 + %1\n"		// C100 = in[0]
		"lv.q			C110, 16 + %1\n"		// C110 = in[1]
		"lv.q			C120, 32 + %1\n"		// C120 = in[2]
		"lv.s			S130,  0 + %2\n"		// S130 = v[0]
		"lv.s			S131,  4 + %2\n"		// S131 = v[1]
		"lv.s			S132,  8 + %2\n"		// S132 = v[2]
		"vhdp.q			S000, C130, C100\n"		// S000 = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2] + in[0][3]
		"vhdp.q			S001, C130, C110\n"		// S001 = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2] + in[1][3]
		"vhdp.q			S002, C130, C120\n"		// S002 = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2] + in[2][3]
		"sv.s			S000,  0 + %0\n"		// out[0] = S000
		"sv.s			S001,  4 + %0\n"		// out[1] = S001
		"sv.s			S002,  8 + %0\n"		// out[2] = S002
		".set			pop\n"					// restore assembler option
		: "=m"( *out )
		: "m"( *in ), "m"( *v )
	);
#else
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2] + in[0][3];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2] + in[1][3];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2] + in[2][3];
#endif // __PSP__
}

void Matrix4x4_VectorITransform( const matrix4x4 in, const float v[3], float out[3] )
{
#ifdef __PSP__
	__asm__ (
		".set			push\n"					// save assembler option
		".set			noreorder\n"			// suppress reordering
		"lv.q			C100,  0 + %1\n"		// C100 = in[0]
		"lv.q			C110, 16 + %1\n"		// C110 = in[1]
		"lv.q			C120, 32 + %1\n"		// C120 = in[2]
		"lv.s			S130,  0 + %2\n"		// S130 = v[0]
		"lv.s			S131,  4 + %2\n"		// S131 = v[1]
		"lv.s			S132,  8 + %2\n"		// S132 = v[2]
		"vsub.t			C130, C130, R103\n"		// C130 = v - in[][3]
		#if 1
		"vtfm3.t		C000, E100, C130\n"		// C000 = E100 * C130
		#else
		"vdot.t			S000, C130, R100\n"		// S000 = dir[0] * in[0][0] + dir[1] * in[1][0] + dir[2] * in[2][0]
		"vdot.t			S001, C130, R101\n"		// S001 = dir[0] * in[0][1] + dir[1] * in[1][1] + dir[2] * in[2][1]
		"vdot.t			S002, C130, R102\n"		// S002 = dir[0] * in[0][2] + dir[1] * in[1][2] + dir[2] * in[2][2]
		#endif
		"sv.s			S000,  0 + %0\n"		// out[0] = S000
		"sv.s			S001,  4 + %0\n"		// out[1] = S001
		"sv.s			S002,  8 + %0\n"		// out[2] = S002
		".set			pop\n"					// restore assembler option
		: "=m"( *out )
		: "m"( *in ), "m"( *v )
	);
#else
	vec3_t	dir;

	dir[0] = v[0] - in[0][3];
	dir[1] = v[1] - in[1][3];
	dir[2] = v[2] - in[2][3];

	out[0] = dir[0] * in[0][0] + dir[1] * in[1][0] + dir[2] * in[2][0];
	out[1] = dir[0] * in[0][1] + dir[1] * in[1][1] + dir[2] * in[2][1];
	out[2] = dir[0] * in[0][2] + dir[1] * in[1][2] + dir[2] * in[2][2];
#endif // __PSP__
}

void Matrix4x4_VectorRotate( const matrix4x4 in, const float v[3], float out[3] )
{
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2];
}

void Matrix4x4_VectorIRotate( const matrix4x4 in, const float v[3], float out[3] )
{
	out[0] = v[0] * in[0][0] + v[1] * in[1][0] + v[2] * in[2][0];
	out[1] = v[0] * in[0][1] + v[1] * in[1][1] + v[2] * in[2][1];
	out[2] = v[0] * in[0][2] + v[1] * in[1][2] + v[2] * in[2][2];
}

void Matrix4x4_ConcatTransforms( matrix4x4 out, const matrix4x4 in1, const matrix4x4 in2 )
{
#ifdef __PSP__
	__asm__ (
		".set			push\n"					// save assembler option
		".set			noreorder\n"			// suppress reordering
		"lv.q			C100,  0 + %1\n"		// C100 = in1[0]
		"lv.q			C110, 16 + %1\n"		// C110 = in1[1]
		"lv.q			C120, 32 + %1\n"		// C120 = in1[2]
		"vzero.q		C130\n"					// C130 = [0, 0, 0, 0]
		"lv.q			C200,  0 + %2\n"		// C100 = in2[0]
		"lv.q			C210, 16 + %2\n"		// C110 = in2[1]
		"lv.q			C220, 32 + %2\n"		// C120 = in2[2]
		"vidt.q			C230\n"					// C230 = [0, 0, 0, 1]
		"vmmul.q		E000, E100, E200\n"		// E000 = E100 * E200
		"sv.q			C000,  0 + %0\n"		// out[0] = C000
		"sv.q			C010, 16 + %0\n"		// out[1] = C010
		"sv.q			C020, 32 + %0\n"		// out[2] = C020
		".set			pop\n"					// restore assembler option
		: "=m"( *out )
		: "m"( *in1 ), "m"( *in2 )
	);
#else
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
#endif // __PSP__
}

void Matrix4x4_SetOrigin( matrix4x4 out, float x, float y, float z )
{
	out[0][3] = x;
	out[1][3] = y;
	out[2][3] = z;
}

void Matrix4x4_OriginFromMatrix( const matrix4x4 in, float *out )
{
	out[0] = in[0][3];
	out[1] = in[1][3];
	out[2] = in[2][3];
}

void Matrix4x4_FromOriginQuat( matrix4x4 out, const vec4_t quaternion, const vec3_t origin )
{
	out[0][0] = 1.0f - 2.0f * quaternion[1] * quaternion[1] - 2.0f * quaternion[2] * quaternion[2];
	out[1][0] = 2.0f * quaternion[0] * quaternion[1] + 2.0f * quaternion[3] * quaternion[2];
	out[2][0] = 2.0f * quaternion[0] * quaternion[2] - 2.0f * quaternion[3] * quaternion[1];
	out[0][3] = origin[0];
	out[0][1] = 2.0f * quaternion[0] * quaternion[1] - 2.0f * quaternion[3] * quaternion[2];
	out[1][1] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[2] * quaternion[2];
	out[2][1] = 2.0f * quaternion[1] * quaternion[2] + 2.0f * quaternion[3] * quaternion[0];
	out[1][3] = origin[1];
	out[0][2] = 2.0f * quaternion[0] * quaternion[2] + 2.0f * quaternion[3] * quaternion[1];
	out[1][2] = 2.0f * quaternion[1] * quaternion[2] - 2.0f * quaternion[3] * quaternion[0];
	out[2][2] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[1] * quaternion[1];
	out[2][3] = origin[2];
	out[3][0] = 0;
	out[3][1] = 0;
	out[3][2] = 0;
	out[3][3] = 1;
}

void Matrix4x4_CreateFromEntity( matrix4x4 out, const vec3_t angles, const vec3_t origin, float scale )
{
	float	angle, sr, sp, sy, cr, cp, cy;

	if( angles[ROLL] )
	{
		angle = angles[YAW] * (M_PI*2 / 360);
		SinCos( angle, &sy, &cy );
		angle = angles[PITCH] * (M_PI*2 / 360);
		SinCos( angle, &sp, &cp );
		angle = angles[ROLL] * (M_PI*2 / 360);
		SinCos( angle, &sr, &cr );

		out[0][0] = (cp*cy) * scale;
		out[0][1] = (sr*sp*cy+cr*-sy) * scale;
		out[0][2] = (cr*sp*cy+-sr*-sy) * scale;
		out[0][3] = origin[0];
		out[1][0] = (cp*sy) * scale;
		out[1][1] = (sr*sp*sy+cr*cy) * scale;
		out[1][2] = (cr*sp*sy+-sr*cy) * scale;
		out[1][3] = origin[1];
		out[2][0] = (-sp) * scale;
		out[2][1] = (sr*cp) * scale;
		out[2][2] = (cr*cp) * scale;
		out[2][3] = origin[2];
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
	}
	else if( angles[PITCH] )
	{
		angle = angles[YAW] * (M_PI*2 / 360);
		SinCos( angle, &sy, &cy );
		angle = angles[PITCH] * (M_PI*2 / 360);
		SinCos( angle, &sp, &cp );

		out[0][0] = (cp*cy) * scale;
		out[0][1] = (-sy) * scale;
		out[0][2] = (sp*cy) * scale;
		out[0][3] = origin[0];
		out[1][0] = (cp*sy) * scale;
		out[1][1] = (cy) * scale;
		out[1][2] = (sp*sy) * scale;
		out[1][3] = origin[1];
		out[2][0] = (-sp) * scale;
		out[2][1] = 0;
		out[2][2] = (cp) * scale;
		out[2][3] = origin[2];
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
	}
	else if( angles[YAW] )
	{
		angle = angles[YAW] * (M_PI*2 / 360);
		SinCos( angle, &sy, &cy );

		out[0][0] = (cy) * scale;
		out[0][1] = (-sy) * scale;
		out[0][2] = 0;
		out[0][3] = origin[0];
		out[1][0] = (sy) * scale;
		out[1][1] = (cy) * scale;
		out[1][2] = 0;
		out[1][3] = origin[1];
		out[2][0] = 0;
		out[2][1] = 0;
		out[2][2] = scale;
		out[2][3] = origin[2];
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
	}
	else
	{
		out[0][0] = scale;
		out[0][1] = 0;
		out[0][2] = 0;
		out[0][3] = origin[0];
		out[1][0] = 0;
		out[1][1] = scale;
		out[1][2] = 0;
		out[1][3] = origin[1];
		out[2][0] = 0;
		out[2][1] = 0;
		out[2][2] = scale;
		out[2][3] = origin[2];
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
	}
}

void Matrix4x4_ConvertToEntity( const matrix4x4 in, vec3_t angles, vec3_t origin )
{
	float xyDist = sqrt( in[0][0] * in[0][0] + in[1][0] * in[1][0] );

	// enough here to get angles?
	if( xyDist > 0.001f )
	{
		angles[0] = RAD2DEG( atan2( -in[2][0], xyDist ) );
		angles[1] = RAD2DEG( atan2( in[1][0], in[0][0] ) );
		angles[2] = RAD2DEG( atan2( in[2][1], in[2][2] ) );
	}
	else	// forward is mostly Z, gimbal lock
	{
		angles[0] = RAD2DEG( atan2( -in[2][0], xyDist ) );
		angles[1] = RAD2DEG( atan2( -in[0][1], in[1][1] ) );
		angles[2] = 0;
	}

	origin[0] = in[0][3];
	origin[1] = in[1][3];
	origin[2] = in[2][3];
}

void Matrix4x4_TransformPositivePlane( const matrix4x4 in, const vec3_t normal, float d, vec3_t out, float *dist )
{
#ifdef __PSP__
	__asm__ (
		".set			push\n"					// save assembler option
		".set			noreorder\n"			// suppress reordering
		"lv.q			C100,  0 + %2\n"		// C100 = in[0]
		"lv.q			C110, 16 + %2\n"		// C110 = in[1]
		"lv.q			C120, 32 + %2\n"		// C120 = in[2]
		"lv.s			S200,  0 + %3\n"		// S200 = normal[0]
		"lv.s			S201,  4 + %3\n"		// S201 = normal[1]
		"lv.s			S202,  8 + %3\n"		// S202 = normal[2]
		"lv.s			S210, %4\n"				// S210 = d
		"vdot.t			S211, C100, C100\n"		// S211 = C100 * C100
		"vsqrt.s		S211, S211\n"			// S211 = sqrt( S211 )
		"vrcp.s			S212, S211\n"			// S212 = 1 / S211
		"vtfm3.t		C000, M100, C200\n"		// C000 = M100 * C200
		"vscl.t			C000, C000, S212\n"		// C000 = C000 * S211
		"vmul.s			S003, S210, S211\n"		// S003 = S210 * S211
		"vdot.t			S010, R103,	C000\n"		// S010 = R103 * C000
		"vadd.s			S003, S003, S010\n"		// S003 = S003 + S010
		"sv.s			S000,  0 + %0\n"		// out[0] = S000
		"sv.s			S001,  4 + %0\n"		// out[1] = S001
		"sv.s			S002,  8 + %0\n"		// out[2] = S002
		"sv.s			S003, %1\n"				// dist = S003
		".set			pop\n"					// restore assembler option
		: "=m"( *out ), "=m"( *dist )
		: "m"( *in ), "m"( *normal ), "m"( d )
	);
#else
	float	scale = sqrt( in[0][0] * in[0][0] + in[0][1] * in[0][1] + in[0][2] * in[0][2] );
	float	iscale = 1.0f / scale;

	out[0] = (normal[0] * in[0][0] + normal[1] * in[0][1] + normal[2] * in[0][2]) * iscale;
	out[1] = (normal[0] * in[1][0] + normal[1] * in[1][1] + normal[2] * in[1][2]) * iscale;
	out[2] = (normal[0] * in[2][0] + normal[1] * in[2][1] + normal[2] * in[2][2]) * iscale;
	*dist = d * scale + ( out[0] * in[0][3] + out[1] * in[1][3] + out[2] * in[2][3] );
#endif // __PSP__
}

void Matrix4x4_TransformStandardPlane( const matrix4x4 in, const vec3_t normal, float d, vec3_t out, float *dist )
{
#ifdef __PSP__
	__asm__ (
		".set			push\n"					// save assembler option
		".set			noreorder\n"			// suppress reordering
		"lv.q			C100,  0 + %2\n"		// C100 = in[0]
		"lv.q			C110, 16 + %2\n"		// C110 = in[1]
		"lv.q			C120, 32 + %2\n"		// C120 = in[2]
		"lv.s			S200,  0 + %3\n"		// S200 = normal[0]
		"lv.s			S201,  4 + %3\n"		// S201 = normal[1]
		"lv.s			S202,  8 + %3\n"		// S202 = normal[2]
		"lv.s			S210, %4\n"				// S210 = d
		"vdot.t			S211, C100, C100\n"		// S211 = C100 * C100
		"vsqrt.s		S211, S211\n"			// S211 = sqrt( S211 )
		"vrcp.s			S212, S211\n"			// S212 = 1 / S211
		"vtfm3.t		C000, M100, C200\n"		// C000 = M100 * C200
		"vscl.t			C000, C000, S212\n"		// C000 = C000 * S211
		"vmul.s			S003, S210, S211\n"		// S003 = S210 * S211
		"vdot.t			S010, R103,	C000\n"		// S010 = R103 * C000
		"vsub.s			S003, S003, S010\n"		// S003 = S003 - S010
		"sv.s			S000,  0 + %0\n"		// out[0] = S000
		"sv.s			S001,  4 + %0\n"		// out[1] = S001
		"sv.s			S002,  8 + %0\n"		// out[2] = S002
		"sv.s			S003, %1\n"				// dist = S003
		".set			pop\n"					// restore assembler option
		: "=m"( *out ), "=m"( *dist )
		: "m"( *in ), "m"( *normal ), "m"( d )
	);
#else
	float scale = sqrt( in[0][0] * in[0][0] + in[0][1] * in[0][1] + in[0][2] * in[0][2] );
	float iscale = 1.0f / scale;

	out[0] = (normal[0] * in[0][0] + normal[1] * in[0][1] + normal[2] * in[0][2]) * iscale;
	out[1] = (normal[0] * in[1][0] + normal[1] * in[1][1] + normal[2] * in[1][2]) * iscale;
	out[2] = (normal[0] * in[2][0] + normal[1] * in[2][1] + normal[2] * in[2][2]) * iscale;
	*dist = d * scale - ( out[0] * in[0][3] + out[1] * in[1][3] + out[2] * in[2][3] );
#endif // __PSP__
}

void Matrix4x4_Invert_Simple( matrix4x4 out, const matrix4x4 in1 )
{
	// we only support uniform scaling, so assume the first row is enough
	// (note the lack of sqrt here, because we're trying to undo the scaling,
	// this means multiplying by the inverse scale twice - squaring it, which
	// makes the sqrt a waste of time)
	float	scale = 1.0f / (in1[0][0] * in1[0][0] + in1[0][1] * in1[0][1] + in1[0][2] * in1[0][2]);

	// invert the rotation by transposing and multiplying by the squared
	// recipricol of the input matrix scale as described above
	out[0][0] = in1[0][0] * scale;
	out[0][1] = in1[1][0] * scale;
	out[0][2] = in1[2][0] * scale;
	out[1][0] = in1[0][1] * scale;
	out[1][1] = in1[1][1] * scale;
	out[1][2] = in1[2][1] * scale;
	out[2][0] = in1[0][2] * scale;
	out[2][1] = in1[1][2] * scale;
	out[2][2] = in1[2][2] * scale;

	// invert the translate
	out[0][3] = -(in1[0][3] * out[0][0] + in1[1][3] * out[0][1] + in1[2][3] * out[0][2]);
	out[1][3] = -(in1[0][3] * out[1][0] + in1[1][3] * out[1][1] + in1[2][3] * out[1][2]);
	out[2][3] = -(in1[0][3] * out[2][0] + in1[1][3] * out[2][1] + in1[2][3] * out[2][2]);

	// don't know if there's anything worth doing here
	out[3][0] = 0;
	out[3][1] = 0;
	out[3][2] = 0;
	out[3][3] = 1;
}
