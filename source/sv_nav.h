// TODO - Rename this to just nav.c? ai_nav.c?



void sv_load_navmesh();



#define NAVMESH_MAX_VERTICES 2048
#define NAVMESH_MAX_POLYGONS 2048
#define NAVMESH_MAX_TRAVERSALS 128
#define NAVMESH_MAX_TRAVERSALS_PER_POLY 32 		// Max number of traversals a polygon can connect to
#define NAVMESH_MAX_VERTICES_PER_POLY 4 		// Max number of vertices a polygon can have



void cl_sv_draw_navmesh();


#define NAVMESH_DEBUG_DRAW

#ifdef NAVMESH_DEBUG_DRAW

void PF_sv_navmesh_set_test_start_pos();
void PF_sv_navmesh_set_test_goal_pos();

#endif // NAVMESH_DEBUG_DRAW