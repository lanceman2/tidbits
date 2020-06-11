#include <iostream>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// C++11
//apt install python3-pybind11 python3-dev

/**
 * Generates a sequence of prime numbers
 * @param how_many   how many primes to generate
 */
std::vector<unsigned int> generate(unsigned int how_many) {
    std::vector<unsigned int> primes;
    primes.reserve(how_many);
    primes.push_back(2); //2 is the first prime number
    unsigned int candidate = 3;
    while(primes.size() < how_many){
        bool potential_prime = true;
        for(unsigned int prime: primes){
            if(candidate % prime == 0){
                candidate += 2;
                potential_prime = false;
                break;
            }
        }
        if(potential_prime)
            primes.push_back(candidate);
    }
    return primes;
}

#ifdef MAIN

int main(void) {

//#define TEST
#ifdef TEST
    std::vector<unsigned int> primes = generate(30);
    for(unsigned int prime : primes){
        std::cout << prime << std::endl;
    }
#else
    generate(50000);
#endif
    return 0;
}

#else
namespace py = pybind11;

/**
 * Generate the python bindings for this C++ function
 */
PYBIND11_PLUGIN(primes_py3) {
    py::module m("example", "Generating primes in c++ with "
            "python bindings using pybind11");
    m.def("generate_primes", &generate,
            "A function which generates a list of primes.  "
            "The length of this list is specified by the user");
    return m.ptr();
}

#endif
