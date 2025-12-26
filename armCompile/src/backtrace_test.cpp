#include <cstdio>

// 手动回溯调用栈 (Manual Backtrace) 原理:
// ARM64 的栈帧 (Stack Frame) 结构通常如下 (高地址 -> 低地址):
// ...
// [ Link Register (X30) ]  <- FP + 8  (保存返回地址)
// [ Frame Pointer (X29) ]  <- FP      (保存上一个栈帧的 FP)
// ... 局部变量 ...
//
// 只要拿到当前函数的 FP (Frame Pointer)，就可以顺藤摸瓜找到所有调用者的 FP 和
// LR。 *FP       = Previous FP
// *(FP + 8) = Return Address (LR)

void C() {
  printf("Inside C\n");

  // 1. 获取当前栈帧指针 (FP / x29)
  // __builtin_frame_address(0) 是 GCC/Clang 内置函数，返回当前函数的 frame
  // address
  void **fp = (void **)__builtin_frame_address(0);

  printf("Backtrace:\n");
  int depth = 0;
  while (fp != nullptr) {
    // 2. 获取返回地址 (LR)
    // LR 位于 FP + 8 字节处 (64位系统指针占8字节，所以是 fp[1])
    void *lr = fp[1];

    // 3. 获取上一个栈帧的 FP
    // FP 指向的位置存放的就是上一个 FP (所以是 fp[0])
    void **next_fp = (void **)fp[0];

    printf("#%d: LR = %p, FP = %p\n", depth++, lr, fp);

    // 简单的终止条件，防止死循环 (实际场景需要更严谨的边界检查)
    if (next_fp <= fp)
      break;
    fp = next_fp;
  }
}

void B() { C(); }

void A() { B(); }

int main() {
  A();
  return 0;
}