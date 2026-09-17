/**
* @file myfirstmotionCode.hpp
 **/

#ifndef myfirstmotionCode_H_
#define myfirstmotionCode_H_

#include "robots/slidingCubes/slidingCubesSimulator.h"
#include "robots/slidingCubes/slidingCubesWorld.h"
#include "robots/slidingCubes/slidingCubesBlockCode.h"

static const int WAKE_UP_MSG_ID = 1001;

using namespace SlidingCubes;

class MyFirstMotionCode : public SlidingCubesBlockCode {
private:
    SlidingCubesBlock *module = nullptr;
    bool isLeader = false;
    bool isMoving = false;

    // Helper functions for our autonomous locomotion algorithm
    bool tryMove();
    bool isTail();
    int getDistanceToGoal(Cell3DPosition pos);

public:
    MyFirstMotionCode(SlidingCubesBlock *host);
    ~MyFirstMotionCode() {}

    void startup() override;

    // Core methods required by the exercise
    void parseUserElements(TiXmlDocument *config) override;
    void parseUserBlockElements(TiXmlElement *config) override;
    void onGlDraw() override;
    void onMotionEnd() override;

    // Message handler to pass the movement token to the tail
    void myBroadcastFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface *sender);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return(new MyFirstMotionCode((SlidingCubesBlock*)host));
    }
};

#endif /* myfirstmotionCode_H_ */