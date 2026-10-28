/******************************************************************************
* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"

#include "user.h"
#include "user_intr.h"

int test1 = 0;

int main()
{
    init_platform();
    user_itr_init();
    test1 = 1;
    
    print("Hello World\n\r");
    print("Successfully ran Hello World application");
    test1 += 1;
    while(1)
    {
        usleep(1000000);
        test1 += 1;
        // printf("cpu1,%d\r\n",test1);
    }
    cleanup_platform();
    return 0;
}
