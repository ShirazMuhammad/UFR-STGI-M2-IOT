/**
* @file tpApp2Code.hpp
 **/

#ifndef tpApp2Code_H_
#define tpApp2Code_H_

#include "robots/blinkyBlocks/blinkyBlocksSimulator.h"
#include "robots/blinkyBlocks/blinkyBlocksWorld.h"
#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"

static const int BROADCAST_MSG_ID = 1001;
static const int ACKNOWLEDGE_MSG_ID = 1002;
static const int START_B_MSG_ID = 1003;
static const int START_C_MSG_ID = 1004;

using namespace BlinkyBlocks;

class TpApp2Code : public BlinkyBlocksBlockCode {
private:
    BlinkyBlocksBlock *module = nullptr;
    bool isA = false;
    int distance = 0;
    int currentRound = 0;
    int nbWaitedAnswers = 0;
    P2PNetworkInterface *parent = nullptr;

    int maxSubtreeDist = 0;
    bID furthestNodeId = 0;
    P2PNetworkInterface *bestChild = nullptr;

    int distanceToB = 0;
    int distanceToC = 0;

    void startElection();

public:
    TpApp2Code(BlinkyBlocksBlock *host);
    ~TpApp2Code() {}

    void startup() override;
    void myBroadcastFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void myAcknowledgeFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void startBFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void startCFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);

    void parseUserBlockElements(TiXmlElement *config) override;
    void onUserKeyPressed(unsigned char c, int x, int y) override;
    void onTap(int face) override;
    string onInterfaceDraw() override;

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return (new TpApp2Code((BlinkyBlocksBlock*)host));
    }
};

#endif /* tpApp2Code_H_ */