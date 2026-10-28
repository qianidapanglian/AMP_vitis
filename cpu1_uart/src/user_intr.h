

#ifndef USER_INTR_H
#define USER_INTR_H

#include "xscugic.h"

#include "../../SG_cyclic_dma/src/user_share.h"


int user_itr_init(void);
void user_softwareIntr(void);
void user_init_share(void);


#endif /* USER_INTR_H */