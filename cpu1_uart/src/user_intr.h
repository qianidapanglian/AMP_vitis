

#ifndef USER_INTR_H
#define USER_INTR_H

#include "xscugic.h"

#define CPU0_ID              XSCUGIC_SPI_CPU0_MASK        //CPU0 ID
#define SOFT_INTR_ID_TO_CPU0 0                            //软件中断号 0 ,范围：0~15
#define SOFT_INTR_ID_TO_CPU1 1                            //软件中断号 1 ,范围：0~15
#define CPU0_ID              XSCUGIC_SPI_CPU0_MASK        //CPU0 ID
#define SHARE_BASE           0xffff0000                   //共享OCM首地址
#define PRIVATE_BASE         0x00200000                   //cpu0的一块私有地址

int user_itr_init(void);
void user_softwareIntr(void);
void user_init_share(void);


#endif /* USER_INTR_H */