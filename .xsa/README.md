1. axi_DMA_circle:axi dma+中断+sg模式，Tx -- fifo0 -- Rx
   axi_DMA_circle2：添加dma1，输出到fifo1。
2. test2：和axi_DMA_circle2一致，dma0为SG模式，RX+TX。DMA1为SG模式，单TX。
3. SGDMA1：给dma1单独加ILA

