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

int main()
{
    init_platform();
    user_itr_init();
    
    print("Hello World\n\r");
    print("Successfully ran Hello World application");
    while(1)
    {
        usleep(1000000);
        // user_softwareIntr();
        // printf("cpu1\r\n");
        // test_xsct_print();
    }
    cleanup_platform();
    return 0;
}
