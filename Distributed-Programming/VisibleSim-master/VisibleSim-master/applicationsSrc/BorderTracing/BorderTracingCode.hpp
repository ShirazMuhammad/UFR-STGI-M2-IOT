#ifndef BORDERTRACINGCODE_H_
#define BORDERTRACINGCODE_H_

#include "robots/blinkyBlocks/blinkyBlocksSimulator.h"
#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"
#include <vector>
#include <map>
#include <set>

#define BORDER_SEARCH_MSG_ID 9001
#define BORDER_BROADCAST_MSG_ID 9002

using namespace BlinkyBlocks;

class BorderSearchMsg : public Message {
public:
    int prevDir;
    std::vector<Cell3DPosition> border;
    Cell3DPosition initiatorPos;

    BorderSearchMsg(int pDir, std::vector<Cell3DPosition> b, Cell3DPosition initPos) : Message() {
        type = BORDER_SEARCH_MSG_ID;
        prevDir = pDir;
        border = b;
        initiatorPos = initPos;
    }

    BorderSearchMsg* clone() const override {
        return new BorderSearchMsg(prevDir, border, initiatorPos);
    }
};

class BorderBroadcastMsg : public Message {
public:
    Cell3DPosition initiatorPos;

    BorderBroadcastMsg(Cell3DPosition initPos) : Message() {
        type = BORDER_BROADCAST_MSG_ID;
        initiatorPos = initPos;
    }

    BorderBroadcastMsg* clone() const override {
        return new BorderBroadcastMsg(initiatorPos);
    }
};

class BorderTracingCode : public BlinkyBlocksBlockCode {
private:
    BlinkyBlocksBlock *module = nullptr;
    std::map<int, Cell3DPosition> dirInit;
    std::set<Cell3DPosition> broadcastStarted;
    bool broadcastReceived = false;
    bool isBorderOrInitiator = false;

public:
    BorderTracingCode(BlinkyBlocksBlock *host);
    ~BorderTracingCode() {};

    void startup() override;
    void myBorderSearchFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void myBorderBroadcastFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);

    bool isEmpty(int x, int y);
    bool isInitiator();
    int getNextDir(int prevDir);
    bool isOnBorder();
    bool isPosLess(Cell3DPosition p1, Cell3DPosition p2);
    P2PNetworkInterface* getInterfaceForDir(int dir);
    void startBroadcast(Cell3DPosition initPos);

    static BlockCode *buildNewBlockCode(BuildingBlock *host);
};

#endif /* BORDERTRACINGCODE_H_ */