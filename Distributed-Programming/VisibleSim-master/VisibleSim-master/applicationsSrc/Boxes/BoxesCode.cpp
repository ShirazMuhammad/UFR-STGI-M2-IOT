/*
 * BoxesCode.cpp
 *
 * Distributed overlapping-boxes algorithm for Blinky Blocks (VisibleSim).
 * See BoxesCode.hpp for the coordinate conventions.
 *
 * Phases
 *   1. SET_D      : every module learns its rank d in its vertical line.
 *   2. LEFT_INFO  : every module gives (d, hasFront) to its right neighbour so
 *                   that the bottom module of a column can evaluate Eq. (2).
 *   3. Rmin       : empty(front) && (d != dLeft || !empty(left+front)).
 *   4. FIND_W/SET_W: an Rmin module measures its width w along its row.
 *   5. LAYER_INFO : (isRmin, d, w) is sent to the module above (+Z).
 *   6. Cmin       : Rmin && !(Rmin(bottom) && d == dBottom && w == wBottom).
 *   7. FIND_H/SET_H: a Cmin module measures the height h of its stack.
 *   8. Box        : ({x,y,z}, {x+w-1, y+d-1, z+h-1}).
 *
 * The algorithm is asynchronous: a message that needs a value which is not
 * yet known is stored and handled as soon as the value is set.
 */
#include "BoxesCode.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "events/scheduler.h"
#include "grid/lattice.h"

using namespace BlinkyBlocks;
using std::placeholders::_1;
using std::placeholders::_2;

namespace {

const Time MSG_DELAY = 100;     // microseconds of simulated time before a send

const float PALETTE[][3] = {
    {0.55f, 0.78f, 0.25f}, {0.98f, 0.82f, 0.12f}, {0.13f, 0.62f, 0.85f},
    {0.88f, 0.17f, 0.52f}, {0.97f, 0.60f, 0.15f}, {0.00f, 0.72f, 0.72f},
    {0.62f, 0.50f, 0.85f}, {0.60f, 0.38f, 0.22f}, {0.85f, 0.10f, 0.10f},
    {0.45f, 0.62f, 0.90f}, {0.30f, 0.30f, 0.80f}, {0.20f, 0.55f, 0.30f}};
const int PALETTE_SIZE = sizeof(PALETTE) / sizeof(PALETTE[0]);

const bool COLOR_BOXES = true;  // set to false to disable the visual flood

} // namespace

// ---------------------------------------------------------------------------
// Static statistics
// ---------------------------------------------------------------------------
unsigned long BoxesCode::algoMsgCount[MSG_BOX_COLOR - MSG_SET_D] = {0};
unsigned long BoxesCode::vizMsgCount = 0;
Time BoxesCode::lastBoxTime = 0;
std::vector<FoundBox> BoxesCode::foundBoxes;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
BoxesCode::BoxesCode(BlinkyBlocksBlock *host)
    : BlinkyBlocksBlockCode(host), self(host) {
    nbr.fill(nullptr);
    d = 0;
    leftExists = false; leftKnown = false; leftD = 0; leftHasFront = false;
    rminKnown = false; isRmin = false;
    wKnown = false; w = 0;
    belowKnown = false; belowRmin = false; belowD = 0; belowW = 0;
    decided = false; isCmin = false; h = 0;

    addMessageEventFunc2(MSG_SET_D,     std::bind(&BoxesCode::handleSetD,      this, _1, _2));
    addMessageEventFunc2(MSG_LEFT_INFO, std::bind(&BoxesCode::handleLeftInfo,  this, _1, _2));
    addMessageEventFunc2(MSG_FIND_W,    std::bind(&BoxesCode::handleFindW,     this, _1, _2));
    addMessageEventFunc2(MSG_SET_W,     std::bind(&BoxesCode::handleSetW,      this, _1, _2));
    addMessageEventFunc2(MSG_LAYER_INFO,std::bind(&BoxesCode::handleLayerInfo, this, _1, _2));
    addMessageEventFunc2(MSG_FIND_H,    std::bind(&BoxesCode::handleFindH,     this, _1, _2));
    addMessageEventFunc2(MSG_SET_H,     std::bind(&BoxesCode::handleSetH,      this, _1, _2));
    addMessageEventFunc2(MSG_BOX_COLOR, std::bind(&BoxesCode::handleBoxColor,  this, _1, _2));
}

BoxesCode::~BoxesCode() {}

// ---------------------------------------------------------------------------
// Local neighbourhood: map every connected interface to its relative offset
// ---------------------------------------------------------------------------
void BoxesCode::discoverNeighbours() {
    nbr.fill(nullptr);
    for (int i = 0; i < 6; ++i) {
        P2PNetworkInterface *itf = self->getInterface((SCLattice::Direction)i);
        if (itf == nullptr || !itf->isConnected()) continue;
        const Cell3DPosition &p = itf->connectedInterface->hostBlock->position;
        int dx = (int)p[0] - (int)self->position[0];
        int dy = (int)p[1] - (int)self->position[1];
        int dz = (int)p[2] - (int)self->position[2];
        if (std::abs(dx) + std::abs(dy) + std::abs(dz) == 1)
            nbr[offsetIndex(dx, dy, dz)] = itf;
    }
}

// ---------------------------------------------------------------------------
// Start-up
// ---------------------------------------------------------------------------
void BoxesCode::startup() {
    discoverNeighbours();

    leftExists = has(-1, 0, 0);
    leftKnown = !leftExists;               // nothing to wait for if there is no left module

    // A module that has a front neighbour is never an Rmin position.
    if (has(0, -1, 0)) declareNotRmin();

    // The module with no back neighbour starts the ranking of its line.
    if (!has(0, 1, 0)) {
        d = 1;
        onDSet();
    }
}

// ---------------------------------------------------------------------------
// Phase 1-3 : d, left information, Rmin
// ---------------------------------------------------------------------------
void BoxesCode::handleSetD(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<SetDMsg>(_msg);
    d = m->d + 1;
    onDSet();
}

void BoxesCode::onDSet() {
    if (has(0, -1, 0)) sendAlgo(new SetDMsg(d), at(0, -1, 0));
    if (has(1, 0, 0))  sendAlgo(new LeftInfoMsg(d, has(0, -1, 0)), at(1, 0, 0));

    // FIND_W messages that arrived before d was known can be served now.
    std::vector<std::pair<int, int>> pending;
    pending.swap(pendingFindW);
    for (auto &p : pending) processFindW(p.first, p.second);

    tryRmin();
}

void BoxesCode::handleLeftInfo(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<LeftInfoMsg>(_msg);
    leftD = m->d;
    leftHasFront = m->hasFront;
    leftKnown = true;
    tryRmin();
}

// empty(front) && (d != dLeft || !empty(left + front))      -- Eq. (2)
void BoxesCode::tryRmin() {
    if (rminKnown || d == 0 || !leftKnown) return;
    if (has(0, -1, 0)) return;             // not a bottom module of its line
    isRmin = !leftExists || (leftD != d) || leftHasFront;
    rminKnown = true;
    if (isRmin) startW();
    else        declareNotRmin();
}

void BoxesCode::declareNotRmin() {
    if (decided) return;
    rminKnown = true;
    isRmin = false;
    isCmin = false;
    decided = true;
    sendLayerInfo(false, 0, 0);
    flushPendingH();
}

// ---------------------------------------------------------------------------
// Phase 4 : width
// ---------------------------------------------------------------------------
void BoxesCode::startW() {
    if (has(1, 0, 0)) {
        sendAlgo(new FindWMsg(self->blockId, d), at(1, 0, 0));
    } else {
        w = 1;
        onWSet();
    }
}

void BoxesCode::handleFindW(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<FindWMsg>(_msg);
    if (d == 0) {                          // d not known yet: keep the message
        pendingFindW.emplace_back(m->origin, m->dsent);
        return;
    }
    processFindW(m->origin, m->dsent);
}

void BoxesCode::processFindW(int origin, int dsent) {
    P2PNetworkInterface *toLeft = at(-1, 0, 0);
    if (d < dsent) {
        sendAlgo(new SetWMsg(origin, dsent, 0), toLeft);       // line too short: stop here
    } else if (!has(1, 0, 0)) {
        sendAlgo(new SetWMsg(origin, dsent, 1), toLeft);       // end of the row
    } else {
        sendAlgo(new FindWMsg(origin, dsent), at(1, 0, 0));    // keep going right
    }
}

void BoxesCode::handleSetW(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<SetWMsg>(_msg);
    if (m->origin == (int)self->blockId) {
        w = m->wsent + 1;
        onWSet();
    } else {
        sendAlgo(new SetWMsg(m->origin, m->dsent, m->wsent + 1), at(-1, 0, 0));
    }
}

void BoxesCode::onWSet() {
    wKnown = true;
    sendLayerInfo(true, d, w);
    tryCmin();
}

// ---------------------------------------------------------------------------
// Phase 5-6 : layer information and Cmin  (3D extension)
// ---------------------------------------------------------------------------
void BoxesCode::sendLayerInfo(bool rmin, int dv, int wv) {
    if (has(0, 0, 1)) sendAlgo(new LayerInfoMsg(rmin, dv, wv), at(0, 0, 1));
}

void BoxesCode::handleLayerInfo(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<LayerInfoMsg>(_msg);
    belowRmin = m->isRmin;
    belowD = m->d;
    belowW = m->w;
    belowKnown = true;
    tryCmin();
}

// Rmin && !(Rmin(bottom) && d == dBottom && w == wBottom)      -- Eq. (4)
void BoxesCode::tryCmin() {
    if (decided || !isRmin || !wKnown) return;
    if (has(0, 0, -1)) {
        if (!belowKnown) return;           // wait for the layer below
        isCmin = !(belowRmin && belowD == d && belowW == w);
    } else {
        isCmin = true;
    }
    decided = true;
    if (isCmin) startH();
    flushPendingH();
}

// ---------------------------------------------------------------------------
// Phase 7 : height along +Z
// ---------------------------------------------------------------------------
void BoxesCode::startH() {
    if (has(0, 0, 1)) {
        sendAlgo(new FindHMsg(self->blockId), at(0, 0, 1));
    } else {
        h = 1;
        finalizeBox();
    }
}

void BoxesCode::handleFindH(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<FindHMsg>(_msg);
    if (!decided) {
        pendingFindH.push_back(m->origin);
        return;
    }
    processFindH(m->origin);
}

// A module continues the stack below it iff it is Rmin and not Cmin, i.e. it
// has the same rectangle (same d and w) as the Rmin module right below it.
void BoxesCode::processFindH(int origin) {
    bool continuing = isRmin && !isCmin;
    if (!continuing)       sendAlgo(new SetHMsg(origin, 0), at(0, 0, -1));
    else if (has(0, 0, 1)) sendAlgo(new FindHMsg(origin),   at(0, 0, 1));
    else                   sendAlgo(new SetHMsg(origin, 1), at(0, 0, -1));
}

void BoxesCode::flushPendingH() {
    std::vector<int> pending;
    pending.swap(pendingFindH);
    for (int origin : pending) processFindH(origin);
}

void BoxesCode::handleSetH(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<SetHMsg>(_msg);
    if (m->origin == (int)self->blockId) {
        h = m->hsent + 1;
        finalizeBox();
    } else {
        sendAlgo(new SetHMsg(m->origin, m->hsent + 1), at(0, 0, -1));
    }
}

// ---------------------------------------------------------------------------
// Phase 8 : the box is known by its Cmin module
// ---------------------------------------------------------------------------
void BoxesCode::finalizeBox() {
    FoundBox b;
    b.id = (int)self->blockId;
    b.cmin[0] = self->position[0];
    b.cmin[1] = self->position[1];
    b.cmin[2] = self->position[2];
    b.cmax[0] = b.cmin[0] + w - 1;
    b.cmax[1] = b.cmin[1] + d - 1;
    b.cmax[2] = b.cmin[2] + h - 1;

    foundBoxes.push_back(b);
    lastBoxTime = std::max(lastBoxTime, getScheduler()->now());

    std::cout << "[box] module " << b.id
              << "  Cmin=(" << b.cmin[0] << "," << b.cmin[1] << "," << b.cmin[2] << ")"
              << "  Cmax=(" << b.cmax[0] << "," << b.cmax[1] << "," << b.cmax[2] << ")"
              << "  (w=" << w << ", d=" << d << ", h=" << h << ")"
              << "  t=" << getScheduler()->now() << " us" << std::endl;

    if (COLOR_BOXES) paintAndFlood(b.id, b.cmin, b.cmax);
}

// ---------------------------------------------------------------------------
// Visualisation: the Cmin module colours its box (flood limited to the box).
// Modules covered by several boxes keep the colour of the last flood.
// This traffic is counted separately and is NOT part of the algorithm cost.
// ---------------------------------------------------------------------------
void BoxesCode::paintAndFlood(int boxId, const int *mn, const int *mx) {
    if (seenBoxes.count(boxId)) return;
    seenBoxes.insert(boxId);
    const float *c = PALETTE[boxId % PALETTE_SIZE];
    self->setColor(Color(c[0], c[1], c[2]));
    BoxColorMsg m(boxId, mn, mx);
    floodBox(m);
}

void BoxesCode::floodBox(const BoxColorMsg &m) {
    static const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (const auto &dv : dirs) {
        P2PNetworkInterface *itf = at(dv[0], dv[1], dv[2]);
        if (itf == nullptr) continue;
        int nx = (int)self->position[0] + dv[0];
        int ny = (int)self->position[1] + dv[1];
        int nz = (int)self->position[2] + dv[2];
        bool inside = nx >= m.cmin[0] && nx <= m.cmax[0] &&
                      ny >= m.cmin[1] && ny <= m.cmax[1] &&
                      nz >= m.cmin[2] && nz <= m.cmax[2];
        if (inside) sendViz(new BoxColorMsg(m), itf);
    }
}

void BoxesCode::handleBoxColor(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    auto m = std::static_pointer_cast<BoxColorMsg>(_msg);
    if (seenBoxes.count(m->boxId)) return;
    seenBoxes.insert(m->boxId);
    const float *c = PALETTE[m->boxId % PALETTE_SIZE];
    self->setColor(Color(c[0], c[1], c[2]));
    floodBox(*m);
}

// ---------------------------------------------------------------------------
// Sending helpers and statistics
// ---------------------------------------------------------------------------
void BoxesCode::sendAlgo(Message *m, P2PNetworkInterface *itf) {
    algoMsgCount[m->type - MSG_SET_D]++;
    sendMessage(m, itf, MSG_DELAY, 0);
}

void BoxesCode::sendViz(Message *m, P2PNetworkInterface *itf) {
    vizMsgCount++;
    sendMessage(m, itf, MSG_DELAY, 0);
}

void BoxesCode::printSummary() {
    static const char *names[] = {"SET_D", "LEFT_INFO", "FIND_W", "SET_W",
                                  "LAYER_INFO", "FIND_H", "SET_H"};
    unsigned long total = 0;
    for (int i = 0; i < MSG_BOX_COLOR - MSG_SET_D; ++i) total += algoMsgCount[i];

    // Boxes entirely contained in another box can be discarded.
    int maximal = 0;
    for (size_t i = 0; i < foundBoxes.size(); ++i) {
        bool inside = false;
        for (size_t j = 0; j < foundBoxes.size() && !inside; ++j) {
            if (i == j) continue;
            bool in = true;
            for (int k = 0; k < 3; ++k)
                in = in && foundBoxes[j].cmin[k] <= foundBoxes[i].cmin[k]
                        && foundBoxes[j].cmax[k] >= foundBoxes[i].cmax[k];
            // identical boxes cannot occur, so no tie-break is needed
            inside = in;
        }
        if (!inside) ++maximal;
    }

    std::cout << "\n================ Overlapping boxes : summary ================\n";
    for (int i = 0; i < MSG_BOX_COLOR - MSG_SET_D; ++i)
        std::cout << std::left << std::setw(12) << names[i] << algoMsgCount[i] << "\n";
    std::cout << "Algorithm messages (total) : " << total << "\n"
              << "Visualisation messages     : " << vizMsgCount << " (not counted)\n"
              << "Boxes found                : " << foundBoxes.size() << "\n"
              << "Boxes after removing nested: " << maximal << "\n"
              << "Time of last box (sim.)    : " << std::fixed << std::setprecision(3)
              << lastBoxTime / 1000.0 << " ms\n"
              << "=============================================================\n"
              << std::endl;
}