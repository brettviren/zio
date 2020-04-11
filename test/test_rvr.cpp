#include <iostream>

struct Thing {
    int x;

    Thing(int exs) : x(exs) {
        std::cout << "int constructor\n";
    }
    Thing(const Thing &rhs) {
        std::cout << "rvalue reference copy constructor\n";
        x = rhs.x;
    }
    Thing& operator=(Thing &rhs) {
        std::cout << "rvalue reference assignment\n";
        x = rhs.x;
        return *this;
    }
    Thing(const Thing &&rhs) {
        std::cout << "rvalue reference copy constructor\n";
        x = rhs.x;
    }
    Thing& operator=(Thing &&rhs) {
        std::cout << "rvalue reference assignment\n";
        x = rhs.x;
        return *this;
    }
    ~Thing() {
        std::cout << "destructor\n";
    }
};

Thing share_my_thing()
{
    Thing t{42};
    //return std::move(t);
    return t;
}

void use_thing(Thing&& thing = Thing{0})
{
    std::cout << thing.x << std::endl;
    thing.x = -1;
}

int main()
{
    use_thing(share_my_thing());
    use_thing();
    Thing t{69};
    use_thing(std::move(t));
    std::cout << t.x << std::endl;
}
