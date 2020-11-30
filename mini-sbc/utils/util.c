#include "util.h"

pj_bool_t num_exists (char *num)
{
	for (int i=0; i<10; i++)
		if (!strcmp(nums[i].num, num))
			return PJ_TRUE;
	return PJ_FALSE;
}

char* num_addr (char *num)
{
	for (int i=0; i<10; i++)
		if (!strcmp(nums[i].num, num))
			return nums[i].addr;
	return NULL;
}
