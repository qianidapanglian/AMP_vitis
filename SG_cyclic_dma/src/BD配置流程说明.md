# AXI DMA BD (Buffer Descriptor) 配置流程说明

## 一、BD 基础概念

### 1.1 什么是 BD？
BD (Buffer Descriptor) 是 DMA 引擎用来描述数据传输的描述符，每个 BD 包含：
- **缓冲区地址** (Buffer Address): 数据在内存中的物理地址
- **传输长度** (Transfer Length): 要传输的字节数
- **控制位** (Control Bits): SOF (Start of Frame)、EOF (End of Frame) 等
- **状态位** (Status Bits): 硬件设置的完成标志、错误标志等

### 1.2 BD 结构
- **BD 大小**: 每个 BD = 16 words × 4 bytes = 64 字节
- **对齐要求**: 最小对齐 = 64 字节 (`XAXIDMA_BD_MINIMUM_ALIGNMENT = 0x40`)
- **BD 数量计算**: 对于 64KB 空间，最多可容纳 `65536 / 64 = 1024` 个 BD

### 1.3 当前项目配置
```c
#define NUMBER_OF_BDS_PER_PKT     2    // 每个数据包使用的 BD 数
#define NUMBER_OF_PKTS_TO_TRANSFER 10  // 单次传输的包数量
#define NUMBER_OF_BDS_TO_TRANSFER  20  // 单次传输的 BD 数量 (10 × 2)
#define NUMBER_OF_CYCLIC_TRANSFERS 100 // 循环传输 100 次
```

## 二、BD 环状态机

BD 环中的 BD 分为四个组，按顺序在组间流转：

### 2.1 四个组的状态

```
┌─────────────────────────────────────────────────────────────┐
│                      BD Ring State Machine                   │
└─────────────────────────────────────────────────────────────┘

    Free (空闲组)          Pre-process (预处理组)     Hardware (硬件组)      Post-process (后处理组)
    ┌─────────┐            ┌──────────────┐         ┌──────────┐          ┌──────────────┐
    │ 可分配  │            │  软件配置中  │         │ 硬件处理 │          │  硬件已完成  │
    │ FreeCnt │            │   PreCnt     │         │  HwCnt   │          │   PostCnt    │
    └─────────┘            └──────────────┘         └──────────┘          └──────────────┘
         │                        │                      │                      │
         │ Alloc()                │ ToHw()               │ FromHw()             │ Free()
         │                        │                      │                      │
         ▼                        ▼                      ▼                      ▼
    ┌─────────┐            ┌──────────────┐         ┌──────────┐          ┌──────────────┐
    │         │───────────�7�4│              │────────�7�4│          │─────────�7�4│              │
    │ 空闲    │            │ 预处理       │         │ 硬件处理 │          │ 后处理       │
    │         │�7�2───────────│              │         │          │          │              │
    └─────────┘            └──────────────┘         └──────────┘          └──────────────┘
         ▲                                                                    │
         │                                                                    │
         └────────────────────────────────────────────────────────────────────┘
                              UnAlloc() (取消分配时使用)
```

### 2.2 状态转换流程

1. **Free → Pre-process**: `XAxiDma_BdRingAlloc()`
   - 从空闲组分配 BD 到预处理组
   - 软件可以配置这些 BD（设置地址、长度、控制位等）

2. **Pre-process → Hardware**: `XAxiDma_BdRingToHw()`
   - 将配置好的 BD 提交给硬件
   - BD 进入硬件组，由硬件处理

3. **Hardware → Post-process**: `XAxiDma_BdRingFromHw()`
   - 硬件处理完成后，软件从硬件组回收 BD
   - BD 进入后处理组，软件可以检查传输结果

4. **Post-process → Free**: `XAxiDma_BdRingFree()`
   - 处理完成后，将 BD 释放回空闲组
   - BD 可以再次被分配使用

## 三、TX 通道 BD 配置流程

### 3.1 初始化阶段 (`TxSetup`)

```c
// 步骤 1: 禁用中断
XAxiDma_BdRingIntDisable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

// 步骤 2: 创建 BD 环
XAxiDma_BdRingCreate(TxRingPtr, 
                     TX_BD_SPACE_BASE_0,      // BD 空间基地址（物理地址）
                     TX_BD_SPACE_BASE_0,      // BD 空间基地址（虚拟地址）
                     XAXIDMA_BD_MINIMUM_ALIGNMENT,  // 对齐：64 字节
                     NUMBER_OF_BDS_TO_TRANSFER);    // BD 数量：20

// 步骤 3: 创建 BD 模板并克隆
XAxiDma_BdClear(&BdTemplate);
XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);

// 步骤 4: 设置中断合并阈值
XAxiDma_BdRingSetCoalesce(TxRingPtr, 
                          COALESCING_COUNT,      // 包阈值：10
                          DELAY_TIMER_COUNT);    // 延迟定时器：100

// 步骤 5: 启用循环 DMA 模式
XAxiDma_BdRingEnableCyclicDMA(TxRingPtr);
XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DMA_TO_DEVICE, 1);

// 步骤 6: 启用中断
XAxiDma_BdRingIntEnable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

// 步骤 7: 启动 TX 通道
XAxiDma_BdRingStart(TxRingPtr);
```

### 3.2 发送数据阶段 (`SendPacket`)

```c
// 步骤 1: 准备数据
// 填充发送缓冲区 (Packet)，数据从 0x0C 开始递增

// 步骤 2: 刷新数据缓存
Xil_DCacheFlushRange((UINTPTR)TxPacket, TX_PKT_LEN_0 * NUMBER_OF_BDS_TO_TRANSFER);

// 步骤 3: 分配 BD (Free → Pre-process)
XAxiDma_BdRingAlloc(TxRingPtr, NUMBER_OF_BDS_TO_TRANSFER, &BdPtr);

// 步骤 4: 配置每个 BD
for (Index = 0; Index < NUMBER_OF_PKTS_TO_TRANSFER; Index++) {
    for (Pkts = 0; Pkts < NUMBER_OF_BDS_PER_PKT; Pkts++) {
        // 4.1 设置缓冲区地址
        XAxiDma_BdSetBufAddr(BdCurPtr, BufferAddr);
        
        // 4.2 设置传输长度
        XAxiDma_BdSetLength(BdCurPtr, TX_PKT_LEN_0, TxRingPtr->MaxTransferLen);
        
        // 4.3 设置控制位
        u32 CrBits = 0;
        if (Pkts == 0) {
            CrBits |= XAXIDMA_BD_CTRL_TXSOF_MASK;  // 包的第一个 BD：设置 SOF
        }
        if (Pkts == (NUMBER_OF_BDS_PER_PKT - 1)) {
            CrBits |= XAXIDMA_BD_CTRL_TXEOF_MASK;  // 包的最后一个 BD：设置 EOF
        }
        XAxiDma_BdSetCtrl(BdCurPtr, CrBits);
        
        // 4.4 设置 BD ID（用于识别）
        XAxiDma_BdSetId(BdCurPtr, BufferAddr);
        
        // 4.5 移动到下一个 BD
        BufferAddr += TX_PKT_LEN_0;
        BdCurPtr = XAxiDma_BdRingNext(TxRingPtr, BdCurPtr);
    }
}

// 步骤 5: 提交 BD 给硬件 (Pre-process → Hardware)
XAxiDma_BdRingToHw(TxRingPtr, NUMBER_OF_BDS_TO_TRANSFER, BdPtr);
// 执行完此函数后，硬件开始传输数据
```

### 3.3 TX 中断处理

```c
// 在中断服务函数中：
// 1. 读取中断状态
IrqStatus = XAxiDma_BdRingGetIrq(TxRingPtr);

// 2. 确认中断
XAxiDma_BdRingAckIrq(TxRingPtr, IrqStatus);

// 3. 处理完成中断
if (IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK)) {
    // 从硬件回收已完成的 BD (Hardware → Post-process)
    BdCount = XAxiDma_BdRingFromHw(TxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);
    
    // 处理数据（可选）
    // ...
    
    // 释放 BD (Post-process → Free)
    XAxiDma_BdRingFree(TxRingPtr, BdCount, BdPtr);//cursor瞎编的，其实没有这句
}
```

## 四、RX 通道 BD 配置流程

### 4.1 初始化阶段 (`RxSetup`)

```c
// 步骤 1: 禁用中断
XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

// 步骤 2: 创建 BD 环
BdCount = NUMBER_OF_BDS_TO_TRANSFER;  // 20 个 BD（与 TX 一致）
XAxiDma_BdRingCreate(RxRingPtr, 
                     RX_BD_SPACE_BASE_0,      // BD 空间基地址
                     RX_BD_SPACE_BASE_0,
                     XAXIDMA_BD_MINIMUM_ALIGNMENT,  // 对齐：64 字节
                     BdCount);                      // BD 数量：20

// 步骤 3: 创建 BD 模板并克隆
XAxiDma_BdClear(&BdTemplate);
XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);

// 步骤 4: 分配所有 BD (Free → Pre-process)
FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);  // 应该等于 20
XAxiDma_BdRingAlloc(RxRingPtr, FreeBdCount, &BdPtr);

// 步骤 5: 配置每个 RX BD
for (Index = 0; Index < FreeBdCount; Index++) {
    // 5.1 设置接收缓冲区地址
    XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
    
    // 5.2 设置接收长度
    XAxiDma_BdSetLength(BdCurPtr, TX_PKT_LEN_0, RxRingPtr->MaxTransferLen);
    
    // 5.3 清除控制位（RX 不需要设置 SOF/EOF，硬件会自动设置）
    XAxiDma_BdSetCtrl(BdCurPtr, 0);
    
    // 5.4 设置 BD ID
    XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);
    
    // 5.5 移动到下一个 BD
    RxBufferPtr += TX_PKT_LEN_0;
    BdCurPtr = XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
}

// 步骤 6: 设置中断合并阈值
XAxiDma_BdRingSetCoalesce(RxRingPtr, 
                          COALESCING_COUNT,      // 包阈值：10
                          DELAY_TIMER_COUNT);    // 延迟定时器：100

// 步骤 7: 提交 BD 给硬件 (Pre-process → Hardware)
XAxiDma_BdRingToHw(RxRingPtr, FreeBdCount, BdPtr);

// 步骤 8: 启用中断
XAxiDma_BdRingIntEnable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

// 步骤 9: 启用循环 DMA 模式
XAxiDma_BdRingEnableCyclicDMA(RxRingPtr);
XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DEVICE_TO_DMA, 1);

// 步骤 10: 启动 RX 通道
XAxiDma_BdRingStart(RxRingPtr);
```

### 4.2 RX 中断处理

```c
// 在中断服务函数中：
static void RxIntrHandler_0(void *Callback)
{
    // 1. 读取中断状态
    IrqStatus = XAxiDma_BdRingGetIrq(RxRingPtr);
    
    // 2. 确认中断
    XAxiDma_BdRingAckIrq(RxRingPtr, IrqStatus);
    
    // 3. 处理错误中断
    if (IrqStatus & XAXIDMA_IRQ_ERROR_MASK) {
        Error = 1;
        XAxiDma_Reset(&AxiDma);
        return;
    }
    
    // 4. 处理完成中断
    if (IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK)) {
        RxCallBack_0(RxRingPtr);
    }
}

// 在回调函数中：
static void RxCallBack_0(XAxiDma_BdRing * RxRingPtr)
{
    // 从硬件回收已完成的 BD (Hardware → Post-process)
    BdCount = XAxiDma_BdRingFromHw(RxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);
    RxDone += BdCount;
    
    // 注意：在循环 DMA 模式下，BD 会自动重新提交给硬件
    // 所以不需要手动调用 XAxiDma_BdRingFree()
    // 硬件会循环使用这些 BD
}
```

## 五、循环 DMA 模式说明

### 5.1 循环模式的特点

- **自动循环**: BD 处理完成后，硬件自动重新使用这些 BD
- **无需释放**: 不需要调用 `XAxiDma_BdRingFree()` 释放 BD
- **持续传输**: 适用于需要持续传输数据的场景

### 5.2 循环模式下的 BD 流转

```
┌─────────────────────────────────────────────────────────────┐
│                  Cyclic DMA Mode BD Flow                     │
└─────────────────────────────────────────────────────────────┘

    Hardware (硬件组)
    ┌──────────┐
    │  BD 1    │──┐
    │  BD 2    │  │
    │  ...     │  │  硬件自动循环使用
    │  BD 20   │──┘
    └──────────┘
         │
         │ 硬件处理完成后，自动重新使用
         │ (不需要软件干预)
         │
         ▼
    ┌──────────┐
    │ 再次处理 │
    └──────────┘
```

### 5.3 循环模式下的中断处理

- **中断触发**: 当达到中断合并阈值时触发中断
- **BD 回收**: 使用 `XAxiDma_BdRingFromHw()` 回收已完成的 BD
- **数据检查**: 检查接收到的数据（如果需要）
- **自动重新提交**: 硬件会自动重新使用这些 BD（循环模式）

## 六、内存布局

### 6.1 BD 空间布局

```
内存地址布局：
┌─────────────────────────────────────────────────────────────┐
│ 0x01000000 - 0x0100FFFF  │  RX BD Space (64KB, 1024 BD max) │
│ 0x01010000 - 0x0101FFFF  │  TX BD Space (64KB, 1024 BD max) │
│ 0x01100000 - 0x012FFFFF  │  TX Buffer (2MB)                 │
│ 0x01300000 - 0x014FFFFF  │  RX Buffer (2MB)                 │
└─────────────────────────────────────────────────────────────┘

当前配置：
- RX BD: 20 个 BD (0x01000000 - 0x010007FF)
- TX BD: 20 个 BD (0x01010000 - 0x010107FF)
```

### 6.2 缓冲区布局

```
TX Buffer (发送缓冲区):
┌─────────────────────────────────────────────────────────────┐
│ 0x01100000 + 0x0000    │  Packet 0, BD 0 (256 bytes)       │
│ 0x01100000 + 0x0100    │  Packet 0, BD 1 (256 bytes)       │
│ 0x01100000 + 0x0200    │  Packet 1, BD 0 (256 bytes)       │
│ ...                    │  ...                               │
│ 0x01100000 + 0x1300    │  Packet 9, BD 1 (256 bytes)       │
└─────────────────────────────────────────────────────────────┘

RX Buffer (接收缓冲区):
┌─────────────────────────────────────────────────────────────┐
│ 0x01300000 + 0x0000    │  RX BD 0 Buffer (256 bytes)       │
│ 0x01300000 + 0x0100    │  RX BD 1 Buffer (256 bytes)       │
│ ...                    │  ...                               │
│ 0x01300000 + 0x1300    │  RX BD 19 Buffer (256 bytes)      │
└─────────────────────────────────────────────────────────────┘
```

## 七、关键 API 函数说明

### 7.1 BD 环管理

| 函数 | 功能 | 状态转换 |
|------|------|----------|
| `XAxiDma_BdRingCreate()` | 创建 BD 环 | 初始化 |
| `XAxiDma_BdRingClone()` | 克隆 BD 模板 | 初始化 |
| `XAxiDma_BdRingAlloc()` | 分配 BD | Free → Pre-process |
| `XAxiDma_BdRingToHw()` | 提交 BD 给硬件 | Pre-process → Hardware |
| `XAxiDma_BdRingFromHw()` | 从硬件回收 BD | Hardware → Post-process |
| `XAxiDma_BdRingFree()` | 释放 BD | Post-process → Free |
| `XAxiDma_BdRingUnAlloc()` | 取消分配 BD | Pre-process → Free |

### 7.2 BD 配置

| 函数 | 功能 |
|------|------|
| `XAxiDma_BdSetBufAddr()` | 设置缓冲区地址 |
| `XAxiDma_BdSetLength()` | 设置传输长度 |
| `XAxiDma_BdSetCtrl()` | 设置控制位（SOF/EOF） |
| `XAxiDma_BdSetId()` | 设置 BD ID |
| `XAxiDma_BdGetLength()` | 获取传输长度 |
| `XAxiDma_BdGetActualLength()` | 获取实际传输长度（RX） |

### 7.3 中断管理

| 函数 | 功能 |
|------|------|
| `XAxiDma_BdRingIntEnable()` | 启用中断 |
| `XAxiDma_BdRingIntDisable()` | 禁用中断 |
| `XAxiDma_BdRingGetIrq()` | 读取中断状态 |
| `XAxiDma_BdRingAckIrq()` | 确认中断 |
| `XAxiDma_BdRingSetCoalesce()` | 设置中断合并阈值 |

### 7.4 通道控制

| 函数 | 功能 |
|------|------|
| `XAxiDma_BdRingStart()` | 启动通道 |
| `XAxiDma_BdRingEnableCyclicDMA()` | 启用循环 DMA 模式 |
| `XAxiDma_SelectCyclicMode()` | 选择循环模式 |

## 八、完整流程图

### 8.1 TX 通道完整流程

```
main()
  │
  ├─�7�4 TxSetup()
  │     │
  │     ├─�7�4 XAxiDma_BdRingCreate()        // 创建 BD 环
  │     ├─�7�4 XAxiDma_BdRingClone()         // 克隆 BD 模板
  │     ├─�7�4 XAxiDma_BdRingSetCoalesce()   // 设置中断合并
  │     ├─�7�4 XAxiDma_BdRingEnableCyclicDMA()  // 启用循环模式
  │     ├─�7�4 XAxiDma_BdRingIntEnable()     // 启用中断
  │     └─�7�4 XAxiDma_BdRingStart()         // 启动通道
  │
  ├─�7�4 SetupIntrSystem()                   // 设置中断系统
  │
  ├─�7�4 SendPacket()
  │     │
  │     ├─�7�4 准备数据                      // 填充发送缓冲区
  │     ├─�7�4 Xil_DCacheFlushRange()        // 刷新缓存
  │     ├─�7�4 XAxiDma_BdRingAlloc()         // 分配 BD
  │     ├─�7�4 配置每个 BD                   // 设置地址、长度、控制位
  │     └─�7�4 XAxiDma_BdRingToHw()          // 提交给硬件
  │
  └─�7�4 等待传输完成
        │
        └─�7�4 TxIntrHandler_0()                // TX 中断处理
              │
              ├─�7�4 XAxiDma_BdRingGetIrq()   // 读取中断状态
              ├─�7�4 XAxiDma_BdRingAckIrq()   // 确认中断
              ├─�7�4 XAxiDma_BdRingFromHw()   // 回收 BD
              └─�7�4 XAxiDma_BdRingFree()     // 释放 BD（非循环模式）
```

### 8.2 RX 通道完整流程

```
main()
  │
  ├─�7�4 RxSetup()
  │     │
  │     ├─�7�4 XAxiDma_BdRingCreate()        // 创建 BD 环
  │     ├─�7�4 XAxiDma_BdRingClone()         // 克隆 BD 模板
  │     ├─�7�4 XAxiDma_BdRingAlloc()         // 分配所有 BD
  │     ├─�7�4 配置每个 RX BD                // 设置接收缓冲区
  │     ├─�7�4 XAxiDma_BdRingSetCoalesce()   // 设置中断合并
  │     ├─�7�4 XAxiDma_BdRingToHw()          // 提交给硬件
  │     ├─�7�4 XAxiDma_BdRingIntEnable()     // 启用中断
  │     ├─�7�4 XAxiDma_BdRingEnableCyclicDMA()  // 启用循环模式
  │     └─�7�4 XAxiDma_BdRingStart()         // 启动通道
  │
  ├─�7�4 SetupIntrSystem()                   // 设置中断系统
  │
  └─�7�4 等待接收完成
        │
        └─�7�4 RxIntrHandler_0()                // RX 中断处理
              │
              ├─�7�4 XAxiDma_BdRingGetIrq()   // 读取中断状态
              ├─�7�4 XAxiDma_BdRingAckIrq()   // 确认中断
              └─�7�4 RxCallBack_0()             // RX 回调函数
                    │
                    └─�7�4 XAxiDma_BdRingFromHw()  // 回收 BD
                          // 注意：循环模式下不需要 Free()
```

## 九、注意事项

### 9.1 缓存一致性

- **TX 数据**: 发送前必须刷新缓存 (`Xil_DCacheFlushRange()`)
- **RX 数据**: 接收后必须失效缓存 (`Xil_DCacheInvalidateRange()`)
- **BD 空间**: BD 空间应设置为非缓存 (`MARK_UNCACHEABLE`)

### 9.2 地址对齐

- **BD 对齐**: 必须 64 字节对齐
- **缓冲区对齐**: 建议字对齐（4 字节）
- **传输长度**: 对于多 BD 传输，每个 BD 的长度必须是字的倍数

### 9.3 中断合并

- **包阈值**: `COALESCING_COUNT = 10`，处理 10 个包后触发中断
- **延迟定时器**: `DELAY_TIMER_COUNT = 100`，延迟 100 个时钟周期后触发中断
- **中断触发条件**: 满足包阈值或延迟定时器条件时触发中断

### 9.4 循环模式

- **自动循环**: BD 处理完成后自动重新使用
- **无需释放**: 不需要调用 `XAxiDma_BdRingFree()`
- **持续传输**: 适用于需要持续传输的场景

### 9.5 错误处理

- **错误检测**: 检查中断状态中的错误位
- **错误恢复**: 发生错误时重置 DMA 引擎
- **BD 状态**: 检查 BD 状态位中的错误标志

## 十、常见问题

### 10.1 为什么 RX BD 数量是 1024 而不是 20？

**原因**: 原来代码使用 `XAxiDma_BdRingCntCalc()` 计算全部 64KB 空间可容纳的 BD 数量：
```c
BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
            RX_BD_SPACE_HIGH_0 - RX_BD_SPACE_BASE_0 + 1);
// 计算结果：65536 / 64 = 1024
```

**解决方案**: 直接使用 `NUMBER_OF_BDS_TO_TRANSFER`：
```c
BdCount = NUMBER_OF_BDS_TO_TRANSFER;  // 20
```

### 10.2 循环模式下需要释放 BD 吗？

**不需要**。在循环 DMA 模式下，硬件会自动重新使用 BD，不需要调用 `XAxiDma_BdRingFree()`。

### 10.3 如何知道传输完成？

- **中断方式**: 通过中断回调函数处理
- **轮询方式**: 使用 `XAxiDma_BdRingFromHw()` 轮询检查
- **状态检查**: 检查 BD 状态位中的完成标志

### 10.4 多包传输如何设置 SOF/EOF？

- **SOF**: 每个包的第一个 BD 设置 `XAXIDMA_BD_CTRL_TXSOF_MASK`
- **EOF**: 每个包的最后一个 BD 设置 `XAXIDMA_BD_CTRL_TXEOF_MASK`
- **中间 BD**: 不设置 SOF/EOF 标志

---

## 总结

BD 配置流程的核心是理解 BD 环的状态机和四个组之间的流转关系。正确配置 BD 的地址、长度和控制位，合理使用中断合并和循环模式，可以高效地实现 DMA 数据传输。

