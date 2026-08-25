/* QCVM bytecode ABI definitions.
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __PR_COMP_H
#define __PR_COMP_H

#include <stdint.h>

// this file is shared by quake and qcc

typedef int32_t	func_t;
typedef int32_t	string_t;

typedef enum {
    ev_void,
    ev_string,
    ev_float,
    ev_vector,
    ev_entity,
    ev_field,
    ev_function,
    ev_pointer
} etype_t;

#define	OFS_NULL		0
#define	OFS_RETURN		1
#define	OFS_PARM0		4	// leave 3 ofs for each parm to hold vectors
#define	OFS_PARM1		7
#define	OFS_PARM2		10
#define	OFS_PARM3		13
#define	OFS_PARM4		16
#define	OFS_PARM5		19
#define	OFS_PARM6		22
#define	OFS_PARM7		25
#define	RESERVED_OFS	28


#define PR_OPCODE_LIST(X) \
	X(OP_DONE, "DONE") \
	X(OP_MUL_F, "MUL_F") \
	X(OP_MUL_V, "MUL_V") \
	X(OP_MUL_FV, "MUL_FV") \
	X(OP_MUL_VF, "MUL_VF") \
	X(OP_DIV_F, "DIV") \
	X(OP_ADD_F, "ADD_F") \
	X(OP_ADD_V, "ADD_V") \
	X(OP_SUB_F, "SUB_F") \
	X(OP_SUB_V, "SUB_V") \
	X(OP_EQ_F, "EQ_F") \
	X(OP_EQ_V, "EQ_V") \
	X(OP_EQ_S, "EQ_S") \
	X(OP_EQ_E, "EQ_E") \
	X(OP_EQ_FNC, "EQ_FNC") \
	X(OP_NE_F, "NE_F") \
	X(OP_NE_V, "NE_V") \
	X(OP_NE_S, "NE_S") \
	X(OP_NE_E, "NE_E") \
	X(OP_NE_FNC, "NE_FNC") \
	X(OP_LE, "LE") \
	X(OP_GE, "GE") \
	X(OP_LT, "LT") \
	X(OP_GT, "GT") \
	X(OP_LOAD_F, "INDIRECT") \
	X(OP_LOAD_V, "INDIRECT") \
	X(OP_LOAD_S, "INDIRECT") \
	X(OP_LOAD_ENT, "INDIRECT") \
	X(OP_LOAD_FLD, "INDIRECT") \
	X(OP_LOAD_FNC, "INDIRECT") \
	X(OP_ADDRESS, "ADDRESS") \
	X(OP_STORE_F, "STORE_F") \
	X(OP_STORE_V, "STORE_V") \
	X(OP_STORE_S, "STORE_S") \
	X(OP_STORE_ENT, "STORE_ENT") \
	X(OP_STORE_FLD, "STORE_FLD") \
	X(OP_STORE_FNC, "STORE_FNC") \
	X(OP_STOREP_F, "STOREP_F") \
	X(OP_STOREP_V, "STOREP_V") \
	X(OP_STOREP_S, "STOREP_S") \
	X(OP_STOREP_ENT, "STOREP_ENT") \
	X(OP_STOREP_FLD, "STOREP_FLD") \
	X(OP_STOREP_FNC, "STOREP_FNC") \
	X(OP_RETURN, "RETURN") \
	X(OP_NOT_F, "NOT_F") \
	X(OP_NOT_V, "NOT_V") \
	X(OP_NOT_S, "NOT_S") \
	X(OP_NOT_ENT, "NOT_ENT") \
	X(OP_NOT_FNC, "NOT_FNC") \
	X(OP_IF, "IF") \
	X(OP_IFNOT, "IFNOT") \
	X(OP_CALL0, "CALL0") \
	X(OP_CALL1, "CALL1") \
	X(OP_CALL2, "CALL2") \
	X(OP_CALL3, "CALL3") \
	X(OP_CALL4, "CALL4") \
	X(OP_CALL5, "CALL5") \
	X(OP_CALL6, "CALL6") \
	X(OP_CALL7, "CALL7") \
	X(OP_CALL8, "CALL8") \
	X(OP_STATE, "STATE") \
	X(OP_GOTO, "GOTO") \
	X(OP_AND, "AND") \
	X(OP_OR, "OR") \
	X(OP_BITAND, "BITAND") \
	X(OP_BITOR, "BITOR")

#define PR_EXTENDED_OPCODE_LIST(X) \
	X(OP_MULSTOREP_F, 68, "MULSTOREP_F") \
	X(OP_MULSTOREP_VF, 69, "MULSTOREP_VF") \
	X(OP_DIVSTOREP_F, 71, "DIVSTOREP_F") \
	X(OP_ADDSTOREP_F, 74, "ADDSTOREP_F") \
	X(OP_ADDSTOREP_V, 75, "ADDSTOREP_V") \
	X(OP_SUBSTOREP_F, 78, "SUBSTOREP_F") \
	X(OP_SUBSTOREP_V, 79, "SUBSTOREP_V") \
	X(OP_BITSETSTOREP_F, 89, "BITSETSTOREP_F") \
	X(OP_BITCLRSTOREP_F, 91, "BITCLRSTOREP_F") \
	X(OP_RAND0, 92, "RAND0") \
	X(OP_RAND1, 93, "RAND1") \
	X(OP_RAND2, 94, "RAND2") \
	X(OP_ADD_I, 116, "ADD_I") \
	X(OP_CONV_ITOF, 122, "CONV_ITOF") \
	X(OP_CONV_FTOI, 123, "CONV_FTOI") \
	X(OP_MUL_I, 132, "MUL_I") \
	X(OP_GLOBALADDRESS, 143, "GLOBALADDRESS") \
	X(OP_ADD_PIW, 144, "ADD_PIW") \
	X(OP_LOADA_F, 145, "LOADA_F") \
	X(OP_LOADA_V, 146, "LOADA_V") \
	X(OP_LOADA_S, 147, "LOADA_S") \
	X(OP_LOADA_ENT, 148, "LOADA_ENT") \
	X(OP_LOADA_FLD, 149, "LOADA_FLD") \
	X(OP_LOADA_FNC, 150, "LOADA_FNC") \
	X(OP_LOADA_I, 151, "LOADA_I") \
	X(OP_LOADP_F, 154, "LOADP_F") \
	X(OP_LOADP_V, 155, "LOADP_V") \
	X(OP_LOADP_S, 156, "LOADP_S") \
	X(OP_LOADP_ENT, 157, "LOADP_ENT") \
	X(OP_LOADP_FLD, 158, "LOADP_FLD") \
	X(OP_LOADP_FNC, 159, "LOADP_FNC") \
	X(OP_LOADP_I, 160, "LOADP_I") \
	X(OP_GSTOREP_I, 197, "GSTOREP_I") \
	X(OP_GSTOREP_F, 198, "GSTOREP_F") \
	X(OP_GSTOREP_ENT, 199, "GSTOREP_ENT") \
	X(OP_GSTOREP_FLD, 200, "GSTOREP_FLD") \
	X(OP_GSTOREP_S, 201, "GSTOREP_S") \
	X(OP_GSTOREP_FNC, 202, "GSTOREP_FNC") \
	X(OP_GSTOREP_V, 203, "GSTOREP_V") \
	X(OP_GLOAD_I, 205, "GLOAD_I") \
	X(OP_GLOAD_F, 206, "GLOAD_F") \
	X(OP_GLOAD_FLD, 207, "GLOAD_FLD") \
	X(OP_GLOAD_ENT, 208, "GLOAD_ENT") \
	X(OP_GLOAD_S, 209, "GLOAD_S") \
	X(OP_GLOAD_FNC, 210, "GLOAD_FNC") \
	X(OP_BOUNDCHECK, 211, "BOUNDCHECK") \
	X(OP_GLOAD_V, 216, "GLOAD_V") \
	X(OP_STOREF_V, 219, "STOREF_V") \
	X(OP_STOREF_F, 220, "STOREF_F") \
	X(OP_STOREF_S, 221, "STOREF_S") \
	X(OP_STOREF_I, 222, "STOREF_I")

enum
{
#define PR_OPCODE_ENUM(op, name) op,
	PR_OPCODE_LIST(PR_OPCODE_ENUM)
#undef PR_OPCODE_ENUM
	OP_VANILLA_COUNT,

#define PR_EXTENDED_OPCODE_ENUM(op, value, name) op = value,
	PR_EXTENDED_OPCODE_LIST(PR_EXTENDED_OPCODE_ENUM)
#undef PR_EXTENDED_OPCODE_ENUM
	OP_COUNT = 223
};

typedef char pr_legacy_opcode_abi_must_not_change[(OP_BITOR == 65) ? 1 : -1];

typedef struct statement_s
{
	uint16_t	op;
	int16_t		a, b, c;
} dstatement_t;

typedef struct
{
	uint16_t	type;	// if DEF_SAVEGLOBAL bit is set
				// the variable needs to be saved in savegames
	uint16_t	ofs;
	int32_t		s_name;
} ddef_t;

#define	DEF_SAVEGLOBAL	(1<<15)

#define	MAX_PARMS	8

typedef struct
{
	int32_t		first_statement;	// negative numbers are builtins
	int32_t		parm_start;
	int32_t		locals;			// total ints of parms + locals

	int32_t		profile;		// runtime

	int32_t		s_name;
	int32_t		s_file;			// source file defined in

	int32_t		numparms;
	byte		parm_size[MAX_PARMS];
} dfunction_t;


#define	PROG_VERSION	6
typedef struct
{
	int32_t		version;
	int32_t		crc;		// check of header file

	int32_t		ofs_statements;
	int32_t		numstatements;	// statement 0 is an error

	int32_t		ofs_globaldefs;
	int32_t		numglobaldefs;

	int32_t		ofs_fielddefs;
	int32_t		numfielddefs;

	int32_t		ofs_functions;
	int32_t		numfunctions;	// function 0 is an empty

	int32_t		ofs_strings;
	int32_t		numstrings;	// first string is a null string

	int32_t		ofs_globals;
	int32_t		numglobals;

	int32_t		entityfields;
} dprograms_t;

#endif
