#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/ax_functions.h"
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "system/memory.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "common/common.h"

#include "utils/exception.h"
#include "program.h"


/* Entry point */
int Menu_Main(void)
{
    //!*******************************************************************
    //!                   Initialize function pointers                   *
    //!*******************************************************************
    //! do OS (for acquire) and sockets first so we got logging
    InitOSFunctionPointers();
    InitSocketFunctionPointers();

	InstallExceptionHandler();
	
    log_init("192.168.178.3");
    log_print("Starting launcher\n");
	
	InitAXFunctionPointers();
	
    InitFSFunctionPointers();
    InitVPadFunctionPointers();

    log_print("Function exports loaded\n");

    //!*******************************************************************
    //!                    Initialize heap memory                        *
    //!*******************************************************************
    log_print("Initialize memory management\n");
    //! We don't need bucket and MEM1 memory so no need to initialize
    memoryInitialize();

    //!*******************************************************************
    //!                        Initialize FS                             *
    //!*******************************************************************
    log_printf("Mount SD partition\n");
    mount_sd_fat("sd");
	
	//!*******************************************************************
    //!                        Setup OSScreen                            *
    //!*******************************************************************
    
	OSScreenInit();
	
	unsigned int buffer0size = OSScreenGetBufferSizeEx(0);
	unsigned int buffer1size = OSScreenGetBufferSizeEx(1);
	
	unsigned char* screenBuffer = MEM1_alloc(buffer0size + buffer1size, 0x100);	
	
	OSScreenSetBufferEx(0, screenBuffer);
	OSScreenSetBufferEx(1, screenBuffer + buffer0size);
	
	OSScreenEnableEx(0, 1);
	OSScreenEnableEx(1, 1);
	
	OSScreenClearBufferEx(0, 0x000000FF); //opaque black is the new transparent black
	OSScreenClearBufferEx(1, 0x000000FF);
	DCFlushRange(screenBuffer, buffer0size + buffer1size);
			
	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);
    //!*******************************************************************
    //!                    Enter main application                        *
    //!*******************************************************************
	runProgram(screenBuffer, buffer0size + buffer1size);
	
	MEM1_free(screenBuffer);
    log_printf("Unmount SD\n");
    unmount_sd_fat("sd");
    log_printf("Release memory\n");
    memoryRelease();
    log_deinit();

    return EXIT_SUCCESS;
}

