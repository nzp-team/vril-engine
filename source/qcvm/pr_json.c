/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2023-2026 NZ:P Team

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
#include "cJSON.h"

#define JSON_MAX_DOCUMENTS 16
#define JSON_HANDLE_STRIDE 1048576
#define JSON_MAX_FILE_SIZE 262144
#define JSON_MAX_DEPTH 32
#define JSON_ERROR_NONE 0
#define JSON_ERROR_NOT_FOUND 1
#define JSON_ERROR_TOO_LARGE 2
#define JSON_ERROR_READ 3
#define JSON_ERROR_PARSE 4
#define JSON_ERROR_DUPLICATE_KEY 5
#define JSON_ERROR_NO_HANDLES 6

typedef struct
{
	cJSON *root;
	cJSON **nodes;
	int count;
	int capacity;
} pr_json_document_t;

static pr_json_document_t pr_json_documents[JSON_MAX_DOCUMENTS];
static int pr_json_error_code;
static int pr_json_error_position;
static char pr_json_error[256];
static char pr_json_string[2048];

static void PR_JSONError(int code, int position, const char *message)
{
	pr_json_error_code = code;
	pr_json_error_position = position;
	strlcpy(pr_json_error, message, sizeof(pr_json_error));
}

static void PR_JSONClearError(void)
{
	pr_json_error_code = JSON_ERROR_NONE;
	pr_json_error_position = 0;
	pr_json_error[0] = 0;
}

static int PR_JSONStripComments(char *text)
{
	int block = 0;
	int depth = 0;
	int escaped = 0;
	int quoted = 0;
	int i;

	for (i = 0; text[i]; i++)
	{
		if (block)
		{
			if (text[i] == '*' && text[i + 1] == '/')
			{
				text[i++] = ' ';
				text[i] = ' ';
				block = 0;
			}
			else if (text[i] != '\n' && text[i] != '\r') text[i] = ' ';
			continue;
		}
		if (quoted)
		{
			if (escaped) escaped = 0;
			else if (text[i] == '\\') escaped = 1;
			else if (text[i] == '"') quoted = 0;
			continue;
		}
		if (text[i] == '"')
		{
			quoted = 1;
			continue;
		}
		if (text[i] == '/' && text[i + 1] == '/')
		{
			text[i++] = ' ';
			text[i] = ' ';
			while (text[i + 1] && text[i + 1] != '\n' && text[i + 1] != '\r') text[++i] = ' ';
			continue;
		}
		if (text[i] == '/' && text[i + 1] == '*')
		{
			text[i++] = ' ';
			text[i] = ' ';
			block = 1;
			continue;
		}
		if (text[i] == '{' || text[i] == '[')
		{
			if (++depth > JSON_MAX_DEPTH)
			{
				PR_JSONError(JSON_ERROR_PARSE, i, "JSON nesting exceeds 32 levels");
				return 0;
			}
		}
		else if (text[i] == '}' || text[i] == ']') depth--;
	}
	if (block)
	{
		PR_JSONError(JSON_ERROR_PARSE, i, "Unterminated JSON block comment");
		return 0;
	}
	return 1;
}

static int PR_JSONCheckDuplicateKeys(cJSON *node)
{
	cJSON *child;
	cJSON *other;

	if ((node->string && strlen(node->string) > 1023) || (cJSON_IsString(node) && strlen(node->valuestring) > 1023))
	{
		PR_JSONError(JSON_ERROR_PARSE, 0, "JSON string exceeds 1023 bytes");
		return 0;
	}
	if (cJSON_IsObject(node))
	{
		for (child = node->child; child; child = child->next)
		{
			for (other = child->next; other; other = other->next)
			{
				if (!strcmp(child->string, other->string))
				{
					snprintf(pr_json_error, sizeof(pr_json_error), "Duplicate JSON key: %s", child->string);
					pr_json_error_code = JSON_ERROR_DUPLICATE_KEY;
					return 0;
				}
			}
		}
	}
	for (child = node->child; child; child = child->next)
		if (!PR_JSONCheckDuplicateKeys(child)) return 0;
	return 1;
}

static int PR_JSONAddNode(int document, cJSON *node)
{
	pr_json_document_t *doc = &pr_json_documents[document];
	cJSON **nodes;
	int i;

	for (i = 0; i < doc->count; i++)
		if (doc->nodes[i] == node) return document * JSON_HANDLE_STRIDE + i + 1;
	if (doc->count == doc->capacity)
	{
		doc->capacity += 64;
		nodes = realloc(doc->nodes, doc->capacity * sizeof(*nodes));
		if (!nodes)
		{
			PR_JSONError(JSON_ERROR_NO_HANDLES, 0, "Could not allocate JSON handles");
			return 0;
		}
		doc->nodes = nodes;
	}
	doc->nodes[doc->count++] = node;
	return document * JSON_HANDLE_STRIDE + doc->count;
}

static cJSON *PR_JSONNode(int handle, int *document)
{
	int slot;
	int index;

	if (handle <= 0) return NULL;
	slot = (handle - 1) / JSON_HANDLE_STRIDE;
	index = (handle - 1) % JSON_HANDLE_STRIDE;
	if (slot < 0 || slot >= JSON_MAX_DOCUMENTS) return NULL;
	if (!pr_json_documents[slot].root || index >= pr_json_documents[slot].count) return NULL;
	if (document) *document = slot;
	return pr_json_documents[slot].nodes[index];
}

static int PR_JSONParseBuffer(char *text)
{
	const char *error;
	cJSON *root;
	int document;

	if (!PR_JSONStripComments(text)) return 0;
	root = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, NULL, 1);
	if (!root)
	{
		error = cJSON_GetErrorPtr();
		PR_JSONError(JSON_ERROR_PARSE, error ? (int)(error - text) : 0, "Malformed JSON");
		return 0;
	}
	if (!PR_JSONCheckDuplicateKeys(root))
	{
		cJSON_Delete(root);
		return 0;
	}
	for (document = 0; document < JSON_MAX_DOCUMENTS; document++)
		if (!pr_json_documents[document].root) break;
	if (document == JSON_MAX_DOCUMENTS)
	{
		cJSON_Delete(root);
		PR_JSONError(JSON_ERROR_NO_HANDLES, 0, "Too many open JSON documents");
		return 0;
	}
	pr_json_documents[document].root = root;
	return PR_JSONAddNode(document, root);
}

void PR_JSONClear(void)
{
	int i;

	for (i = 0; i < JSON_MAX_DOCUMENTS; i++)
	{
		if (pr_json_documents[i].root) cJSON_Delete(pr_json_documents[i].root);
		free(pr_json_documents[i].nodes);
		memset(&pr_json_documents[i], 0, sizeof(pr_json_documents[i]));
	}
	PR_JSONClearError();
}

void PF_json_parse(void)
{
	char *text;
	int length;

	PR_JSONClearError();
	length = strlen(G_STRING(OFS_PARM0));
	if (length > JSON_MAX_FILE_SIZE)
	{
		PR_JSONError(JSON_ERROR_TOO_LARGE, 0, "JSON input exceeds 256 KiB");
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	text = malloc(length + 1);
	if (!text)
	{
		PR_JSONError(JSON_ERROR_READ, 0, "Could not allocate JSON input");
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	memcpy(text, G_STRING(OFS_PARM0), length + 1);
	G_FLOAT(OFS_RETURN) = PR_JSONParseBuffer(text);
	free(text);
}

void PF_json_parse_file(void)
{
	FILE *file;
	char *text;
	int length;

	PR_JSONClearError();
	length = COM_FOpenFile(G_STRING(OFS_PARM0), &file);
	if (length < 0)
	{
		PR_JSONError(JSON_ERROR_NOT_FOUND, 0, "JSON file not found");
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	if (length > JSON_MAX_FILE_SIZE)
	{
		fclose(file);
		PR_JSONError(JSON_ERROR_TOO_LARGE, 0, "JSON file exceeds 256 KiB");
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	text = malloc(length + 1);
	if (!text)
	{
		fclose(file);
		PR_JSONError(JSON_ERROR_READ, 0, "Could not allocate JSON input");
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	if ((int)fread(text, 1, length, file) != length)
	{
		fclose(file);
		free(text);
		PR_JSONError(JSON_ERROR_READ, 0, "Could not read JSON file");
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	fclose(file);
	text[length] = 0;
	G_FLOAT(OFS_RETURN) = PR_JSONParseBuffer(text);
	free(text);
}

void PF_json_free(void)
{
	cJSON *node;
	int document;

	node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), &document);
	if (!node || node != pr_json_documents[document].root) return;
	cJSON_Delete(pr_json_documents[document].root);
	free(pr_json_documents[document].nodes);
	memset(&pr_json_documents[document], 0, sizeof(pr_json_documents[document]));
}

void PF_json_get_value_type(void)
{
	cJSON *node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), NULL);

	if (cJSON_IsString(node)) G_FLOAT(OFS_RETURN) = 0;
	else if (cJSON_IsNumber(node)) G_FLOAT(OFS_RETURN) = 1;
	else if (cJSON_IsObject(node)) G_FLOAT(OFS_RETURN) = 2;
	else if (cJSON_IsArray(node)) G_FLOAT(OFS_RETURN) = 3;
	else if (cJSON_IsTrue(node)) G_FLOAT(OFS_RETURN) = 4;
	else if (cJSON_IsFalse(node)) G_FLOAT(OFS_RETURN) = 5;
	else G_FLOAT(OFS_RETURN) = 6;
}

void PF_json_get_integer(void)
{
	cJSON *node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), NULL);
	G_FLOAT(OFS_RETURN) = cJSON_IsNumber(node) ? node->valueint : 0;
}

void PF_json_get_float(void)
{
	cJSON *node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), NULL);
	G_FLOAT(OFS_RETURN) = cJSON_IsNumber(node) ? node->valuedouble : 0;
}

static void PR_JSONReturnString(const char *value)
{
	if (!value) value = "";
	strlcpy(pr_json_string, value, sizeof(pr_json_string));
	G_INT(OFS_RETURN) = PR_SetString(pr_json_string);
}

void PF_json_get_string(void)
{
	cJSON *node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), NULL);
	PR_JSONReturnString(cJSON_IsString(node) ? node->valuestring : "");
}

void PF_json_find_object_child(void)
{
	cJSON *node;
	cJSON *child;
	int document;

	node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), &document);
	child = cJSON_IsObject(node) ? cJSON_GetObjectItemCaseSensitive(node, G_STRING(OFS_PARM1)) : NULL;
	G_FLOAT(OFS_RETURN) = child ? PR_JSONAddNode(document, child) : 0;
}

void PF_json_get_length(void)
{
	cJSON *node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), NULL);
	G_FLOAT(OFS_RETURN) = cJSON_IsArray(node) || cJSON_IsObject(node) ? cJSON_GetArraySize(node) : 0;
}

void PF_json_get_child_at_index(void)
{
	cJSON *node;
	cJSON *child;
	int document;

	node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), &document);
	child = cJSON_IsArray(node) || cJSON_IsObject(node) ? cJSON_GetArrayItem(node, (int)G_FLOAT(OFS_PARM1)) : NULL;
	G_FLOAT(OFS_RETURN) = child ? PR_JSONAddNode(document, child) : 0;
}

void PF_json_get_name(void)
{
	cJSON *node = PR_JSONNode((int)G_FLOAT(OFS_PARM0), NULL);
	PR_JSONReturnString(node ? node->string : "");
}

void PF_json_get_error_code(void)
{
	G_FLOAT(OFS_RETURN) = pr_json_error_code;
}

void PF_json_get_error(void)
{
	PR_JSONReturnString(pr_json_error);
}

void PF_json_get_error_position(void)
{
	G_FLOAT(OFS_RETURN) = pr_json_error_position;
}