#include "user_dma.h"

static void TxIntrHandler_0(void *Callback);
static void RxIntrHandler_0(void *Callback);
static void TxIntrHandler_1(void *Callback);
void UartHandler(void *CallBackRef, u32 Event, unsigned int EventData);


XAxiDma  AxiDma0;
XAxiDma  AxiDma1;

volatile int TxDone;
volatile int RxDone;
volatile int Error;
INTC Intc;	/* Instance of the Interrupt Controller */
XAxiDma_Bd *BD_first_test ;
XAxiDma_Bd *BD_last_test ;

//UART
XUartPs  UartPs;
volatile int  uart_rx_ready = 0;
volatile int  uart_rx_select = 0;
volatile u32  rx_len   = 0;
u8            rx_buf[RX_BUF_SIZE];
//用户自定义的CyclicBd地址，当选为Rx的BD有效地址时，会破坏循环接收，使其只能接收一次。
UINTPTR user_cyclicBd_addr = 0x000005c0;

//调试变量
int rx_intr_count0 = 0;
volatile int test_int_1 = 0;
volatile int test_int_2 = 0;
volatile int test_int_3 = 0;


//dma0的发射缓冲区和BD配置。
Txdata_cfg Txdata_0 = 
{
    .bd_baseaddr = (u32 *) TX_BD_SPACE_BASE_0,
    .bd_number = NUMBER_OF_BDS_TO_TRANSFER,
    .buffer_length = TX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER,
    .buffer_bd_length = TX_PKT_LEN_0,
    .buffer_baseaddr = (u32 *) TX_BUFFER_BASE_0
};
//dma0的接收缓冲区和BD配置。
Txdata_cfg Rxdata_0 = 
{
    .bd_baseaddr = (u32 *) RX_BD_SPACE_BASE_0,
    .bd_number = NUMBER_OF_BDS_TO_TRANSFER,
    .buffer_length = RX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER,
    .buffer_bd_length = RX_PKT_LEN_0,
    .buffer_baseaddr = (u32 *) RX_BUFFER_BASE_0
};


//dma0的发射缓冲区和BD配置。
Txdata_cfg Txdata_1 = 
{
    .bd_baseaddr = (u32 *) TX_BD_SPACE_BASE_1,
    .bd_number = NUMBER_OF_BDS_TO_TRANSFER,
    .buffer_length = TX_PKT_LEN_1 * NUMBER_OF_BDS_TO_TRANSFER,
    .buffer_bd_length = TX_PKT_LEN_1,
    .buffer_baseaddr = (u32 *) TX_BUFFER_BASE_1
};

Dmaintr_cfg Dmaintr_cfg_0 = 
{
    .axidma_cur = &AxiDma0,
    .tx_id = TX_INTR_ID_0,
    .rx_id = RX_INTR_ID_0,
    .TxIntrHandler_cur = TxIntrHandler_0,//中断处理函数的函数指针
    .RxIntrHandler_cur = RxIntrHandler_0
};

Dmaintr_cfg Dmaintr_cfg_1 = 
{
    .axidma_cur = &AxiDma1,
    .tx_id = TX_INTR_ID_1,
    .rx_id = NULL_INTR_ID,
    .TxIntrHandler_cur = TxIntrHandler_1,
    .RxIntrHandler_cur = NULL
};

/**
 * @brief 可以当成普通函数理解
 */
static void TxCallBack_1(XAxiDma_BdRing * TxRingPtr)
{
	int BdCount;
	XAxiDma_Bd *BdPtr;

	if(!TxRingPtr -> Cyclic)//非循环模式，执行【工作组】-【后工作组】-【空闲组】
	{
		BdCount = XAxiDma_BdRingFromHw(TxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);
		XAxiDma_BdRingFree(TxRingPtr,BdCount,BdPtr);
	}
}

/**
*
* @brief 中断服务函数
*
* @param	Callback is a pointer to TX channel of the DMA engine.
*
* @return	无
*
******************************************************************************/
static void TxIntrHandler_1(void *Callback)
{
	XAxiDma_BdRing *TxRingPtr = (XAxiDma_BdRing *) Callback;
	u32 IrqStatus;
	int TimeOut;

	//根本没进入中断函数，没有执行任何代码。
	/* 读取当前挂起的中断状态 */
	IrqStatus = XAxiDma_BdRingGetIrq(TxRingPtr);
	/* 当场清除这些中断标志 */
	XAxiDma_BdRingAckIrq(TxRingPtr, IrqStatus);
	/* 如果其实没有任何有效中断，直接返回 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/* 遇到错误中断就打印寄存器、拉起错误标志，并尝试复位 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {
		XAxiDma_BdRingDumpRegs(TxRingPtr);
		Error = 1;
		/* 发送通道理论上一定可以复位成功 */
		XAxiDma_Reset(&AxiDma0);
		TimeOut = RESET_TIMEOUT_COUNTER;
		while (TimeOut) {
			if (XAxiDma_ResetIsDone(&AxiDma0)) {
				break;
			}
			TimeOut -= 1;
		}

		return;
	}

	/* 延迟或完成中断：调用回调统计已完成的 BD */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		TxCallBack_1(TxRingPtr);
	}
}

/**
 * @brief 可以当成普通函数理解
 */
static void TxCallBack_0(XAxiDma_BdRing * TxRingPtr)
{
	int BdCount;
	XAxiDma_Bd *BdPtr;

	if(!TxRingPtr -> Cyclic)//非循环模式，执行【工作组】-【后工作组】-【空闲组】
	{
		BdCount = XAxiDma_BdRingFromHw(TxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);
		XAxiDma_BdRingFree(TxRingPtr,BdCount,BdPtr);
	}
}

/**
*
* @brief 中断服务函数
*
* @param	Callback is a pointer to TX channel of the DMA engine.
*
* @return	无
*
******************************************************************************/
static void TxIntrHandler_0(void *Callback)
{
	XAxiDma_BdRing *TxRingPtr = (XAxiDma_BdRing *) Callback;
	u32 IrqStatus;
	int TimeOut;

	//根本没进入中断函数，没有执行任何代码。
	/* 读取当前挂起的中断状态 */
	IrqStatus = XAxiDma_BdRingGetIrq(TxRingPtr);
	/* 当场清除这些中断标志 */
	XAxiDma_BdRingAckIrq(TxRingPtr, IrqStatus);
	/* 如果其实没有任何有效中断，直接返回 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/* 遇到错误中断就打印寄存器、拉起错误标志，并尝试复位 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {
		XAxiDma_BdRingDumpRegs(TxRingPtr);
		Error = 1;
		/* 发送通道理论上一定可以复位成功 */
		XAxiDma_Reset(&AxiDma0);
		TimeOut = RESET_TIMEOUT_COUNTER;
		while (TimeOut) {
			if (XAxiDma_ResetIsDone(&AxiDma0)) {
				break;
			}
			TimeOut -= 1;
		}

		return;
	}

	/* 延迟或完成中断：调用回调统计已完成的 BD */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		TxCallBack_0(TxRingPtr);
	}
}

/*
*
* This is the DMA RX callback function called by the RX interrupt handler.
* This function handles finished BDs by hardware, attaches new buffers to those
* BDs, and give them back to hardware to receive more incoming packets
*
* @param	RxRingPtr is a pointer to RX channel of the DMA engine.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void RxCallBack_0(XAxiDma_BdRing * RxRingPtr)
{
	int BdCount;
	XAxiDma_Bd *BdPtr;

	Xil_DCacheFlushRange(RX_BUFFER_BASE_0,RX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER);
	/* Get finished BDs from hardware */
	if(!RxRingPtr -> Cyclic)//非循环模式，执行【工作组】-【后工作组】-【空闲组】
	{
		BdCount = XAxiDma_BdRingFromHw(RxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);
		XAxiDma_BdRingFree(RxRingPtr,BdCount,BdPtr);
	}
	rx_intr_count0 += 1;
	RxDone += BdCount;
}

/*
*
* This is the DMA RX interrupt handler function
*
* It gets the interrupt status from the hardware, acknowledges it, and if any
* error happens, it resets the hardware. Otherwise, if a completion interrupt
* presents, then it calls the callback function.
*
* @param	Callback is a pointer to RX channel of the DMA engine.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void RxIntrHandler_0(void *Callback)
{
	XAxiDma_BdRing *RxRingPtr = (XAxiDma_BdRing *) Callback;
	u32 IrqStatus;
	int TimeOut;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_BdRingGetIrq(RxRingPtr);

	/* Acknowledge pending interrupts */
	XAxiDma_BdRingAckIrq(RxRingPtr, IrqStatus);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		XAxiDma_BdRingDumpRegs(RxRingPtr);

		Error = 1;


		/* Reset could fail and hang
		 * NEED a way to handle this or do not call it??
		 */
		XAxiDma_Reset(&AxiDma0);

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if(XAxiDma_ResetIsDone(&AxiDma0)) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If completion interrupt is asserted, call RX call back function
	 * to handle the processed BDs and then raise the according flag.
	 */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		RxCallBack_0(RxRingPtr);
	}
}

void UartHandler(void *CallBackRef, u32 Event, unsigned int EventData)
{
    if ((Event == XUARTPS_EVENT_RECV_DATA) ||
        (Event == XUARTPS_EVENT_RECV_TOUT)) {

        if (EventData > 0 && uart_rx_ready == 0) {
            rx_len   = EventData;
            uart_rx_ready = 1;   /* 只置标志，不发送 */
			switch (rx_buf[0])
			{
			case 'A':
				uart_rx_select = SELECT_DMA0_TX;
				break;
			
			case 'B':
				uart_rx_select = SELECT_DMA1_TX;
				break;

			case 'C':
				uart_rx_select = SELECT_UART_DTSD;
				break;

			case 'D':
				uart_rx_select = SELECT_DMA0_RX;
				break;

			default:
				uart_rx_select = SELECT_DMA0_TX;
				break;
			}
		}

        {
            xil_printf("recv_success\r\n");
            // BspUartWrite(rx_buf, rx_len);//数据回环
            // rx_ready = 0;
            // rx_len   = 0;
        }
        /* 重新启动接收 */
        XUartPs_Recv(&UartPs, rx_buf, RX_BUF_SIZE);
    }
}


/**
 * 测试函数
 */
u32 test_function(u32 a)
{
    u32 b = a*2;
    return b;
}



/*****************************************************************************/
/** 
*  @brief DMA TX通道初始化函数
*  @note 此函数设置 DMA 引擎的 TX 通道为循环DMA模式，使其准备好进行数据包传输。
*  @param	AxiDmaInstPtr 是指向 DMA 引擎实例的指针。
*  @return XST_SUCCESS 如果设置成功。
*          XST_FAILURE 否则。
*/
int txsetup_sg(XAxiDma * AxiDmaInstPtr,Txdata_cfg* txdata_cfg,u8 cyclic)
{
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(AxiDmaInstPtr);
	XAxiDma_Bd BdTemplate;
	int Status;

	/*BD空间初始化*/
	XAxiDma_BdRingIntDisable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);//禁用所有 TX 中断
	Status = XAxiDma_BdRingCreate(TxRingPtr, txdata_cfg->bd_baseaddr,//创建 TxBD 空间
				     txdata_cfg->bd_baseaddr,
				     XAXIDMA_BD_MINIMUM_ALIGNMENT, txdata_cfg->bd_number);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed create BD ring\r\n");//创建 TxBD 空间失败
		return XST_FAILURE;
	}
	XAxiDma_BdClear(&BdTemplate);//创建一个模板并将所有 BD 设置为与模板相同
	Status = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);//复制模板到每个 BD
	if (Status != XST_SUCCESS) {
		xil_printf("Failed clone BDs\r\n");
		return XST_FAILURE;
	}

	//设置中断合并阈值
	Status = XAxiDma_BdRingSetCoalesce(TxRingPtr, COALESCING_COUNT,
			DELAY_TIMER_COUNT);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed set coalescing"
		" %d/%d\r\n",COALESCING_COUNT, DELAY_TIMER_COUNT);
		return XST_FAILURE;
	}

	if(cyclic){
		XAxiDma_BdRingEnableCyclicDMA(TxRingPtr);
		XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DMA_TO_DEVICE, 1);//1为开启，0为关闭
	}
	/* 使能循环 DMA 模式 */
	

	XAxiDma_BdRingIntEnable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);//使能所有 TX 中断。
	//即让axi dma具备能够拉高中断的能力，作用对象为axi dma的寄存器。

	return XST_SUCCESS;
}

/*****************************************************************************/
/*
*
* @brief 配置 DMA 的 RX 接收通道，准备好收包
* @note 主要做三件事：
*       1) 在预留的内存里创建并初始化 RX 的 BD 环；
*       2) 给每个 BD 绑定接收缓冲区和长度；
*       3) 配置中断合并、打开循环接收，最后启动 RX 通道。
*
* @param	AxiDmaInstPtr 指向 DMA 实例的指针
*
* @return	- XST_SUCCESS 配置成功
*		- XST_FAILURE 失败
*
* @note		无
*
******************************************************************************/
int rxsetup_sg(XAxiDma * AxiDmaInstPtr,Rxdata_cfg* rxdata_cfg,u8 cyclic)
{
	XAxiDma_BdRing *RxRingPtr;
	int Status;
	XAxiDma_Bd BdTemplate;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;
	int BdCount;
	int FreeBdCount;
	UINTPTR RxBufferPtr;
	int Index;

	RxRingPtr = XAxiDma_GetRxRing(AxiDmaInstPtr);

	/* 先把 RX 相关中断都关掉，避免初始化时被打断 */
	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* cursor：这里使用 NUMBER_OF_BDS_TO_TRANSFER 而不是全部空间，
	 * 以保持与 TX 通道一致的 BD 数量（20个），避免在循环模式下
	 * 返回过多已完成的 BD */
	BdCount = rxdata_cfg->bd_number;

	Status = XAxiDma_BdRingCreate(RxRingPtr, rxdata_cfg->bd_baseaddr,
					rxdata_cfg->bd_baseaddr,
					XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd create failed with %d\r\n", Status);
		return XST_FAILURE;
	}
	

	/*
	 * 做一个 RX 用的 BD 模板，然后克隆到每个 RX BD 上
	 */
	XAxiDma_BdClear(&BdTemplate);
	Status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx bd clone failed with %d\r\n", Status);
		return XST_FAILURE;
	}

	
	FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);
	BdPtr = RxRingPtr->FreeHead;//手动得到BdPtr
	BdCurPtr = BdPtr;
	RxBufferPtr = rxdata_cfg->buffer_baseaddr;
/* 给每个 RX BD 绑上接收缓冲区，这样硬件就能往里收数据了 */
	for (Index = 0; Index < FreeBdCount; Index++) {

		Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set buffer addr %x on BD %x failed %d\r\n",
			(unsigned int)RxBufferPtr,
			(UINTPTR)BdCurPtr, Status);

			return XST_FAILURE;
		}

		Status = XAxiDma_BdSetLength(BdCurPtr, rxdata_cfg->buffer_bd_length,
					RxRingPtr->MaxTransferLen);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set length %d on BD %x failed %d\r\n",
			    rxdata_cfg->buffer_bd_length, (UINTPTR)BdCurPtr, Status);

			return XST_FAILURE;
		}

		/* 接收这边不需要自己设置控制位
		 * 硬件会根据流的状态自动打 SOF/EOF 标志
		 */
		XAxiDma_BdSetCtrl(BdCurPtr, 0);

		XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);

		RxBufferPtr += rxdata_cfg->buffer_bd_length;
		BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
	}

	/*
	 * 设置中断合并阈值，这里尽量凑成一次中断再通知
	 *
	 * 如果想更频繁地中断，就把 COALESCING_COUNT 调小
	 */
	Status = XAxiDma_BdRingSetCoalesce(RxRingPtr, COALESCING_COUNT,
			DELAY_TIMER_COUNT);
	if (Status != XST_SUCCESS) {
		xil_printf("Rx set coalesce failed with %d\r\n", Status);
		return XST_FAILURE;
	}

	/* 打开 RX 相关的所有中断 */
	XAxiDma_BdRingIntEnable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);//即让axi dma具备能够拉高中断的能力。

	/* 开启循环 DMA 模式（一直收，不用反复提交 BD） */
	if(cyclic){
		XAxiDma_BdRingEnableCyclicDMA(RxRingPtr);
		XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DEVICE_TO_DMA, 1);
	}
	
	//手动刷新cache
	Xil_DCacheFlushRange(RX_BD_SPACE_BASE_0,0x00001FFF);
	

	return XST_SUCCESS;
}

/**
 * @brief 用户自定义sg模式dma初始化，需要手动对每个dma来配置
 */
int user_dma_sg_init()
{
    XAxiDma_Config *Config;
    XAxiDma *AxiDma_temp;
    u8 Status = 0;                 //函数执行状态
    XAxiDma_BdRing *TxRingPtr_test0 = XAxiDma_GetTxRing(&AxiDma0);//临时发射环测试变量
	XAxiDma_BdRing *RxRingPtr_test0 = XAxiDma_GetRxRing(&AxiDma0);//临时发射环测试变量
    XAxiDma_BdRing *TxRingPtr_test1 = XAxiDma_GetTxRing(&AxiDma1);
	XAxiDma_BdRing *RxRingPtr_test1 = XAxiDma_GetRxRing(&AxiDma1);
    u8 * test1 = TxRingPtr_test0 ;
    u8 * test2 = RxRingPtr_test0 ;
    u8 * test3 = TxRingPtr_test1 ;
    u8 * test4 = RxRingPtr_test1 ;

    //DMA0初始化
    {
        Config = XAxiDma_LookupConfig(XPAR_AXI_DMA_0_DEVICE_ID);
        if (!Config) {
            xil_printf("No config found for %d\r\n", XPAR_AXI_DMA_0_DEVICE_ID);
            return XST_FAILURE;
        }
        AxiDma_temp = &AxiDma0;
        XAxiDma_CfgInitialize(AxiDma_temp, Config);
        if(!XAxiDma_HasSg(AxiDma_temp)) {
            xil_printf("Device configured as Simple mode \r\n");
            return XST_FAILURE;
        }

        Status = txsetup_sg(AxiDma_temp,&Txdata_0,0);
        if(Status) {
            xil_printf("txsetup_sg failed \r\n");
            return XST_FAILURE;
        }
        Status = rxsetup_sg(AxiDma_temp,&Rxdata_0,1);
        if(Status) {
            xil_printf("rxsetup_sg failed \r\n");
            return XST_FAILURE;
        }
    }
	
    //DMA1初始化
    {
        Config = XAxiDma_LookupConfig(XPAR_AXI_DMA_1_DEVICE_ID);
        if (!Config) {
            xil_printf("No config found for %d\r\n", XPAR_AXI_DMA_1_DEVICE_ID);
            return XST_FAILURE;
        }
        AxiDma_temp = &AxiDma1;
        XAxiDma_CfgInitialize(AxiDma_temp, Config);//
        if(!XAxiDma_HasSg(AxiDma_temp)) {
            xil_printf("Device configured as Simple mode \r\n");
            return XST_FAILURE;
        }

        Status = txsetup_sg(AxiDma_temp,&Txdata_1,1);
        if(Status) {
            xil_printf("txsetup_sg failed \r\n");
            return XST_FAILURE;
        }
    }

    return XST_SUCCESS;

}


int Txtran_sg(XAxiDma * AxiDmaInstPtr,Txdata_cfg *txdata_cfg)
{
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(AxiDmaInstPtr);
	XAxiDma_Bd *BdPtr;
	int Status;

	
	//BD：空闲-前工作
	Status = XAxiDma_BdRingAlloc(TxRingPtr, txdata_cfg->bd_number,
								&BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed bd alloc\r\n");
		return XST_FAILURE;
	}

	//BD：前工作-工作
	Status = XAxiDma_BdRingToHw(TxRingPtr, txdata_cfg->bd_number,
						BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to hw, length %d\r\n",
			(int)XAxiDma_BdGetLength(BdPtr,
					TxRingPtr->MaxTransferLen));
		return XST_FAILURE;
	}


	/* 启动 TX 通道 */
	Status = XAxiDma_BdRingStart(TxRingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed bd start\r\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;  
}



int Rxtran_sg(XAxiDma * AxiDmaInstPtr,Rxdata_cfg *rxdata_cfg)
{
	XAxiDma_BdRing *RxRingPtr = XAxiDma_GetRxRing(AxiDmaInstPtr);
	XAxiDma_Bd *BdPtr;
	int Status;

	
	//BD：空闲-前工作
	Status = XAxiDma_BdRingAlloc(RxRingPtr, rxdata_cfg->bd_number,
								&BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed bd alloc\r\n");
		return XST_FAILURE;
	}

	//BD：前工作-工作
	Status = XAxiDma_BdRingToHw(RxRingPtr, rxdata_cfg->bd_number,
						BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to hw, length %d\r\n",
			(int)XAxiDma_BdGetLength(BdPtr,
					RxRingPtr->MaxTransferLen));
		return XST_FAILURE;
	}


	/* 启动 TX 通道 */
	Status = XAxiDma_BdRingStart(RxRingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed bd start\r\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;  
}



int  user_dma_sg_Txtransfer(u32 dma_id)
{
    XAxiDma *AxiDma_temp;
    Txdata_cfg *txdata_temp; 
    u32 Status = 0;
    
    //选择全局dma实例指针
	switch (dma_id)
	{
	case XPAR_AXI_DMA_0_DEVICE_ID:
		AxiDma_temp = &AxiDma0;
        txdata_temp = &Txdata_0;
		break;
	
    case XPAR_AXI_DMA_1_DEVICE_ID:
		AxiDma_temp = &AxiDma1;
        txdata_temp = &Txdata_1;
		break;

	default:
		AxiDma_temp = &AxiDma0;
        txdata_temp = &Txdata_0;
		break;
	}
    Txtran_sg(AxiDma_temp,txdata_temp);
}


int  user_dma_sg_Rxtransfer(u32 dma_id)
{
    XAxiDma *AxiDma_temp;
    Rxdata_cfg *rxdata_temp; 
    u32 Status = 0;
    
    //选择全局dma实例指针
	switch (dma_id)
	{
	case XPAR_AXI_DMA_0_DEVICE_ID:
		AxiDma_temp = &AxiDma0;
        rxdata_temp = &Rxdata_0;
		break;


	default:
		AxiDma_temp = &AxiDma0;
        rxdata_temp = &Rxdata_0;
		break;
	}
    Rxtran_sg(AxiDma_temp,rxdata_temp);
}



/**
*
* This function disables the interrupts for DMA engine.
*
* @param	IntcInstancePtr is the pointer to the INTC component instance
* @param	TxIntrId is interrupt ID associated w/ DMA TX channel
* @param	RxIntrId is interrupt ID associated w/ DMA RX channel
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
// void disableintr_sg(INTC * IntcInstancePtr,
// 					u16 TxIntrId, u16 RxIntrId)
// {
// #ifdef XPAR_INTC_0_DEVICE_ID
// 	/* Disconnect the interrupts for the DMA TX and RX channels */
// 	XIntc_Disconnect(IntcInstancePtr, TxIntrId);
// 	XIntc_Disconnect(IntcInstancePtr, RxIntrId);
// #else
// 	XScuGic_Disconnect(IntcInstancePtr, TxIntrId);
// 	XScuGic_Disconnect(IntcInstancePtr, RxIntrId);
// #endif
// }


// void user_dma_itr_disable()
// {
//     disableintr_sg(&Intc, TX_INTR_ID_0, RX_INTR_ID_0);
    
// }



/**
 * @brief 配置缓冲区数据，配置每个BD的缓冲区地址、长度
 * 默认填充为递增的数据
 */
int tx_datainit(XAxiDma * AxiDmaInstPtr,Txdata_cfg* txdata_cfg)
{
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(AxiDmaInstPtr);
	XAxiDma_Bd *BdPtr, *BdCurPtr;
	int Status;
	int Index;
	UINTPTR BufferAddr;

	/* 检查每个包的总长度是否超过硬件限制
	 * 每个包由多个 BD 组成，总长度不能超过硬件支持的最大传输长度
	 * 注意：如果硬件支持存储转发功能，这个限制可能不适用
	 */
	if (txdata_cfg->buffer_length >
			TxRingPtr->MaxTransferLen) {
		xil_printf("Invalid total per packet transfer length for the "
		    "packet %d/%d\r\n",
		    txdata_cfg->buffer_length,
		    TxRingPtr->MaxTransferLen);
		return XST_INVALID_PARAM;
	}

	
    u32 *TxPacket = txdata_cfg->buffer_baseaddr;

    u32 Value = 0x0; 
    for(Index = 0; Index < txdata_cfg->buffer_length/sizeof(u32);
    							Index ++) {
    	TxPacket[Index] = Value;
    	Value = (Value + 1) & 0xFFF;  /* 值递增，保持在 0-255 范围内 */
    }//0-总包长
	
	Xil_DCacheFlushRange((UINTPTR)TxPacket, txdata_cfg->buffer_length);


	
	BdPtr = TxRingPtr->FreeHead;//代替原有的XAxiDma_BdRingAlloc
    BD_first_test = BdPtr;
	/* 初始化缓冲区地址和当前 BD 指针 */
	BufferAddr = (UINTPTR)TxPacket;  /* 从发送缓冲区基地址开始 */
	BdCurPtr = BdPtr;              /* 指向第一个 BD */
    for(Index = 0; Index < txdata_cfg->bd_number; Index++) //配置每个包
	{
			u32 CrBits = 0;  /* 控制位，用于标记包的开始和结束 */
			/* 设置 BD 指向的缓冲区地址：告诉 DMA 从这个地址读取数据 */
			Status = XAxiDma_BdSetBufAddr(BdCurPtr, BufferAddr);
			if (Status != XST_SUCCESS) {
				xil_printf("Tx set buffer addr %x on BD %x failed %d\r\n",
				(unsigned int)BufferAddr,
				(UINTPTR)BdCurPtr, Status);
				return XST_FAILURE;
			}

			/* 设置 BD 的传输长度：每个 BD 传输 txdata_cfg->buffer_bd_length 字节 */
			Status = XAxiDma_BdSetLength(BdCurPtr, txdata_cfg->buffer_bd_length,
						TxRingPtr->MaxTransferLen);
			if (Status != XST_SUCCESS) {
				xil_printf("Tx set length %d on BD %x failed %d\r\n",
				txdata_cfg->buffer_bd_length, (UINTPTR)BdCurPtr, Status);
				return XST_FAILURE;
			}

			/* 如果是包的第一个 BD，设置 SOF（Start of Frame）标志
			 * 告诉硬件：这是数据包的开始
			 */
			if (Index == 0) {
				CrBits |= XAXIDMA_BD_CTRL_TXSOF_MASK;  /* 设置包开始标志 */

// #if (XPAR_AXIDMA_0_SG_INCLUDE_STSCNTRL_STRM == 1)
// 				/* 如果硬件支持流控制，在第一个 BD 中设置整个包的总长度
// 				 * 这主要用于环回测试场景
// 				 */
// 				Status = XAxiDma_BdSetAppWord(BdCurPtr,
// 				    XAXIDMA_LAST_APPWORD,
// 				    txdata_cfg->buffer_bd_length * NUMBER_OF_BDS_PER_PKT);

// 				if (Status != XST_SUCCESS) {
// 					xil_printf("Set app word failed with %d\r\n",
// 					Status);
// 				}
// #endif
			}
			/* 如果是包的最后一个 BD，设置 EOF（End of Frame）标志
			 * 告诉硬件：这是数据包的结束
			 */
			if(Index == (txdata_cfg->bd_number - 1)) {
				CrBits |= XAXIDMA_BD_CTRL_TXEOF_MASK;  /* 设置包结束标志 */
                BD_last_test = BdCurPtr;
			}
			/* 应用控制位到 BD */
			XAxiDma_BdSetCtrl(BdCurPtr, CrBits);
			/* 设置 BD 的 ID（标识符），用于后续识别这个 BD */
			XAxiDma_BdSetId(BdCurPtr, BufferAddr);

			/* 移动到下一个缓冲区地址和下一个 BD */
			BufferAddr += txdata_cfg->buffer_bd_length;  
			BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(TxRingPtr, BdCurPtr);  
		
	}
	
	return XST_SUCCESS;  
}

void user_tx_datainit()
{
    XAxiDma *AxiDma_temp;
    u32 Status = 0;
    //初始化DMA0的发射数据缓冲
    {
        AxiDma_temp = &AxiDma0;
        Status = tx_datainit(AxiDma_temp,&Txdata_0);
        if(Status != XST_SUCCESS)
        {
            xil_printf("tx_datainit failed\r\n");
        }
    }

    //初始化DMA1的发射数据缓冲
    {
        AxiDma_temp = &AxiDma1;
        Status = tx_datainit(AxiDma_temp,&Txdata_1);
        if(Status != XST_SUCCESS)
        {
            xil_printf("tx_datainit failed\r\n");
        }
    }
    
}

/**
 * 每个dma外设的中断配置
 */
int dma_intr_config(INTC * IntcInstancePtr,
			   Dmaintr_cfg* Dmaintr_cfg_cur)
{
    XAxiDma_BdRing *TxRingPtr;
    XAxiDma_BdRing *RxRingPtr;
    int Status;

    //如果存在TX中断
    if(Dmaintr_cfg_cur->tx_id != NULL_INTR_ID)
    {
        TxRingPtr = XAxiDma_GetTxRing(Dmaintr_cfg_cur->axidma_cur);
        XScuGic_SetPriorityTriggerType(IntcInstancePtr, Dmaintr_cfg_cur->tx_id, 0xA0, 0x3);//设置触发类型，优先级
        //注册中断向量表
        Status = XScuGic_Connect(IntcInstancePtr, Dmaintr_cfg_cur->tx_id,
                    Dmaintr_cfg_cur->TxIntrHandler_cur,
                    TxRingPtr);
        if (Status != XST_SUCCESS) {
            return Status;
        }
        XScuGic_Enable(IntcInstancePtr, Dmaintr_cfg_cur->tx_id);//使能中断
    }

    //如果存在Rx中断
	if(Dmaintr_cfg_cur->rx_id != NULL_INTR_ID)
    {
        RxRingPtr = XAxiDma_GetRxRing(Dmaintr_cfg_cur->axidma_cur);
		XScuGic_SetPriorityTriggerType(IntcInstancePtr, Dmaintr_cfg_cur->rx_id, 0xA0, 0x3);//设置触发类型，优先级
        Status = XScuGic_Connect(IntcInstancePtr, Dmaintr_cfg_cur->rx_id,
				Dmaintr_cfg_cur->RxIntrHandler_cur,
				RxRingPtr);
        if (Status != XST_SUCCESS) {
            return Status;
        }
        XScuGic_Enable(IntcInstancePtr, Dmaintr_cfg_cur->rx_id);//使能中断
    }

}

int uart_intr_config(INTC * IntcInstancePtr)
{
	int Status;
	Status = XScuGic_Connect(
        IntcInstancePtr,
        UART_INTR_ID,
        (Xil_ExceptionHandler)XUartPs_InterruptHandler,
        &UartPs
    );
    if (Status != XST_SUCCESS)
        return Status;

    XScuGic_Enable(IntcInstancePtr, UART_INTR_ID);
	// XScuGic_SetPriorityTriggerType(IntcInstancePtr, UART_INTR_ID, 0xA0, 0x3);//设置触发类型，优先级

}

/**
 * 串口外设初始化
 */
int BspUartInit(void)
{
    int Status;
    XUartPs_Config *Config;

    Config = XUartPs_LookupConfig(UART_DEVICE_ID);
    if (!Config)
        return XST_FAILURE;

    Status = XUartPs_CfgInitialize(&UartPs, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS)
	{
		xil_printf("UART not found");
		return Status;
	}
        

    XUartPs_SelfTest(&UartPs);

    XUartPs_SetBaudRate(&UartPs, 115200);
    XUartPs_SetOperMode(&UartPs, XUARTPS_OPER_MODE_NORMAL);

    XUartPs_SetHandler(&UartPs, UartHandler, &UartPs);

    XUartPs_SetInterruptMask(
        &UartPs,
        XUARTPS_IXR_RXFULL |
        XUARTPS_IXR_RXOVR  |
        XUARTPS_IXR_TOUT
    );

    XUartPs_SetRecvTimeout(&UartPs, 8);

    XUartPs_Recv(&UartPs, rx_buf, RX_BUF_SIZE);

    return XST_SUCCESS;
}


//软件中断函数
static void soft_intr_handler(void *CallbackRef)
{
	xil_printf("This is CPU0,Soft Interrupt from CPU1\r\n");
	
}



/**
 * 软件中断初始化（用于不同cpu间通信）
 * 
 */
//CPU0中断初始化
static void cpu0_intr_init(INTC * IntcInstancePtr)
{
	//初始化中断控制器
	// XScuGic_Config *intc_cfg_ptr;
	// intc_cfg_ptr = XScuGic_LookupConfig(INTC_DEVICE_ID);
    // XScuGic_CfgInitialize(intc_ptr, intc_cfg_ptr,
    // 		intc_cfg_ptr->CpuBaseAddress);
    //设置并打开中断异常处理功能
    // Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
    // 		(Xil_ExceptionHandler)XScuGic_InterruptHandler, intc_ptr);
    // Xil_ExceptionEnable();

    XScuGic_Connect(IntcInstancePtr, SOFT_INTR_ID_TO_CPU0,
          (Xil_ExceptionHandler)soft_intr_handler, (void *)IntcInstancePtr);

    XScuGic_Enable(IntcInstancePtr, SOFT_INTR_ID_TO_CPU0); //CPU0软件中断
}



/**
*
* @brief 配置中断控制器，需要手动设置每个DMA外设及UART的配置。
*
* @return
*		- XST_SUCCESS 成功
*		- XST_FAILURE 失败
*
* @note		无
*
******************************************************************************/
int user_itr_init()
{
	int Status;

#ifdef XPAR_INTC_0_DEVICE_ID //若编译条件不成立，则不会编译，代码发虚。
	/* 初始化可编程中断控制器，并把对应的回调连上去 */
	Status = XIntc_Initialize(&Intc, INTC_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed init intc\r\n");
		return XST_FAILURE;
	}
	Status = XIntc_Connect(&Intc, TxIntrId,
			       (XInterruptHandler) TxIntrHandler_0, TxRingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed tx connect intc\r\n");
		return XST_FAILURE;
	}
	Status = XIntc_Connect(&Intc, RxIntrId,
			       (XInterruptHandler) RxIntrHandler_0, RxRingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed rx connect intc\r\n");
		return XST_FAILURE;
	}
	/* 启动中断控制器 */
	Status = XIntc_Start(&Intc, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to start intc\r\n");
		return XST_FAILURE;
	}
	XIntc_Enable(&Intc, TxIntrId);
	XIntc_Enable(&Intc, RxIntrId);

#else

	XScuGic_Config *IntcConfig;

	//初始化GIC
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(&Intc, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}



#endif

	//注册IRQ异常
	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)INTC_HANDLER,
			(void *)&Intc);
	Xil_ExceptionEnable();

	    //对每个外设进行配置（可调换顺序）
    dma_intr_config(&Intc,&Dmaintr_cfg_0);
    dma_intr_config(&Intc,&Dmaintr_cfg_1);
	uart_intr_config(&Intc);
	cpu0_intr_init(&Intc);

	// xil_printf("SetupIntrSystem_Success");
	xil_printf("cpu0 SetupIntrSystem_Success\r\n");
	return XST_SUCCESS;
}


/**
 * 发送软中断给cpu1
 */
void user_softwareIntr(void)
{
	XScuGic_SoftwareIntr(&Intc,SOFT_INTR_ID_TO_CPU1,CPU1_ID);
}

//"SEV"指令唤醒CPU1并跳转至相应的程序
#define sev()                __asm__("sev")               //C语言内嵌汇编写法 send event指令


void start_cpu1()
{
	//向 CPU1_COPY_ADDR(0Xffffffff0)地址写入 CPU1 的访问内存基地址
	Xil_Out32(CPU1_COPY_ADDR, CPU1_START_ADDR);
	dmb();  //等待内存写入完成(同步)
	sev();  //通过"SEV"指令唤醒CPU1并跳转至相应的程序
}

