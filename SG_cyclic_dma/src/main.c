/******************************************************************************
*

*
******************************************************************************/
/*****************************************************************************/
/**
 *
 *
 *
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -------------------------------------------------------
 * 9.4   adk  25/07/17 Initial version.
 * 9.6   rsp  02/14/18 Support data buffers above 4GB.Use UINTPTR for storing
 *                     and typecasting buffer address(CR-992638).
 * 9.8   rsp  07/24/18 Set TX DMACR[Cyclic BD enable] before starting DMA
 *                     operation i.e. in TxSetup.
 * </pre>
 *
 * ***************************************************************************
 */
/***************************** Include Files *********************************/
#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"、
#include "sleep.h"

#ifdef __aarch64__
#include "xil_mmu.h"
#endif


#ifndef DEBUG
extern void xil_printf(const char *format, ...);
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
 #include "xintc.h"
#else
 #include "xscugic.h"
#endif

#include "user_dma.h"
#include "user.h"
/******************** Constant Definitions **********************************/
/*
 * Device hardware build related constants.
 */

#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID



/************************** Function Prototypes ******************************/

static int CheckData(int Length, u8 StartValue);


/************************** Variable Definitions *****************************/
/*
 * Device instance definitions
 */
extern XAxiDma AxiDma0;
extern XAxiDma AxiDma1;
extern INTC Intc;	/* Instance of the Interrupt Controller */

/*
 * Flags interrupt handlers use to notify the application context the events.
 */
extern volatile int TxDone;
extern volatile int RxDone;
extern volatile int Error;
extern volatile int  uart_rx_ready;
extern volatile int  uart_rx_select;
extern volatile int test_int_1;
extern volatile int test_int_2;
extern volatile int test_int_3;
volatile int test_int_4 = 0;
int cycle_varie40_120 = 40;

XAxiDma_BdRing *TxRingPtr_test0;//临时发射环测试变量
XAxiDma_BdRing *TxRingPtr_test1;//临时发射环测试变量
XAxiDma_BdRing *RxRingPtr_test0;//临时发射环测试变量


XAxiDma_ChannelRegs* Tx_REG_0;//dma0的Tx寄存器
XAxiDma_ChannelRegs* Tx_REG_1;//dma1的Tx寄存器


//指向Tx BD的指针
user_XAxiDma_Bd* BD_test_0;//测试BD指针
user_XAxiDma_Bd* BD_test_1;//测试BD指针
user_XAxiDma_Bd* BD_test_2;//测试BD指针

/*****************************************************************************/
/**
******************************************************************************/
int main(void)
{
	int Status;
	XAxiDma_Config *Config;
	
	
	TxRingPtr_test0 = XAxiDma_GetTxRing(&AxiDma0);
	TxRingPtr_test1 = XAxiDma_GetTxRing(&AxiDma1);
	RxRingPtr_test0 = XAxiDma_GetRxRing(&AxiDma0);
	Tx_REG_0 = (UINTPTR)0x40400000;
	Tx_REG_1 = (UINTPTR)0x40410000;
	BD_test_0 = (UINTPTR)TX_BD_SPACE_BASE_0;
	xil_printf("\r\n--- Entering main() --- \r\n");
	
	ClearMemory(TX_BUFFER_BASE_0,0x100);//不用全清，看下数据是否刷新即可
	ClearMemory(RX_BUFFER_BASE_0,0x100);
	
	//对dma0的tx和rx都配置
	Status = user_dma_sg_init();
	//调试信息
	
	Status = BspUartInit();//配置串口
	
	
	usleep(1000);
	/* 配置中断系统 */
	user_itr_init();
	
	
	/* 在启动传输测试前初始化标志位 */
	TxDone = 0;
	RxDone = 0;
	Error = 0;

	BD_test_1 = (user_XAxiDma_Bd *)XAxiDma_BdRingNext(TxRingPtr_test0, BD_test_0);
    BD_test_2 = (user_XAxiDma_Bd *)XAxiDma_BdRingNext(TxRingPtr_test0, BD_test_1);

	// Rx_REG_0 = (UINTPTR)(RxRingPtr_test0->ChanBase);
	user_tx_datainit();//Tx发送缓冲
	start_cpu1();

	/*
	 * 等待 TX 完成与 RX 完成
	 */
	while(1) {
		// usleep(600000);
		// test_int_4 += 1;
		// user_softwareIntr();
		// xil_printf("cpu0 = %d\r\n",test_int_4);
		
		if(uart_rx_ready)
		{
			// switch (uart_rx_select)
			// {
			// 	//单次发送，并且改变发射缓冲区的数据。
			// case SELECT_DMA0_TX:
			// 	 Status = user_dma_sg_Txtransfer(XPAR_AXI_DMA_0_DEVICE_ID);
			// 	 usleep(1000);
			// 	 Xil_DCacheFlushRange(RX_BUFFER_BASE_0,RX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER);
			// 	 // user_data_process(TX_BUFFER_BASE_0,TX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER/sizeof(u32));
			// 	 user_data_filedwithDC(TX_BUFFER_BASE_0,
			// 	 	TX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER/sizeof(u32),cycle_varie40_120);
			// 	 cycle_varie40_120 = (cycle_varie40_120 == 120) ? 40 : (cycle_varie40_120 + 40);
			// 	break;
			// case SELECT_DMA0_RX:
			// 	 Status = user_dma_sg_Rxtransfer(XPAR_AXI_DMA_0_DEVICE_ID);
			// 	 Xil_DCacheFlushRange(RX_BUFFER_BASE_0,RX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER);
			// 	break;
			// case SELECT_DMA1_TX:
			// 	 Status = user_dma_sg_Txtransfer(XPAR_AXI_DMA_1_DEVICE_ID);
			// 	break;
			// case SELECT_UART_DTSD:
			// 	 user_printf_u32(RX_BUFFER_BASE_0,RX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER/sizeof(u32));
			// 	break;
			// default:
			// 	break;
			// }
			uart_rx_ready = 0;
			user_softwareIntr();

		}

		
		// usleep(1000);
	}

	// XAxiDma_Reset(&AxiDma0);
	
}


/*****************************************************************************/
/*
*
* This function checks data buffer after the DMA transfer is finished.
*
* We use the static tx/rx buffers.
*
* @param	Length is the length to check
* @param	StartValue is the starting value of the first byte
*
* @return	- XST_SUCCESS if validation is successful
*		- XST_FAILURE if validation fails.
*
* @note		None.
*
******************************************************************************/
static int CheckData(int Length, u8 StartValue)
{
	u8 *RxPacket;
	int Index = 0;
	u8 Value;

	RxPacket = (u8 *) RX_BUFFER_BASE_0;
	Value = StartValue;

	/* Invalidate the DestBuffer before receiving the data, in case the
	 * Data Cache is enabled
	 */
#ifndef __aarch64__
	Xil_DCacheInvalidateRange((UINTPTR)RxPacket, Length);
#endif

	for(Index = 0; Index < Length; Index++) {
		if (RxPacket[Index] != Value) {
			xil_printf("Data error %d: %x/%x\r\n",
			    Index, RxPacket[Index], Value);

			return XST_FAILURE;
		}
		Value = (Value + 1) & 0xFF;
	}

	return XST_SUCCESS;
}











