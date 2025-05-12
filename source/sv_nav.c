#include "quakedef.h"

// TODO - Rename this to just nav.c? ai_nav.c?

#include <stdlib.h>



typedef struct navmesh_poly_s {
	int vert_idxs[NAVMESH_MAX_VERTICES_PER_POLY];
	int n_verts;
	char *doortarget; // nullptr or contains string matching "targetname" of a door. Polygon is used iff no door or door.state == STATE_BOTTOM

	int connected_traversals[NAVMESH_MAX_TRAVERSALS_PER_POLY];
	int n_connected_traversals;

	// Derived variables calculated at load-time
	vec3_t center; // Center of polygon (mean of vertex positions) in 3D space
	int connected_polies[NAVMESH_MAX_VERTICES_PER_POLY]; // Number of polygons this polygon shares an edge with
	int n_connected_polies; // List of index to each polygon we share an edge with
	int connected_polies_left_vert[NAVMESH_MAX_VERTICES_PER_POLY]; // Left vertex of the shared edge (from this polygon's perspective)
	int connected_polies_right_vert[NAVMESH_MAX_VERTICES_PER_POLY]; // Right vertex of the shared edge (from this polygon's perspective)
	// int connected_polies_neighbor_edge_index[NAVMESH_MAX_VERTICES_PER_POLY]; // Neighbor polygon edge index that connects to this polygon
} navmesh_poly_t;



vec3_t sv_navmesh_verts[NAVMESH_MAX_VERTICES];
int sv_navmesh_n_verts = 0;
navmesh_poly_t sv_navmesh_polies[NAVMESH_MAX_POLYGONS];
int sv_navmesh_n_polies = 0;












// 
// Returns some number > 0 if point p is to the left of line a->b
// Returns some number < 0 if point is to the right of line a->b
// Returns 0 if point is on the line
// (Left / Right is defined in the xy plane, z=0)
// 
float  pathfind_point_is_to_left(vec3_t a, vec3_t b, vec3_t p) {
	return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]);
}


//
// Given a funnel defined by `funnel_left` -> `funnel_corner` -> `funnel_right`
// Returns 0 if point `p` is inside of the funnel
// Returns -1 if point `p` is outside of the funnel to the left
// Returns 1 if point `p` is outside of the funel to the right
//
int pathfind_point_in_funnel(vec3_t p, vec3_t funnel_corner, vec3_t funnel_left, vec3_t funnel_right) {

	// If outside of funnel left edge, return 1
	float p_inside_left_edge = pathfind_point_is_to_left( funnel_left, funnel_corner, p);
	if(p_inside_left_edge < 0) {
		return -1;
	}

	// If outside of funnel right edge, return 1
	float p_inside_right_edge = pathfind_point_is_to_left( funnel_corner, funnel_right, p);
	if(p_inside_right_edge < 0) {
		return 1;
	}

	return 0;
}


// //
// // Returns the distance from a point to a line segment (a,b)
// //
// float pathfind_2d_line_point_dist(vec3_t a, vec3_t b, vec3_t p) {
// 	vec3_t a_to_b;
// 	VectorSubtract(b, a, a_to_b);
// 	vec3_t b_to_p;
// 	VectorSubtract(p, b, b_to_p);
// 	float dot = DotProduct(a_to_b, b_to_p);
// 	if(dot > 0) {
// 		return VectorLength(b_to_p);
// 	}

// 	vec3_t b_to_a;
// 	VectorSubtract(a, b, b_to_a);
// 	vec3_t a_to_p;
// 	VectorSubtract(p, a, a_to_p);
// 	dot = DotProduct(b_to_a, a_to_p);
// 	if(dot > 0) {
// 		return VectorLength(a_to_p);
// 	}

// 	// 2D cross product between (b - a) and (pos - a)
// 	vec3_t p_to_a;
// 	VectorSubtract(a, p, p_to_a);
// 	vec3_t c;
// 	CrossProduct(a_to_b, p_to_a, c);
// 	float dist = VectorLength(c) / VectorLength(a_to_b);
// 	return fabs(dist);
// }



//
// Returns the distance from a point `p` to a line segment defined by points `a` and `b`
// Ignores distance in 
//
float pathfind_2d_line_point_dist(vec3_t a, vec3_t b, vec3_t p) {
	vec3_t a_to_b, a_to_p, b_to_p;
	VectorSubtract2D(b, a, a_to_b);   // a_to_b = b - a
	VectorSubtract2D(p, a, a_to_p);   // a_to_p = p - a
	VectorSubtract2D(p, b, b_to_p);   // b_to_p = p - b

	// Check if p projects outside the segment on the b side
	if (DotProduct(a_to_b, b_to_p) > 0) {
		// Distance to endpoint b
		return VectorLength(b_to_p);  
	}

	// Check if p projects outside the segment on the a side
	if (DotProduct(a_to_b, a_to_p) < 0) {
		// Distance to endpoint a
		return VectorLength(a_to_p);  
	}

	// 2D cross product for area, then divide by segment length
	float cross = a_to_b[0] * a_to_p[1] - a_to_b[1] * a_to_p[0];
	float dist = fabs(cross) / VectorLength(a_to_b);
	return dist;
}








//
// Returns 1 if `pos_z` is within 30 qu of any vertex's z position
// Returns 0 otherwise
//
int sv_navmesh_near_poly_z_axis(float pos_z, int poly_idx) {
	for(int i = 0; i < sv_navmesh_polies[poly_idx].n_verts; i++) {
		int vert_idx = sv_navmesh_polies[poly_idx].vert_idxs[i];
		float vert_pos_z = sv_navmesh_verts[vert_idx][2];
		// If within 30qu of this vertex's z-coord, this polygon is good
		if(pos_z >= vert_pos_z - 30 && pos_z <= vert_pos_z + 30) {
			return 1;
		}
	}

	// No vertex was within 30-qu in z-axis, we are too far
	return 0;
}


// 
// Returns 1 if `pos` is inside poly at index `poly_idx`
// Returns 0 otherwise
//
int sv_navmesh_is_inside_poly(vec3_t pos, int poly_idx) {
	int n_verts = sv_navmesh_polies[poly_idx].n_verts;

	// We are considered to be in polygon, if pos is on the _left_ of all edges of the polygon
	// (Assumes verts are ordered CCW order) 
	for(int i = 0; i < n_verts; i++) {
		int vert_idx = sv_navmesh_polies[poly_idx].vert_idxs[i];
		int next_vert_idx = sv_navmesh_polies[poly_idx].vert_idxs[(i + 1) % n_verts];

		float vert_to_next_x = sv_navmesh_verts[next_vert_idx][0] - sv_navmesh_verts[vert_idx][0];
		float vert_to_next_y = sv_navmesh_verts[next_vert_idx][1] - sv_navmesh_verts[vert_idx][1];
		float vert_to_pos_x = pos[0] - sv_navmesh_verts[vert_idx][0];
		float vert_to_pos_y = pos[1] - sv_navmesh_verts[vert_idx][1];

		// If point is to right of edge, stop
		if((vert_to_next_x * vert_to_pos_y - vert_to_next_y * vert_to_pos_x) < 0) {
			return 0;
		}
	}
	return 1;
}


// 
// Returns distance from `pos` to an edge of the polygon if pos is very close to one of the edges
// Returns -1 if the polygon is too far
// 
float sv_navmesh_dist_to_poly(vec3_t pos, int poly_idx) {
	float epsilon = 30; // Must be within 30 qu to be considered "close"

	// TODO - Use: pathfind_2d_line_point_dist(vec3_t a, vec3_t b, vec3_t p)
	int n_verts = sv_navmesh_polies[poly_idx].n_verts;
	float shortest_dist = INFINITY;

	for(int i = 0; i < n_verts; i++) {
		int vert_idx = sv_navmesh_polies[poly_idx].vert_idxs[i];
		int next_vert_idx = sv_navmesh_polies[poly_idx].vert_idxs[(i + 1) % n_verts];

		// To calculate in 2D: Set vertex position .z to 0
		float dist = pathfind_2d_line_point_dist(sv_navmesh_verts[vert_idx], sv_navmesh_verts[next_vert_idx], pos);

		if(dist < shortest_dist) {
			shortest_dist = dist;
		}
	}
	if(shortest_dist < epsilon) {
		return shortest_dist;
	}
	return -1;
}




//
// Returns polygon that contains point `pos`
// If we aren't in a polygon, return a polygon within an epsilon of `pos`
// Otherwise, return -1
// 
int sv_navmesh_get_containing_poly(vec3_t pos) {

	int closest_poly = -1;
	float closest_poly_dist = INFINITY;

	// TODO - Do better than this, we can do better than O(n)
	for(int i = 0; i < sv_navmesh_n_polies; i++) {
		// If we're not close enough to polygon in z-axis, skip
		if(sv_navmesh_near_poly_z_axis(pos[2], i) == 0) {
			Con_Printf("Too far from polygon %d in z\n", i);
			continue;
		}

		// Check if we are in the 2D plane of polygon i
		if(sv_navmesh_is_inside_poly(pos, i)) {
			Con_Printf("Pos is inside poly: %d.\n", i);
			return i;
		}

		// If we are not in polygon i, check if we are very close to one of its edges
		// TODO - Is there a quick and dirty signed distance function?

		float dist = sv_navmesh_dist_to_poly(pos, i);
		if(dist >= 0) {
			if(dist < closest_poly_dist) {
				closest_poly = i;
				closest_poly_dist = dist;
			}
		}
	}

	if(closest_poly == -1) {
		Con_Printf("Pos is not in or near any polygons.\n");
	}
	else {
		Con_Printf("Pos is near polygon %d.\n", closest_poly);
	}
	return closest_poly;
}


// TODO - What structs do I need for pathfinding?
// Let's go dig into the structs used in navmesh_editor


#define NAVMESH_PATHFIND_POLYGON_SET_NONE 		0 // Used for A* unevaluated polygons
#define NAVMESH_PATHFIND_POLYGON_SET_OPEN 		1 // Used for A* candidate polygons
#define NAVMESH_PATHFIND_POLYGON_SET_CLOSED 	2 // Used for A* fully evaluated polygons

typedef struct {
	vec3_t start_pos;
	vec3_t goal_pos;

	int start_poly_idx;
	int goal_poly_idx;


	uint8_t poly_set[NAVMESH_MAX_POLYGONS]; // Index i contains navmesh's i-th polygon set membership
	// These two arrays track the f-score of all polygons in the open-set
	// These arrays are kept synchronized and updated together via the "*openset*" functions
	int openset_poly_rank[NAVMESH_MAX_POLYGONS]; // Index `i` contains navmesh's `i`-th polygon rank in openset. (0 <= rank < NAVMESH_MAX_POLYGONS)
	int openset_rank_poly[NAVMESH_MAX_POLYGONS]; // Index `i` contains the navmesh polygon index with rank `i`. Index 0 contains index of best polygon.`
	int n_openset_polies; // Number of polygons in `openset_rank_poly`


	int poly_prev_poly[NAVMESH_MAX_POLYGONS]; // = -1; Index i contains the polygon we walked from to get to polygon i
	float poly_g_score[NAVMESH_MAX_POLYGONS];
	float poly_f_score[NAVMESH_MAX_POLYGONS];

	// TODO - Add structs for final traced polygon path
	// TODO - Add structs for final funnel algorithm vector path
	int path_polygons[NAVMESH_MAX_POLYGONS];
	int n_path_polygons;

	vec3_t path_portals_left[NAVMESH_MAX_POLYGONS];		// Note - `NAVMESH_MAX_POLYGONS` is max theoretical limit, but may be overkill. Might balloon the struct too much
	vec3_t path_portals_right[NAVMESH_MAX_POLYGONS];
	int n_path_portals;


	vec3_t path_points[NAVMESH_MAX_POLYGONS];
	int n_path_points;
} navmesh_pathfind_results_t;



void sv_pathfind_remove_from_openset(navmesh_pathfind_results_t *pathfind_result, int poly_idx) {
	if(pathfind_result->poly_set[poly_idx] != NAVMESH_PATHFIND_POLYGON_SET_OPEN) {
		return;
	}
	pathfind_result->poly_set[poly_idx] = NAVMESH_PATHFIND_POLYGON_SET_NONE;

	// Remove the polygon from the sorted openset polygon array and shift elements down
	for(int i = pathfind_result->openset_poly_rank[poly_idx]; i < pathfind_result->n_openset_polies + 1; i++) {
		int rank_poly_idx = pathfind_result->openset_rank_poly[i + 1];
		pathfind_result->openset_rank_poly[i] = rank_poly_idx;
		pathfind_result->openset_poly_rank[rank_poly_idx] = i;
	}
	pathfind_result->openset_poly_rank[poly_idx] = -1;
	pathfind_result->n_openset_polies -= 1;
}


//
// Removes and returns polygon from openset with lowest f score
//
int sv_pathfind_pop_openset_lowest_f_score_poly(navmesh_pathfind_results_t *pathfind_result) {

	int poly_idx = -1;
	if(pathfind_result->n_openset_polies <= 0) {
		return poly_idx;
	}

	// First index contains lowest f-score poly:
	poly_idx = pathfind_result->openset_rank_poly[0];

	// Remove polygon from openset
	sv_pathfind_remove_from_openset(pathfind_result, poly_idx);

	return poly_idx;
}


void sv_pathfind_add_to_closed_set(navmesh_pathfind_results_t *pathfind_result, int poly_idx) {
	// Redundant safety check -- TODO - Disable this
	if(pathfind_result->poly_set[poly_idx] == NAVMESH_PATHFIND_POLYGON_SET_OPEN) {
		Con_Printf("Warning: Added openset polygon to closedset.\n");
		sv_pathfind_remove_from_openset(pathfind_result, poly_idx);
	}
	pathfind_result->poly_set[poly_idx] = NAVMESH_PATHFIND_POLYGON_SET_CLOSED;
}


//
// Adds polygon `poly_idx` to openset with f-score `poly_f_score`
// Returns 1 if polygon `poly_idx` was added to openset with score `poly_f_score`
// Returns 0 otherwise
// 
int sv_pathfind_add_to_openset(navmesh_pathfind_results_t *pathfind_result, int poly_idx, float poly_f_score) {
	if(poly_idx < 0 || poly_idx >= NAVMESH_MAX_POLYGONS) {
		Con_Printf("Warning: sv_pathfind_add_to_openset -- Invalid poly_idx: %d\n", poly_idx);
		return 0;
	}
	// Base case, empty list:
	if(pathfind_result->n_openset_polies == 0) {
		pathfind_result->poly_set[poly_idx] = NAVMESH_PATHFIND_POLYGON_SET_OPEN;
		pathfind_result->openset_rank_poly[0] = poly_idx;
		pathfind_result->openset_poly_rank[poly_idx] = 0;
		pathfind_result->n_openset_polies = 1;
		return 1;
	}

	// If we do end up inserting, only search array between [0,right_idx]
	int right_idx = pathfind_result->n_openset_polies;


	// If already in openset, check if `poly_f_score` is better than its current score
	if(pathfind_result->poly_set[poly_idx] == NAVMESH_PATHFIND_POLYGON_SET_OPEN) {
		// If not better, don't add
		if(poly_f_score >= pathfind_result->poly_f_score[poly_idx]) {
			return 0;
		}
		// New insertion index must be within [0, polygon previous rank]
		right_idx = pathfind_result->openset_poly_rank[poly_idx];
		// Now remove from the sorted openset list
		sv_pathfind_remove_from_openset(pathfind_result, poly_idx);
	}


	// ------------------------------------------------------------------------
	// Binary search to find insertion index into array
	// ------------------------------------------------------------------------
	int left_idx = 0;
	// right_idx = pathfind_result->n_openset_polies;
	while(left_idx < right_idx) {
		int midpoint_idx = (right_idx + left_idx) / 2;
		if(poly_f_score <= pathfind_result->poly_f_score[pathfind_result->openset_rank_poly[midpoint_idx]]) {
			right_idx = midpoint_idx;
		}
		else {
			left_idx = midpoint_idx + 1;
		}
	}
	int insertion_idx = left_idx;
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// Shift up all indices >= insertion index to make space for new entry
	// ------------------------------------------------------------------------
	for(int i = pathfind_result->n_openset_polies; i > insertion_idx; i--) {
		int rank_poly_idx = pathfind_result->openset_rank_poly[i - 1];
		// Move polygon to new sorted array position
		pathfind_result->openset_rank_poly[i] = rank_poly_idx;
		// Update rank for polygon `rank_poly_idx`
		pathfind_result->openset_poly_rank[rank_poly_idx] = i;
	}
	// ------------------------------------------------------------------------

	// Insert at freshly vacated index
	pathfind_result->poly_set[poly_idx] = NAVMESH_PATHFIND_POLYGON_SET_OPEN;
	pathfind_result->openset_rank_poly[insertion_idx] = poly_idx;
	pathfind_result->openset_poly_rank[poly_idx] = insertion_idx;
	pathfind_result->n_openset_polies += 1;
	return 1;

}








#ifdef NAVMESH_DEBUG_DRAW
#include <pspgu.h>
#include <pspgum.h>




// Max possible number of vertices to draw
float cl_draw_navmesh_poly_verts[NAVMESH_MAX_POLYGONS * 2 * NAVMESH_MAX_VERTICES_PER_POLY * 3]; // triangulated polygons to draw
int cl_draw_navmesh_n_poly_verts = 0;



float cl_draw_navmesh_edge_verts[NAVMESH_MAX_POLYGONS * NAVMESH_MAX_VERTICES_PER_POLY * 2 * 3];
int cl_draw_navmesh_n_edge_verts  = 0;



// Debug locations to place
vec3_t sv_navmesh_test_start_pos;
vec3_t sv_navmesh_test_goal_pos;
int sv_navmesh_test_start_pos_placed = 0; // Flag set when placed
int sv_navmesh_test_goal_pos_placed = 0; // Flag set when placed



navmesh_pathfind_results_t sv_navmesh_test_pathfind_result;


//
// Clear a `navmesh_pathfind_results_t` struct for reuse
//
void navmesh_clear_result(navmesh_pathfind_results_t *pathfind_result) {
	for(int i = 0; i < NAVMESH_MAX_POLYGONS; i++) {
		pathfind_result->poly_set[i] = NAVMESH_PATHFIND_POLYGON_SET_NONE;
		// pathfind_result->openset_poly_idx[i] = 0; 	// Note - technically not needed, setting length to 0 should suffice
		pathfind_result->openset_poly_rank[i] = -1;
		pathfind_result->openset_rank_poly[i] = -1; 	// Note - technically not needed, setting length to 0 should suffice


		pathfind_result->path_polygons[i] = -1; 	// Note - technically not needed, setting length to 0 should suffice

		pathfind_result->poly_prev_poly[i] = -1; 
		pathfind_result->poly_f_score[i] = 0;
		pathfind_result->poly_g_score[i] = 0;
	

		VectorClear(pathfind_result->path_portals_left[i]);		// Note - technically not needed, setting length to 0 should suffice
		VectorClear(pathfind_result->path_portals_right[i]);	// Note - technically not needed, setting length to 0 should suffice
		VectorClear(pathfind_result->path_points[i]);			// Note - technically not needed, setting length to 0 should suffice
	}

	VectorClear(pathfind_result->start_pos);
	VectorClear(pathfind_result->goal_pos);

	pathfind_result->start_poly_idx = -1;
	pathfind_result->goal_poly_idx = -1;

	pathfind_result->n_openset_polies = 0;
	pathfind_result->n_path_polygons = 0;
	pathfind_result->n_path_portals = 0;
	pathfind_result->n_path_points = 0;
}



//
// Returns heuristic distance (squared) between polygon `poly_a_idx` and polygon `poly_b_idx`
// Distance squared saves a sqrt, since we're only using them for comparison
// Used for both h-score (distance along path) and g-score (distance to goal) calculations
// 
static inline float sv_pathfind_polies_dist(int poly_a_idx, int poly_b_idx) {
	return VectorDistanceSquared(sv_navmesh_polies[poly_a_idx].center, sv_navmesh_polies[poly_b_idx].center);
}


//
// Searches for door entity whose `targetname` field = `doortarget`
// Returns 1 if door exists and is closed
// Returns 0 otherwise
//
#define DOOR_STATE_TOP		0
#define DOOR_STATE_BOTTOM	1
#define DOOR_STATE_UP		2
#define DOOR_STATE_DOWN		3


// TODO - Should come up with some caching mechanism to quickly check all doors on the map
// TODO   Maybe at navmesh load, we search through map to find all recognized door ents and store their name->ent_idx lookups?
// TODO   Then, we can compute the checksum value for all used doors and their states, though this is a util that should probably be handled by the server elsewhere?


int get_door_is_closed(const char *doortarget) {
	if(doortarget == NULL) {
		return 0;
	}

	for (int ent_idx = 1; ent_idx < sv.num_edicts; ent_idx++) {
		edict_t	*ent_dict = EDICT_NUM(ent_idx);
		if (ent_dict->free) {
			continue;
		}
		char *ent_navtargetname = pr_strings + ent_dict->v.nav_targetname;
		if(!strcmp( doortarget, ent_navtargetname)) {
			if(ent_dict->v.state == DOOR_STATE_BOTTOM) {
				return 1;
			}
			break;
		}
	}
	return 0;
}





void navmesh_pathfind_a_star(navmesh_pathfind_results_t *pathfind_result) {
	// ---------------------------------------–--------------------------------
	// Calculate f, h, and g scores (g=f+h)
	// ---------------------------------------–--------------------------------
	// Distance from start polygon start to polygon center along path
	pathfind_result->poly_g_score[pathfind_result->start_poly_idx] = 0;
	// Heuristic distance from polygon center to goal polygon center
	pathfind_result->poly_f_score[pathfind_result->start_poly_idx] = sv_pathfind_polies_dist(pathfind_result->start_poly_idx, pathfind_result->goal_poly_idx);
	// ---------------------------------------–--------------------------------


	// ------------------------------------------------------------------------
	// Add start polygon to open set
	// ------------------------------------------------------------------------
	sv_pathfind_add_to_openset(pathfind_result, pathfind_result->start_poly_idx, pathfind_result->poly_f_score[pathfind_result->start_poly_idx]);
	// ------------------------------------------------------------------------


	// int cur_poly = start_poly_idx;
	int pathfind_success = 0;

	while(pathfind_result->n_openset_polies > 0) {
		Con_Printf("sv_navmesh_test_pathfind -- Pathfind loop\n");
		Con_Printf("\tsv_navmesh_test_pathfind -- Pathfind loop - Openset (%d): [ ", pathfind_result->n_openset_polies);
		for(int i = 0; i < pathfind_result->n_openset_polies; i++) {
			int poly_idx = pathfind_result->openset_rank_poly[i];
			Con_Printf(" (idx %d, poly %d, f %.2f, set %d, rank %d), ", i, pathfind_result->openset_rank_poly[i], pathfind_result->poly_f_score[poly_idx], pathfind_result->poly_set[poly_idx], pathfind_result->openset_poly_rank[poly_idx]);
		}
		Con_Printf("]\n");


		// Get next best polygon from open set
		int cur_poly = sv_pathfind_pop_openset_lowest_f_score_poly(pathfind_result);
		// Move polygon to closed set
		sv_pathfind_add_to_closed_set(pathfind_result, cur_poly);

		if(cur_poly == goal_poly_idx) {
			Con_Printf("sv_navmesh_test_pathfind -- Pathfind loop - found goal poly\n");
			pathfind_success = 1;
			break;
		}

		for(int i = 0; i < sv_navmesh_polies[cur_poly].n_connected_polies; i++) {
			int neighbor_poly = sv_navmesh_polies[cur_poly].connected_polies[i];
			Con_Printf("sv_navmesh_test_pathfind -- Checking poly %d neighbor[%d/%d] : %d\n", cur_poly, i, sv_navmesh_polies[cur_poly].n_connected_polies, neighbor_poly);

			// If already in closed-set, skip this neighbor
			if(pathfind_result->poly_set[neighbor_poly] == NAVMESH_PATHFIND_POLYGON_SET_CLOSED) {
				continue;
			}

			// ----------------------------------------------------------------
			// Door check
			// If polygon's door hasn't been opened, don't consider it.
			// ----------------------------------------------------------------
			// If this polygon references a door:
			if(sv_navmesh_polies[neighbor_poly].doortarget != NULL) {
				if(get_door_is_closed(sv_navmesh_polies[neighbor_poly].doortarget)) {
					Con_Printf("sv_navmesh_test_pathfind -- Checking poly %d neighbor[%d] %d -- door is closed\n", cur_poly, i, neighbor_poly);
					// This neighbor polygon has not yet been enabled by its door
					// skip it from consideration
					continue;
				}
			}
			Con_Printf("sv_navmesh_test_pathfind -- Checking poly %d neighbor[%d] %d -- past door check\n", cur_poly, i, neighbor_poly);
			// ----------------------------------------------------------------


			// Distance along path of polygon centers from start polygon to neighbor polygon
			float g_score = pathfind_result->poly_g_score[cur_poly] + sv_pathfind_polies_dist(cur_poly, neighbor_poly);
			float h_score = sv_pathfind_polies_dist(cur_poly, pathfind_result->goal_poly_idx);
			float f_score = g_score + h_score;

			// Add to openset (or update in openset if better)
			if(sv_pathfind_add_to_openset(pathfind_result, neighbor_poly, f_score)) {
				pathfind_result->poly_g_score[neighbor_poly] = g_score;
				pathfind_result->poly_f_score[neighbor_poly] = f_score;
				// Only update the `poly_prev_poly` chain for this polygon if this is its new best f-score
				pathfind_result->poly_prev_poly[neighbor_poly] = cur_poly;
			}
		}
	}

	// TODO - Return pathfind_success?
}


// 
// Trace final polygon path
//
void navmesh_pathfind_trace_a_star_polygon_path(navmesh_pathfind_results_t *pathfind_result) {
	int path_length = 1;
	int cur_poly = pathfind_result->goal_poly_idx;
	while(cur_poly != -1 && cur_poly != pathfind_result->start_poly_idx) {
		cur_poly = pathfind_result->poly_prev_poly[cur_poly];
		path_length++;
	}

	cur_poly = pathfind_result->goal_poly_idx;
	for(int i = 0; i < path_length; i++) {
		// Insert in reverse order (to build path from start to finish)
		pathfind_result->path_polygons[path_length - 1 - i] = cur_poly;
		cur_poly = pathfind_result->poly_prev_poly[cur_poly];
	}
	pathfind_result->n_path_polygons = path_length;
}


//
// Get list of portals along polygon path
//
void navmesh_pathfind_get_path_portals(navmesh_pathfind_results_t *pathfind_result) {
	for(int i = 1; i < pathfind_result->n_path_polygons; i++) {
		int cur_poly_idx = pathfind_result->path_polygons[i];
		int prev_poly_idx = pathfind_result->path_polygons[i - 1];

		// Find shared edge between the two polygons

		// Find which index of prev_poly_idx's connected verts is cur_poly_idx
		int prev_poly_edge_idx = -1;
		for(int j = 0; j < sv_navmesh_polies[prev_poly_idx].n_connected_polies; j++) {
			if(sv_navmesh_polies[prev_poly_idx].connected_polies[j] == cur_poly_idx) {
				prev_poly_edge_idx = j;
				break;
			}
		}
		if(prev_poly_edge_idx < 0) {
			Con_Printf("ERROR: Unable to find edge connecting polygons %d and %d\n", prev_poly_idx, cur_poly_idx);
			break; // return? idk
		}

		int portal_left_vert_idx = sv_navmesh_polies[prev_poly_idx].connected_polies_left_vert[prev_poly_edge_idx];
		int portal_right_vert_idx = sv_navmesh_polies[prev_poly_idx].connected_polies_right_vert[prev_poly_edge_idx];
		vec3_t portal_left_pos;
		vec3_t portal_right_pos;
		VectorCopy( sv_navmesh_verts[portal_left_vert_idx], portal_left_pos);
		VectorCopy( sv_navmesh_verts[portal_right_vert_idx], portal_right_pos);

		// // --------------------------------------------------------------------
		// // Narrow the portal from each side by 20% or 20qu, whichever is smaller
		// // --------------------------------------------------------------------
		// vec3_t portal_ofs;
		// // Get vector pointing from portal left pos to portal right pos
		// VectorSubtract( portal_right_pos, portal_left_pos, portal_ofs); // portal_ofs = portal_right_pos - portal_left_pos
		// float portal_edge_len = VectorLength(portal_ofs);
		// vec3_t portal_edge_dir;
		// VectorCopy(portal_ofs, portal_edge_dir);
		// VectorNormalizeFast(portal_edge_dir);
		// // 20% of portal edge length, or 20qu, whichever is smaller
		// float portal_inset_dist = fmin(20, portal_edge_len * 0.2);
		// vec3_t portal_left_inset_ofs;
		// VectorScale(portal_edge_dir, portal_inset_dist, portal_left_inset_ofs);
		// vec3_t portal_right_inset_ofs;
		// VectorScale(portal_edge_dir, -portal_inset_dist, portal_right_inset_ofs);
		// // -
		// VectorAdd( portal_left_pos, portal_left_inset_ofs, portal_left_pos);
		// VectorAdd( portal_right_pos, portal_right_inset_ofs, portal_right_pos);
		// // --------------------------------------------------------------------

		// Store the portal
		VectorCopy( portal_left_pos, pathfind_result->path_portals_left[pathfind_result->n_path_portals]);
		VectorCopy( portal_right_pos, pathfind_result->path_portals_right[pathfind_result->n_path_portals]);
		pathfind_result->n_path_portals++;
	}

	// Add goal position as the final path portal
	VectorCopy( pathfind_result->goal_pos, pathfind_result->path_portals_left[pathfind_result->n_path_portals]);
	VectorCopy( pathfind_result->goal_pos, pathfind_result->path_portals_right[pathfind_result->n_path_portals]);
	pathfind_result->n_path_portals++;
}


void navmesh_pathfind_funnel_algorithm(navmesh_pathfind_results_t *pathfind_result) {
	// ------------------------------------------------------------------------
	// Perform funnel algorithm to produce the final smoothed path
	// ------------------------------------------------------------------------
	vec3_t funnel_pos;
	vec3_t funnel_left_pos;
	vec3_t funnel_right_pos;
	// NOTE - The reason we need to keep track of funnel portal indices...
	// NOTE   Consider the following situation:
	// NOTE   Corner:  (5,10)
	// NOTE   Portals: A: [(0,20),(10,20)], B: [(2,30),(20,30)], C: [(28,40),(32,40)]
	// NOTE   The first iteration will skip portal B
	// NOTE   On the next iteration, portal C will be outside of funnel
	// NOTE   but the funnel is reset, we can't simply use portal C as the funnel corners
	// NOTE   This would lead to a funnel that clips outside of the navmesh
	// NOTE   Instead, the next funnel needs to be created from the funnel's right corner
	// NOTE   _and_ the portal immediately after the funnel's right corner
	int funnel_left_portal_idx = 0;
	int funnel_right_portal_idx = 0;


	VectorCopy(pathfind_result->start_pos, funnel_pos);
	VectorCopy(pathfind_result->path_portals_left[funnel_left_portal_idx], funnel_left_pos);
	VectorCopy(pathfind_result->path_portals_right[funnel_right_portal_idx], funnel_right_pos);
	vec3_t portal_left;
	vec3_t portal_right;


	// The first funnel [0] is defined by the first portal, skip it, start at portal [1]
	for(int i = 1; i < pathfind_result->n_path_portals; i++) {
		VectorCopy(pathfind_result->path_portals_left[i], portal_left);
		VectorCopy(pathfind_result->path_portals_right[i], portal_right);

		Con_Printf("Checking portal: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f) ...\n", 
			portal_left[0], portal_left[1], portal_left[2],
			portal_right[0], portal_right[1], portal_right[2]
		);
		Con_Printf("Against funnel portal: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)\n", 
			funnel_left_pos[0], funnel_left_pos[1], funnel_left_pos[2],
			funnel_pos[0], funnel_pos[1], funnel_pos[2],
			funnel_right_pos[0], funnel_right_pos[1], funnel_right_pos[2]
		);

		int portal_left_in_funnel = pathfind_point_in_funnel(portal_left, funnel_pos, funnel_left_pos, funnel_right_pos);
		int portal_right_in_funnel = pathfind_point_in_funnel(portal_right, funnel_pos, funnel_left_pos, funnel_right_pos);


		Con_Printf("Portal left in funnel: %d\n", portal_left_in_funnel);
		Con_Printf("Portal right in funnel: %d\n", portal_right_in_funnel);


		// If both portal points are outside of funnel, advance to next portal without narrowing funnel
		if(portal_left_in_funnel < 0 && portal_right_in_funnel > 0) {
			continue;
		}

		// If either edge is in the funnel, update that edge, advance to next portal
		if(portal_left_in_funnel == 0 || portal_right_in_funnel == 0) {
			if(portal_left_in_funnel == 0) {
				VectorCopy(portal_left, funnel_left_pos);
				funnel_left_portal_idx = i;
			}
			if(portal_right_in_funnel == 0) {
				VectorCopy(portal_right, funnel_right_pos);
				funnel_right_portal_idx = i;
			}
			continue;
		}

		// Otherwise, cur portal does not overlap cur funnel

		// If portal is outside of funnel to the left, funnel left becomes new funnel pos
		if(portal_left_in_funnel < 0 && portal_left_in_funnel < 0) {
			Con_Printf("Portal left of funnel, adding point: (%.2f, %.2f, %.2f)\n", funnel_left_pos[0], funnel_left_pos[1], funnel_left_pos[2]);

			// Add the left endpoint to the path
			VectorCopy(funnel_left_pos, pathfind_result->path_points[pathfind_result->n_path_points]);

			// Place funnel corner at current left endpoint
			VectorCopy(funnel_left_pos, funnel_pos);
			// Make funnel left / right edges be the very next portal
			funnel_left_portal_idx += 1;
			funnel_right_portal_idx = funnel_left_portal_idx;
		}
		// If portal is outside of funnel to the right, funnel right becomes new funnel pos
		else {
			Con_Printf("Portal right of funnel, adding point: (%.2f, %.2f, %.2f)\n", funnel_right_pos[0], funnel_right_pos[1], funnel_right_pos[2]);

			// Add the right endpoint to the path
			VectorCopy(funnel_right_pos, pathfind_result->path_points[pathfind_result->n_path_points]);

			// Place funnel corner at current right endpoint
			VectorCopy(funnel_right_pos, funnel_pos);
			// Make funnel left / right edges be the very next portal
			funnel_right_portal_idx += 1;
			funnel_left_portal_idx = funnel_right_portal_idx;
		}

		pathfind_result->n_path_points += 1;
		VectorCopy(pathfind_result->path_portals_left[funnel_left_portal_idx], funnel_left_pos);
		VectorCopy(pathfind_result->path_portals_right[funnel_right_portal_idx], funnel_right_pos);
		// Restart the loop from the portal _after_ the funnel's new endpoints
		i = funnel_right_portal_idx;
	}

	// Always add the goal position to the path
	VectorCopy(pathfind_result->goal_pos, pathfind_result->path_points[pathfind_result->n_path_points]);
	pathfind_result->n_path_points += 1;
	// ------------------------------------------------------------------------
}




//
// Pathfind implementation that invokes standalone functions
//
void sv_navmesh_test_pathfind_v2() {
	if(sv_navmesh_test_start_pos_placed == 0) {
		return;
	}
	if(sv_navmesh_test_goal_pos_placed == 0) { 
		return;
	}

	navmesh_pathfind_results_t *pathfind_result = &sv_navmesh_test_pathfind_result;
	navmesh_clear_result(pathfind_result);


	VectorCopy(sv_navmesh_test_start_pos, pathfind_result->start_pos);
	VectorCopy(sv_navmesh_test_goal_pos, pathfind_result->goal_pos);
	

	Con_Printf("Getting position for test start pos: (%.2f, %.2f, %.2f)\n", pathfind_result->start_pos[0], pathfind_result->start_pos[1], pathfind_result->start_pos[2]);
	int start_poly_idx = sv_navmesh_get_containing_poly(pathfind_result->start_pos);
	Con_Printf("Getting position for test goal pos: (%.2f, %.2f, %.2f)\n", pathfind_result->goal_pos[0], pathfind_result->goal_pos[1], pathfind_result->goal_pos[2]);
	int goal_poly_idx = sv_navmesh_get_containing_poly(pathfind_result->goal_pos);
	pathfind_result->start_poly_idx = start_poly_idx;
	pathfind_result->goal_poly_idx = goal_poly_idx;


	Con_Printf("sv_navmesh_test_pathfind -- Start poly: %d\n", pathfind_result->start_poly_idx);
	Con_Printf("sv_navmesh_test_pathfind -- Goal poly: %d\n", pathfind_result->goal_poly_idx);


	if(pathfind_result->start_poly_idx == -1) {
		Con_Printf("sv_navmesh_test_pathfind -- Stopping due to invalid start poly.\n");
		return;
	}
	if(pathfind_result->goal_poly_idx == -1) {
		Con_Printf("sv_navmesh_test_pathfind -- Stopping due to invalid goal poly.\n");
		return;
	}

	if(pathfind_result->start_poly_idx == pathfind_result->goal_poly_idx) {
		Con_Printf("sv_navmesh_test_pathfind -- Stopping due to trivial case: start poly = goal poly.\n");
		pathfind_result->path_polygons[0] = start_poly_idx;
		pathfind_result->n_path_polygons = 1;
		// TODO - Calculate vector based path (start pos directly to goal pos)
		return;
	}


	navmesh_pathfind_a_star(pathfind_result);
	// TODO - If "navmesh_pathfind_a_star" fails, error out?

	Con_Printf("sv_navmesh_test_pathfind -- Tracing final path\n");
	navmesh_pathfind_trace_a_star_polygon_path(pathfind_result);

	// ------------------------------
	// Print polygon path
	// ------------------------------
	Con_Printf("Pathfind success, Path (%d) : [ ", pathfind_result->n_path_polygons);
	for(int i = 0; i < pathfind_result->n_path_polygons; i++) {
		if(i > 0) {
			Con_Printf(", ");
		}
		Con_Printf("P%d", pathfind_result->path_polygons[i]);
	}
	Con_Printf(" ]\n");
	// ------------------------------
	
	navmesh_pathfind_get_path_portals(pathfind_result);
		
	// ------------------------------
	// Print polygon path portals
	// ------------------------------
	Con_Printf("Start pos: (%.2f, %.2f, %.2f)\n", pathfind_result->start_pos[0], pathfind_result->start_pos[1], pathfind_result->start_pos[2]);
	Con_Printf("Goal pos: (%.2f, %.2f, %.2f)\n", pathfind_result->goal_pos[0], pathfind_result->goal_pos[1], pathfind_result->goal_pos[2]);
	Con_Printf("Path portals (%d):", pathfind_result->n_path_portals);
	for(int i = 0; i < pathfind_result->n_path_portals; i++) {
		Con_Printf(" [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)], ", 
			pathfind_result->path_portals_left[i][0],
			pathfind_result->path_portals_left[i][1],
			pathfind_result->path_portals_left[i][2],
			pathfind_result->path_portals_right[i][0],
			pathfind_result->path_portals_right[i][1],
			pathfind_result->path_portals_right[i][2]
		);
	}
	// ------------------------------
	
	navmesh_pathfind_funnel_algorithm(pathfind_result);
		
	// ------------------------------
	// Print final path
	// ------------------------------
	Con_Printf("Path points (%d): [", pathfind_result->n_path_points);
	for(int i = 0; i < pathfind_result->n_path_points; i++) {
		Con_Printf(" (%.2f, %.2f, %.2f),", 
			pathfind_result->path_points[i][0],
			pathfind_result->path_points[i][1],
			pathfind_result->path_points[i][2]
		);
	}
	Con_Printf("]\n");
	// ------------------------------

	// Requires structs: 
	// - path polygon indices from start to goal
	// - path traversal indices

	// Final vector-based path
	// Eventually - List of points that make up the final path
	// Eventually - List of traversals? Actually index of traversal for point i
}



void sv_navmesh_test_pathfind() {
	if(sv_navmesh_test_start_pos_placed == 0) {
		return;
	}
	if(sv_navmesh_test_goal_pos_placed == 0) { 
		return;
	}

	navmesh_pathfind_results_t *pathfind_result = &sv_navmesh_test_pathfind_result;
	navmesh_clear_result(pathfind_result);


	VectorCopy(sv_navmesh_test_start_pos, pathfind_result->start_pos);
	VectorCopy(sv_navmesh_test_goal_pos, pathfind_result->goal_pos);
	

	Con_Printf("Getting position for test start pos: (%.2f, %.2f, %.2f)\n", pathfind_result->start_pos[0], pathfind_result->start_pos[1], pathfind_result->start_pos[2]);
	int start_poly_idx = sv_navmesh_get_containing_poly(pathfind_result->start_pos);
	Con_Printf("Getting position for test goal pos: (%.2f, %.2f, %.2f)\n", pathfind_result->goal_pos[0], pathfind_result->goal_pos[1], pathfind_result->goal_pos[2]);
	int goal_poly_idx = sv_navmesh_get_containing_poly(pathfind_result->goal_pos);
	pathfind_result->start_poly_idx = start_poly_idx;
	pathfind_result->goal_poly_idx = goal_poly_idx;


	Con_Printf("sv_navmesh_test_pathfind -- Start poly: %d\n", pathfind_result->start_poly_idx);
	Con_Printf("sv_navmesh_test_pathfind -- Goal poly: %d\n", pathfind_result->goal_poly_idx);


	if(pathfind_result->start_poly_idx == -1) {
		Con_Printf("sv_navmesh_test_pathfind -- Stopping due to invalid start poly.\n");
		return;
	}
	if(pathfind_result->goal_poly_idx == -1) {
		Con_Printf("sv_navmesh_test_pathfind -- Stopping due to invalid goal poly.\n");
		return;
	}

	if(pathfind_result->start_poly_idx == pathfind_result->goal_poly_idx) {
		Con_Printf("sv_navmesh_test_pathfind -- Stopping due to trivial case: start poly = goal poly.\n");
		pathfind_result->path_polygons[0] = start_poly_idx;
		pathfind_result->n_path_polygons = 1;
		// TODO - Calculate vector based path (start pos directly to goal pos)
		return;
	}


	// ---------------------------------------–--------------------------------
	// Calculate f, h, and g scores (g=f+h)
	// ---------------------------------------–--------------------------------
	// Distance from start polygon start to polygon center along path
	pathfind_result->poly_g_score[start_poly_idx] = 0;
	// Heuristic distance from polygon center to goal polygon center
	pathfind_result->poly_f_score[start_poly_idx] = sv_pathfind_polies_dist(start_poly_idx, goal_poly_idx);
	// ---------------------------------------–--------------------------------


	// ------------------------------------------------------------------------
	// Add start polygon to open set
	// ------------------------------------------------------------------------
	sv_pathfind_add_to_openset(pathfind_result, start_poly_idx, pathfind_result->poly_f_score[start_poly_idx]);
	// ------------------------------------------------------------------------


	// int cur_poly = start_poly_idx;
	int pathfind_success = 0;

	while(pathfind_result->n_openset_polies > 0) {
		Con_Printf("sv_navmesh_test_pathfind -- Pathfind loop\n");
		Con_Printf("\tsv_navmesh_test_pathfind -- Pathfind loop - Openset (%d): [ ", pathfind_result->n_openset_polies);
		for(int i = 0; i < pathfind_result->n_openset_polies; i++) {
			int poly_idx = pathfind_result->openset_rank_poly[i];
			Con_Printf(" (idx %d, poly %d, f %.2f, set %d, rank %d), ", i, pathfind_result->openset_rank_poly[i], pathfind_result->poly_f_score[poly_idx], pathfind_result->poly_set[poly_idx], pathfind_result->openset_poly_rank[poly_idx]);
		}
		Con_Printf("]\n");



		// Get next best polygon from open set
		int cur_poly = sv_pathfind_pop_openset_lowest_f_score_poly(pathfind_result);
		// Move polygon to closed set
		sv_pathfind_add_to_closed_set(pathfind_result, cur_poly);

		if(cur_poly == goal_poly_idx) {
			Con_Printf("sv_navmesh_test_pathfind -- Pathfind loop - found goal poly\n");
			pathfind_success = 1;
			break;
		}

		for(int i = 0; i < sv_navmesh_polies[cur_poly].n_connected_polies; i++) {
			int neighbor_poly = sv_navmesh_polies[cur_poly].connected_polies[i];
			Con_Printf("sv_navmesh_test_pathfind -- Checking poly %d neighbor[%d/%d] : %d\n", cur_poly, i, sv_navmesh_polies[cur_poly].n_connected_polies, neighbor_poly);

			// If already in closed-set, skip this neighbor
			if(pathfind_result->poly_set[neighbor_poly] == NAVMESH_PATHFIND_POLYGON_SET_CLOSED) {
				continue;
			}

			// ----------------------------------------------------------------
			// Door check
			// If polygon's door hasn't been opened, don't consider it.
			// ----------------------------------------------------------------
			// If this polygon references a door:
			if(sv_navmesh_polies[neighbor_poly].doortarget != NULL) {
				if(get_door_is_closed(sv_navmesh_polies[neighbor_poly].doortarget)) {
					Con_Printf("sv_navmesh_test_pathfind -- Checking poly %d neighbor[%d] %d -- door is closed\n", cur_poly, i, neighbor_poly);
					// This neighbor polygon has not yet been enabled by its door
					// skip it from consideration
					continue;
				}
			}
			Con_Printf("sv_navmesh_test_pathfind -- Checking poly %d neighbor[%d] %d -- past door check\n", cur_poly, i, neighbor_poly);
			// ----------------------------------------------------------------


			// Distance along path of polygon centers from start polygon to neighbor polygon
			float g_score = pathfind_result->poly_g_score[cur_poly] + sv_pathfind_polies_dist(cur_poly, neighbor_poly);
			float h_score = sv_pathfind_polies_dist(cur_poly, goal_poly_idx);
			float f_score = g_score + h_score;

			// Add to openset (or update in openset if better)
			if(sv_pathfind_add_to_openset(pathfind_result, neighbor_poly, f_score)) {
				pathfind_result->poly_g_score[neighbor_poly] = g_score;
				pathfind_result->poly_f_score[neighbor_poly] = f_score;
				// Only update the `poly_prev_poly` chain for this polygon if this is its new best f-score
				pathfind_result->poly_prev_poly[neighbor_poly] = cur_poly;
			}
		}
	}


	Con_Printf("sv_navmesh_test_pathfind -- Tracing final path\n");


	// ------------------------------------------------------------------------
	// Trace final polygon path
	// ------------------------------------------------------------------------
	int path_length = 1;
	int cur_poly = goal_poly_idx;
	while(cur_poly != -1 && cur_poly != start_poly_idx) {
		cur_poly = pathfind_result->poly_prev_poly[cur_poly];
		path_length++;
	}

	cur_poly = goal_poly_idx;
	for(int i = 0; i < path_length; i++) {
		// Insert in reverse order (to build path from start to finish)
		pathfind_result->path_polygons[path_length - 1 - i] = cur_poly;
		cur_poly = pathfind_result->poly_prev_poly[cur_poly];
	}
	pathfind_result->n_path_polygons = path_length;
	// ------------------------------
	// Print path
	// ------------------------------
	Con_Printf("Pathfind success, Path (%d) : [ ", pathfind_result->n_path_polygons);
	for(int i = 0; i < pathfind_result->n_path_polygons; i++) {
		if(i > 0) {
			Con_Printf(", ");
		}
		Con_Printf("P%d", pathfind_result->path_polygons[i]);
	}
	Con_Printf(" ]\n");
	// ------------------------------
	// ------------------------------------------------------------------------



	// ------------------------------------------------------------------------
	// Get list of portals along polygon path
	// ------------------------------------------------------------------------
	for(int i = 1; i < pathfind_result->n_path_polygons; i++) {
		int cur_poly_idx = pathfind_result->path_polygons[i];
		int prev_poly_idx = pathfind_result->path_polygons[i - 1];

		// Find shared edge between the two polygons

		// Find which index of prev_poly_idx's connected verts is cur_poly_idx
		int prev_poly_edge_idx = -1;
		for(int j = 0; j < sv_navmesh_polies[prev_poly_idx].n_connected_polies; j++) {
			if(sv_navmesh_polies[prev_poly_idx].connected_polies[j] == cur_poly_idx) {
				prev_poly_edge_idx = j;
				break;
			}
		}
		if(prev_poly_edge_idx < 0) {
			Con_Printf("ERROR: Unable to find edge connecting polygons %d and %d\n", prev_poly_idx, cur_poly_idx);
			break; // return? idk
		}


		int portal_left_vert_idx = sv_navmesh_polies[prev_poly_idx].connected_polies_left_vert[prev_poly_edge_idx];
		int portal_right_vert_idx = sv_navmesh_polies[prev_poly_idx].connected_polies_right_vert[prev_poly_edge_idx];
		vec3_t portal_left_pos;
		vec3_t portal_right_pos;
		VectorCopy( sv_navmesh_verts[portal_left_vert_idx], portal_left_pos);
		VectorCopy( sv_navmesh_verts[portal_right_vert_idx], portal_right_pos);

		// // --------------------------------------------------------------------
		// // Narrow the portal from each side by 20% or 20qu, whichever is smaller
		// // --------------------------------------------------------------------
		// vec3_t portal_ofs;
		// // Get vector pointing from portal left pos to portal right pos
		// VectorSubtract( portal_right_pos, portal_left_pos, portal_ofs); // portal_ofs = portal_right_pos - portal_left_pos
		// float portal_edge_len = VectorLength(portal_ofs);
		// vec3_t portal_edge_dir;
		// VectorCopy(portal_ofs, portal_edge_dir);
		// VectorNormalizeFast(portal_edge_dir);
		// // 20% of portal edge length, or 20qu, whichever is smaller
		// float portal_inset_dist = fmin(20, portal_edge_len * 0.2);
		// vec3_t portal_left_inset_ofs;
		// VectorScale(portal_edge_dir, portal_inset_dist, portal_left_inset_ofs);
		// vec3_t portal_right_inset_ofs;
		// VectorScale(portal_edge_dir, -portal_inset_dist, portal_right_inset_ofs);
		// // -
		// VectorAdd( portal_left_pos, portal_left_inset_ofs, portal_left_pos);
		// VectorAdd( portal_right_pos, portal_right_inset_ofs, portal_right_pos);
		// // --------------------------------------------------------------------

		// Store the portal
		VectorCopy( portal_left_pos, pathfind_result->path_portals_left[pathfind_result->n_path_portals]);
		VectorCopy( portal_right_pos, pathfind_result->path_portals_right[pathfind_result->n_path_portals]);
		pathfind_result->n_path_portals++;
	}

	// Add goal position as the final path portal
	VectorCopy( pathfind_result->goal_pos, pathfind_result->path_portals_left[pathfind_result->n_path_portals]);
	VectorCopy( pathfind_result->goal_pos, pathfind_result->path_portals_right[pathfind_result->n_path_portals]);
	pathfind_result->n_path_portals++;
	// ------------------------------------------------------------------------



	Con_Printf("Start pos: (%.2f, %.2f, %.2f)\n", pathfind_result->start_pos[0], pathfind_result->start_pos[1], pathfind_result->start_pos[2]);
	Con_Printf("Goal pos: (%.2f, %.2f, %.2f)\n", pathfind_result->goal_pos[0], pathfind_result->goal_pos[1], pathfind_result->goal_pos[2]);
	Con_Printf("Path portals (%d):", pathfind_result->n_path_portals);
	for(int i = 0; i < pathfind_result->n_path_portals; i++) {
		Con_Printf(" [(%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f)], ", 
			pathfind_result->path_portals_left[i][0],
			pathfind_result->path_portals_left[i][1],
			pathfind_result->path_portals_left[i][2],
			pathfind_result->path_portals_right[i][0],
			pathfind_result->path_portals_right[i][1],
			pathfind_result->path_portals_right[i][2]
		);
	}


	// ------------------------------------------------------------------------
	// Perform funnel algorithm to produce the final smoothed path
	// ------------------------------------------------------------------------
	vec3_t funnel_pos;
	vec3_t funnel_left_pos;
	vec3_t funnel_right_pos;
	// NOTE - The reason we need to keep track of funnel portal indices...
	// NOTE   Consider the following situation:
	// NOTE   Corner:  (5,10)
	// NOTE   Portals: A: [(0,20),(10,20)], B: [(2,30),(20,30)], C: [(28,40),(32,40)]
	// NOTE   The first iteration will skip portal B
	// NOTE   On the next iteration, portal C will be outside of funnel
	// NOTE   but the funnel is reset, we can't simply use portal C as the funnel corners
	// NOTE   This would lead to a funnel that clips outside of the navmesh
	// NOTE   Instead, the next funnel needs to be created from the funnel's right corner
	// NOTE   _and_ the portal immediately after the funnel's right corner
	int funnel_left_portal_idx = 0;
	int funnel_right_portal_idx = 0;


	VectorCopy(pathfind_result->start_pos, funnel_pos);
	VectorCopy(pathfind_result->path_portals_left[funnel_left_portal_idx], funnel_left_pos);
	VectorCopy(pathfind_result->path_portals_right[funnel_right_portal_idx], funnel_right_pos);
	vec3_t portal_left;
	vec3_t portal_right;


	// The first funnel [0] is defined by the first portal, skip it, start at portal [1]
	for(int i = 1; i < pathfind_result->n_path_portals; i++) {
		VectorCopy(pathfind_result->path_portals_left[i], portal_left);
		VectorCopy(pathfind_result->path_portals_right[i], portal_right);

		Con_Printf("Checking portal: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f) ...\n", 
			portal_left[0], portal_left[1], portal_left[2],
			portal_right[0], portal_right[1], portal_right[2]
		);
		Con_Printf("Against funnel portal: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)\n", 
			funnel_left_pos[0], funnel_left_pos[1], funnel_left_pos[2],
			funnel_pos[0], funnel_pos[1], funnel_pos[2],
			funnel_right_pos[0], funnel_right_pos[1], funnel_right_pos[2]
		);

		int portal_left_in_funnel = pathfind_point_in_funnel(portal_left, funnel_pos, funnel_left_pos, funnel_right_pos);
		int portal_right_in_funnel = pathfind_point_in_funnel(portal_right, funnel_pos, funnel_left_pos, funnel_right_pos);


		Con_Printf("Portal left in funnel: %d\n", portal_left_in_funnel);
		Con_Printf("Portal right in funnel: %d\n", portal_right_in_funnel);


		// If cur portal left point is in cur funnel, narrow the cur funnel


		// If portal overlaps with funnel, narrow and skip
		// 	value is 0 if either point is in funnel
		int portal_funnel_overlap = portal_left_in_funnel * portal_right_in_funnel;

		// If both portal points are outside of funnel, advance to next portal without narrowing funnel
		//  value is -1 iff points are outside of funnel on opposite sides (which implies overlap)

		// FIXME - Bug here, following scenario: 
		// FIXME   right point is to left of funnel --> portal_right_in_funnel == -1
		// FIXME   left point is to right of funnel --> portal_left_in_funnel == 1
		// FIXME   --> portal_funnel_overlap == -1
		// FIXME   --
		// FIXME   Is this possible? This implies a portal has its left / right points flipped, is that possible? It may not be
		// FIXME   Could easily resolve this by instead checking: if(portal_left_in_funnel < 0 && portal_right_in_funnel > 0)
		if(portal_funnel_overlap == -1) {
			continue;
		}
		// If value is 0, one (or both) of the points is in the funnel
		else if(portal_funnel_overlap == 0) {
			// Check the left point
			if(portal_left_in_funnel == 0) {
				VectorCopy(portal_left, funnel_left_pos);
				funnel_left_portal_idx = i;
			}
			// Check the right point
			if(portal_right_in_funnel == 0) {
				VectorCopy(portal_right, funnel_right_pos);
				funnel_right_portal_idx = i;
			}
			continue;
		}
		// Otherwise, cur portal does not overlap cur funnel

		// If portal is outside of funnel to the left, funnel left becomes new funnel pos
		if(portal_left_in_funnel < 0 && portal_left_in_funnel < 0) {
			Con_Printf("Portal left of funnel, adding point: (%.2f, %.2f, %.2f)\n", funnel_left_pos[0], funnel_left_pos[1], funnel_left_pos[2]);

			// Add the left endpoint to the path
			VectorCopy(funnel_left_pos, pathfind_result->path_points[pathfind_result->n_path_points]);

			// Place funnel corner at current left endpoint
			VectorCopy(funnel_left_pos, funnel_pos);
			// Make funnel left / right edges be the very next portal
			funnel_left_portal_idx += 1;
			funnel_right_portal_idx = funnel_left_portal_idx;
		}
		// If portal is outside of funnel to the right, funnel right becomes new funnel pos
		else {
			Con_Printf("Portal right of funnel, adding point: (%.2f, %.2f, %.2f)\n", funnel_right_pos[0], funnel_right_pos[1], funnel_right_pos[2]);

			// Add the right endpoint to the path
			VectorCopy(funnel_right_pos, pathfind_result->path_points[pathfind_result->n_path_points]);

			// Place funnel corner at current right endpoint
			VectorCopy(funnel_right_pos, funnel_pos);
			// Make funnel left / right edges be the very next portal
			funnel_right_portal_idx += 1;
			funnel_left_portal_idx = funnel_right_portal_idx;
		}

		pathfind_result->n_path_points += 1;
		VectorCopy(pathfind_result->path_portals_left[funnel_left_portal_idx], funnel_left_pos);
		VectorCopy(pathfind_result->path_portals_right[funnel_right_portal_idx], funnel_right_pos);
		// Restart the loop from the portal _after_ the funnel's new endpoints
		i = funnel_right_portal_idx;
	}

	// Always add the goal position to the path
	VectorCopy(pathfind_result->goal_pos, pathfind_result->path_points[pathfind_result->n_path_points]);
	pathfind_result->n_path_points += 1;
	// ------------------------------------------------------------------------


	Con_Printf("Path points (%d): [", pathfind_result->n_path_points);
	for(int i = 0; i < pathfind_result->n_path_points; i++) {
		Con_Printf(" (%.2f, %.2f, %.2f),", 
			pathfind_result->path_points[i][0],
			pathfind_result->path_points[i][1],
			pathfind_result->path_points[i][2]
		);
	}
	Con_Printf("]\n");

	// Requires structs: 
	// - path polygon indices from start to goal
	// - path traversal indices

	// Final vector-based path
	// Eventually - List of points that make up the final path
	// Eventually - List of traversals? Actually index of traversal for point i
}


void sv_navmesh_set_test_start_pos(vec3_t pos) {
	Con_Printf("sv_navmesh_set_test_start_pos( vec3(%.2f, %.2f, %.2f))\n", pos[0], pos[1], pos[2]);
	VectorCopy(pos, sv_navmesh_test_start_pos);
	sv_navmesh_test_start_pos_placed = 1;
	sv_navmesh_test_pathfind();
}

void sv_navmesh_set_test_goal_pos(vec3_t pos) {
	Con_Printf("sv_navmesh_set_test_goal_pos( vec3(%.2f, %.2f, %.2f))\n", pos[0], pos[1], pos[2]);
	VectorCopy(pos, sv_navmesh_test_goal_pos);
	sv_navmesh_test_goal_pos_placed = 1;
	sv_navmesh_test_pathfind();
}


void PF_sv_navmesh_set_test_start_pos() {
	float *pos_vec = G_VECTOR(OFS_PARM0);
	sv_navmesh_set_test_start_pos(pos_vec);
}

void PF_sv_navmesh_set_test_goal_pos() {
	float *pos_vec = G_VECTOR(OFS_PARM0);
	sv_navmesh_set_test_goal_pos(pos_vec);
}





// Unit cube vertex locations
#define BOTTOM_LEFT_FRONT  -0.5f, -0.5f,  0.5f
#define BOTTOM_RIGHT_FRONT  0.5f, -0.5f,  0.5f
#define TOP_LEFT_FRONT     -0.5f,  0.5f,  0.5f
#define TOP_RIGHT_FRONT     0.5f,  0.5f,  0.5f
#define BOTTOM_LEFT_BACK   -0.5f, -0.5f, -0.5f
#define BOTTOM_RIGHT_BACK   0.5f, -0.5f, -0.5f
#define TOP_LEFT_BACK      -0.5f,  0.5f, -0.5f
#define TOP_RIGHT_BACK      0.5f,  0.5f, -0.5f

// Unit cube triangles
int unit_cube_n_vertices = 6 * 2 * 3; // (n_faces) * (n_tris_per_face) * (n_verts_per_tri)
float unit_cube_vertices[] = {
	// Front face
	BOTTOM_LEFT_FRONT, BOTTOM_RIGHT_FRONT, TOP_RIGHT_FRONT,
	BOTTOM_LEFT_FRONT, TOP_RIGHT_FRONT, TOP_LEFT_FRONT,
	// Back face
	BOTTOM_LEFT_BACK, BOTTOM_RIGHT_BACK, TOP_RIGHT_BACK,
	BOTTOM_LEFT_BACK, TOP_RIGHT_BACK, TOP_LEFT_BACK,
	// Left face
	BOTTOM_LEFT_BACK, BOTTOM_LEFT_FRONT, TOP_LEFT_FRONT,
	BOTTOM_LEFT_BACK, TOP_LEFT_FRONT, TOP_LEFT_BACK,
	// Right face
	BOTTOM_RIGHT_BACK, BOTTOM_RIGHT_FRONT, TOP_RIGHT_FRONT,
	BOTTOM_RIGHT_BACK, TOP_RIGHT_FRONT, TOP_RIGHT_BACK,
	// Top face
	TOP_LEFT_BACK, TOP_RIGHT_BACK, TOP_RIGHT_FRONT,
	TOP_LEFT_BACK, TOP_RIGHT_FRONT, TOP_LEFT_FRONT,
	// Bottom face
	BOTTOM_LEFT_BACK, BOTTOM_RIGHT_BACK, BOTTOM_RIGHT_FRONT,
	BOTTOM_LEFT_BACK, BOTTOM_RIGHT_FRONT, BOTTOM_LEFT_FRONT,
};


void set_transparent_drawmodes() {
	// return;
	// ------------------------------------------------------------------------
	// Transparent drawmodes
	// ------------------------------------------------------------------------
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_CULL_FACE);
	sceGuEnable(GU_BLEND);
	sceGuEnable(GU_ALPHA_TEST);
	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuDepthMask(GU_TRUE);
	// sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0xFFFFFFFF);
	// ------------------------------------------------------------------------
}

void end_transparent_drawmodes() {
	// return;
	// ------------------------------------------------------------------------
	// End Transparent drawmode
	// ------------------------------------------------------------------------
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDepthMask(GU_FALSE);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	// sceGuDisable(GU_ALPHA_TEST);
	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_TEXTURE_2D);
	// ------------------------------------------------------------------------
}


// TODO - Need a util function that draws a box with a specific color and transparency
void cl_draw_box(vec3_t pos, vec3_t mins, vec3_t maxs, vec3_t color, float alpha) {

	sceGumPushMatrix();
	sceGumLoadIdentity();

	vec3_t box_size;
	vec3_t box_ofs; // Position of box relative to global `pos`
	VectorAdd(mins, maxs, box_ofs);
	VectorScale(box_ofs, 2, box_ofs); // box_ofs = (mins + maxs) / 2
	VectorSubtract(maxs, mins, box_size); // box_size = maxs - mins


	vec3_t box_pos;
	VectorAdd(pos, box_ofs, box_pos); // box_pos = pos + box_ofs

	ScePspFVector3 psp_box_scale;
	psp_box_scale.x = box_size[0];
	psp_box_scale.y = box_size[1];
	psp_box_scale.z = box_size[2];

	ScePspFVector3 psp_box_pos;
	psp_box_pos.x = box_pos[0];
	psp_box_pos.y = box_pos[1];
	psp_box_pos.z = box_pos[2];

	sceGumTranslate(&psp_box_pos);
	sceGumScale(&psp_box_scale);


	set_transparent_drawmodes();

	sceGuColor(GU_COLOR( color[0], color[1], color[2], alpha));
	sceGumDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF|GU_TRANSFORM_3D, unit_cube_n_vertices, NULL, unit_cube_vertices);

	end_transparent_drawmodes();
	sceGumPopMatrix();
}

void cl_draw_edge(vec3_t pos_a, vec3_t pos_b, float width, vec3_t color, float alpha) {

	vec3_t ofs_a_to_b;
	VectorSubtract(pos_b, pos_a, ofs_a_to_b);
	float dist_a_to_b = VectorLength(ofs_a_to_b);

	vec3_t dir_a_to_b;
	VectorCopy(ofs_a_to_b, dir_a_to_b);
	VectorNormalizeFast(dir_a_to_b);

	vec3_t midpoint_pos;
	VectorAdd(pos_a, pos_b, midpoint_pos);
	VectorScale(midpoint_pos, 0.5, midpoint_pos);


	// // Start Debug
	// vec3_t debug_mins = {-2,-2,-2};
	// vec3_t debug_maxs = {2,2,2};
	// vec3_t debug_color = {1,1,1};
	// cl_draw_box(pos_a, debug_mins, debug_maxs, debug_color, 1);
	// cl_draw_box(pos_b, debug_mins, debug_maxs, debug_color, 1);
	// cl_draw_box(midpoint_pos, debug_mins, debug_maxs, debug_color, 1);
	// // End Debug


	vec3_t up = {0.0f, 0.0f, 1.0f};
	vec3_t right;
	CrossProduct(up, dir_a_to_b, right);
	VectorNormalizeFast(right);

	CrossProduct(dir_a_to_b, right, up);
	VectorNormalizeFast(up);


	// matrix4x4 transform = {
	// 	{dir_a_to_b[0] * scale_x,  right[0] * scale_x, up[0] * scale_z, midpoint_pos[0]},
	// 	{dir_a_to_b[1] * scale_x,  right[1] * scale_y, up[1] * scale_z, midpoint_pos[1]},
	// 	{dir_a_to_b[2] * scale_x,  right[2] * scale_z, up[2] * scale_z, midpoint_pos[2]},
	// 	{				    0.0f,  				 0.0f,            0.0f,            1.0f}
	// }
	vec3_t scale = {dist_a_to_b, width, width};

	ScePspFMatrix4 transform;
	transform.x.x = dir_a_to_b[0] * scale[0];
	transform.x.y = dir_a_to_b[1] * scale[0];
	transform.x.z = dir_a_to_b[2] * scale[0];
	transform.x.w = 0.0f;
	transform.y.x = right[0] * scale[1]; 
	transform.y.y = right[1] * scale[1]; 
	transform.y.z = right[2] * scale[1];	
	transform.y.w = 0.0f; 
	transform.z.x = up[0] * scale[2]; 
	transform.z.y = up[1] * scale[2]; 
	transform.z.z = up[2] * scale[2]; 
	// --
	// transform.x.x = 		1.0f;
	// transform.x.y = 		0.0f;
	// transform.x.z = 		0.0f;
	// transform.x.w = 0.0f;
	// transform.y.x = 		0.0f;
	// transform.y.y = 		1.0f;
	// transform.y.z = 		0.0f;
	// transform.y.w = 0.0f;
	// transform.z.x = 		0.0f;
	// transform.z.y = 		0.0f;
	// transform.z.z = 		1.0f;
	// transform.z.w = 0.0f;
	// --
	transform.w.x = midpoint_pos[0];
	transform.w.y = midpoint_pos[1];
	transform.w.z = midpoint_pos[2];
	transform.w.w = 1.0f;

	sceGumPushMatrix();
	// sceGumLoadIdentity();
	// sceGumMultMatrix(&transform);
	sceGumLoadMatrix(&transform);

	set_transparent_drawmodes();

	sceGuColor(GU_COLOR( color[0], color[1], color[2], alpha));
	sceGumDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF|GU_TRANSFORM_3D, unit_cube_n_vertices, NULL, unit_cube_vertices);

	end_transparent_drawmodes();
	sceGumPopMatrix();
}



// Edit - nvm, this is overcomplicated...
// // Holds a pointer to a list of triangulated vertices for each navmesh polygon
// float *cl_draw_navmesh_polygons_cache[NAVMESH_MAX_POLYGONS]; // FIXME - need to clear this array to nullpointers when clearing the navmesh
// void cl_draw_polygon(int poly_idx, vec3_t color, float alpha) {
// 	// This is tricky... need to triangulate the polygon, and stash them somewhere... but we can't...
// 	// Unless... we create a cache array in which to stash it for the polygon...

// 	if(cl_draw_navmesh_polygons_cache[poly_idx] == NULL) {

// 		int n_polygon_verts = sv_navmesh_polies[poly_idx].n_verts;
// 		int n_tris = n_verts - 2; // First 3 verts generate 1 triangle, every vert therafter generates 1 triangle
// 		cl_draw_navmesh_polygons_cache[poly_idx] = (float*) malloc(n_tris * 3 * 3 * sizeof(float)); // n_tris * n_vert_per_tri * n_floats_per_vert * sizeof(float)

// 		// ------------------------------------------------------------

// 		// ------------------------------------------------------------

// 	}
// }



// NOTE - This reaches into sv_navmesh structs from the client to draw
// FIXME - This should not be called on non-host clients
void cl_sv_draw_navmesh_test_ents() {

	vec3_t ent_mins = {-16, -16, -32};
	vec3_t ent_maxs = {16, 16, 40};
	float alpha = 0.4;

	if(sv_navmesh_test_start_pos_placed) {
		vec3_t color = {0,1,0};
		cl_draw_box(sv_navmesh_test_start_pos, ent_mins, ent_maxs, color, alpha);
	}
	if(sv_navmesh_test_goal_pos_placed) {
		vec3_t color = {1,0,0};
		cl_draw_box(sv_navmesh_test_goal_pos, ent_mins, ent_maxs, color, alpha);
	}
}

// NOTE - This reaches into sv_navmesh structs from the client to draw
// FIXME - This should not be called on non-host clients
void cl_sv_draw_navmesh() {
	if(cl_draw_navmesh_n_poly_verts == 0) {
		for(int i = 0; i < sv_navmesh_n_polies; i++) {
			int n_verts = sv_navmesh_polies[i].n_verts-2;
			// For this convex n-gon...
			// Iterate through verts drawing triangles for: [[0,1,2], [0,2,3], [0,3,4], ..., [0, n_verts-2, n_verts-1] ]
			// This implicitly handles drawing an n-gon by triangulating it
			for(int j = 1; j <= n_verts; j++) {
				// Emit triangle for vertices [0, j, j+1]
				VectorCopy( sv_navmesh_verts[sv_navmesh_polies[i].vert_idxs[0]], (&cl_draw_navmesh_poly_verts[3 * cl_draw_navmesh_n_poly_verts]));
				cl_draw_navmesh_n_poly_verts++;
				VectorCopy( sv_navmesh_verts[sv_navmesh_polies[i].vert_idxs[j]], (&cl_draw_navmesh_poly_verts[3 * cl_draw_navmesh_n_poly_verts]));
				cl_draw_navmesh_n_poly_verts++;
				VectorCopy( sv_navmesh_verts[sv_navmesh_polies[i].vert_idxs[j+1]], (&cl_draw_navmesh_poly_verts[3 * cl_draw_navmesh_n_poly_verts]));
				cl_draw_navmesh_n_poly_verts++;
			}
		}
	}

	if(cl_draw_navmesh_n_edge_verts == 0) {

		for(int i = 0; i < sv_navmesh_n_polies; i++) {
			int poly_n_verts = sv_navmesh_polies[i].n_verts;
			if(poly_n_verts == 3 || poly_n_verts == 4) {

				for(int j = 0; j < poly_n_verts; j++) {
					int edge_start_vert_idx = sv_navmesh_polies[i].vert_idxs[j];
					int edge_end_vert_idx = sv_navmesh_polies[i].vert_idxs[(j + 1) % poly_n_verts];

					VectorCopy( sv_navmesh_verts[edge_start_vert_idx], (&cl_draw_navmesh_edge_verts[3 * cl_draw_navmesh_n_edge_verts]));
					cl_draw_navmesh_n_edge_verts++;
					VectorCopy( sv_navmesh_verts[edge_end_vert_idx], (&cl_draw_navmesh_edge_verts[3 * cl_draw_navmesh_n_edge_verts]));
					cl_draw_navmesh_n_edge_verts++;
				}
			}
		}
	}


	sceGumPushMatrix();
	sceGumLoadIdentity();



	set_transparent_drawmodes();

	// ------------- Draw navmesh polygons ---------------------
	// sceGuColor(GU_COLOR(1.0,0.0,0.0,0.5)); // RGBA red
	sceGuColor(GU_COLOR( 0.2, 0.8, 0.2, 0.1)); // RGBA transparent green
	sceGumDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF|GU_TRANSFORM_3D, cl_draw_navmesh_n_poly_verts, NULL, cl_draw_navmesh_poly_verts);
	// ---------------------------------------------------------

	// ------------- Draw navmesh edges ------------------------
	sceGuColor(GU_COLOR( 0.5, 0.5, 0.5, 0.9)); // RGBA transparent gray
	sceGumDrawArray(GU_LINES, GU_VERTEX_32BITF|GU_TRANSFORM_3D, cl_draw_navmesh_n_edge_verts, NULL, cl_draw_navmesh_edge_verts);
	// ---------------------------------------------------------


	end_transparent_drawmodes();
	sceGumPopMatrix();


	// TODO - Draw door polygon as red: [0.8,0.2,0.2], alpha=0.1
	// TODO - Skip drawing door polygons above?


	// ------------- Draw navmesh verts ------------------------
	vec3_t vert_color = {0, 0, 1};
	float vert_alpha = 0.9;
	vec3_t vert_mins = {-2,-2,-2};
	vec3_t vert_maxs = { 2, 2, 2};

	for(int i = 0; i < sv_navmesh_n_verts; i++) {
		cl_draw_box(sv_navmesh_verts[i], vert_mins, vert_maxs, vert_color, vert_alpha);
	}
	// ---------------------------------------------------------

	// // ---------------------------------------------------------
	// // Draw polygon shared edge left / right neighbors
	// // ---------------------------------------------------------
	// for(int i = 0; i < sv_navmesh_n_polies; i++) {
	// 	vec3_t left_pos_ofs = {0,0,-2};
	// 	vec3_t right_pos_ofs = {0,0,2};
	// 	float thickness = 4;
	// 	float alpha = 0.4;
	// 	vec3_t portal_left_color = {0.5, 0.5, 1.0};
	// 	vec3_t portal_right_color = {0.5, 1.0, 0.5};
	// 	for(int j = 0; j < sv_navmesh_polies[i].n_connected_polies; j++) {
	// 		vec3_t left_pos;
	// 		vec3_t right_pos;
	// 		VectorCopy(sv_navmesh_verts[sv_navmesh_polies[i].connected_polies_left_vert[j]], left_pos);
	// 		VectorCopy(sv_navmesh_verts[sv_navmesh_polies[i].connected_polies_right_vert[j]], right_pos);
	// 		// Move the points 10% of the way towards the center of the edge
	// 		vec3_t edge_center;
	// 		VectorAdd(left_pos, right_pos, edge_center);
	// 		VectorScale(edge_center, 0.5, edge_center);
	// 		VectorLerp(left_pos, 0.10, edge_center, left_pos);
	// 		VectorLerp(right_pos, 0.10, edge_center, right_pos);
	// 		// Move the points 5% of the way towards the center of the polygon to disambiguate
	// 		VectorLerp(left_pos, 0.10, sv_navmesh_polies[i].center, left_pos);
	// 		VectorLerp(right_pos, 0.10, sv_navmesh_polies[i].center, right_pos);
	// 		VectorAdd(left_pos, left_pos_ofs, left_pos);
	// 		VectorAdd(right_pos, right_pos_ofs, right_pos);
	// 		cl_draw_box(left_pos, vert_mins, vert_maxs, portal_left_color, alpha);
	// 		cl_draw_box(right_pos, vert_mins, vert_maxs, portal_right_color, alpha);
	// 	}
	// }
	// // ---------------------------------------------------------


	cl_sv_draw_navmesh_test_ents();

	// -------------------- Draw start / end polygon ------------------

	if(sv_navmesh_test_start_pos_placed && sv_navmesh_test_pathfind_result.start_poly_idx >= 0) {
		float thickness = 2;
		vec3_t color = {0,1,0};
		float alpha = 0.2;
		int n_verts = sv_navmesh_polies[sv_navmesh_test_pathfind_result.start_poly_idx].n_verts;

		for(int i = 0; i < n_verts; i++) {
			int vert_a_idx = sv_navmesh_polies[sv_navmesh_test_pathfind_result.start_poly_idx].vert_idxs[i];
			int vert_b_idx = sv_navmesh_polies[sv_navmesh_test_pathfind_result.start_poly_idx].vert_idxs[(i + 1) % n_verts];
			cl_draw_edge(sv_navmesh_verts[vert_a_idx], sv_navmesh_verts[vert_b_idx], thickness, color, alpha );
		}
	}
	if(sv_navmesh_test_goal_pos_placed && sv_navmesh_test_pathfind_result.goal_poly_idx >= 0) {
		float thickness = 2;
		vec3_t color = {1,0,0};
		float alpha = 0.2;
		int n_verts = sv_navmesh_polies[sv_navmesh_test_pathfind_result.goal_poly_idx].n_verts;

		for(int i = 0; i < n_verts; i++) {
			int vert_a_idx = sv_navmesh_polies[sv_navmesh_test_pathfind_result.goal_poly_idx].vert_idxs[i];
			int vert_b_idx = sv_navmesh_polies[sv_navmesh_test_pathfind_result.goal_poly_idx].vert_idxs[(i + 1) % n_verts];
			cl_draw_edge(sv_navmesh_verts[vert_a_idx], sv_navmesh_verts[vert_b_idx], thickness, color, alpha );
		}
	}


	if(sv_navmesh_test_start_pos_placed && sv_navmesh_test_goal_pos_placed) {
		if(sv_navmesh_test_pathfind_result.n_path_polygons > 1) {
			float thickness = 3.0f;
			vec3_t color = {1, 1, 0};
			float alpha = 0.8;

			for(int i = 0; i < sv_navmesh_test_pathfind_result.n_path_polygons - 1; i++) {
				int poly_a_idx = sv_navmesh_test_pathfind_result.path_polygons[i];
				int poly_b_idx = sv_navmesh_test_pathfind_result.path_polygons[i + 1];
				cl_draw_edge(sv_navmesh_polies[poly_a_idx].center, sv_navmesh_polies[poly_b_idx].center, thickness, color, alpha );
			}
		}
		if(sv_navmesh_test_pathfind_result.n_path_portals > 0) {
			float thickness = 4;
			vec3_t color = {0.5, 0.5, 1.0};
			float alpha = 0.4;
			vec3_t draw_ofs = {0,0,5};
			vec3_t portal_left_color = {0.5, 0.5, 1.0};
			vec3_t portal_right_color = {0.5, 1.0, 0.5};


			for(int i = 0; i < sv_navmesh_test_pathfind_result.n_path_portals; i++) {
				vec3_t portal_left;
				vec3_t portal_right;
				VectorAdd(sv_navmesh_test_pathfind_result.path_portals_left[i], draw_ofs, portal_left);
				VectorAdd(sv_navmesh_test_pathfind_result.path_portals_right[i], draw_ofs, portal_right);

				cl_draw_box(portal_left, vert_mins, vert_maxs, portal_left_color, alpha);
				cl_draw_box(portal_right, vert_mins, vert_maxs, portal_right_color, alpha);
				cl_draw_edge( portal_left, portal_right, thickness, color, alpha);
			}
		}

		if(sv_navmesh_test_pathfind_result.n_path_points > 0) {
			float thickness = 4;
			vec3_t color = {1, 0, 0};
			float alpha = 0.4;
			vec3_t draw_ofs = {0,0,10};

			vec3_t cur_point;
			vec3_t prev_point;


			VectorAdd(sv_navmesh_test_pathfind_result.start_pos, draw_ofs, prev_point);
			VectorAdd(sv_navmesh_test_pathfind_result.path_points[0], draw_ofs, cur_point);
			cl_draw_box( prev_point, vert_mins, vert_maxs, color, alpha);


			for(int i = 0; i < sv_navmesh_test_pathfind_result.n_path_points; i++) {
				VectorAdd(sv_navmesh_test_pathfind_result.path_points[i], draw_ofs, cur_point);
				cl_draw_box( cur_point, vert_mins, vert_maxs, color, alpha);
				cl_draw_edge( prev_point, cur_point, thickness, color, alpha);
				VectorCopy(cur_point, prev_point);
			}
		}
	}	
	// ----------------------------------------------------------------
}



#endif // NAVMESH_DEBUG_DRAW



//
// Resets all navmesh data structures to empty navmesh
// 
void sv_navmesh_clear_navmesh() {
	// for(int i = 0; i < NAVMESH_MAX_VERTICES; i++) {
	// 	VectorClear(sv_navmesh_verts[i]);
	// }
	memset(sv_navmesh_verts, 0, sizeof(vec3_t) * NAVMESH_MAX_VERTICES);

	for(int i = 0; i < NAVMESH_MAX_POLYGONS; i++) {
		for(int j = 0; j < NAVMESH_MAX_VERTICES_PER_POLY; j++) {
			sv_navmesh_polies[i].vert_idxs[j] = -1;
			sv_navmesh_polies[i].connected_polies[j] = -1;
			sv_navmesh_polies[i].connected_polies_left_vert[j] = -1;
			sv_navmesh_polies[i].connected_polies_right_vert[j] = -1;
			// sv_navmesh_polies[i].connected_polies_neighbor_edge_index[j] = -1;
		}
		sv_navmesh_polies[i].n_verts = 0;
		sv_navmesh_polies[i].doortarget = NULL;
		for(int j = 0; j < NAVMESH_MAX_TRAVERSALS_PER_POLY; j++) {
			sv_navmesh_polies[i].connected_traversals[j] = -1;
		}
		sv_navmesh_polies[i].n_connected_traversals = 0;
		VectorClear(sv_navmesh_polies[i].center);
	}
	sv_navmesh_n_verts = 0;
	sv_navmesh_n_polies = 0;
}






// 
// qsort Container struct to sort vertex indices by angle
// 
typedef struct {
	int vert_idx;
	float angle;
} sort_poly_verts_struct_t;

//
// Comparison function used by sv_navmesh_sort_poly_verts_ccw's qsort_r(...) invocation
//
int sv_navmesh_sort_poly_verts_ccw_comparator(const void *a, const void *b) {
	const sort_poly_verts_struct_t *vert_a = (const sort_poly_verts_struct_t*) a;
	const sort_poly_verts_struct_t *vert_b = (const sort_poly_verts_struct_t*) b;

	// When values are same, qsort treats order as undefined, so catch them here
	if(vert_a->angle <= vert_b->angle) {
		return -1;
	}
	else {
		return 1;
	}
}


// 
// Sorts vert indices in `vert_idxs` in-place from smallest angle in vert_angles to largest
// 
void sv_navmesh_sort_poly_verts_ccw( int *vert_idxs, const int n_verts, const float *vert_angles) {
	// Con_Printf("qsort -- before\n");
	// Con_Printf("Num Verts: %d\n", n_verts);
	// Con_Printf("Verts before: ");
	// for(int i = 0; i < n_verts; i++) { Con_Printf("(%d, %.2f), ", vert_idxs[i], vert_angles[i]); }
	// Con_Printf("\n");

	sort_poly_verts_struct_t vert_sort_structs[n_verts];
	for(int i = 0; i < n_verts; i++) {
		vert_sort_structs[i].vert_idx = vert_idxs[i];
		vert_sort_structs[i].angle = vert_angles[i];
	}
	qsort(vert_sort_structs, n_verts, sizeof(sort_poly_verts_struct_t), sv_navmesh_sort_poly_verts_ccw_comparator);
	for(int i = 0; i < n_verts; i++) {
		vert_idxs[i] = vert_sort_structs[i].vert_idx;
	}

	// Con_Printf("Verts after: ");
	// for(int i = 0; i < n_verts; i++) { Con_Printf("(%d, %.2f), ", vert_idxs[i], vert_angles[i]); }
	// Con_Printf("\n");
}



// 
// Calculates and stores polygon centers
//
void sv_nvmesh_calc_poly_centers() {
	for(int i = 0; i < sv_navmesh_n_polies; i++) {
		int n_verts = sv_navmesh_polies[i].n_verts;
		if(n_verts < 3 || n_verts > NAVMESH_MAX_VERTICES_PER_POLY) {
			Con_Printf("WARNING: Polygon %d has %d verts.\n", i, n_verts);
			// TODO - clear navmesh, skip polygon?
			break;
		}

		vec3_t poly_center;
		VectorClear(poly_center);
		for(int j = 0; j < n_verts; j++) {
			int vert_idx = sv_navmesh_polies[i].vert_idxs[j];
			VectorAdd(poly_center, sv_navmesh_verts[vert_idx], poly_center);
		}
		VectorScale(poly_center, 1.0 / n_verts, sv_navmesh_polies[i].center);
	}	
}


//
// Orders polygon vertices to be in CCW winding order (when viewed from above)
// NOTE - This assumes all polygon centers have already been calculated via `sv_nvmesh_calc_poly_centers`
// 
void sv_navmesh_sort_navmesh_poly_verts_ccw() {
	for(int i = 0; i < sv_navmesh_n_polies; i++) {
		int n_verts = sv_navmesh_polies[i].n_verts;

		Con_Printf("/// Polygon %d  n_verts: %d, min verts: %d, max vers: %d\n", i, n_verts, 3, NAVMESH_MAX_VERTICES_PER_POLY);


		if(n_verts < 3) {
			Con_Printf("Polygon n_verts %d somehow less than %d\n", n_verts, 3);
		}
		// Okay this one runs... wtf? (3 > 4) is somehow true
		if(n_verts > NAVMESH_MAX_VERTICES_PER_POLY) {
			Con_Printf("Polygon n_verts %d somehow greater than %d\n", n_verts, NAVMESH_MAX_VERTICES_PER_POLY);
		}


		// FIXME - Why is this condition being triggerd on polygon 0 which has 3 verts? weird...
		if(n_verts < 3 || n_verts > NAVMESH_MAX_VERTICES_PER_POLY) {
			Con_Printf("WARNING: Polygon %d has %d verts.\n", i, n_verts);
			// TODO - clear navmesh, skip polygon?
			break;
		}


		float vert_angles[n_verts];
		for(int j = 0; j < n_verts; j++) {
			// Calculate angle between (center -> vertex) and (center -> origin)
			int vert_idx = sv_navmesh_polies[i].vert_idxs[j];
			vert_angles[j] = atan2(sv_navmesh_verts[vert_idx][1] - sv_navmesh_polies[i].center[1], sv_navmesh_verts[vert_idx][0] - sv_navmesh_polies[i].center[0]);
		}

		// Con_Printf("Poly %d Verts (%d) before sorting: [ %d, %d, %d, %d ]\n", i, 
		// 	n_verts, sv_navmesh_polies[i].vert_idxs[0], sv_navmesh_polies[i].vert_idxs[1], sv_navmesh_polies[i].vert_idxs[2], sv_navmesh_polies[i].vert_idxs[3]
		// );
		// Con_Printf("\tPoly %d Vert (%d) angles: [ %f, %f, %f, %f ]\n", i, 
		// 	n_verts, vert_angles[0], vert_angles[1], vert_angles[2], vert_angles[3]
		// );

		// Sort vertices based on angle from least to greatest
		sv_navmesh_sort_poly_verts_ccw( sv_navmesh_polies[i].vert_idxs, n_verts, vert_angles);

		// Con_Printf("\tPoly %d Verts (%d) after sorting: [ %d, %d, %d, %d ]\n", i, 
		// 	n_verts, sv_navmesh_polies[i].vert_idxs[0], sv_navmesh_polies[i].vert_idxs[1], sv_navmesh_polies[i].vert_idxs[2], sv_navmesh_polies[i].vert_idxs[3]
		// );

		// Verify polygon is concave
		for(int j = 0; j < n_verts; j++) {
			int prev_vert_idx = (j + n_verts - 1) % n_verts;
			int vert_idx = j;
			int next_vert_idx = (j + 1) % n_verts;
			vec3_t *prev_vert_pos = &sv_navmesh_verts[prev_vert_idx];
			vec3_t *vert_pos = &sv_navmesh_verts[vert_idx];
			vec3_t *next_vert_pos = &sv_navmesh_verts[next_vert_idx];

			// For polygon to be concave, next_vert _must_ be to the left of the line [prev_vert -> vert]
			if(pathfind_point_is_to_left(*prev_vert_pos, *vert_pos, *next_vert_pos) <= 0) {
				Con_Printf("WARNING: Polygon %d is not concave.\n", i);
				// TODO - Skip polygon? Stop? Error out?
			}
		}

		// Set unused vertex slots to: -1
		for(int j = n_verts; j < NAVMESH_MAX_TRAVERSALS_PER_POLY; j++) {
			sv_navmesh_polies[i].vert_idxs[j] = -1;
		}
	}
}


//
// Precomputes navmesh polygon adjacencies for faster pathfinding
// NOTE - This assumes all polygon centers have already been calculated via `sv_nvmesh_calc_poly_centers`
//
void sv_navmesh_calc_connected_polies() {
	for(int i = 0; i < sv_navmesh_n_polies; i++) {
		for(int j = i + 1; j < sv_navmesh_n_polies; j++) {

			// Check if poly j shares an edge with poly i
			//
			// Edge case: where two opposite corners (two vertices that do not share an edge) of a quad
			// are shared with another polygon. This implies one of the polygons is non-convex, so not handled here
			// 
			// Check if poly j shares at least 2 vertices with poly i
			//		if two quads share 4: they are the same 						-> bad topology
			//		if two quads share 3: one of them is convex, one is concave 	-> bad topology
			//		if a quad shares 3 with a tri: the tri is within the quad 		-> bad topology
			//		if two tris share 3: they are the same							-> bad topology
			// 
			int n_shared_verts = 0;
			int shared_vert_idxs[2] = {-1, -1};
			// int poly_i_shared_verts_index[2] = {-1, -1}; // The index of the vert in the ith polygon's vertex list
			// int poly_j_shared_verts_index[2] = {-1, -1}; // The index of the vert in the jth polygon's vertex list

			for(int k = 0; k < sv_navmesh_polies[i].n_verts; k++) {
				for(int l = 0; l < sv_navmesh_polies[j].n_verts; l++) {
					if(sv_navmesh_polies[i].vert_idxs[k] == sv_navmesh_polies[j].vert_idxs[l]) {
						if(n_shared_verts >= 2) {
							// If we have found more more verts than should be allowed, count them for error printing, but skip
							n_shared_verts++;
							continue;
						}

						shared_vert_idxs[n_shared_verts] = sv_navmesh_polies[i].vert_idxs[k];
						// poly_i_shared_verts_index[n_shared_verts] = k;
						// poly_j_shared_verts_index[n_shared_verts] = l;
						n_shared_verts++;
					}
				}
			}

			if(n_shared_verts > 2) {
				Con_Printf("Warning: Navmesh polygons %d and %d share %d vertices, which is more than 2. Check navmesh topology.\n", i, j, n_shared_verts);
				continue;
			}

			if(n_shared_verts < 2) {
				continue;
			}

			// Now we know that polygons i and j share exactly 2 vertices
			// Mark polygons as connected to each other via a edge / portal

			if(sv_navmesh_polies[i].n_connected_polies >= 4) {
				Con_Printf("Warning: Navmesh polygon %d shares an edge with more than 4 other polygons. Check navmesh topology.\n", i);
				break;
			}
			if(sv_navmesh_polies[j].n_connected_polies >= 4) {
				Con_Printf("Warning: Navmesh polygon %d shares an edge with more than 4 other polygons. Check navmesh topology.\n", j);
				break;
			}


			int poly_i_neighbor_idx = sv_navmesh_polies[i].n_connected_polies;
			int poly_j_neighbor_idx = sv_navmesh_polies[j].n_connected_polies;
			sv_navmesh_polies[i].connected_polies[poly_i_neighbor_idx] = j;
			sv_navmesh_polies[j].connected_polies[poly_j_neighbor_idx] = i;

			//=======================================================================================
			// Calculating shared edge left / right vertices
			//
			// (Calculated from the perspective of moving from poly[i] to poly[j])
			//=======================================================================================

			// If `shared_vert_idxs[0]` is to the right of the line connecting polygon i --> j, flip the order so it's the left vertex
			if(pathfind_point_is_to_left(sv_navmesh_polies[i].center, sv_navmesh_polies[j].center, sv_navmesh_verts[shared_vert_idxs[0]]) < 0) {
				// Swap 'em
				int temp = shared_vert_idxs[0];
				shared_vert_idxs[0] = shared_vert_idxs[1];
				shared_vert_idxs[1] = temp;

				// // Swap 'em
				// temp = poly_i_shared_verts_index[0];
				// poly_i_shared_verts_index[0] = poly_i_shared_verts_index[1];
				// poly_i_shared_verts_index[1] = temp;

				// // Swap 'em
				// temp = poly_j_shared_verts_index[0];
				// poly_j_shared_verts_index[0] = poly_j_shared_verts_index[1];
				// poly_j_shared_verts_index[1] = temp;
			}

			// Now we've guaranteed that `shared_vert_idxs[0]` lies to the left of the line from center of polygon i --> center of polygon j

			// From polygon i to j, assign left/right edges
			sv_navmesh_polies[i].connected_polies_left_vert[poly_i_neighbor_idx] = shared_vert_idxs[0];
			sv_navmesh_polies[i].connected_polies_right_vert[poly_i_neighbor_idx] = shared_vert_idxs[1];


			// FIXME - We don't actually know which connected polygon index for j... do we? Oh wait it appears that we do... we assign it above


			// From polygon j to i, assign them with the order flipped
			sv_navmesh_polies[j].connected_polies_left_vert[poly_j_neighbor_idx] = shared_vert_idxs[1];
			sv_navmesh_polies[j].connected_polies_right_vert[poly_j_neighbor_idx] = shared_vert_idxs[0];

			// // For polygon i, record which of polygon j's edges we are crossing to get into polygon i
			// sv_navmesh_polies[i].connected_polies_neighbor_edge_index[sv_navmesh_polies[i].n_connected_polies] = poly_j_shared_verts_index[0];
			// // For polygon j, record which of polygon i's edges we are crossing to get into polygon j
			// sv_navmesh_polies[j].connected_polies_neighbor_edge_index[sv_navmesh_polies[j].n_connected_polies] = poly_i_shared_verts_index[1];

			sv_navmesh_polies[i].n_connected_polies++;
			sv_navmesh_polies[j].n_connected_polies++;
		}
	}
}



//
// Given a open-for-reading file handle `file`
// Reads all characters from current position to the next '\n'
//
// Returns length of line read (excluding null character)
//
int file_read_line(int file, char *str_out, int str_out_len) {
	int count;
	char buffer;

	count = Sys_FileRead(file, &buffer, 1);
	// Skip carriage return
	if (count && buffer == '\r') {
		count = Sys_FileRead(file, &buffer, 1);	// skip
	}
	// EndOfFile
	if (!count)	{
		return 0;
	}

	int i = 0;
	while (count && buffer != '\n') {
		// no place for character in temp string
		if (i < str_out_len-1) {
			str_out[i++] = buffer;
		}

		// read next character
		count = Sys_FileRead(file, &buffer, 1);
		// carriage return
		if (count && buffer == '\r') {
			count = Sys_FileRead(file, &buffer, 1);	// skip
		}
	}
	// Returns n chars read, not including null character
	str_out[i] = 0;
	return i;
}


//
// Parse the navmesh data structure from "{map_name}.nav" file
//
void sv_navmesh_load_from_file() {
	int file = 0;
	Sys_FileOpenRead( va( "%s/maps/%s.nav", com_gamedir, sv.name), &file);

	const int tmp_str_len = 64;
	char tmp_str[tmp_str_len];

	if(file == -1) {
		Con_DPrintf("No navmesh file \"%s/maps/%s.nav\" found.", com_gamedir, sv.name);
		return;
	}

	// TODO - Clear navmesh data structures
	// TODO - Check navmesh file version number

	int tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
	

	int v_major;
	int v_minor;
	int v_patch;
	if(tmp_line_len <= 0) {
		Con_Printf("Error reading navmesh file. Could not read version line\n");
		return;
	}

	sscanf(tmp_str, "%d.%d.%d", &v_major, &v_minor, &v_patch);
	if(!(v_major == 1 && v_minor == 0 && v_patch == 0)) {
		Con_Printf("Unsupported navmesh file version %d.%d.%d\n", v_major, v_minor, v_patch);
		return;

	}
	Con_Printf("Loaded navmesh file version: %d, %d, %d\n", v_major, v_minor, v_patch);

	tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
	if(tmp_line_len <= 0) {
		Con_Printf("Error reading navmesh file. Could not read n_vertices\n");
		return;
	}
	int n_vertices = -1;
	sscanf(tmp_str, "%d", &n_vertices);
	Con_Printf("n_vertices: %d\n", n_vertices);
	if(n_vertices <= 0 || n_vertices >= NAVMESH_MAX_VERTICES) {
		Con_Printf("Error reading navmesh file. Invalid n_vertices: %d\n", n_vertices);
		return;
	}

	sv_navmesh_n_verts = n_vertices;


	for(int i = 0; i < n_vertices; i++) {
		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		if(tmp_line_len <= 0) {
			Con_Printf("Error reading navmesh file. Could not read vertex %d pos\n", i);
			return;
		}
		vec3_t vertex_pos;
		W_stov( tmp_str, vertex_pos);
		Con_Printf("Vertex %d pos: (%.2f, %.2f, %.2f)\n", i, vertex_pos[0], vertex_pos[1], vertex_pos[2]);

		VectorCopy( vertex_pos, sv_navmesh_verts[i]);
	}


	tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
	if(tmp_line_len <= 0) {
		Con_Printf("Error reading navmesh file. Could not read n_polygons\n");
		return;
	}
	int n_polygons = -1;
	sscanf(tmp_str, "%d", &n_polygons);
	Con_Printf("n_polygons: %d\n", n_polygons);
	if(n_polygons <= 0 || n_polygons >= NAVMESH_MAX_POLYGONS) {
		Con_Printf("Error reading navmesh file. Invalid n_polygons: %d\n", n_polygons);
		return;
	}

	sv_navmesh_n_polies = n_polygons;


	for(int i = 0; i < n_polygons; i++) {
		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		// TODO - If failed to read line, error out here
		int polygon_n_verts = -1;
		sscanf(tmp_str, "%d", &polygon_n_verts);

		// TODO - If failed to read vert indices, error out
		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		int polygon_vert_0_idx = -1;
		sscanf(tmp_str, "%d", &polygon_vert_0_idx);

		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		int polygon_vert_1_idx = -1;
		sscanf(tmp_str, "%d", &polygon_vert_1_idx);

		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		int polygon_vert_2_idx = -1;
		sscanf(tmp_str, "%d", &polygon_vert_2_idx);

		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		int polygon_vert_3_idx = -1;
		sscanf(tmp_str, "%d", &polygon_vert_3_idx);


		Con_Printf("Polygon %d waypoints(%d) (%d, %d, %d, %d)\n", 
			i, 
			polygon_n_verts, 
			polygon_vert_0_idx,
			polygon_vert_1_idx,
			polygon_vert_2_idx,
			polygon_vert_3_idx
		);


		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		int polygon_n_traversals = 0;
		sscanf(tmp_str, "%d", &polygon_n_traversals);

		// FIXME - Traversal parsing isn't supported yet, if we find one, break out and stop here
		if(polygon_n_traversals > 0) {
			Con_Printf("Polygon %i has %d traversals, stopping.\n", i, polygon_n_traversals);
			break;
		}
		// for(int j = 0; j < polygon_n_traversals; j++) {
		// 	// TODO Parse each traversal struct
		// }

		// TODO - Write into polygon data structure...
		sv_navmesh_polies[i].n_verts = polygon_n_verts;
		sv_navmesh_polies[i].vert_idxs[0] = polygon_vert_0_idx;
		sv_navmesh_polies[i].vert_idxs[1] = polygon_vert_1_idx;
		sv_navmesh_polies[i].vert_idxs[2] = polygon_vert_2_idx;
		sv_navmesh_polies[i].vert_idxs[3] = polygon_vert_3_idx;


		// Read polygon doortarget
		tmp_line_len = file_read_line(file, tmp_str, tmp_str_len);
		if(tmp_line_len > 0) {
			// TODO - Store the polygon's door targetname
			Con_Printf("Polygon %i has door targetname: \"%s\"\n", i, tmp_str);
			sv_navmesh_polies[i].doortarget = (char*) malloc(sizeof(char) * strlen(tmp_str) + 1);
			strcpy(sv_navmesh_polies[i].doortarget, tmp_str);
		}
		else {
			sv_navmesh_polies[i].doortarget = NULL;
		}
	}

	// n_polygons
	//      - n_verts
	//      - vert_0_idx
	//      - vert_1_idx
	//      - vert_2_idx
	//      - vert_3_idx
		//      - poly_center vtos (why is this stored?)
		//      - n_links
		//      - linked_poly_0_idx
		//      - linked_poly_1_idx
		//      - linked_poly_2_idx
		//      - linked_poly_3_idx
		//      - linked_poly_0_left_vert_idx
		//      - linked_poly_1_left_vert_idx
		//      - linked_poly_2_left_vert_idx
		//      - linked_poly_3_left_vert_idx
		//      - linked_poly_0_right_vert_idx
		//      - linked_poly_1_right_vert_idx
		//      - linked_poly_2_right_vert_idx
		//      - linked_poly_3_right_vert_idx
		//      - linked_poly_0_neighbor_edge_idx
		//      - linked_poly_1_neighbor_edge_idx
		//      - linked_poly_2_neighbor_edge_idx
		//      - linked_poly_3_neighbor_edge_idx
	//      - n_traversals
	//          - traversal_idx
	//      - polygon_door_targetname
	// n_traversals
	//      - {traversal data structure}

	Sys_FileClose(file);
}


void sv_load_navmesh() {
	Con_Printf("sv_load_navmesh...\n");
	sv_navmesh_clear_navmesh();
	sv_navmesh_load_from_file();
	Con_Printf("Calc poly centers...\n");
	sv_nvmesh_calc_poly_centers();
	Con_Printf("Sort poly verts...\n");
	sv_navmesh_sort_navmesh_poly_verts_ccw();
	sv_navmesh_calc_connected_polies();


	// TODO - Construct BSP tree from polygons?
	// 			- Requires profiling how long it takes to query a navmesh triangle
	//			- Requires profiling before and after


	// ------------------------------------------------------------------------
}

