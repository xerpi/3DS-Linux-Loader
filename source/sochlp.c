#include <3ds.h>
#include "sochlp.h"

Result SOCInit() {

	Result ret;

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	if(SOC_buffer == 0) {
		return 1;
	}

	ret = SOC_Initialize(SOC_buffer, SOC_BUFFERSIZE);
	if(ret != 0) {
		free(SOC_buffer);
		return 1;
	} 

	return 0;
}

Result SOCExit() {
	SOC_Shutdown();
	if (SOC_buffer)
		free(SOC_buffer);
	return 0;
}