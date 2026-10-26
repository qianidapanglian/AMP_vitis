#ifndef USER_DMA_H
#define USER_DMA_H

#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xuartps.h"
#include <string.h>


#ifdef XPAR_INTC_0_DEVICE_ID
 #include "xintc.h"
#else
 #include "xscugic.h"
#endif

//地址宏定义
#ifdef XPAR_AXI_7SDDR_0_S_AXI_BASEADDR
#define DDR_BASE_ADDR		XPAR_AXI_7SDDR_0_S_AXI_BASEADDR
#elif XPAR_MIG7SERIES_0_BASEADDR
#define DDR_BASE_ADDR	XPAR_MIG7SERIES_0_BASEADDR
#elif XPAR_MIG_0_BASEADDR
#define DDR_BASE_ADDR	XPAR_MIG_0_BASEADDR
#elif XPAR_PSU_DDR_0_S_AXI_BASEADDR
#define DDR_BASE_ADDR	XPAR_PSU_DDR_0_S_AXI_BASEADDR
#endif

#ifndef DDR_BASE_ADDR
#warning CHECK FOR THE VALID DDR ADDRESS IN XPARAMETERS.H, \
			DEFAULT SET TO 0x01000000
// #define MEM_BASE_ADDR		0x01000000
#define MEM_BASE_ADDR		0x01000000
#else
#define MEM_BASE_ADDR		(DDR_BASE_ADDR + 0x1000000)
#endif

//DMA0
/* 接收通道 BD 描述符空间地址范围（64KB） */
#define RX_BD_SPACE_BASE_0	(MEM_BASE_ADDR)				
#define RX_BD_SPACE_HIGH_0	(MEM_BASE_ADDR + 0x0000FFFF)	
#define TX_BD_SPACE_BASE_0	(MEM_BASE_ADDR + 0x00010000)	
#define TX_BD_SPACE_HIGH_0	(MEM_BASE_ADDR + 0x0001FFFF)	

#define BUFFER_SIZE_0           0x001FFFFF              //0x001FFFFF表示2MB
#define TX_BUFFER_BASE_0		(MEM_BASE_ADDR + 0x00100000)	
#define TX_BUFFER_HIGH_0		TX_BUFFER_BASE_0 + BUFFER_SIZE_0	
#define RX_BUFFER_BASE_0		(MEM_BASE_ADDR + 0x00300000)	
#define RX_BUFFER_HIGH_0		RX_BUFFER_BASE_0 + BUFFER_SIZE_0	

#define TX_PKT_LEN_0		0x10
#define RX_PKT_LEN_0		0x10

//DMA1	
#define TX_BD_SPACE_BASE_1	(MEM_BASE_ADDR + 0x00020000)	
#define TX_BD_SPACE_HIGH_1	(MEM_BASE_ADDR + 0x0002FFFF)	

#define BUFFER_SIZE_1           0x001FFFFF              //0x001FFFFF表示2MB
#define TX_BUFFER_BASE_1		(MEM_BASE_ADDR + 0x00500000)	
#define TX_BUFFER_HIGH_1		TX_BUFFER_BASE_0 + BUFFER_SIZE_0	

#define TX_PKT_LEN_1		0x010
// #define MARK_UNCACHEABLE        0x701


/* Timeout loop counter for reset
 */
#define RESET_TIMEOUT_COUNTER	10000




//BD宏定义。
//这里的包数是用户定义的，也可以就一个包。
//在硬件眼里，只有不同BD之分。
#define NUMBER_OF_BDS_PER_PKT		20   //每个数据包使用的BD数
#define NUMBER_OF_PKTS_TO_TRANSFER 	1	//定义单次传输的包数量
#define NUMBER_OF_BDS_TO_TRANSFER	(NUMBER_OF_PKTS_TO_TRANSFER * \
						NUMBER_OF_BDS_PER_PKT)

/* 中断合并和超时值
 * 中断合并的范围：1 to 255
 */
#define COALESCING_COUNT		NUMBER_OF_BDS_TO_TRANSFER
#define DELAY_TIMER_COUNT		100

//外设中断宏定义
#ifdef XPAR_INTC_0_DEVICE_ID
#define INTC_DEVICE_ID          XPAR_INTC_0_DEVICE_ID
#else
#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
#define RX_INTR_ID_0		XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define TX_INTR_ID_0		XPAR_INTC_0_AXIDMA_0_MM2S_INTROUT_VEC_ID
#else
#define RX_INTR_ID_0		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define TX_INTR_ID_0		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
#define TX_INTR_ID_1		XPAR_FABRIC_AXIDMA_1_VEC_ID

#endif
#define NULL_INTR_ID        0xFFFF//某个dma未设置RX或Tx的通道。

//GIC
#ifdef XPAR_INTC_0_DEVICE_ID
 #define INTC		XIntc
 #define INTC_HANDLER	XIntc_InterruptHandler
#else
 #define INTC		XScuGic
 #define INTC_HANDLER	XScuGic_InterruptHandler
#endif

//软件中断
#define CPU0_ID              XSCUGIC_SPI_CPU0_MASK        //CPU0 ID
#define CPU1_ID              XSCUGIC_SPI_CPU1_MASK        //CPU1 ID
#define SOFT_INTR_ID_TO_CPU0 0                            //软件中断号 0 ,范围：0~15
#define SOFT_INTR_ID_TO_CPU1 1                            //软件中断号 1 ,范围：0~15
#define CPU1_COPY_ADDR       0xfffffff0                   //存放CPU1应用起始地址的地址
#define CPU1_START_ADDR      0x10000000                   //CPU1应用起始地址


//串口宏定义
#define UART_DEVICE_ID     XPAR_XUARTPS_0_DEVICE_ID
#define UART_INTR_ID       XPAR_XUARTPS_0_INTR //此处原为XPAR_XUARTPS_0_INTR

#define RX_BUF_SIZE        64
#define SELECT_DMA0_TX     0
#define SELECT_DMA1_TX     1
#define SELECT_UART_DTSD   2
#define SELECT_DMA0_RX     3




//结构体
//Tx数据缓冲区配置结构体
typedef struct{
    u32 * bd_baseaddr;        //第一个BD的地址
    u32 bd_number;          //BD数
    u32 buffer_length;      //缓冲区总字节长度
    u32 buffer_bd_length;   //每个BD对应的缓冲区长度
    u32 * buffer_baseaddr;    //缓冲区基地址
    
}Txdata_cfg;

//结构体
//Rx数据缓冲区配置结构体
typedef struct{
    u32 * bd_baseaddr;        //第一个BD的地址
    u32 bd_number;          //BD数
    u32 buffer_length;      //缓冲区总字节长度
    u32 buffer_bd_length;   //每个BD对应的缓冲区长度
    u32 * buffer_baseaddr;    //缓冲区基地址
    
}Rxdata_cfg;


//结构体
//dma中断结构体
typedef struct{
    XAxiDma* axidma_cur;        //dma引擎指针
    u32 tx_id;          //
    u32 rx_id;      //
    Xil_InterruptHandler TxIntrHandler_cur;        //Tx服务函数指针
    Xil_InterruptHandler RxIntrHandler_cur;        //Rx服务函数指针
}Dmaintr_cfg;

typedef volatile struct
{
    uint32_t CR;              // 0x00 Control Register
    uint32_t SR;              // 0x04 Status Register
    uint32_t CDESC;           // 0x08 Current Descriptor
    uint32_t CDESC_MSB;       // 0x0C Current Descriptor MSB
    uint32_t TDESC;           // 0x10 Tail Descriptor
    uint32_t TDESC_MSB;       // 0x14 Tail Descriptor MSB

    uint32_t reg_0x18;      //0x18
    uint32_t reg_0x1C;      //0x1C
    
    uint32_t Reserved0[2];    // 0x20~0x27

    uint32_t BUFFLEN;         // 0x28
    uint32_t SGCTL;           // 0x2C

    uint32_t Reserved1[4];    // 0x30~0x3F

    /* Multi-Channel Registers */

    uint32_t RX_CDESC0;       // 0x40
    uint32_t RX_CDESC0_MSB;   // 0x44
    uint32_t RX_TDESC0;       // 0x48
    uint32_t RX_TDESC0_MSB;   // 0x4C

} XAxiDma_ChannelRegs;

typedef volatile struct
{
    uint32_t NDESC;              // 0x00 
    uint32_t NDESC_MSB;              // 0x04 
    uint32_t BUFA;           // 0x08 
    uint32_t BUFA_MSB;       // 0x0C 
    uint32_t MCCTL;           // 0x10 
    uint32_t STRIDE_VSIZE;       // 0x14 

    uint32_t CTRL_LEN;      //0x18
    uint32_t STS;      //0x1C
    
    uint32_t USR0_4[5];    // 0x20~0x30

    uint32_t ID;         // 0x34
    uint32_t HAS_STSCNTRL;           // 0x38
    uint32_t HAS_DRE;           // 0x3C

} user_XAxiDma_Bd;


u32 test_function(u32 a);
int txsetup_sg(XAxiDma * AxiDmaInstPtr,Txdata_cfg* txdata_cfg,u8 cyclic);
int rxsetup_sg(XAxiDma * AxiDmaInstPtr,Rxdata_cfg* rxdata_cfg,u8 cyclic);
int user_dma_sg_init();
int Txtran_sg(XAxiDma * AxiDmaInstPtr,Txdata_cfg *txdata_cfg);
int user_dma_sg_Txtransfer(u32 dma_id);
int setupintr_sg(INTC * IntcInstancePtr,
			   XAxiDma * AxiDmaPtr, u16 TxIntrId, u16 RxIntrId);
int user_dma_itr_init();
// void disableintr_sg(INTC * IntcInstancePtr,
// 					u16 TxIntrId, u16 RxIntrId);
// void user_dma_itr_disable();
int tx_datainit(XAxiDma * AxiDmaInstPtr,Txdata_cfg* txdata_cfg);
void user_tx_datainit();
int user_itr_init();
void user_softwareIntr(void);
void start_cpu1();



#endif /* USER_DMA_H */