

#ifndef USER_INTR_H
#define USER_INTR_H

#include "xscugic.h"

#define CPU0_ID              XSCUGIC_SPI_CPU0_MASK        //CPU0 ID
#define SOFT_INTR_ID_TO_CPU0 0                            //软件中断号 0 ,范围：0~15
#define SOFT_INTR_ID_TO_CPU1 1                            //软件中断号 1 ,范围：0~15
#define CPU0_ID              XSCUGIC_SPI_CPU0_MASK        //CPU0 ID

int user_itr_init(void);
void user_softwareIntr(void);


#endif /* USER_INTR_H */