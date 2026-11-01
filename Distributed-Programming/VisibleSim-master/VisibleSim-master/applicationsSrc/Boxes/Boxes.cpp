/*
* Boxes.cpp
 *
 * Entry point: configuration recognition via overlapping boxes on
 * Blinky Blocks (VisibleSim).
 *
 * Usage:  ./Boxes -c config.xml
 */
#include "robots/blinkyBlocks/blinkyBlocksSimulator.h"

#include "BoxesCode.hpp"

using namespace BlinkyBlocks;

int main(int argc, char **argv) {
    createSimulator(argc, argv, BoxesCode::buildNewBlockCode);

    getScheduler()->waitForSchedulerEnd();

    BoxesCode::printSummary();   // messages, boxes and simulated time

    deleteSimulator();
    return 0;
}