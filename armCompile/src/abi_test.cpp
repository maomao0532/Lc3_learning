#include <iostream>

// simple_func 函数调用约定 (AAPCS64) 说明:
// 1. 参数传递:
//    - 前 8 个整型/指针参数通过寄存器 X0 - X7 传递。
//    - 超过 8 个的参数通过栈 (Stack) 传递。
//    - 浮点数参数通过 SIMD/FP 寄存器 V0 - V7 传递。
// 2. 返回值:
//    - 整型返回值存储在 X0。
//    - 浮点返回值存储在 V0。
//
// 观察点:
// - p1~p8 -> X0~X7
// - p9 -> [SP, #0]
// - p10 -> [SP, #8]
// - f1 -> S0 (或者 D0, 取决于精度)
//
// 我们定义 10 个 long 类型参数，超过了 X0-X7 的数量
// 再加一个 float 参数，观察浮点寄存器的分配
void complex_func(long p1, long p2, long p3, long p4, long p5, long p6, long p7,
                  long p8, long p9, long p10, float f1) {
  // 随便做点逻辑，防止编译器优化掉
  long sum = p1 + p2 + p8 + p9 + p10 + (long)f1;
  // 这里打个断点: b complex_func
  // info registers (查看 x0-x7, s0)
  // x/2xg $sp (查看栈上前两个参数)
}

int main() {
  // 调用 complex_func
  // 参数 1-8 放入寄存器
  // 参数 9-10 压栈
  // 浮点参数 f1 放入浮点寄存器
  complex_func(123, 2, 3, 4, 5, 6, 7, 8, 9, 10, 3.14f);
  return 0;
}