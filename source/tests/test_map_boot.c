#include "nzportable_def.h"

test_status_t Test_MapBoot_Start(void)
{
	return TEST_STATUS_RUNNING;
}

test_status_t Test_MapBoot_Frame(void)
{
	if (!sv.active || sv.time < 5.0)
		return TEST_STATUS_RUNNING;
	Sys_CaptureScreenshot();
	return TEST_STATUS_PASSED;
}
