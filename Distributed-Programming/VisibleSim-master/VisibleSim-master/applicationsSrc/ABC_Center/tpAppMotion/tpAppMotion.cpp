#include <iostream>
#include "tpAppMotionCode.hpp"

using namespace std;
using namespace SlidingCubes;

int main(int argc, char **argv) {
    try {
        createSimulator(argc, argv, TpAppMotionCode::buildNewBlockCode);
        getSimulator()->printInfo();
        BaseSimulator::getWorld()->printInfo();
        deleteSimulator();
    } catch(std::exception const& e) {
        cerr << "Uncaught exception: " << e.what();
    }
    return 0;
}