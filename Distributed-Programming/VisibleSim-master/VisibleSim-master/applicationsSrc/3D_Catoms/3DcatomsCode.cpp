/**
 * @author Muhammad Shiraz
 * @date October 18, 2026
 */

#include "3DcatomsCode.hpp"

// ---------------------------------------------------------------------
//  Live statistics (window title + console). Display only: the algorithm
//  itself never reads these numbers.
// ---------------------------------------------------------------------
static int g_wave = 0;
static int g_transferred = 0;
static int g_running = 0;
static int g_motions = 0;
static int g_messages = 0;
static string g_recent = "None";

#if !USE_MESSAGES
static Catoms3DBlock *g_registry[NB_MM][MODULES_PER_MM] = {{nullptr}};
#endif

static void updateTitle() {
    string t = string("[LIVE MONITOR] Phase: ") + phaseName(g_wave) +
               " | Active Wave: " + to_string(g_wave + 1) + "/" + to_string(NB_WAVES) +
               " | Transferred Blocks: " + to_string(g_transferred) + "/" + to_string(TOTAL_MOVES) +
               " | Running Tasks: " + to_string(g_running) +
               " | Messages: " + to_string(g_messages) +
               " | Recent: " + g_recent;
    glutSetWindowTitle(t.c_str());
}

static inline int seenKey(int wave, int index, int serial) {
    return (wave * MODULES_PER_MM + index) * 4 + serial;
}

// ---------------------------------------------------------------------
//  3D axes, same look as the professor's picture:
//  X red, Y green, Z blue, thick shaded arrows, tick marks, white origin.
// ---------------------------------------------------------------------
static void drawArrow(GLUquadric *q, float r, float g, float b, float L,
                      float rx, float ry, float rz, float angle) {
    const float col[4] = {r, g, b, 1.0f};
    glColor3f(r, g, b);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);

    const float shaft = 1.3f;     // shaft radius
    const float headR = 3.6f;     // arrow head radius
    const float headL = 11.0f;    // arrow head length

    glPushMatrix();
    if (angle != 0.0f) glRotatef(angle, rx, ry, rz);   // axis is drawn along +Z, then rotated
    gluCylinder(q, shaft, shaft, L - headL, 16, 1);    // shaft

    glPushMatrix();                                    // head
    glTranslatef(0, 0, L - headL);
    gluCylinder(q, headR, 0.0, headL, 20, 1);
    gluDisk(q, 0.0, headR, 20, 1);
    glPopMatrix();

    for (float t = 12.0f; t < L - headL - 2.0f; t += 12.0f) {   // tick marks
        glPushMatrix();
        glTranslatef(0, 0, t);
        gluCylinder(q, 2.4, 2.4, 1.2, 12, 1);
        gluDisk(q, 0.0, 2.4, 12, 1);
        glPopMatrix();
    }
    glPopMatrix();
}

void MMLocomotionBlockCode::onGlDraw() {
#if DRAW_AXES
    if (module->blockId != 1) return;   // only one module draws the axes

    const float L = 70.0f;              // axis length (world units)
    GLboolean tex = glIsEnabled(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_2D);           // the ground texture made the arrows grey

    GLUquadric *q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);

    drawArrow(q, 0.85f, 0.0f, 0.0f, L, 0, 1, 0,  90.0f);   // X (red)
    drawArrow(q, 0.0f, 0.75f, 0.0f, L, 1, 0, 0, -90.0f);   // Y (green)
    drawArrow(q, 0.0f, 0.0f, 0.9f, L, 0, 0, 1,   0.0f);    // Z (blue)

    const float white[4] = {1, 1, 1, 1};                    // origin ball
    glColor3f(1, 1, 1);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, white);
    gluSphere(q, 2.8, 20, 20);

    gluDeleteQuadric(q);
    if (tex) glEnable(GL_TEXTURE_2D);
#endif
}

// ---------------------------------------------------------------------
MMLocomotionBlockCode::MMLocomotionBlockCode(Catoms3DBlock *host)
    : Catoms3DBlockCode(host) {
    module = host;
    if (!host) return;
    addMessageEventFunc2(GO_MSG_TYPE,
        std::bind(&MMLocomotionBlockCode::onGo, this,
                  std::placeholders::_1, std::placeholders::_2));
}

// ---------------------------------------------------------------------
//  Step 1: who am I?  (meta-module + index, from my start position)
// ---------------------------------------------------------------------
void MMLocomotionBlockCode::detectRole() {
    for (int k = 0; k < NB_MM; k++) {
        Cell3DPosition a(FIRST_ANCHOR_X + MM_SPACING * k, ANCHOR_Y, ANCHOR_Z);
        bool fb = (k % 2 == 0);
        const auto &arr = fb ? FB_RELATIVE_POSITIONS : BF_RELATIVE_POSITIONS;
        for (int i = 0; i < MODULES_PER_MM; i++) {
            Cell3DPosition expected(a[0] + arr[i][0], a[1] + arr[i][1], a[2] + arr[i][2]);
            if (expected == module->position) {
                homeMM = k;
                idx = i;
                curU = k;
                return;
            }
        }
    }
}

void MMLocomotionBlockCode::startup() {
    detectRole();
#if !USE_MESSAGES
    if (homeMM >= 0) g_registry[homeMM][idx] = module;
#endif
    // The first module of the left-most meta-module starts the whole process
    if (homeMM == 0 && idx == 0) {
        cout << "=== 3D CATOMS LOCOMOTION: " << NB_WAVES << " waves ("
             << CYCLES << " cycles forward, " << UPLEFT_CYCLES << " up-left, and back), "
             << PIPELINE_WIDTH << " modules at the same time ===" << endl;
        startMove(0);
    }
}

// Where do I have to go in this wave?
Cell3DPosition MMLocomotionBlockCode::targetCell(int wave) const {
    WaveInfo w = waveInfo(wave);
    Cell3DPosition a = anchorOf(w.dst, w.bent);
    const auto &arr = isFB(w.dst) ? FB_RELATIVE_POSITIONS : BF_RELATIVE_POSITIONS;
    int t = w.build[rankOf(wave, idx)];       // the cell this module has to build
    return Cell3DPosition(a[0] + arr[t][0], a[1] + arr[t][1], a[2] + arr[t][2]);
}

// ---------------------------------------------------------------------
//  Step 2: messages. A GO message is flooded through the structure and
//  only the module (curU == srcMM(wave), idx == index) reacts to it.
// ---------------------------------------------------------------------
void MMLocomotionBlockCode::floodGo(const GoMsg &m, P2PNetworkInterface *except) {
    for (unsigned int k = 0; k < module->getNbInterfaces(); k++) {
        P2PNetworkInterface *p2p = module->getInterface(k);
        if (p2p && p2p != except && p2p->isConnected()) {
            sendMessage("GO", new GoMsg(m), p2p, 100, 0);
            g_messages++;
        }
    }
}

// Wake up the module that has position r in the leaving order of this wave
void MMLocomotionBlockCode::wake(int wave, int r, int serial) {
    if (wave >= NB_WAVES || r >= MODULES_PER_MM) return;
    int target = orderIdx(wave, r);
#if USE_MESSAGES
    seen.insert(seenKey(wave, target, serial));
    floodGo(GoMsg(wave, target, serial), nullptr);
    onWakeMe(wave, target);   // the module to wake may be me (e.g. start of the backward phase)
#else
    for (int k = 0; k < NB_MM; k++)        // find who is in the source slot now
        for (int i = 0; i < MODULES_PER_MM; i++) {
            Catoms3DBlock *b = g_registry[k][i];
            if (!b || !b->blockCode) continue;
            auto *bc = static_cast<MMLocomotionBlockCode *>(b->blockCode);
            if (bc->curU == srcMM(wave) && bc->idx == target) { bc->onWakeMe(wave, target); return; }
        }
#endif
}

void MMLocomotionBlockCode::onGo(MessagePtr msg, P2PNetworkInterface *sender) {
    auto m = std::static_pointer_cast<GoMsg>(msg);
    int key = seenKey(m->wave, m->index, m->serial);
    if (seen.count(key)) return;          // already handled
    seen.insert(key);

    floodGo(*m, sender);                  // forward to the others
    onWakeMe(m->wave, m->index);          // is it for me?
}

void MMLocomotionBlockCode::onWakeMe(int wave, int target) {
    if (moving) return;                   // already travelling
    if (curU != srcMM(wave) || idx != target) return;   // not me / already moved
    startMove(wave);
}

// ---------------------------------------------------------------------
//  Step 3: movement. Re-plan from the CURRENT position before every
//  rotation, so that the path stays valid while other modules move.
// ---------------------------------------------------------------------
bool MMLocomotionBlockCode::planAndStep() {
    Cell3DPosition start = module->position;
    vector<Cell3DPosition> p = findPath(start, goal);
    if (p.empty()) return false;

    if (routeLen == 0) {                  // first plan of this trip
        routeLen = (int)p.size();
        // wake the next module after 1/PIPELINE_WIDTH of my route
        int width = pipelineWidth(myWave);
        launchAt = max(1, (routeLen + width - 1) / width);
    }
    // never ask the simulator for a rotation it cannot do
    if (!module->canMoveTo(p[0])) return false;

    getScheduler()->schedule(
        new Catoms3DRotationStartEvent(getScheduler()->now() + STEP_DELAY_US, module, p[0]));
    return true;
}

void MMLocomotionBlockCode::startMove(int wave) {
    myWave = wave;
    myRank = rankOf(wave, idx);
    newIdx = waveInfo(wave).build[myRank];
    goal = targetCell(wave);
    stepsDone = 0;
    routeLen = 0;
    launched = false;

    if (!planAndStep()) {
        cout << "Module " << module->blockId << " (idx " << idx
             << "): no path yet, waiting to be woken again" << endl;
        return;
    }
    moving = true;
    g_wave = wave;
    g_running++;
    updateTitle();
}

void MMLocomotionBlockCode::doNextStep() {
    if (!planAndStep()) {
        moving = false;
        g_running = max(0, g_running - 1);
        updateTitle();
        cout << "Module " << module->blockId << " (idx " << idx
             << "): BLOCKED at " << module->position << " (goal " << goal << ")" << endl;
    }
}

void MMLocomotionBlockCode::onMotionEnd() {
    if (!moving) return;
    g_motions++;
    stepsDone++;

    if (module->position != goal) {
        // Pipeline: after 1/3 of my route, the next module can start
        int r = myRank;
        if (!launched && stepsDone >= launchAt) {
            launched = true;
            wake(myWave, r + 1, 0);
        }
        doNextStep();
        return;
    }

    // ---- arrived at my target cell ----
    moving = false;
    curU = dstMM(myWave);
    idx = newIdx;                        // my place in the new meta-module
    g_transferred++;
    g_running = max(0, g_running - 1);
    g_recent = "Block ID " + to_string(module->blockId) + " (idx " + to_string(idx) + ")";
    updateTitle();
    cout << "[LIVE MONITOR] Phase: " << phaseName(myWave)
         << " | Active Wave: " << myWave + 1 << "/" << NB_WAVES
         << " | Transferred Blocks: " << g_transferred << "/" << TOTAL_MOVES
         << " | Running Tasks: " << g_running
         << " | Messages: " << g_messages
         << " | Recent: " << g_recent << endl;

    int r = myRank;
    if (r + 1 < MODULES_PER_MM) {
        wake(myWave, r + 1, 1);          // safety: wake the successor again
    } else if (myWave + 1 < NB_WAVES) {
        wake(myWave + 1, 0, 0);          // last module of the wave: next wave starts
    } else {
        string end = "[COMPLETED] " + to_string(NB_WAVES) + " waves | " +
                     to_string(g_transferred) + " module moves | " +
                     to_string(g_motions) + " motions | " +
                     to_string(g_messages) + " messages | " +
                     to_string((unsigned long long)getScheduler()->now()) + " us";
        glutSetWindowTitle(end.c_str());
        cout << "=============================================================" << endl;
        cout << end << endl;
        cout << "=============================================================" << endl;
    }
}

// ---------------------------------------------------------------------
//  Professor's BFS. Only change: a module cannot use its own start cell
//  as a pivot (that cell becomes empty as soon as it leaves).
// ---------------------------------------------------------------------
vector<Cell3DPosition> MMLocomotionBlockCode::findPath(Cell3DPosition &start,
                                                       Cell3DPosition &goal) {
    pathStart = start;
    vector<Cell3DPosition> path;
    set<Cell3DPosition> visited;
    map<Cell3DPosition, Cell3DPosition> parentMap;
    queue<Cell3DPosition> q;
    q.push(start);
    visited.insert(start);
    bool found = false;
    while (!q.empty() and !found) {
        Cell3DPosition current = q.front();
        q.pop();
        if (current == goal) {
            found = true;
            break;
        }
        vector<Cell3DPosition> reachablePositions;
        bool success = getAllPossibleMotionsFromPosition(current, reachablePositions);
        if (success) {
            for (auto &pos : reachablePositions) {
                if (visited.find(pos) == visited.end()) {
                    visited.insert(pos);
                    parentMap[pos] = current;
                    q.push(pos);
                }
            }
        }
    }
    if (found) {
        Cell3DPosition step = goal;
        while (step != start) {
            path.push_back(step);
            step = parentMap[step];
        }
        reverse(path.begin(), path.end());
    }
    return path;
}

bool MMLocomotionBlockCode::getAllPossibleMotionsFromPosition(
    Cell3DPosition position,
    vector<Cell3DPosition> &reachablePositions) {

    // A cell occupied by a module: only the rotations that this module can
    // really do (professor's code), so the first step of a path is always valid.
    Catoms3DBlock *mod = static_cast<Catoms3DBlock *>(lattice->getBlock(position));
    if (mod) {
        for (auto &neighPos : lattice->getFreeNeighborCells(position)) {
            if (mod->canMoveTo(neighPos)) {
                reachablePositions.push_back(neighPos);
            }
        }
        return !reachablePositions.empty();
    }

    // An empty cell: where could a module go from here, using the
    // neighbours as pivots?
    bool found = false;
    for (auto &neighPos : lattice->getActiveNeighborCells(position)) {
        if (neighPos == pathStart) continue;       // my old cell will be empty
        Catoms3DBlock *neigh = static_cast<Catoms3DBlock *>(lattice->getBlock(neighPos));
        if (!neigh) continue;

        vector<Catoms3DMotionRulesLink *> vec;
        Catoms3DMotionRules motionRulesInstance;
        short conFrom = neigh->getConnectorId(position);
        if (conFrom < 0) continue;

        motionRulesInstance.getValidMotionListFromPivot(
            neigh, conFrom, vec, static_cast<FCCLattice *>(lattice), NULL);

        for (auto link : vec) {
            Cell3DPosition toPos;
            neigh->getNeighborPos(link->getConToID(), toPos);
            reachablePositions.push_back(toPos);
            found = true;
        }
    }
    return found;
}