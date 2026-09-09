/**
* @file tpApp1Code.hpp
 **/

#ifndef tpApp1Code_H_
#define tpApp1Code_H_

#include "robots/blinkyBlocks/blinkyBlocksSimulator.h"
#include "robots/blinkyBlocks/blinkyBlocksWorld.h"
#include "robots/blinkyBlocks/blinkyBlocksBlockCode.h"

static const int BROADCAST_MSG_ID = 1001;
static const int ACKNOWLEDGE_MSG_ID = 1002;

using namespace BlinkyBlocks;

class TpApp1Code : public BlinkyBlocksBlockCode {
private:
    BlinkyBlocksBlock *module = nullptr;

    bool isLeader = false;
    int leaderId = -1;
    Color myColor = GREY;
    int distance = 9999;
    P2PNetworkInterface *parent = nullptr;

public:
    TpApp1Code(BlinkyBlocksBlock *host);
    ~TpApp1Code() {};

    void startup() override;
    void myBroadcastFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void myAcknowledgeFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);
    void parseUserBlockElements(TiXmlElement *config) override;
    void parseUserElements(TiXmlDocument *config) override;
    void onUserArrowKeyPressed(unsigned char c, int x, int y) override;
    void onTap(int face) override;

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return (new TpApp1Code((BlinkyBlocksBlock*)host));
    }
};

#endif /* tpApp1Code_H_ */