#ifndef _KBD_H_
#define _KBD_H_

#include "dynamic_libs/os_functions.h"

//Huge thanks to rw-r-r_0644 who figured this all out; all the callbacks and everything

char(*KBDSetup)(void *connection_callback, void *disconnection_callback, void* key_callback);
char(*KBDTeardown)();

void InitKBDFunctionPointers() {
	unsigned int nsyskbd_handle;
	OSDynLoad_Acquire("nsyskbd.rpl", &nsyskbd_handle);
	OSDynLoad_FindExport(nsyskbd_handle, 0, "KBDSetup", &KBDSetup);
	OSDynLoad_FindExport(nsyskbd_handle, 0, "KBDTeardown", &KBDTeardown);
}

struct KBDKeyState {
	unsigned char channel;
	unsigned char scancode;
	unsigned int state;
	char unknown[4];
	unsigned short UTF16;
};

#define KBD_KEY_RELEASED 0
#define KBD_KEY_PRESSED 1

#endif //_KBD_H_