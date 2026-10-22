#ifndef USER_H
#define USER_H

#include "xparameters.h"
#include "xil_cache.h"

void ClearMemory(UINTPTR StartAddr, u32 Length);
void user_printf_u32(u32* baseaddr,u32 length_u32);
void user_data_process(u32* baseaddr,u32 length_u32);
void user_data_filedwithDC(u32* baseaddr,u32 length_u32,u32 data_filled);




#endif /* USER_H */