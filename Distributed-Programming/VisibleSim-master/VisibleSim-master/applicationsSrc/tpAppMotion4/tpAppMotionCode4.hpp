/**
* @file tpAppMotionCode4.hpp
 **/

#ifndef TPAPPMOTIONCODE4_H_
#define TPAPPMOTIONCODE4_H_

#include "robots/slidingCubes/slidingCubesSimulator.h"
#include "robots/slidingCubes/slidingCubesWorld.h"
#include "robots/slidingCubes/slidingCubesBlockCode.h"

using namespace SlidingCubes;

extern Cell3DPosition goalPosition;
static const int ELECTFIRST_MSG_ID = 1001;

class TpAppMotionCode4 : public SlidingCubesBlockCode {
private:
    SlidingCubesBlock *module = nullptr;
    bool isLeader = false;
    bool hasPrevious = false;
    Cell3DPosition previousPosition;

    void passBaton();
    void moveToNextStep();

public:
    TpAppMotionCode4(SlidingCubesBlock *host);
    ~TpAppMotionCode4() {}

    void startup() override;
    void parseUserElements(TiXmlDocument *config) override;
    void parseUserBlockElements(TiXmlElement *config) override;
    void onGlDraw() override;
    void onMotionEnd() override;

    void myElectFirstFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return (new TpAppMotionCode4((SlidingCubesBlock*)host));
    }
};

#endif /* TPAPPMOTIONCODE4_H_ */