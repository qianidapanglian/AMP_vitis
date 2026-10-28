#ifndef USER_SHARE_H
#define USER_SHARE_H

/**
 * 用于多个cpu共同配置
 */

//软件中断
#define CPU0_ID              XSCUGIC_SPI_CPU0_MASK        //CPU0 ID
#define CPU1_ID              XSCUGIC_SPI_CPU1_MASK        //CPU1 ID
#define SOFT_INTR_ID_TO_CPU0 0                            //软件中断号 0 ,范围：0~15
#define SOFT_INTR_ID_TO_CPU1 1                            //软件中断号 1 ,范围：0~15

#define SHARE_BASE           0xffff0000                   //共享OCM首地址
#define CPU1_COPY_ADDR       0xfffffff0                   //存放CPU1应用起始地址的地址
#define CPU1_START_ADDR      0x10000000                   //CPU1应用起始地址
#define PRIVATE_BASE         0x00200000                   //cpu0的一块私有地址


#endif /* USER_SHARE_H */