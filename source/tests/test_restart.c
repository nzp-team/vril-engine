#include "../nzportable_def.h"

#define NZP_RESTART_TEST_COUNT 100
#define NZP_RESTART_TEST_DELAY 5.0

static int restart_count;
static double next_restart_time;

test_status_t Test_Restart_Start(void)
{
	restart_count = 0;
	next_restart_time = 0;
	return TEST_STATUS_RUNNING;
}

test_status_t Test_Restart_Frame(void)
{
	if (!sv.active)
		return TEST_STATUS_RUNNING;
	if (!next_restart_time)
	{
		next_restart_time = sv.time + NZP_RESTART_TEST_DELAY;
		return TEST_STATUS_RUNNING;
	}
	if (sv.time < next_restart_time)
		return TEST_STATUS_RUNNING;

	restart_count++;
	Con_Printf("Restart stress test: %i/%i\n", restart_count, NZP_RESTART_TEST_COUNT);
	SV_RestartServer();
	next_restart_time = sv.time + NZP_RESTART_TEST_DELAY;
	if (restart_count < NZP_RESTART_TEST_COUNT)
		return TEST_STATUS_RUNNING;

	Con_Printf("Restart stress test passed after %i restarts.\n", restart_count);
	return TEST_STATUS_PASSED;
}
