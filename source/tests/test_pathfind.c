/*
 * Copyright (C) 2026 NZP Team.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
// test_pathfind.c -- Robust pathfinding tests
#include "../nzportable_def.h"
#include "../qcvm/cJSON.h"

extern int
sv_way_pathfind(int start_way, int end_way);
extern int
TraceMove(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t * ent);
extern qboolean
WaypointGroundOrigin(int waypoint, edict_t * ent, vec3_t position);
extern int process_list[MAX_WAYPOINTS];
extern int process_list_length;

static waypoint_ai graph[MAX_WAYPOINTS];
static waypoint_ai saved_graph[MAX_WAYPOINTS];
static float reference_cost[MAX_WAYPOINTS];
static qboolean visited[MAX_WAYPOINTS];
extern zombie_ai zombie_list[MAX_AI_COUNT];
static FILE * report;
static cJSON * fall_events;
static char test_map[MAX_QPATH];
static int count, phase, source, destination, errors, pairs, unreachable, skipped, walks, falls;
static qboolean running, invalid_graph;
static vec3_t stopped_origin;
static int reference_source = -1;
static int invalid_links[MAX_WAYPOINTS * 8][3];
static int invalid_link_count, bad_source_id, bad_target_id;
static qboolean direct_walk, edge_walk;
static vec3_t walk_goal;
static edict_t * walk_actor;
static qboolean ignored_brushes[MAX_EDICTS];
static int walk_destination;

qboolean
Test_Pathfind_IgnoreBrush(int entnum, qboolean startsolid, qboolean allsolid, float normal_z, float end_z)
{
    if (!running || phase != 2 || !ignored_brushes[entnum])
        return false;

    return startsolid || allsolid || normal_z <= 0.7f || end_z > graph[walk_destination].origin[2] + 20;
}

void
Test_Pathfind_InvalidLink(int source, int source_id, int target_id)
{
    if (invalid_link_count >= MAX_WAYPOINTS * 8)
        return;

    invalid_links[invalid_link_count][0]   = source;
    invalid_links[invalid_link_count][1]   = source_id;
    invalid_links[invalid_link_count++][2] = target_id;
}

static void
Test_Pathfind_Error(const char * kind, int from, int to, float expected, float actual)
{
    cJSON * entry = cJSON_CreateObject();
    char * text;
    int axis;

    cJSON_AddStringToObject(entry, "kind", kind);

    if (!strcmp(kind, "missing_waypoint_reference")) {
        cJSON_AddNumberToObject(entry, "source_file_id", bad_source_id);
        cJSON_AddNumberToObject(entry, "target_file_id", bad_target_id);
    }

    cJSON_AddStringToObject(entry, "phase", phase == 1 ? "all_open" : phase == 2 ? "zombie_walk" : "current");
    cJSON_AddNumberToObject(entry, "source", from);
    cJSON_AddNumberToObject(entry, "destination", to);
    cJSON_AddNumberToObject(entry, "expected", expected);
    cJSON_AddNumberToObject(entry, "actual", actual);

    if (from >= 0 && from < count) {
        cJSON * position = cJSON_AddArrayToObject(entry, "source_origin");

        for (axis = 0; axis < 3; axis++)
            cJSON_AddItemToArray(position, cJSON_CreateNumber(graph[from].origin[axis]));
    }

    if (to >= 0 && to < count) {
        cJSON * position = cJSON_AddArrayToObject(entry, "destination_origin");

        for (axis = 0; axis < 3; axis++)
            cJSON_AddItemToArray(position, cJSON_CreateNumber(graph[to].origin[axis]));
    }

    if (phase == 2) {
        cJSON * position = cJSON_AddArrayToObject(entry, "stopped_origin");

        for (axis = 0; axis < 3; axis++)
            cJSON_AddItemToArray(position, cJSON_CreateNumber(stopped_origin[axis]));
    }

    if (phase == 2 && strcmp(kind, "walk_invalid_start")) {
        vec3_t direction, end;
        trace_t trace;
        VectorSubtract(walk_goal, stopped_origin, direction);
        direction[2] = 0;
        VectorNormalize(direction);
        VectorMA(stopped_origin, 32, direction, end);
        trace = SV_Move(stopped_origin, ai_hull_mins, ai_hull_maxs, end, MOVE_NOMONSTERS, walk_actor);
        cJSON_AddNumberToObject(entry, "movement_fraction", trace.fraction);
        cJSON_AddStringToObject(entry, "movement_blocker", trace.ent ? PR_GetString(trace.ent->v.classname) : "");
        cJSON_AddStringToObject(entry, "movement_blocker_model", trace.ent ? PR_GetString(trace.ent->v.model) : "");

        cJSON * position = cJSON_AddArrayToObject(entry, "goal_origin");

        for (axis = 0; axis < 3; axis++)
            cJSON_AddItemToArray(position, cJSON_CreateNumber(walk_goal[axis]));
    }

    if (!strcmp(kind, "walk_invalid_start")) {
        trace_t trace = SV_Move(stopped_origin, ai_hull_mins, ai_hull_maxs, stopped_origin, MOVE_NOMONSTERS,
            walk_actor);
        cJSON * mins = cJSON_AddArrayToObject(entry, "movement_mins");
        cJSON * maxs = cJSON_AddArrayToObject(entry, "movement_maxs");
        cJSON_AddBoolToObject(entry, "startsolid", trace.startsolid);
        cJSON_AddBoolToObject(entry, "allsolid", trace.allsolid);
        cJSON_AddStringToObject(entry, "blocker", trace.ent ? PR_GetString(trace.ent->v.classname) : "worldspawn");

        for (axis = 0; axis < 3; axis++) {
            cJSON_AddItemToArray(mins, cJSON_CreateNumber(ai_hull_mins[axis]));
            cJSON_AddItemToArray(maxs, cJSON_CreateNumber(ai_hull_maxs[axis]));
        }
    }

    text = cJSON_PrintUnformatted(entry);

    if (text) {
        fprintf(report, "%s%s", errors ? ",\n" : "\n", text);
        free(text);
    }

    cJSON_Delete(entry);

    if (errors < 20)
        Con_Printf("[FAIL] Pathfind: %s (%i -> %i, expected %g, actual %g)\n", kind, from, to, (double) expected,
          (double) actual);
    errors++;
    fflush(report);
} /* Test_Pathfind_Error */

static cJSON *
Test_Pathfind_Graph(void)
{
    cJSON * nodes = cJSON_CreateArray();

    for (int i = 0; i < count && i < MAX_WAYPOINTS; i++) {
        cJSON * node   = cJSON_CreateObject();
        cJSON * origin = cJSON_AddArrayToObject(node, "origin");
        cJSON * links  = cJSON_AddArrayToObject(node, "targets");
        cJSON_AddNumberToObject(node, "index", i);
        cJSON_AddBoolToObject(node, "open", graph[i].open);
        cJSON_AddStringToObject(node, "tag", graph[i].special);
        for (int axis = 0; axis < 3; axis++)
            cJSON_AddItemToArray(origin, cJSON_CreateNumber(graph[i].origin[axis]));
        for (int link = 0; link < 8; link++)
            if (graph[i].target[link] >= 0)
                cJSON_AddItemToArray(links, cJSON_CreateNumber(graph[i].target[link]));
        cJSON_AddItemToArray(nodes, node);
    }
    return nodes;
}

static test_status_t
Test_Pathfind_Finish(void)
{
    int failed = errors != 0;

    if (report) {
        cJSON * nodes     = Test_Pathfind_Graph();
        char * graph_text = cJSON_PrintUnformatted(nodes);
        char * events     = cJSON_PrintUnformatted(fall_events);
        fprintf(report,
          "\n],\"events\":%s,\"graph\":%s,\"summary\":{\"waypoints\":%i,\"pairs\":%i,\"unreachable\":%i,\"skipped_closed\":%i,\"walks\":%i,\"falls\":%i,\"errors\":%i},\"walk_mode\":\"%s\",\"movement_probe\":\"actor\",\"waypoint_placement\":\"ground\",\"brush_policy\":\"ignore_gameplay\",\"passed\":%s}\n",
          events ? events : "[]", graph_text ? graph_text : "[]", count, pairs, unreachable, skipped, walks, falls,
          errors, edge_walk ? "edges" : direct_walk ? "direct" : "path", failed ? "false" : "true");
        free(events);
        free(graph_text);
        cJSON_Delete(nodes);
        if (ferror(report))
            failed = 1;
        if (fclose(report))
            failed = 1;
        report = NULL;
    }
    cJSON_Delete(fall_events);
    fall_events = NULL;
    running     = false;
    Con_Printf(
        "Pathfind tests %s. %i waypoints, %i pairs, %i unreachable, %i closed pairs skipped, %i zombie walks, %i falls, %i errors.\n",
        failed ? "failed" : "passed", count, pairs, unreachable, skipped, walks, falls, errors);
    return failed ? TEST_STATUS_FAILED : TEST_STATUS_PASSED;
}

qboolean
Test_Pathfind_Begin(void)
{
    char path[sizeof(com_gamedir) + sizeof("/" FILE_SPECIAL_PREFIX "pathfind-report.json" FILE_SPECIAL_SUFFIX)];
    cJSON * map;
    char * text;
    int i, slot, axis;

    if (running || !sv.active)
        return false;

    snprintf(path, sizeof(path), "%s/" FILE_SPECIAL_PREFIX "pathfind-report.json" FILE_SPECIAL_SUFFIX, com_gamedir);
    report = fopen(path, "w");

    if (!report) {
        Con_Printf("Could not write pathfind report: %s\n", path);
        return false;
    }

    count       = n_waypoints;
    direct_walk = edge_walk = false;
    walk_actor  = NULL;
    memset(ignored_brushes, 0, sizeof(ignored_brushes));
    phase            = source = destination = errors = pairs = unreachable = skipped = walks = falls = 0;
    fall_events      = cJSON_CreateArray();
    invalid_graph    = false;
    reference_source = -1;
    running          = true;
    snprintf(test_map, sizeof(test_map), "%s", sv.name);
    memcpy(graph, waypoints, sizeof(graph));
    map  = cJSON_CreateString(test_map);
    text = cJSON_PrintUnformatted(map);
    fprintf(report, "{\"map\":%s,\"errors\":[", text ? text : "null");
    free(text);
    cJSON_Delete(map);
    Con_Printf("Starting pathfind tests on %s. Report: %s\n", test_map, path);

    for (i = 0; i < invalid_link_count; i++) {
        bad_source_id = invalid_links[i][1];
        bad_target_id = invalid_links[i][2];
        Test_Pathfind_Error("missing_waypoint_reference", invalid_links[i][0], -1, 1, 0);
    }

    if (count <= 0 || count > MAX_WAYPOINTS) {
        Test_Pathfind_Error("invalid_waypoint_count", -1, -1, 1, count);
        invalid_graph = true;
        return true;
    }

    for (i = 0; i < count; i++) {
        qboolean empty = false;

        if (!graph[i].used) {
            Test_Pathfind_Error("unused_waypoint", i, -1, 1, 0);
            invalid_graph = true;
        }

        for (axis = 0; axis < 3; axis++)
            if (!isfinite(graph[i].origin[axis])) {
                Test_Pathfind_Error("invalid_origin", i, -1, 0, graph[i].origin[axis]);
                invalid_graph = true;
            }

        for (slot = 0; slot < 8; slot++) {
            int to = graph[i].target[slot];
            float distance;

            if (to < 0) {
                empty = true;
                continue;
            }

            if (to >= count || empty) {
                Test_Pathfind_Error("invalid_link", i, to, count, slot);
                invalid_graph = true;
                continue;
            }

            distance = VecLength2(graph[i].origin, graph[to].origin);

            if (!isfinite(distance)) {
                Test_Pathfind_Error("invalid_link_distance", i, to, 0, distance);
                invalid_graph = true;
                continue;
            }

            if (!isfinite(graph[i].dist[slot]) || fabsf(graph[i].dist[slot] - distance) > fmaxf(0.01f,
              distance * 0.0001f))
                Test_Pathfind_Error("incorrect_edge_cost", i, to, distance, graph[i].dist[slot]);
        }
    }

    return true;
} /* Test_Pathfind_Begin */

static void
Test_Pathfind_Reference(int start)
{
    int i, pass, slot;

    memset(visited, 0, sizeof(visited));

    for (i = 0; i < count; i++)
        reference_cost[i] = INFINITY;
    reference_cost[start] = 0;

    for (pass = 0; pass < count; pass++) {
        int best = -1;

        for (i = 0; i < count; i++)
            if (!visited[i] && isfinite(reference_cost[i]) && (best < 0 || reference_cost[i] < reference_cost[best]))
                best = i;

        if (best < 0)
            break;

        visited[best] = true;

        for (slot = 0; slot < 8; slot++) {
            int to = graph[best].target[slot];
            float cost;

            if (to < 0)
                break;

            if (phase != 1 && !graph[to].open)
                continue;

            cost = reference_cost[best] + VecLength2(graph[best].origin, graph[to].origin);

            if (cost < reference_cost[to])
                reference_cost[to] = cost;
        }
    }
} /* Test_Pathfind_Reference */

static void
Test_Pathfind_Pair(void)
{
    int found, i, slot;
    float cost = 0;

    if (!phase && (!graph[source].open || !graph[destination].open)) {
        skipped++;
        return;
    }

    pairs++;
    found = sv_way_pathfind(source, destination);

    if (!isfinite(reference_cost[destination])) {
        unreachable++;

        if (found)
            Test_Pathfind_Error("unexpected_route", source, destination, INFINITY, 0);
        return;
    }

    if (!found) {
        Test_Pathfind_Error("missing_route", source, destination, reference_cost[destination], INFINITY);
        return;
    }

    if (process_list_length < 1 || process_list_length > count || process_list[0] != destination ||
      process_list[process_list_length - 1] != source)
    {
        Test_Pathfind_Error("invalid_route", source, destination, 1, process_list_length);
        return;
    }

    for (i = 0; i < process_list_length; i++) {
        int node = process_list[i];

        if (node < 0 || node >= count || (!phase && !graph[node].open)) {
            Test_Pathfind_Error("invalid_route_node", source, destination, 1, node);
            return;
        }
    }

    for (i = process_list_length - 1; i > 0; i--) {
        int node = process_list[i];

        for (slot = 0; slot < 8; slot++)
            if (graph[node].target[slot] == process_list[i - 1])
                break;

        if (slot == 8) {
            Test_Pathfind_Error("invalid_route_link", source, destination, node, process_list[i - 1]);
            return;
        }

        cost += VecLength2(graph[node].origin, graph[process_list[i - 1]].origin);
    }

    if (fabsf(cost - reference_cost[destination]) > fmaxf(0.05f, reference_cost[destination] * 0.0001f))
        Test_Pathfind_Error("suboptimal_route", source, destination, reference_cost[destination], cost);
} /* Test_Pathfind_Pair */

static int
Test_Pathfind_Frame(void)
{
    double start_time;

    int i;

    if (!running || invalid_graph)
        return -1;

    if (phase == 2)
        return 1;

    memcpy(saved_graph, waypoints, sizeof(saved_graph));
    memcpy(waypoints, graph, sizeof(graph));
    start_time = Sys_FloatTime();
    do{
        for (i = 0; i < count; i++)
            waypoints[i].open = phase || graph[i].open;

        if (!destination)
            Test_Pathfind_Reference(source);
        Test_Pathfind_Pair();

        if (++destination == count) {
            destination = 0;

            if (++source == count) {
                source = 0;
                phase++;
            }
        }
    } while (phase < 2 && Sys_FloatTime() - start_time < 0.004);
    memcpy(waypoints, saved_graph, sizeof(saved_graph));
    return phase == 2;
} /* Test_Pathfind_Frame */

void
Test_Pathfind_Reset(void)
{
    invalid_link_count = 0;

    if (!running)
        return;

    Test_Pathfind_Error("map_reloaded", -1, -1, 0, 1);
    Test_Pathfind_Finish();
}

qboolean
Test_Pathfind_Active(void)
{
    return running;
}

void
Test_Pathfind_Command(void)
{
    char text[1024];

    if (cmd_source == src_command && cls.state == ca_connected) {
        Cmd_ForwardToServer();
        return;
    }

    if (cmd_source != src_command || !sv.active) {
        Con_Printf("pathfind_test requires a loaded server.\n");
        return;
    }

    snprintf(text, sizeof(text), "pathfind_test %s", Cmd_Argc() > 1 ? Cmd_Args() : "");

    if (!PR_ParseClientCommand(sv.edicts, text))
        Con_Printf("pathfind_test requires the updated QuakeC command.\n");
}

void
PF_pathfind_test(void)
{
    int operation = G_FLOAT(OFS_PARM0);
    int from      = pr_argc > 1 ? G_FLOAT(OFS_PARM1) : 0;
    int to        = pr_argc > 2 ? G_FLOAT(OFS_PARM2) : 0;
    float outcome = pr_argc > 3 ? G_FLOAT(OFS_PARM3) : 0;

    G_FLOAT(OFS_RETURN) = -1;

    if (!operation && Cvar_VariableValue("sv_cheats") != 1)
        return;

    if (!operation) {
        G_FLOAT(OFS_RETURN) = Test_Pathfind_Begin();
        return;
    }

    if (operation == 2) {
        G_FLOAT(OFS_RETURN) = n_waypoints;
        return;
    }

    if (!running)
        return;

    switch (operation) {
        case 1:
            G_FLOAT(OFS_RETURN) = Test_Pathfind_Frame();
            break;

        case 3:
            if (from >= 0 && from < count)
                G_FLOAT(OFS_RETURN) = graph[from].open;
            break;

        case 4:
            walk_destination = to >= 0 && to < count ? to : 0;

            if (invalid_graph || phase != 2 || from < 0 || from >= count || to < 0 || to >= count ||
              (!direct_walk && (!graph[from].open || !graph[to].open)))
                break;

            if (edge_walk) {
                int link;

                for (link = 0; link < 8 && graph[from].target[link] != to; link++);

                if (link == 8)
                    break;
            }

            if (direct_walk) {
                G_FLOAT(OFS_RETURN) = VecLength2(graph[from].origin, graph[to].origin);
                break;
            }

            if (reference_source != from) {
                Test_Pathfind_Reference(from);
                reference_source = from;
            }

            if (isfinite(reference_cost[to]))
                G_FLOAT(OFS_RETURN) = reference_cost[to];
            break;

        case 5:
            walk_actor = pr_argc > 6 ? G_EDICT(OFS_PARM6) : NULL;
            walks++;
            VectorCopy(G_VECTOR(OFS_PARM4), stopped_origin);

            if (pr_argc > 5) {
                VectorCopy(G_VECTOR(OFS_PARM5), walk_goal);
            } else if (to >= 0 && to < count) {
                VectorCopy(graph[to].origin, walk_goal);
            } else {
                VectorClear(walk_goal);
            }

            if (outcome != 1)
                Test_Pathfind_Error(outcome == -5 ? "walk_no_route" : outcome == -4 ? "walk_invalid_start" : outcome ==
                  -1 ? "walk_timeout" : outcome == -3 ? "walk_cancelled" : "walk_stalled", from, to, 1, outcome);
            G_FLOAT(OFS_RETURN) = 1;
            break;

        case 6:
            G_FLOAT(OFS_RETURN) = Test_Pathfind_Finish() == TEST_STATUS_PASSED;
            break;

        case 7: {
            cJSON * event    = cJSON_CreateObject();
            cJSON * position = cJSON_AddArrayToObject(event, "origin");
            int axis;
            cJSON_AddStringToObject(event, "kind", "fall");
            cJSON_AddNumberToObject(event, "source", from);
            cJSON_AddNumberToObject(event, "destination", to);

            for (axis = 0; axis < 3; axis++)
                cJSON_AddItemToArray(position, cJSON_CreateNumber(G_VECTOR(OFS_PARM4)[axis]));
            cJSON_AddItemToArray(fall_events, event);
            falls++;
            G_FLOAT(OFS_RETURN) = 1;
            break;
        }

        case 9: {
            trace_t trace = SV_Move(G_VECTOR(OFS_PARM4), ai_hull_mins, ai_hull_maxs, G_VECTOR(OFS_PARM4),
                MOVE_NOMONSTERS, PROG_TO_EDICT(pr_global_struct->self));
            G_FLOAT(OFS_RETURN) = !trace.startsolid && !trace.allsolid;
            break;
        }

        case 11: {
            if (from < 0 || from >= count || to < 0 || to >= count)
                break;

            vec3_t origin, end;
            edict_t * actor   = PROG_TO_EDICT(pr_global_struct->self);
            qboolean walkable = WaypointGroundOrigin(from, actor, origin) && WaypointGroundOrigin(to, actor,
                end) && TraceMove(origin, ai_hull_mins, ai_hull_maxs, end, MOVE_NOMONSTERS, actor);
            cJSON * event = cJSON_CreateObject();
            cJSON_AddStringToObject(event, "kind", "walking_clearance");
            cJSON_AddNumberToObject(event, "source", from);
            cJSON_AddNumberToObject(event, "destination", to);
            cJSON_AddBoolToObject(event, "walkable", walkable);
            cJSON_AddItemToArray(fall_events, event);
            G_FLOAT(OFS_RETURN) = walkable;
            break;
        }

        case 12: {
            if (pr_argc < 8)
                break;

            edict_t * brush = G_EDICT(OFS_PARM6);
            ignored_brushes[NUM_FOR_EDICT(brush)] = true;

            cJSON * event = cJSON_CreateObject();
            cJSON_AddStringToObject(event, "kind", "ignored_dynamic_brush");
            cJSON_AddStringToObject(event, "classname", PR_GetString(brush->v.classname));
            cJSON_AddStringToObject(event, "model", PR_GetString(brush->v.model));
            cJSON_AddStringToObject(event, "reason", G_STRING(OFS_PARM7));
            cJSON_AddItemToArray(fall_events, event);
            G_FLOAT(OFS_RETURN) = 1;
            break;
        }

        case 10:
            direct_walk         = true;
            edge_walk           = from == 1;
            G_FLOAT(OFS_RETURN) = 1;
            break;

        case 8:
            Test_Pathfind_Error("test_cancelled", -1, -1, 1, 0);
            G_FLOAT(OFS_RETURN) = 1;
            break;
    }
} /* PF_pathfind_test */

void
PF_pathfind_origin(void)
{
    int index = G_FLOAT(OFS_PARM0);

    VectorCopy(vec3_origin, G_VECTOR(OFS_RETURN));

    if (running && index >= 0 && index < count) {
        VectorCopy(graph[index].origin, G_VECTOR(OFS_RETURN));

        if (pr_argc > 1) {
            vec3_t position;

            if (WaypointGroundOrigin(index, G_EDICT(OFS_PARM1), position)) {
                VectorCopy(position, G_VECTOR(OFS_RETURN));
            } else {
                G_VECTOR(OFS_RETURN)[2] += 0.03125f;
            }
        }
    }
}

void
PF_pathfind_clear(void)
{
    int entnum = G_EDICTNUM(OFS_PARM0);
    int i;

    for (i = 0; i < MAX_AI_COUNT; i++)
        if (zombie_list[i].zombienum == entnum)
            memset(&zombie_list[i], 0, sizeof(zombie_list[i]));
}
