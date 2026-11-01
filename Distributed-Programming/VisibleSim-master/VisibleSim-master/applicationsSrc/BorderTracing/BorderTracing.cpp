/**
* @file BorderTracing.cpp
 * @author Muhammad Shiraz
 * @date October 18, 2026
 * @brief Entry point: distributed border tracing on Blinky Blocks (VisibleSim).
 *
 * Run, for example:   ./BorderTracing -c config.xml
 * (add -p to run without the graphical interface; the report is printed on exit).
 */
#include <cstdlib>
#include <iostream>

#include "blinkyBlocksSimulator.h"
#include "BorderTracingCode.hpp"

using namespace BlinkyBlocks;

int main(int argc, char **argv) {
    createSimulator(argc, argv, BorderTracingCode::buildNewBlockCode);

    std::atexit(BorderTracingCode::printGlobalReport);

    getSimulator()->printInfo();
    BaseSimulator::getWorld()->printInfo();
    deleteSimulator();
    return 0;
}