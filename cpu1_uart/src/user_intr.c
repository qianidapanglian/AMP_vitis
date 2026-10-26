#include "user_intr.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xil_printf.h"

#ifdef XPAR_INTC_0_DEVICE_ID
#include "xintc.h"
#else
#include "xscugic.h"
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
#define INTC_DEVICE_ID          XPAR_INTC_0_DEVICE_ID
#else
#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
#define INTC        XIntc
#define INTC_HANDLER    XIntc_InterruptHandler
#else
#define INTC        XScuGic
#define INTC_HANDLER    XScuGic_InterruptHandler
#endif

static INTC Intc;


static void soft_intr_handler(void *CallbackRef);


/**
 * 注册cpu1的软件中断，使其可以被其他cpu通知。
 */
static void cpu1_intr_init(INTC *IntcInstancePtr)
{
	XScuGic_Connect(IntcInstancePtr, SOFT_INTR_ID_TO_CPU1,
			(Xil_ExceptionHandler)soft_intr_handler, (void *)IntcInstancePtr);

	XScuGic_Enable(IntcInstancePtr, SOFT_INTR_ID_TO_CPU1);
}

/**
 * 初始化GIC
 */
int user_itr_init(void)
{
	int Status;

	XScuGic_Config *IntcConfig;

	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(&Intc, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)INTC_HANDLER,
			(void *)&Intc);
	Xil_ExceptionEnable();

	//可调换顺序
	cpu1_intr_init(&Intc);

	xil_printf("cpu1 SetupIntrSystem_Success\r\n");
	return XST_SUCCESS;
}

/**
 * 发送软中断给cpu0
 */
void user_softwareIntr(void)
{
    XScuGic_SoftwareIntr(&Intc,SOFT_INTR_ID_TO_CPU0,CPU0_ID);
}

/**
 * 中断回调函数
 */
static void soft_intr_handler(void *CallbackRef)
{
	xil_printf("This is CPU1, Soft Interrupt from CPU0\r\n");
	user_softwareIntr();
}