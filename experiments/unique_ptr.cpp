/*********************************************************************
* File: unique_ptr.cpp
* Description: Basic unique pointer implementation.
* Prints:
* Returns:
*********************************************************************/

/**************** Includes ****************/
#include <iostream>

/**************** Global Variables ****************/

/**************** Constants ****************/

/**************** Structs ****************/

struct Point {
    int x;
    int y;
};

/**************** Classes *************************/

template<typename T>
class UniquePtr {
private:
    T* ptr;

public:
    // Constructor
    explicit UniquePtr(T* p) {
        ptr = p;
    };

    // Destructor
    ~UniquePtr() {
        delete ptr;
    };

    // Prevent copy - this is a UNIQUE pointer
    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;

    // Move constructor
    UniquePtr(UniquePtr&& other) noexcept {
        ptr = other.ptr;
        other.ptr = nullptr;
    };

    // Move assignment
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    T* get() const { return ptr; }

    T* release() {
        T* temp = ptr;
        ptr = nullptr;
        return temp;
    };

    void reset(T* p = nullptr) {
        delete ptr;
        ptr = p;
    };

    T& operator*() const {return *ptr; }

    T* operator->() const { return ptr; }
};


/**************** Worker Functions ****************/

/**************** Main Function ****************/
int main() {
    // Test constructor:
    int* x = new int(5);
    UniquePtr<int> ptr_x = UniquePtr(x);
    std::cout << "Value of pointer should be 5, and is: " << *ptr_x.get() << " At address: " << ptr_x.get() << std::endl;

    // Move constructor
    UniquePtr<int> ptr_y = std::move(ptr_x);
    std::cout << "Value of pointer y should be 5, and is: " << *ptr_y.get() << " At address: " << ptr_y.get() << std::endl;
    std::cout << "Value of pointer x should be nullptr, and is: " << ptr_x.get() << std::endl;

    // Move assignment
    ptr_x = std::move(ptr_y);
    std::cout << "Value of pointer x should be 5, and is: " << *ptr_x.get() << " At address: " << ptr_x.get() << std::endl;
    std::cout << "Value of pointer y should be nullptr, and is: " << ptr_y.get() << std::endl;

    int* raw_ptr = ptr_x.release();
    std::cout << "Value of raw pointer is: " << *raw_ptr << " At address: " << raw_ptr << std::endl;

    int* z = new int(10);
    UniquePtr<int> ptr_z = UniquePtr(z);
    ptr_z.reset();
    std::cout << "Value of pointer z should be null and is: " << ptr_z.get() << std::endl;

    Point* point = new Point(1, 2);
    UniquePtr<Point> point_ptr(point);
    std::cout << "Value of point x should be 1 and is: " << point_ptr->x << std::endl;
    std::cout << "Value of point y should be 2 and is: " << point_ptr->y << std::endl;


    return 0;
}