/*********************************************************************
* File: move_semantics.cpp
* Description: EXPERIMENT: Simple buffer with the rule of 5.
* Prints:
* Returns:
*********************************************************************/

/**************** Includes ****************/
#include <algorithm>
#include <cstddef>
#include <iostream>
/**************** Global Variables ****************/

/**************** Constants ****************/

/**************** Structs ****************/
struct Buffer {
    int* data;
    size_t size;

    //Constructor
    Buffer(size_t n) {
        size = n;
        data = new int[n];
    };

    // Destructor
    ~Buffer() {
        size = 0;
        delete[] data;
    };

    // Copy Constructor - deep copy,
    Buffer(const Buffer& other) {
        size = other.size;
        data = new int[size];
        std::copy(other.data, other.data + size, data);
    };

    // Copy assignment
    // Deepy copy, handle old data removal.
    Buffer& operator=(const Buffer& other) {
        if (this == &other) {
            return *this;
        }
        delete[] data;
        size = other.size;
        data = new int[size];
        std::copy(other.data, other.data + size, data);
        return *this;
    };

    // Move contstructor
    Buffer(Buffer&& other) {
        data = other.data;
        size = other.size;
        other.data = nullptr;
        other.size = 0;
    };

    // Move assignment
    Buffer& operator=(Buffer&& other) {
        if (this == &other) {
            return *this;
        }
        delete[] data;
        size = other.size;
        data = other.data;
        other.data = nullptr;
        other.size = 0;
        return *this;
    };
};

/**************** Worker Functions ****************/

/**************** Main Function ****************/
int main() {
    // Constructor
    Buffer b1(4);
    b1.data[0] = 1;
    b1.data[1] = 2;
    b1.data[2] = 3;
    b1.data[3] = 4;
    std::cout << "b1 constructed, size=" << b1.size << std::endl;

    // Copy constructor
    Buffer b2 = b1;
    std::cout << "b2 copy of b1, b2.data[0]=" << b2.data[0] << std::endl;
    std::cout << "b2 has own data (not same ptr as b1): " << (b1.data != b2.data) << std::endl;
    // Copy assignment
    Buffer b3(2);
    b3 = b1;
    std::cout << "b3 copy-assignment" << b3.data[1] << std::endl;
    std::cout << "b3 has own data (not same ptr as b1): " << (b1.data != b2.data) << std::endl;


    // Move constructor
    Buffer b4 = std::move(b1);
    std::cout << "b4 after move, data=" << b4.data << std::endl;
    std::cout << "b1 after move, data=" << b1.data << " size=" << b1.size << std::endl;

    // Move assignment
    Buffer b5(1);
    b5 = std::move(b2);
    std::cout << "b5 move-assigned from b2, b5.data[0]=" << b5.data[0] << "\n";
    std::cout << "b2 after move, data=" << b2.data << " size=" << b2.size << std::endl;

    return 0;
}