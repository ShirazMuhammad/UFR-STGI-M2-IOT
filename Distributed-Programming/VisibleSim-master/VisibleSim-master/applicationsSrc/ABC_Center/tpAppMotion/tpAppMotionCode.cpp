#include "tpAppMotionCode.hpp"

TpAppMotionCode::TpAppMotionCode(SlidingCubesBlock *host):SlidingCubesBlockCode(host),module(host) {
    if (not host) return;
}

void TpAppMotionCode::startup() {
    console << "start block " << getId() << "\n";
}

void TpAppMotionCode::parseUserBlockElements(TiXmlElement *config) {
}

void TpAppMotionCode::parseUserElements(TiXmlDocument *config) {
}

void TpAppMotionCode::onMotionEnd() {
}

void TpAppMotionCode::onGlDraw() {
}