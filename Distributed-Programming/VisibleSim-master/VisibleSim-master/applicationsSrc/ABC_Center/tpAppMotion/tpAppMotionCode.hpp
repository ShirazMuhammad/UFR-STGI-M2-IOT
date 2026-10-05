#ifndef tpAppMotionCode_H_
#define tpAppMotionCode_H_

#include "robots/slidingCubes/slidingCubesSimulator.h"
#include "robots/slidingCubes/slidingCubesWorld.h"
#include "robots/slidingCubes/slidingCubesBlockCode.h"

using namespace SlidingCubes;

class TpAppMotionCode : public SlidingCubesBlockCode {
private:
    SlidingCubesBlock *module = nullptr;

public:
    TpAppMotionCode(SlidingCubesBlock *host);
    ~TpAppMotionCode() {};

    void startup() override;
    void parseUserElements(TiXmlDocument *config) override;
    void parseUserBlockElements(TiXmlElement *config) override;
    void onMotionEnd() override;
    void onGlDraw() override;

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return(new TpAppMotionCode((SlidingCubesBlock*)host));
    }
};

#endif /* tpAppMotionCode_H_ */