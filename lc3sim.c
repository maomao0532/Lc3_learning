#include <stdio.h>
#include <stdint.h>

// LC-3存储65536个字节，每个地址用16bit表示
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

// 寄存器定义，8个通用，1个程序计数器，1个条件标志寄存器
