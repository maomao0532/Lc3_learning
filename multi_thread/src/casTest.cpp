#include <atomic>
#include <cstdint>
#include <sys/types.h>

using namespace std;

// 简单的链表节点结构
struct Node {
  int data;
  Node *next;
  Node() {
    data = 0;
    next = nullptr;
  }
  Node(int data) {
    this->data = data;
    this->next = nullptr;
  }
};

/**
 * @brief 无锁栈实现示例
 *
 * 使用 CAS (Compare-And-Swap) 操作来实现线程安全的入栈操作，无需使用互斥锁。
 */
class LockFreeStack {
public:
  /**
   * @brief 入栈操作
   * @param val 要压入的数据
   *
   * 原理：
   * 1. 创建新节点。
   * 2. 读取当前栈顶 top 到新节点的 next 指针。
   * 3. 使用 compare_exchange_weak (CAS) 尝试更新 top：
   *    - 如果 top 仍然等于 newNode->next (说明在此期间没有其他线程修改 top)，
   *      则将 top 更新为 newNode，操作成功，循环结束。
   *    - 如果 top 不等于 newNode->next (说明其他线程修改了 top)，
   *      CAS 失败，将 top 的最新值加载到 newNode->next 中，并返回
   * false，循环继续重试。
   */
  void push(int val) {
    Node *newNode = new Node(val);
    // 乐观地假设当前 top 就是我们要挂载的位置
    newNode->next = top;

    // CAS 循环：不断尝试直到成功
    // compare_exchange_weak 参数说明：
    // 1. expected (newNode->next): 期望的当前值。如果 *this == expected，则修改
    // *this = desired。
    //    如果比较失败，expected 会被更新为 *this 的当前值。
    // 2. desired (newNode): 期望写入的新值。
    // 3. success (memory_order_release): CAS 成功时的内存序。Release
    // 保证之前的写操作（newNode 初始化）对其他线程可见。
    // 4. failure (memory_order_relaxed): CAS 失败时的内存序。Relaxed
    // 表示不需要特殊的同步，只需保证原子性，因为我们会在循环中重试。
    while (!top.compare_exchange_weak(newNode->next, newNode,
                                      std::memory_order_release,
                                      std::memory_order_relaxed))
      ;
  }

private:
  std::atomic<Node *> top;
};

/**
 * @brief 对象头标记示例
 *
 * 模拟 JVM 或其他运行时系统中的对象头操作，使用 CAS
 * 原子地设置标记位（如锁标志、GC 标记等）。
 */
class ObjectHeader {
public:
  ObjectHeader(uint64_t mark_word) { this->mark_word = mark_word; }

  /**
   * @brief 原子设置标记位
   * @param bitMask 要设置的位掩码
   *
   * 即使会有多个线程同时尝试设置不同的位，CAS
   * 也能保证修改不会建立在过期的值之上， 从而避免丢失更新。
   */
  void AtomicSetMarkBit(uint64_t bitMask) {
    // 1. 获取旧值 (Relaxed 即可，因为后续 CAS 会验证)
    uint64_t oldWord = mark_word.load(std::memory_order_relaxed);
    // 2. 计算新值
    uint64_t newWord = oldWord | bitMask;

    // 3. CAS 循环更新
    while (!this->mark_word.compare_exchange_weak(oldWord, newWord,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
      // 如果 CAS 失败（oldWord 被其他线程修改了），
      // oldWord 会自动被 compare_exchange_weak 更新为最新的 mark_word 值。
      // 我们需要基于这个最新的 oldWord 重新计算 newWord。
      newWord = oldWord | bitMask;
    }
  }

private:
  std::atomic<uint64_t> mark_word;
};

int main() {
  // 示例代码，仅用于编译通过性检查
  LockFreeStack stack;
  stack.push(1);
  stack.push(2);

  ObjectHeader header(0);
  header.AtomicSetMarkBit(1);

  return 0;
}
