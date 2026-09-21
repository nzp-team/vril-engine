/*
Copyright (C) 1996-1997 Id Software, Inc.

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

#include "../nzportable_def.h"
#include "pr_vm.h"

/*

*/

typedef struct
{
	int				s;
	dfunction_t		*f;
} prstack_t;

#define	MAX_STACK_DEPTH		32
static prstack_t	pr_stack[MAX_STACK_DEPTH];
static int			pr_depth;

#define	LOCALSTACK_SIZE		2048
static int			localstack[LOCALSTACK_SIZE];
static int			localstack_used;


qboolean	pr_trace;
dfunction_t	*pr_xfunction;
int			pr_xstatement;


int		pr_argc;

static inline eval_t *PR_VM_ResolvePointer (int pointer)
{
	return PR_VM_PointerAddress(pointer, pr_globals, sv.edicts);
}

static const char *const pr_opnames[OP_COUNT] =
{
#define PR_OPCODE_NAME(op, name) [op] = name,
	PR_OPCODE_LIST(PR_OPCODE_NAME)
#undef PR_OPCODE_NAME
#define PR_EXTENDED_OPCODE_NAME(op, value, name) [op] = name,
	PR_EXTENDED_OPCODE_LIST(PR_EXTENDED_OPCODE_NAME)
#undef PR_EXTENDED_OPCODE_NAME
};

char *PR_GlobalString (int ofs);
char *PR_GlobalStringNoContents (int ofs);


//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
void PR_PrintStatement (dstatement_t *s)
{
	int		i;

	if ((unsigned)s->op < OP_COUNT && pr_opnames[s->op])
	{
		Con_Printf ("%s ",  pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i<10 ; i++)
			Con_Printf (" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf ("%sbranch %i",PR_GlobalString(s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		Con_Printf ("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Con_Printf ("%s",PR_GlobalString(s->a));
		Con_Printf ("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			Con_Printf ("%s",PR_GlobalString(s->a));
		if (s->b)
			Con_Printf ("%s",PR_GlobalString(s->b));
		if (s->c)
			Con_Printf ("%s", PR_GlobalStringNoContents(s->c));
	}
	Con_Printf ("\n");
}

/*
============
PR_StackTrace
============
*/
void PR_StackTrace (void)
{
	dfunction_t	*f;
	int			i;

	if (pr_depth == 0)
	{
		Con_Printf ("<NO STACK>\n");
		return;
	}

	pr_stack[pr_depth].f = pr_xfunction;
	for (i=pr_depth ; i>=0 ; i--)
	{
		f = pr_stack[i].f;

		if (!f)
		{
			Con_Printf ("<NO FUNCTION>\n");
		}
		else
			Con_Printf ("%12s : %s\n", PR_GetString(f->s_file), PR_GetString(f->s_name));
	}
}


/*
============
PR_Profile_f

============
*/
void PR_Profile_f (void)
{
	dfunction_t	*f, *best;
	int			max;
	int			num;
	int			i;

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<progs->numfunctions ; i++)
		{
			f = &pr_functions[i];
			if (f->profile > max)
			{
				max = f->profile;
				best = f;
			}
		}
		if (best)
		{
			if (num < 10)
				Con_Printf ("%7i %s\n", best->profile, PR_GetString(best->s_name));
			num++;
			best->profile = 0;
		}
	} while (best);
}


/*
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError (char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	vsprintf (string,error,argptr);
	va_end (argptr);

	PR_PrintStatement (pr_statements + pr_xstatement);
	PR_StackTrace ();
	Con_Printf ("%s\n", string);

	pr_depth = 0;		// dump the stack so host_error can shutdown functions

	Host_Error ("Program error");
}

static inline eval_t *PR_VM_GlobalAt (int index, int width)
{
	if (index < 0 || index > progs->numglobals - width)
		PR_RunError ("global array index %i out of range", index);
	return (eval_t *)&pr_globals[index];
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
static inline int PR_EnterFunction (dfunction_t *f)
{
	int		i, j, c, o;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
		PR_RunError ("stack overflow");

// save off any locals that the new function steps on
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError ("PR_ExecuteProgram: locals stack overflow\n");

	for (i=0 ; i < c ; i++)
		localstack[localstack_used+i] = ((int *)pr_globals)[f->parm_start + i];
	localstack_used += c;

// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)pr_globals)[o] = ((int *)pr_globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
static inline int PR_LeaveFunction (void)
{
	int		i, c;

	if (pr_depth <= 0)
		Sys_Error ("prog stack underflow");

// restore locals from the stack
	c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		PR_RunError ("PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used+i];

// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}


/*
====================
PR_ExecuteProgram
====================
*/
void PR_ExecuteProgram (func_t fnum)
{
	eval_t	*a, *b, *c;
	int			s;
	dstatement_t	*st;
	dfunction_t	*f, *newf;
	int		runaway;
	int		i;
	edict_t	*ed;
	int		exitdepth;
	eval_t	*ptr;
	// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start
	char	*funcname;
	char	*remaphint;
	// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end

	if (!fnum || fnum >= progs->numfunctions)
	{
		if (pr_global_struct->self)
			ED_Print (PROG_TO_EDICT(pr_global_struct->self));
		Host_Error ("PR_ExecuteProgram: NULL function");
	}

	f = &pr_functions[fnum];

	runaway = 400000;
	pr_trace = false;

// make a stack frame
	exitdepth = pr_depth;

	s = PR_EnterFunction (f);

while (1)
{
	s++;	// next statement

	st = &pr_statements[s];
	a = (eval_t *)&pr_globals[st->a];
	b = (eval_t *)&pr_globals[st->b];
	c = (eval_t *)&pr_globals[st->c];

	if (!--runaway)
		PR_RunError ("runaway loop error");

	pr_xfunction->profile++;
	pr_xstatement = s;

	if (pr_trace)
		PR_PrintStatement (st);

	switch (st->op)
	{
#define PR_VM_EXEC_CASE(op, handler) case op: handler(a, b, c); break;
	PR_VM_CORE_OPCODES(PR_VM_EXEC_CASE)
#undef PR_VM_EXEC_CASE

	case OP_NOT_S:
		c->_float = !a->string || !*PR_GetString(a->string);
		break;
	case OP_NOT_ENT:
		c->_float = (PROG_TO_EDICT(a->edict) == sv.edicts);
		break;
	case OP_EQ_S:
		c->_float = !strcmp(PR_GetString(a->string),PR_GetString(b->string));
		break;
	case OP_NE_S:
		c->_float = strcmp(PR_GetString(a->string),PR_GetString(b->string));
		break;

//==================
	case OP_STORE_F:
	case OP_STORE_ENT:
	case OP_STORE_FLD:		// integers
	case OP_STORE_S:
	case OP_STORE_FNC:		// pointers
		b->_int = a->_int;
		break;
	case OP_STORE_V:
		b->vector[0] = a->vector[0];
		b->vector[1] = a->vector[1];
		b->vector[2] = a->vector[2];
		break;

	case OP_STOREP_F:
	case OP_STOREP_ENT:
	case OP_STOREP_FLD:		// integers
	case OP_STOREP_S:
	case OP_STOREP_FNC:		// pointers
		ptr = PR_VM_ResolvePointer(b->_int);
		ptr->_int = a->_int;
		break;
	case OP_STOREP_V:
		ptr = PR_VM_ResolvePointer(b->_int);
		ptr->vector[0] = a->vector[0];
		ptr->vector[1] = a->vector[1];
		ptr->vector[2] = a->vector[2];
		break;

	case OP_MULSTOREP_F:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_MulStorePFloat(a, ptr, c);
		break;
	case OP_MULSTOREP_VF:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_MulStorePVectorFloat(a, ptr, c);
		break;
	case OP_DIVSTOREP_F:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_DivStorePFloat(a, ptr, c);
		break;
	case OP_ADDSTOREP_F:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_AddStorePFloat(a, ptr, c);
		break;
	case OP_ADDSTOREP_V:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_AddStorePVector(a, ptr, c);
		break;
	case OP_SUBSTOREP_F:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_SubStorePFloat(a, ptr, c);
		break;
	case OP_SUBSTOREP_V:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_SubStorePVector(a, ptr, c);
		break;
	case OP_BITSETSTOREP_F:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_BitSetStorePFloat(a, ptr);
		break;
	case OP_BITCLRSTOREP_F:
		ptr = PR_VM_ResolvePointer(b->_int);
		PR_VM_BitClearStorePFloat(a, ptr);
		break;

	case OP_STOREF_F:
	case OP_STOREF_S:
	case OP_STOREF_I:
		ed = PROG_TO_EDICT(a->edict);
		if (ed == (edict_t *)sv.edicts && sv.state == ss_active)
			PR_RunError ("assignment to world entity");
		ptr = (eval_t *)((int *)&ed->v + b->_int);
		PR_VM_StoreScalar(c, ptr);
		break;
	case OP_STOREF_V:
		ed = PROG_TO_EDICT(a->edict);
		if (ed == (edict_t *)sv.edicts && sv.state == ss_active)
			PR_RunError ("assignment to world entity");
		ptr = (eval_t *)((int *)&ed->v + b->_int);
		PR_VM_StoreVector(c, ptr);
		break;

	case OP_RAND0:
		c->_float = PR_VM_RandomUnit(rand());
		break;
	case OP_RAND1:
		c->_float = PR_VM_RandomScale(rand(), a->_float);
		break;
	case OP_RAND2:
		c->_float = PR_VM_RandomRange(rand(), a->_float, b->_float);
		break;

	case OP_GLOBALADDRESS:
		i = st->a + b->_int;
		PR_VM_GlobalAt(i, 1);
		c->_int = PR_VM_GlobalPointer((uint32_t)i * sizeof(int));
		break;
	case OP_ADD_PIW:
		c->_int = PR_VM_AddPointerWords(a->_int, b->_int);
		break;

	case OP_LOADA_F:
	case OP_LOADA_S:
	case OP_LOADA_ENT:
	case OP_LOADA_FLD:
	case OP_LOADA_FNC:
	case OP_LOADA_I:
		ptr = PR_VM_GlobalAt(st->a + b->_int, 1);
		PR_VM_StoreScalar(ptr, c);
		break;
	case OP_LOADA_V:
		ptr = PR_VM_GlobalAt(st->a + b->_int, 3);
		PR_VM_StoreVector(ptr, c);
		break;

	case OP_LOADP_F:
	case OP_LOADP_S:
	case OP_LOADP_ENT:
	case OP_LOADP_FLD:
	case OP_LOADP_FNC:
	case OP_LOADP_I:
		ptr = PR_VM_ResolvePointer(PR_VM_AddPointerWords(a->_int, b->_int));
		PR_VM_StoreScalar(ptr, c);
		break;
	case OP_LOADP_V:
		ptr = PR_VM_ResolvePointer(PR_VM_AddPointerWords(a->_int, b->_int));
		PR_VM_StoreVector(ptr, c);
		break;

	case OP_GLOAD_I:
	case OP_GLOAD_F:
	case OP_GLOAD_FLD:
	case OP_GLOAD_ENT:
	case OP_GLOAD_S:
	case OP_GLOAD_FNC:
		ptr = PR_VM_GlobalAt(a->_int, 1);
		PR_VM_StoreScalar(ptr, c);
		break;
	case OP_GLOAD_V:
		ptr = PR_VM_GlobalAt(a->_int, 3);
		PR_VM_StoreVector(ptr, c);
		break;
	case OP_GSTOREP_I:
	case OP_GSTOREP_F:
	case OP_GSTOREP_ENT:
	case OP_GSTOREP_FLD:
	case OP_GSTOREP_S:
	case OP_GSTOREP_FNC:
		ptr = PR_VM_GlobalAt(b->_int, 1);
		PR_VM_StoreScalar(a, ptr);
		break;
	case OP_GSTOREP_V:
		ptr = PR_VM_GlobalAt(b->_int, 3);
		PR_VM_StoreVector(a, ptr);
		break;

	case OP_BOUNDCHECK:
		if (!PR_VM_InBounds(a->_int, (uint16_t)st->c, (uint16_t)st->b))
			PR_RunError ("Progs boundcheck failed. Value is %i. Must be %u<=value<%u",
				a->_int, (uint16_t)st->c, (uint16_t)st->b);
		break;

	case OP_ADDRESS:
		ed = PROG_TO_EDICT(a->edict);

		if (ed == (edict_t *)sv.edicts && sv.state == ss_active)
			PR_RunError ("assignment to world entity");
		c->_int = (byte *)((int *)&ed->v + b->_int) - (byte *)sv.edicts;
		break;

	case OP_LOAD_F:
	case OP_LOAD_FLD:
	case OP_LOAD_ENT:
	case OP_LOAD_S:
	case OP_LOAD_FNC:
		ed = PROG_TO_EDICT(a->edict);
		a = (eval_t *)((int *)&ed->v + b->_int);
		c->_int = a->_int;
		break;

	case OP_LOAD_V:
		ed = PROG_TO_EDICT(a->edict);
		a = (eval_t *)((int *)&ed->v + b->_int);
		c->vector[0] = a->vector[0];
		c->vector[1] = a->vector[1];
		c->vector[2] = a->vector[2];
		break;

//==================

	case OP_IFNOT:
		if (!a->_int)
			s += st->b - 1;	// offset the s++
		break;

	case OP_IF:
		if (a->_int)
			s += st->b - 1;	// offset the s++
		break;

	case OP_GOTO:
		s += st->a - 1;	// offset the s++
		break;

	case OP_CALL0:
	case OP_CALL1:
	case OP_CALL2:
	case OP_CALL3:
	case OP_CALL4:
	case OP_CALL5:
	case OP_CALL6:
	case OP_CALL7:
	case OP_CALL8:
		pr_argc = st->op - OP_CALL0;
		if (!a->function)
			PR_RunError ("NULL function");
		newf = &pr_functions[a->function];
		if (newf->first_statement < 0)
		{	// negative statements are built in functions
			i = -newf->first_statement;
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start
			if ( (i >= pr_numbuiltins)
			||   (pr_builtins[i] == pr_ebfs_builtins[0].function) )
			{
				funcname = PR_GetString(newf->s_name);
				if (pr_builtin_remap.value)
				{
					remaphint = NULL;
				}
				else
				{
					remaphint = "Try \"builtin remapping\" by setting PR_BUILTIN_REMAP to 1\n";
				}
				PR_RunError ("Bad builtin call number %i for %s\nPlease contact the PROGS.DAT author\nUse BUILTINLIST to see all assigned builtin functions\n%s", i, funcname, remaphint);
			}
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end
			pr_builtins[i] ();
			break;
		}

		s = PR_EnterFunction (newf);
		break;

	case OP_DONE:
	case OP_RETURN:
		pr_globals[OFS_RETURN] = pr_globals[st->a];
		pr_globals[OFS_RETURN+1] = pr_globals[st->a+1];
		pr_globals[OFS_RETURN+2] = pr_globals[st->a+2];

		s = PR_LeaveFunction ();
		if (pr_depth == exitdepth)
			return;		// all done
		break;

	case OP_STATE:
		ed = PROG_TO_EDICT(pr_global_struct->self);
		ed->v.nextthink = pr_global_struct->time + 0.1f;
		if (a->_float != ed->v.frame)
		{
			ed->v.frame = a->_float;
		}
		ed->v.think = b->function;
		break;

	default:
		PR_RunError ("Bad opcode %i", st->op);
	}
}

}
