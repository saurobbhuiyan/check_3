#include <iostream>

int main() {

#if defined(_WIN32)
    std::cout << "Operating System: Windows\n";

#elif defined(__linux__)
    std::cout << "Operating System: Linux\n";

#else
    std::cout << "Operating System: Unknown\n";

#endif
    return 0;
}