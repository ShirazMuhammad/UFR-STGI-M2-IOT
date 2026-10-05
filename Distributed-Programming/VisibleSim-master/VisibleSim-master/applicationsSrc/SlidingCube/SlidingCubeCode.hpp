#ifndef SlidingCubeCode_H_
#define SlidingCubeCode_H_

#include "robots/slidingCubes/slidingCubesSimulator.h"
#include "robots/slidingCubes/slidingCubesWorld.h"
#include "robots/slidingCubes/slidingCubesBlockCode.h"

static const int ELECTFIRST_MSG_ID = 1001;

using namespace SlidingCubes;

class SlidingCubeCode : public SlidingCubesBlockCode {
public:
    bool isLocked = false;
private:
    SlidingCubesBlock *module = nullptr;
    bool isLeader = false;
    Color myColor = BLACK;
    Cell3DPosition previousPosition;
    bool hasPrevious = false;

public:
    SlidingCubeCode(SlidingCubesBlock *host);
    ~SlidingCubeCode() {};

    void startup() override;
    void myElectFirstFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface *sender);
    void parseUserElements(TiXmlDocument *config) override;
    void parseUserBlockElements(TiXmlElement *config) override;
    void onMotionEnd() override;
    void onGlDraw() override;
    void moveToNextStep();
    void passBaton();

    bool updateGoal();
    BuildingBlock* getBlockAt(const Cell3DPosition &pos);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return(new SlidingCubeCode((SlidingCubesBlock*)host));
    }
};

#endif /* SlidingCubeCode_H_ */