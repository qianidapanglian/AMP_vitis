#include "user.h"
/*****************************************************************************/
/**
*
* 清除内存数据函数
*
* 本函数将指定内存区域的所有数据清零。适用于DMA缓冲区初始化、
* 测试数据清除等场景。
*
* @param	StartAddr 内存起始地址（UINTPTR类型，支持32位和64位地址）
* @param	Length 要清除的字节长度
*
* @return	无
*
* @note	函数会自动处理缓存一致性，清零后会刷新数据缓存，
*		确保DMA或其他硬件能访问到最新的数据。
*
******************************************************************************/
void ClearMemory(UINTPTR StartAddr, u32 Length)
{
	u8 *MemPtr;
	u32 *MemPtr32;
	u32 Index;
	u32 AlignedLength;
	u32 Remainder;
	u32 OriginalLength;

	/* 参数检查 */
	if (StartAddr == 0 || Length == 0) {
		return;
	}

	/* 保存原始长度，用于缓存刷新 */
	OriginalLength = Length;

	/* 将地址转换为字节指针 */
	MemPtr = (u8 *)StartAddr;

	/* 优化：对于大块内存，使用32位对齐写入以提高效率 */
	if (Length >= 4) {
		/* 处理地址不对齐的前几个字节 */
		Remainder = StartAddr & 0x3;
		if (Remainder != 0) {
			Remainder = 4 - Remainder;
			for (Index = 0; Index < Remainder && Index < Length; Index++) {
				MemPtr[Index] = 0;
			}
			MemPtr += Remainder;
			Length -= Remainder;
		}

		/* 32位对齐清零（更高效） */
		if (Length >= 4) {
			MemPtr32 = (u32 *)MemPtr;
			AlignedLength = Length / 4;
			for (Index = 0; Index < AlignedLength; Index++) {
				MemPtr32[Index] = 0;
			}
			MemPtr += AlignedLength * 4;
			Length -= AlignedLength * 4;
		}
	}

	/* 处理剩余的不对齐字节 */
	for (Index = 0; Index < Length; Index++) {
		MemPtr[Index] = 0;
	}

	/* 刷新数据缓存，确保DMA能访问到清零后的数据
	 * 这是因为CPU可能把数据放在缓存里，但DMA直接访问内存
	 */
	Xil_DCacheFlushRange(StartAddr, OriginalLength);
}

//打印u32数据，到vofa中查看波形
void user_printf_u32(u32* baseaddr,u32 length_u32)
{
	u32* addr_cur = baseaddr;
	for(int i=0;i < length_u32;i++)
	{
		xil_printf("sb: %d\r\n",addr_cur[i]);
	}
}

/**
 * @brief 对指定段u32数据，变为1280-该数。
 * @param length_u32,u32数据的个数
 */
void user_data_process(u32* baseaddr,u32 length_u32)
{
	u32* addr_cur = baseaddr;
	for(int i=0;i < length_u32;i++)
	{
		addr_cur[i] = length_u32 - addr_cur[i];
	}
	Xil_DCacheFlushRange(baseaddr,length_u32 * sizeof(u32));
}

/**
 * @brief 对指定段u32数据，全部填充指定的单一数据
 * @param length_u32,u32数据的个数
 */
void user_data_filedwithDC(u32* baseaddr,u32 length_u32,u32 data_filled)
{
	u32* addr_cur = baseaddr;
	for(int i=0;i < length_u32;i++)
	{
		addr_cur[i] = data_filled;
	}
	Xil_DCacheFlushRange(baseaddr,length_u32 * sizeof(u32));
}