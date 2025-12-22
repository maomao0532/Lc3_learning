#include <stdio.h>
#include <stdint.h>
#include <signal.h>
/* unix only */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>


// LC-3存储65536个字节，每个地址用16bit表示
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

// 寄存器定义，8个通用，1个程序计数器，1个条件标志寄存器
enum {
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT,
};
uint16_t reg[R_COUNT];

// 补充两个内存直接访问寄存器
enum {
    MR_KBSR = 0xFE00,   /* keyboard status */
    MR_KBDR = 0xFE02    /* keyboard data */
};

// 16条指令
enum {
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

// 标志位，3种分别表示正、0、负
enum {
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

uint16_t running = 1;


/********************************IO操作函数********************************/
struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}


/********************************指令集********************************/
// 符号扩展
uint16_t sign_extend(uint16_t x, int bit_count) {
    // 取符号位
    if (x >> (bit_count - 1) & 1) {
        // 负号 补1
        x |= (0xFFFF << bit_count);
    }
    // 正数不需要操作
    return x;
}

// 标志位更新
uint16_t update_flags(uint16_t r) {
    if (reg[r] == 0) 
        reg[R_COND] = FL_ZRO;
    else if (reg[r] >> 15) 
        reg[R_COND] = FL_NEG;
    else 
        reg[R_COND] = FL_POS; 
}

// 读取memory
uint16_t mem_read(uint16_t add) {
    if (add == MR_KBSR) {
        // 读取键盘状态
        if (check_key()) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else 
            memory[MR_KBSR] = 0;
    }
    return memory[add];
}

// 写入memory
void mem_write(uint16_t add, uint16_t data) {
    memory[add] = data;
}

// ADD
void ADD(uint16_t instr) {
    uint16_t reg_DR = (instr >> 9) & 0x0007;
    uint16_t reg_SR1 = (instr >> 6) & 0x0007;
    if ((instr >> 5) & 1) {
        // 立即数
        uint16_t imm5 = sign_extend(instr & 0x001F, 5);
        reg[reg_DR] = reg[reg_SR1] + imm5;
    }
    else {
        uint16_t reg_SR2 = instr & 0x0007;
        reg[reg_DR] = reg[reg_SR1] + reg[reg_SR2];
    }
    update_flags(reg_DR);
}

// LDI
void LDI(uint16_t instr) {
    uint16_t DR = (instr >> 9) & 0x7;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    reg[DR] = mem_read(mem_read(reg[R_PC] + offset));
    update_flags(DR);
}

// AND
void AND(uint16_t instr) {
    uint16_t DR = (instr >> 9) & 0x7;
    uint16_t SR1 = (instr >> 6) & 0x7;
    if ((instr >> 5) & 0x1) {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[DR] = reg[SR1] & imm5;
    }
    else {
        uint16_t SR2 = instr & 0x7;
        reg[DR] = reg[SR1] & reg[SR2];
    }
    update_flags(DR);
}

// BR
void BR(uint16_t instr) {
    uint16_t nzp = (instr >> 9) & 0x7;
    if (nzp & reg[R_COND]) {
        uint16_t offset = sign_extend(instr & 0x1FF, 9);
        reg[R_PC] += offset;
    }
}

// JMP
void JMP(uint16_t instr) {
    uint16_t BaseR = (instr >> 6) & 0x7;
    reg[R_PC] = reg[BaseR];
}


// JSR
void JSR(uint16_t instr) {
    reg[R_R7] = reg[R_PC];
    if ((instr >> 11) & 0x1) {
        uint16_t offset = sign_extend(instr & 0x07FF, 11);
        reg[R_PC] += offset;
    }
    else {
        uint16_t BaseR = (instr >> 6) & 0x7;
        reg[R_PC] = reg[BaseR];
    }
}

// LD
void LD(uint16_t instr) {
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    uint16_t DR = (instr >> 9) & 0x7;
    reg[DR] = mem_read(reg[R_PC] + offset);
    update_flags(DR);
}

// LDR
void LDR(uint16_t instr) {
    uint16_t DR = (instr >> 9) & 0x7;
    uint16_t BaseR = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    reg[DR] = mem_read(reg[BaseR] + offset);
    update_flags(DR);
}

// LEA
void LEA(uint16_t instr) {
    uint16_t DR = (instr >> 9) & 0x7;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    reg[DR] = reg[R_PC] + offset;
    update_flags(DR);
}

// NOT
void NOT(uint16_t instr) {
    uint16_t DR = (instr >> 9) & 0x7;
    uint16_t SR = (instr >> 6) & 0x7;
    reg[DR] = ~reg[SR];
    update_flags(DR);
}

// ST
void ST(uint16_t instr) {
    uint16_t SR = (instr >> 9) & 0x7;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    mem_write(reg[R_PC] + offset, reg[SR]);
}

// STI
void STI(uint16_t instr) {
    uint16_t SR = (instr >> 9) & 0x7;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);
    mem_write(mem_read(reg[R_PC] + offset), reg[SR]);
}

// STR
void STR(uint16_t instr) {
    uint16_t SR = (instr >> 9) & 0x7;
    uint16_t BaseR = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    mem_write(reg[BaseR] + offset, reg[SR]);
}

enum {
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

// TRAP PUTS 打印R0为起始地址的string
void trap_puts() {
    uint16_t* c = memory + reg[R_R0];
    while (*c) {
        putc((char)*c, stdout);
        ++c;
    }
    fflush(stdout);
}

// TRAP GETC
void trap_getc() {
    reg[R_R0] = (uint16_t)getchar();
    update_flags(R_R0);
}

// TRAP OUT 打印R0内容
void trap_out() {
    putc((char)reg[R_R0], stdout);
    fflush(stdout);
}

// TRAP IN 读取一个char，并存在R0中
void trap_in() {
    printf("Enter a character: ");
    char c = getchar();
    putc(c, stdout);
    fflush(stdout);
    reg[R_R0] = (uint16_t)c;
    update_flags(R_R0);
}

// TRAP PUTSP
void trap_putsp() {
    // 16位存两个char
    uint16_t* c = memory + reg[R_R0];
    while (*c) {
        char c1 = (*c) & 0xFF;
        putc(c1, stdout);
        char c2 = (*c) >> 8;
        if (c2) putc(c2, stdout);
        ++c;
    }
    fflush(stdout);
}

// TRAP HALT 终止程序
void trap_halt() {
    puts("HALT");
    fflush(stdout);
    running = 0;
}

// TRAP
void TRAP(uint16_t instr) {
    reg[R_R7] = reg[R_PC];
    switch (instr & 0xFF) {
        case TRAP_GETC:
            trap_getc();
            break;
        case TRAP_OUT:
            trap_out();
            break;
        case TRAP_PUTS:
            trap_puts();
            break;
        case TRAP_IN:
            trap_in();
            break;
        case TRAP_PUTSP:
            trap_putsp();
            break;
        case TRAP_HALT:
            trap_halt();
            break;
    }
}


/********************************加载程序********************************/
uint16_t swap16(uint16_t x) {
    // LC3是大端序，现在默认机器都为小端序
    return x >> 8 | x << 8;
}

void read_image_file(FILE* file) {
    // LC3可执行文件格式[第1个uint16_t: origin(加载地址)] [指令1] [指令2] [指令3] ... [指令N]
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    // 最多可读取指令条数
    uint16_t read_max = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    fread(p, sizeof(uint16_t), read_max, file);

    while (read_max--) {
        *p = swap16(*p);
        ++p;
    }
}

// 从路径加载
int read_image(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return 0;
    read_image_file(file);
    return 1;
}

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

int main(int argc, char* argv[]) {
    // 处理输入
    if (argc < 2) {
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }
    for (int j = 1; j < argc; ++j) {
        if (!read_image(argv[j])) {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    // 输入处理
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    // 初始化
    reg[R_COND] = FL_ZRO;
    enum {PC_START = 0x3000};
    reg[R_PC] = PC_START;

    while (running) {
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;
        switch(op) {
            case OP_ADD:
                ADD(instr);
                break;
            case OP_AND:
                AND(instr);
                break;
            case OP_NOT:
                NOT(instr);
                break;
            case OP_BR:
                BR(instr);
                break;
            case OP_JMP:
                JMP(instr);
                break;
            case OP_JSR:
                JSR(instr);
                break;
            case OP_LD:
                LD(instr);
                break;
            case OP_LDI:
                LDI(instr);
                break;
            case OP_LDR:
                LDR(instr);
                break;
            case OP_LEA:
                LEA(instr);
                break;
            case OP_ST:
                ST(instr);
                break;
            case OP_STI:
                STI(instr);
                break;
            case OP_STR:
                STR(instr);
                break;
            case OP_TRAP:
                TRAP(instr);
                break; 
            case OP_RES:
            case OP_RTI:
            default:
                abort();
                break;
        }
    }
    restore_input_buffering();
}









