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
// r_mesh.c: triangle model functions

#include "../nzportable_def.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

model_t		*aliasmodel;
aliashdr_t	*paliashdr;

qboolean	used[MAXALIASTRIS];

// the command list holds counts and s/t values that are valid for
// every frame
int		commands[MAXALIASTRIS * 7 + 1];
int		numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int		vertexorder[MAXALIASTRIS * 3];
int		numorder;

int		allverts, alltris;

int		stripverts[128];
int		striptris[128];
int		stripcount;

/*
================
StripLength
================
*/
int	StripLength (int starttri, int startv)
{
	int			m1, m2;
	int			j;
	mtriangle_t	*last, *check;
	int			k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+2)%3];
	m2 = last->vertindex[(startv+1)%3];

	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[ (k+2)%3 ];
			else
				m1 = check->vertindex[ (k+2)%3 ];

			if (stripcount >= (int)(sizeof(stripverts) / sizeof(*stripverts)) - 2)
				goto done;
			stripverts[stripcount+2] = check->vertindex[ (k+2)%3 ];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
===========
FanLength
===========
*/
int	FanLength (int starttri, int startv)
{
	int		m1, m2;
	int		j;
	mtriangle_t	*last, *check;
	int		k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+0)%3];
	m2 = last->vertindex[(startv+2)%3];


	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[ (k+2)%3 ];

			if (stripcount >= (int)(sizeof(stripverts) / sizeof(*stripverts)) - 2)
				goto done;
			stripverts[stripcount+2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}


/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
void BuildTris (void)
{
	int		i, j, k;
	int		startv;
	float	s, t;
	int		len, bestlen;
	int 	besttype = 0;
	int		bestverts[1024];
	int		besttris[1024];
	int		type;

	//
	// build tristrips
	//
	numorder = 0;
	numcommands = 0;
	memset (used, 0, sizeof(used));
	for (i=0 ; i<pheader->numtris ; i++)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0 ; type < 2 ; type++)
//	type = 1;
		{
			for (startv =0 ; startv < 3 ; startv++)
			{
				if (type == 1)
					len = StripLength (i, startv);
				else
					len = FanLength (i, startv);
				if (len > bestlen)
				{
					besttype = type;
					bestlen = len;
					for (j=0 ; j<bestlen+2 ; j++)
						bestverts[j] = stripverts[j];
					for (j=0 ; j<bestlen ; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j=0 ; j<bestlen ; j++)
			used[besttris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = (bestlen+2);
		else
			commands[numcommands++] = -(bestlen+2);

		for (j=0 ; j<bestlen+2 ; j++)
		{
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2.0f;	// on back side
			s = (s + 0.5f) / pheader->skinwidth;
			t = (t + 0.5f) / pheader->skinheight;

			memcpy(&commands[numcommands++], &s, sizeof(float));
			memcpy(&commands[numcommands++], &t, sizeof(float));
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	Con_DPrintf ("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}


/*
================
GL_MakeAliasModelDisplayLists
================
*/
void R_BuildAliasMesh(model_t *model, aliashdr_t *header)
{
    int *cmds, *compact;
    unsigned short *remap;
    int vertices, i, count, source, target;
    aliasmodel = model;
    paliashdr = header;
    BuildTris();
    header->command_vertices = numorder;
    if (Hyena_PackedArraysSupported()) {
        int words = 2;
        const int *scan = commands;
        while ((count = *scan++) != 0) {
            words += 1 + abs(count) * (header->numposes <= 1 ? 2 : 1);
            scan += abs(count) * 2;
        }
        cmds = Hunk_Alloc(words * sizeof(*cmds));
        header->commands = (byte *)cmds - (byte *)header;
        header->command_words = words;
        header->poseverts = numorder;
        cmds[0] = words;
        source = 0;
        target = 1;
        vertices = 0;
        while ((count = commands[source++]) != 0) {
            cmds[target++] = count;
            for (i = 0; i < abs(count); i++, vertices++, source += 2) {
                float uv[2];
                short packed[2];
                memcpy(uv, commands + source, sizeof(uv));
                packed[0] = uv[0] * 32767;
                packed[1] = uv[1] * 32767;
                memcpy(cmds + target++, packed, sizeof(packed));
                if (header->numposes <= 1)
                    memcpy(cmds + target++, poseverts[0] + vertexorder[vertices], sizeof(trivertx_t));
            }
        }
        cmds[target] = 0;
        if (header->numposes > 1)
            header->posedata = R_StoreAliasPoses(header, (const trivertx_t *const *)poseverts,
                header->numposes, numorder, vertexorder, &header->posemap);
        return;
    }
    header->poseverts = numorder;
    if (Hyena_MorphMeshesSupported()) {
        header->poseverts = header->numverts;
        remap = Hunk_Alloc(numorder * sizeof(*remap));
        header->command_remap = (byte *)remap - (byte *)header;
        for (i = 0; i < numorder; i++)
            remap[i] = vertexorder[i];
    } else {
        compact = malloc(numorder * (sizeof(*compact) + sizeof(*remap)));
        if (compact) {
            remap = (unsigned short *)(compact + numorder);
            vertices = R_CompactAliasVertices(commands, vertexorder, numorder, remap, compact);
            if (vertices && vertices < numorder) {
                unsigned short *stored = Hunk_Alloc(numorder * sizeof(*stored));
                memcpy(stored, remap, numorder * sizeof(*stored));
                header->command_remap = (byte *)stored - (byte *)header;
                header->poseverts = vertices;
                memcpy(vertexorder, compact, vertices * sizeof(*compact));
            }
            free(compact);
        }
    }
    cmds = Hunk_Alloc(numcommands * sizeof(*cmds));
    header->commands = (byte *)cmds - (byte *)header;
    memcpy(cmds, commands, numcommands * sizeof(*cmds));
    header->posedata = R_StoreAliasPoses(header, (const trivertx_t *const *)poseverts,
        header->numposes, header->poseverts, Hyena_MorphMeshesSupported() ? NULL : vertexorder, &header->posemap);
}
