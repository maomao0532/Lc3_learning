#include <cstdint>
#include <iostream>

using namespace std;

// 枚举不同种类型
enum class Type : uint64_t { Int = 0, String, Object };

class HeapObject {
public:
private:
  // 0..1:GC mark; 2..17:TypeID; 18..63:对象大小
  uint64_t header;
};

