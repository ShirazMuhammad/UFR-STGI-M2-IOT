/**
* @author Muhammad Shiraz
 * @date October 18, 2026
 */

#pragma once
#include "robots/catoms3D/catoms3DSimulator.h"
#include "robots/catoms3D/catoms3DWorld.h"
#include "robots/catoms3D/catoms3DBlockCode.h"
#include "robots/catoms3D/catoms3DMotionRules.h"
#include "robots/catoms3D/catoms3DRotationEvents.h"
#include <array>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

#ifdef USE_FREEGLUT
#include <GL/freeglut.h>
#elif defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif

using namespace Catoms3D;
using namespace std;

// =====================================================================
//  SETTINGS
// =====================================================================
// 1 = modules wake each other with real messages (distributed, what the
//     professor evaluates).
// 0 = test mode: direct call between modules (no messages). Use it only
//     to check the rest of the algorithm, never for the final report.
#define USE_MESSAGES 1

static const int PIPELINE_WIDTH = 4;   // modules travelling at the same time
                                       // (if "BLOCKED" appears in the console, set it back to 3)

// Up-left and return phases: modules cross the tilted structure through the same
// narrow gaps, so two travelling together can try to enter the same cell
// (DoubleInsertionException). There they leave one at a time.
static const int DIAG_PIPELINE_WIDTH = 1;

// One cycle = the whole line moves by its own length (4 waves).
// CYCLES        : cycles along X (left -> right), and the same number back at the end.
// UPLEFT_CYCLES : extra cycles going up and to the left after the X cycles,
//                 then the same number back (set 0 to switch the up-left part off).
static const int CYCLES        = 3;
static const int UPLEFT_CYCLES = 3;

// A module waits this long (simulated microseconds) before each rotation, so
// that a message sent just before the rotation can leave before the module
// disconnects from its neighbours.
static const int STEP_DELAY_US = 3000;

// 3D axes (X red, Y green, Z blue) drawn in the simulation window
// 1 = on (simple lines only)   0 = off
// If the window turns white / the program crashes at start, set it to 0.
#define DRAW_AXES 1

// ---------- Professor's relative positions ----------
static const std::array<Cell3DPosition, 12> FB_RELATIVE_POSITIONS = {
    Cell3DPosition(-2, -1, 3), Cell3DPosition(-1, -1, 2), Cell3DPosition(-2, -1, 1),
    Cell3DPosition(-1, -1, 3), Cell3DPosition(-1, -1, 1), Cell3DPosition(0, 0, 4),
    Cell3DPosition(0, 0, 0),   Cell3DPosition(1, 0, 4),   Cell3DPosition(1, 0, 0),
    Cell3DPosition(1, 0, 3),   Cell3DPosition(1, 0, 1),   Cell3DPosition(2, 1, 2)};

static const std::array<Cell3DPosition, 12> BF_RELATIVE_POSITIONS = {
    Cell3DPosition(-2, 0, 3), Cell3DPosition(-1, 1, 2), Cell3DPosition(-2, 0, 1),
    Cell3DPosition(-1, 0, 3), Cell3DPosition(-1, 0, 1), Cell3DPosition(0, 0, 4),
    Cell3DPosition(0, 0, 0),  Cell3DPosition(1, 0, 4),  Cell3DPosition(1, 0, 0),
    Cell3DPosition(1, -1, 3), Cell3DPosition(1, -1, 1), Cell3DPosition(2, -1, 2)};

// ---------- Values from config.xml ----------
static const int NB_MM           = 4;   // meta-modules in the line
static const int MODULES_PER_MM  = 12;
static const int FIRST_ANCHOR_X  = 4;   // anchor (0,0,0) of the green one
static const int ANCHOR_Y        = 4;
static const int ANCHOR_Z        = 2;
static const int MM_SPACING      = 4;   // distance in x between two anchors

// ---------- Where a meta-module sits ----------
// A "slot" u is a place for one meta-module. On the straight track slot u is
// at x = 4 + 4*u. On the bent track the slots u < BEND_U go up and to the left:
// every slot is UP_DX in x and UP_DZ in z from the next one (z step must be even
// in the FCC lattice, and this step keeps 5 contacts between neighbours, like
// the straight chain).
static const int FWD_WAVES = CYCLES * NB_MM;        // 12
static const int UP_WAVES  = UPLEFT_CYCLES * NB_MM; // 12
static const int BEND_U    = FWD_WAVES;             // first slot of the line after the X cycles
static const int UP_DX     = -2;
static const int UP_DZ     = 4;

static inline Cell3DPosition anchorOf(int u, bool bent) {
    if (!bent || u >= BEND_U)
        return Cell3DPosition(FIRST_ANCHOR_X + MM_SPACING * u, ANCHOR_Y, ANCHOR_Z);
    int j = BEND_U - u;
    return Cell3DPosition(FIRST_ANCHOR_X + MM_SPACING * BEND_U + j * UP_DX,
                          ANCHOR_Y,
                          ANCHOR_Z + j * UP_DZ);
}
static inline bool isFB(int u) { return u % 2 == 0; }   // shapes alternate FB, BF, FB...

// ---------- Waves ----------
// One wave = one meta-module is dismantled and rebuilt NB_MM slots further.
//   phase 0 FORWARD  : CYCLES cycles to the right (straight track)
//   phase 1 UP-LEFT  : UPLEFT_CYCLES cycles up and left (bent track)
//   phase 2 RETURN   : the same way back, down to the straight track
//   phase 3 BACKWARD : CYCLES cycles back to the start (straight track)
static const int NB_WAVES    = 2 * FWD_WAVES + 2 * UP_WAVES;   // 48
static const int TOTAL_MOVES = NB_WAVES * MODULES_PER_MM;      // 576

// Leaving / building orders (values are indices 0..11 in the FB/BF arrays).
// ORDER_ID / ORDER_REV : professor's order for the straight track.
// BUILD_LOW : build order of a ring that has its neighbour BELOW-RIGHT of it
//             (starts on the cells that touch that neighbour, then zig-zags around).
// BUILD_UP  : same, for a ring whose neighbour is ABOVE-LEFT of it.
// LEAVE_x   : dismantle order = BUILD_x reversed (the far side leaves first).
static const int ORDER_ID[12]   = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const int ORDER_REV[12]  = {11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
static const int BUILD_LOW[12]  = {8, 10, 6, 11, 4, 9, 2, 7, 1, 5, 0, 3};
static const int LEAVE_LOW[12]  = {3, 0, 5, 1, 7, 2, 9, 4, 11, 6, 10, 8};
static const int BUILD_UP[12]   = {3, 5, 0, 7, 1, 9, 2, 11, 4, 10, 6, 8};
static const int LEAVE_UP[12]   = {8, 6, 10, 4, 11, 2, 9, 1, 7, 0, 5, 3};

struct WaveInfo {
    int src;      // slot of the meta-module that is moved
    int dst;      // slot where it is rebuilt
    bool bent;    // which track
    int phase;    // 0..3, see above
    const int *leave;   // leave[r] = index of the module that leaves at rank r
    const int *build;   // build[r] = index of the cell that rank r builds
};

static inline WaveInfo waveInfo(int w) {
    const int last = BEND_U + NB_MM - 1;               // right-most slot of the line
    if (w < FWD_WAVES)
        return {w, w + NB_MM, false, 0, ORDER_ID, ORDER_ID};
    w -= FWD_WAVES;
    if (w < UP_WAVES) {                                // leaves from the right end, builds up-left
        int s = last - w;
        return {s, s - NB_MM, true, 1, (s > BEND_U) ? ORDER_REV : LEAVE_UP, BUILD_LOW};
    }
    w -= UP_WAVES;
    if (w < UP_WAVES) {                                // exact reverse of the up-left phase
        int s = BEND_U - UP_WAVES + w;
        int d = s + NB_MM;
        return {s, d, true, 2, LEAVE_LOW, (d > BEND_U) ? ORDER_ID : BUILD_UP};
    }
    w -= UP_WAVES;
    return {last - w, last - w - NB_MM, false, 3, ORDER_REV, ORDER_REV};
}
static inline int srcMM(int wave) { return waveInfo(wave).src; }
static inline int dstMM(int wave) { return waveInfo(wave).dst; }

static inline const char *phaseName(int wave) {
    static const char *names[4] = {"FORWARD", "UP-LEFT", "RETURN", "BACKWARD"};
    return names[waveInfo(wave).phase];
}
// How many modules may travel at the same time in this wave
static inline int pipelineWidth(int wave) {
    int p = waveInfo(wave).phase;
    return (p == 1 || p == 2) ? DIAG_PIPELINE_WIDTH : PIPELINE_WIDTH;
}
// Module that leaves at rank r of this wave, and the rank of a given module.
static inline int orderIdx(int wave, int r) { return waveInfo(wave).leave[r]; }
static inline int rankOf(int wave, int index) {
    const int *l = waveInfo(wave).leave;
    for (int r = 0; r < MODULES_PER_MM; r++) if (l[r] == index) return r;
    return 0;
}

// ---------- Message ----------
static const int GO_MSG_TYPE = 9001;

class GoMsg : public Message {
public:
    int wave;    // which wave
    int index;   // which module (0..11) must start now
    int serial;  // 0 = early launch, 1 = re-wake on arrival
    GoMsg(int w, int i, int s) : Message(), wave(w), index(i), serial(s) {
        type = GO_MSG_TYPE;
        id = GO_MSG_TYPE;
    }
    Message *clone() const override { return new GoMsg(*this); }
};

class MMLocomotionBlockCode : public Catoms3DBlockCode {
public:
    Catoms3DBlock *module = nullptr;

    int homeMM = -1;   // meta-module I belong to at the beginning (0..3)
    int idx = -1;      // my index 0..11 in the FB/BF array
    int curU = -1;     // slot of the meta-module I belong to NOW (changes after each move)

    // movement state
    Cell3DPosition goal;
    Cell3DPosition pathStart;
    bool moving = false;
    bool launched = false;   // successor already woken up?
    int myWave = -1;
    int myRank = 0;          // my place in the leaving order of this wave
    int newIdx = 0;          // my index in the new meta-module after arriving
    int stepsDone = 0;
    int routeLen = 0;
    int launchAt = 1;

    set<int> seen;           // messages already handled (anti-loop)

    MMLocomotionBlockCode(Catoms3DBlock *host);
    ~MMLocomotionBlockCode() {}

    void startup() override;
    void onMotionEnd() override;
    void onGlDraw();         // 3D axes (no 'override': harmless if the hook is missing)

    void detectRole();
    Cell3DPosition targetCell(int wave) const;
    void startMove(int wave);
    bool planAndStep();
    void doNextStep();

    // wake-up of the next module (messages)
    void wake(int wave, int r, int serial);
    void onWakeMe(int wave, int target);
    void floodGo(const GoMsg &m, P2PNetworkInterface *except);
    void onGo(MessagePtr msg, P2PNetworkInterface *sender);

    // Professor's helper functions
    vector<Cell3DPosition> findPath(Cell3DPosition &start, Cell3DPosition &goal);
    bool getAllPossibleMotionsFromPosition(Cell3DPosition position,
                                           vector<Cell3DPosition> &reachablePositions);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new MMLocomotionBlockCode(static_cast<Catoms3DBlock *>(host));
    }
};