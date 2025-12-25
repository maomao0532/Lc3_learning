#include <atomic>
#include <cstdint>
#include <sys/types.h>

using namespace std;

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
class LockFreeStack {
public:
  void push(int val) {
    Node *newNode = new Node(val);
    newNode->next = top;
    while (!top.compare_exchange_weak(newNode->next, newNode,
                                      std::memory_order_release,
                                      std::memory_order_relaxed))
      ;
  }

private:
  std::atomic<Node *> top;
};

class ObjectHeader {
public:
  ObjectHeader(uint64_t mark_word) { this->mark_word = mark_word; }
  void AtomicSetMarkBit(uint64_t bitMask) {
    uint64_t oldWord = mark_word.load(std::memory_order_relaxed);
    uint64_t newWord = oldWord | bitMask;
    while (!this->mark_word.compare_exchange_weak(oldWord, newWord,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
      newWord = oldWord | bitMask;
    }
  }

private:
  std::atomic<uint64_t> mark_word;
};

int main() {}
