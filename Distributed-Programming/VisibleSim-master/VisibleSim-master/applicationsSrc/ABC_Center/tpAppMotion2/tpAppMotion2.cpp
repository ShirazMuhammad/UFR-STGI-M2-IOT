/**
* @file tpAppMotion2.cpp
 * @author Muhammad Omais Rana, Muhammad Shiraz
 **/

#include <iostream>
#include "tpAppMotionCode2.hpp"

using namespace std;
using namespace SlidingCubes;

int main(int argc, char **argv) {
    try {
        createSimulator(argc, argv, TpAppMotion2Code::buildNewBlockCode);
        getSimulator()->printInfo();
        BaseSimulator::getWorld()->printInfo();
        deleteSimulator();
    } catch(std::exception const& e) {
        cerr << "Uncaught exception: " << e.what();
    }

    return 0;
}