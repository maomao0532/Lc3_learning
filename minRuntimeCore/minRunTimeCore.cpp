#include <iostream>
#include <string>
#include <vector>

using namespace std;

/***************************Tagged Pointer******************************/
/*
    标签指针，实现万能变量，Value既可以表示整数，也可以是指针（对象）
*/
using Value = uint64_t; // 最后一位是1就是整数；最后一位是0就是指针

class MyObject {
public:
    MyObject(string name = " ", int age = 0) {
        this->name = name;
        this->age = age;
    }
    string getName() {return this->name;}
    int getAge() {return this->age;}

private:
    string name;
    int age;
};

Value createInt(int num) {
    Value v = 0;
    v = (num << 1) | 0x1;
    return v;
}

int getInt(Value v) {
    int num = 0;
    num = v >> 1;
    return num;
}

Value createPtr(MyObject* obj) {
    Value x = 0;
    x = (Value)obj << 1;    // MyObject* 64位，低48位有效
    return x;
}

int main() {
    MyObject* obj = new MyObject("liudachuan", 23);
    vector<Value> vec;
    Value num = createInt(-42);
    Value objPoint = createPtr(obj);
    vec.push_back(num);
    vec.push_back(objPoint);

    cout << "num: " << getInt(vec[0]) << endl;
    cout << "name: " << ((MyObject*)(objPoint >> 1))->getName() << endl;
    cout << "age: " << ((MyObject*)(objPoint >> 1))->getAge() << endl;

}



