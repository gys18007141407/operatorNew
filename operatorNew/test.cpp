#include <iostream>
#include <list>
#include <mutex>
#include "syn.h"


/*

你定义的类重载了new运算符，所以当你使用new来定义该类的对象的时候，首先会执行你定义的重载函数( operator new( size_t ) )，
而你定义的重载函数里面最后一条语句又调用了 全局的new（通过::new， 前面的::表示全局），全局的new首先在堆上分配内存，然后在调用构造函数，最后返回到你定义的重载函数，你的重载函数返回，又调用一次构造函数。
你每次使用new运算符都要调用构造函数的，不管new有没有被重载，因此new也可以用来实现二次调用构造函数，实现委托构造函数。
所以你的代码中只创建了一个对象，并不是两个对象，只是构造函数被调用两次而已。*/

/*对于new A：1. 申请内存（通过operator new），大小为sizeof(A)。2. 调用A构造函数*/
/*对于delete pA：1. 调用A析构函数。2. 释放内存（通过operator delete）。*/

/*对于new A[]：1. 根据数组大小，申请内存空间（通过operator new[]），并记录该大小（在deletep [] pA 时用到）。2. 为构造所需数量的对象，也就是调用数组大小次数的构造函数。*/
/*对于delete [] pA：1. 根据之前存储的数组大小，调用相应次数的析构函数。2. 释放空间（通过operator delete[]）。*/


/*when use new[] or delete[], there are 8 bytes to save array infomation unless A is builtin type or A only have default deconstructor*/


extern std::atomic<bool> _syn;

class A{
public:
    int a;
    A(){std::cout << "A defult construct"<< std::endl;}
    ~A(){std::cout << "A defult deconstruct"<< std::endl;}
};

class MemoryLeak{
public:
    void deletea(void*);
    void deletev(void*);
    MemoryLeak(){
        _syn.store(true, std::memory_order_acq_rel);
    }
    ~MemoryLeak(){
        for(auto& c : this->_memory_list){
            std::cout << c.addr << " leak " << c.len << std::endl;
            
             free(c.addr);
            // if array will core dump
        }
        _syn.store(false, std::memory_order_release);

    }

    struct listNode{
        void* addr;
        int len;
        bool isarray;
    };

    void insert(void* p, int n, bool flag){
        std::cout << "ml insert " << p << std::endl;
        std::lock_guard<std::mutex> g_lock(this->_mt);
        _memory_list.push_back({p,n, flag});
    }

    void remove(void* p){
        std::cout << "ml remove " << p << std::endl;
        std::lock_guard<std::mutex> g_lock(this->_mt);
        _memory_list.remove_if([p](listNode& x)->bool{
            return x.addr == p;
        });
        
    }

private:
    std::list<listNode> _memory_list;
    std::mutex _mt;
    std::atomic<bool> _valid;

};

MemoryLeak ml;

// operator new(sizeof(T), a, b, c, ...)   == new(a, b, c, ...) T;
void* operator new(std::size_t n, const char name[], int line, MemoryLeak& ml = ml){
    std::cout << "my new func" << std::endl;
    std::cout << "n = " << n << " , line = " << line << " , file = " << name << std::endl;
    void* p = ::malloc(n);
    std::cout << "new get addr " << p << std::endl;
    ml.insert(p, n, false);
    return p;
}

void* operator new[](std::size_t n, const char name[], int line, MemoryLeak& ml = ml){
    std::cout << "my new func[]" << std::endl;
    std::cout << "n = " << n << " , line = " << line << " , file = " << name << std::endl;
    void* p = ::malloc(n);
    std::cout << "new[] get addr " << p << std::endl;
    ml.insert(p, n, true);

    return p;
}
void operator delete(void* p){
    std::cout << "my delete func " << std::endl;
    if(_syn)ml.remove(p);   // after ml deconstructed, before all programs finished(some memory need to be delete), call ml.remove will core dump. (because these memory malloc before ml construct)
    free(p);
    p = nullptr;
}

void operator delete[](void* p){
    std::cout << "my delete func[] " << std::endl;
    if(_syn)ml.remove(p);
    free(p);
}


#define new new(__FILE__, __LINE__)



int main(){

    std::cout << sizeof(A) << std::endl;

    A* t = new A;
    std::cout << "get addr " << t << std::endl;
    std::cout << "========================================" << std::endl;

    A* addr = new A[3]; 
    std::cout << "get addr " << addr << std::endl;

    std::cout << "========================================" << std::endl;

    std::cout << "**************" << std::endl;
    delete t;
    std::cout << "**************" << std::endl;
    delete[] (addr); // if delete [], addr must be A* type, otherwise it will call delete instead of delete[]
    std::cout << "**************" << std::endl;


    int* buildin = new int[1];
    std::cout << "get addr " << buildin << std::endl;
    std::cout << "**************" << std::endl;
    A* temp = new A;
    std::cout << "get addr " << temp << std::endl;
    std::cout << "**************" << std::endl;
    A* temp2  =new A[5];
    std::cout << "get addr " << temp2 << std::endl;

    return 0;
}