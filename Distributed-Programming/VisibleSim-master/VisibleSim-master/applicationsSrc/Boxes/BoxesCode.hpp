/*
 * BoxesCode.hpp
 *
 * Configuration recognition via overlapping boxes (Blinky Blocks, VisibleSim).
 *
 * Coordinate conventions used in the whole file
 *   left   = (x-1, y,   z)      right = (x+1, y,   z)
 *   back   = (x,   y+1, z)      front = (x,   y-1, z)
 *   up     = (x,   y,   z+1)    down  = (x,   y,   z-1)
 *
 *   d  : rank of a module in its vertical line, counted from the module that
 *        has no back neighbour (d = 1) towards the front.
 *   w  : number of consecutive columns, starting at this module and going
 *        right, whose d is >= d of this module (same row).
 *   h  : number of identical rectangles stacked along +Z (3D extension).
 */
#ifndef BOXESCODE_HPP_
#define BOXESCODE_HPP_

#include <array>
#include <memory>
#include <set>
#include <vector>

#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"
#include "robots/blinkyBlocks/blinkyBlocksSimulator.h"

// ---------------------------------------------------------------------------
// Message types
// ---------------------------------------------------------------------------
enum BoxesMsgType {
    MSG_SET_D = 3001,   // column ranking, travels towards the front
    MSG_LEFT_INFO,      // (d, hasFront) sent to the right neighbour
    MSG_FIND_W,         // width search, travels to the right
    MSG_SET_W,          // width answer, travels back to the left
    MSG_LAYER_INFO,     // (isRmin, d, w) sent to the module above (+Z)
    MSG_FIND_H,         // height search, travels up
    MSG_SET_H,          // height answer, travels back down
    MSG_BOX_COLOR       // visualisation only: floods the found box
};

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------
class SetDMsg : public Message {
public:
    int d;
    explicit SetDMsg(int d_) : Message(), d(d_) { type = MSG_SET_D; }
    ~SetDMsg() override {}
    Message *clone() const override { return new SetDMsg(*this); }
};

class LeftInfoMsg : public Message {
public:
    int d;
    bool hasFront;
    LeftInfoMsg(int d_, bool hasFront_) : Message(), d(d_), hasFront(hasFront_) {
        type = MSG_LEFT_INFO;
    }
    ~LeftInfoMsg() override {}
    Message *clone() const override { return new LeftInfoMsg(*this); }
};

class FindWMsg : public Message {
public:
    int origin;   // id of the Rmin module that started the search
    int dsent;
    FindWMsg(int origin_, int dsent_) : Message(), origin(origin_), dsent(dsent_) {
        type = MSG_FIND_W;
    }
    ~FindWMsg() override {}
    Message *clone() const override { return new FindWMsg(*this); }
};

class SetWMsg : public Message {
public:
    int origin;
    int dsent;
    int wsent;
    SetWMsg(int origin_, int dsent_, int wsent_)
        : Message(), origin(origin_), dsent(dsent_), wsent(wsent_) {
        type = MSG_SET_W;
    }
    ~SetWMsg() override {}
    Message *clone() const override { return new SetWMsg(*this); }
};

class LayerInfoMsg : public Message {
public:
    bool isRmin;
    int d;
    int w;
    LayerInfoMsg(bool isRmin_, int d_, int w_)
        : Message(), isRmin(isRmin_), d(d_), w(w_) {
        type = MSG_LAYER_INFO;
    }
    ~LayerInfoMsg() override {}
    Message *clone() const override { return new LayerInfoMsg(*this); }
};

class FindHMsg : public Message {
public:
    int origin;
    explicit FindHMsg(int origin_) : Message(), origin(origin_) { type = MSG_FIND_H; }
    ~FindHMsg() override {}
    Message *clone() const override { return new FindHMsg(*this); }
};

class SetHMsg : public Message {
public:
    int origin;
    int hsent;
    SetHMsg(int origin_, int hsent_) : Message(), origin(origin_), hsent(hsent_) {
        type = MSG_SET_H;
    }
    ~SetHMsg() override {}
    Message *clone() const override { return new SetHMsg(*this); }
};

class BoxColorMsg : public Message {
public:
    int boxId;
    int cmin[3];
    int cmax[3];
    BoxColorMsg(int boxId_, const int *mn, const int *mx) : Message(), boxId(boxId_) {
        for (int i = 0; i < 3; ++i) { cmin[i] = mn[i]; cmax[i] = mx[i]; }
        type = MSG_BOX_COLOR;
    }
    ~BoxColorMsg() override {}
    Message *clone() const override { return new BoxColorMsg(*this); }
};

// ---------------------------------------------------------------------------
// A box found by the algorithm (kept only for the final report)
// ---------------------------------------------------------------------------
struct FoundBox {
    int id;
    int cmin[3];
    int cmax[3];
};

// ---------------------------------------------------------------------------
// Block code
// ---------------------------------------------------------------------------
class BoxesCode : public BlinkyBlocks::BlinkyBlocksBlockCode {
public:
    explicit BoxesCode(BlinkyBlocks::BlinkyBlocksBlock *host);
    ~BoxesCode() override;

    void startup() override;

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new BoxesCode(static_cast<BlinkyBlocks::BlinkyBlocksBlock *>(host));
    }

    // Final report (messages, simulated time, boxes). Called from main().
    static void printSummary();

private:
    // ---- local geometry ---------------------------------------------------
    BlinkyBlocks::BlinkyBlocksBlock *self;
    std::array<P2PNetworkInterface *, 27> nbr;   // indexed by relative offset
    static int offsetIndex(int dx, int dy, int dz) {
        return (dx + 1) * 9 + (dy + 1) * 3 + (dz + 1);
    }
    P2PNetworkInterface *at(int dx, int dy, int dz) const { return nbr[offsetIndex(dx, dy, dz)]; }
    bool has(int dx, int dy, int dz) const { return at(dx, dy, dz) != nullptr; }
    void discoverNeighbours();

    // ---- state of the 2D part ----------------------------------------------
    int d;                  // 0 = not yet known
    bool leftExists;
    bool leftKnown;         // info of the left neighbour received (or no left neighbour)
    int leftD;
    bool leftHasFront;
    bool rminKnown;
    bool isRmin;
    bool wKnown;
    int w;
    std::vector<std::pair<int, int>> pendingFindW;   // (origin, dsent) received before d was set

    // ---- state of the 3D extension -------------------------------------------
    bool belowKnown;
    bool belowRmin;
    int belowD, belowW;
    bool decided;           // isCmin / "continuing" status is final
    bool isCmin;
    int h;
    std::vector<int> pendingFindH;                   // origins received before 'decided'

    // ---- visualisation --------------------------------------------------------
    std::set<int> seenBoxes;

    // ---- algorithm steps ---------------------------------------------------
    void onDSet();
    void tryRmin();
    void declareNotRmin();
    void startW();
    void onWSet();
    void tryCmin();
    void startH();
    void finalizeBox();
    void sendLayerInfo(bool rmin, int dv, int wv);
    void processFindW(int origin, int dsent);
    void processFindH(int origin);
    void flushPendingH();
    void floodBox(const BoxColorMsg &m);
    void paintAndFlood(int boxId, const int *mn, const int *mx);

    // ---- message handlers --------------------------------------------------
    void handleSetD(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void handleLeftInfo(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void handleFindW(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void handleSetW(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void handleLayerInfo(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void handleFindH(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void handleSetH(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void handleBoxColor(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);

    // ---- sending / statistics --------------------------------------------
    void sendAlgo(Message *m, P2PNetworkInterface *itf);   // counted algorithm message
    void sendViz(Message *m, P2PNetworkInterface *itf);    // visualisation message (not counted)

    static unsigned long algoMsgCount[MSG_BOX_COLOR - MSG_SET_D];
    static unsigned long vizMsgCount;
    static Time lastBoxTime;
    static std::vector<FoundBox> foundBoxes;
};

#endif /* BOXESCODE_HPP_ */