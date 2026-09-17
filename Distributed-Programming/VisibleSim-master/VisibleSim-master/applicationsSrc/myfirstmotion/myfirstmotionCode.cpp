/**
 * @file myfirstmotionCode.cpp
 **/

#include "myfirstmotionCode.hpp"

// Global variable accessible by all modules
Cell3DPosition goalPosition;

MyFirstMotionCode::MyFirstMotionCode(SlidingCubesBlock *host):SlidingCubesBlockCode(host),module(host) {
    if (!host) return;
    addMessageEventFunc2(WAKE_UP_MSG_ID,
                         std::bind(&MyFirstMotionCode::myBroadcastFunc,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2)
                        );
}

void MyFirstMotionCode::parseUserElements(TiXmlDocument *config) {
    TiXmlNode *vs = config->FirstChild("vs");
    if (!vs) return;
    TiXmlNode *node = vs->FirstChild("goal");
    if (!node) return;
    TiXmlElement *element = node->ToElement();
    const char *attr = element->Attribute("position");
    if (attr) {
        goalPosition = Simulator::extractCell3DPositionFromString(attr);
        std::cout << "goalPosition = " << goalPosition << std::endl;
    } else {
        goalPosition.set(1,5,0);
    }
}

void MyFirstMotionCode::parseUserBlockElements(TiXmlElement *config) {
    const char *attr = config->Attribute("leader");
    isLeader = (attr ? Simulator::extractBoolFromString(attr) : false);
}

void MyFirstMotionCode::onGlDraw() {
    static const float thick = 0.8;
    static const float color[4] = {1.0f, 0.2f, 0.2f, 1.0f}; // Red circle
    const Vector3D gl = lattice->gridScale;

    glDisable(GL_TEXTURE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
    glPushMatrix();
    glNormal3f(0, 0, 1);
    glScalef(gl[0], gl[1], gl[2]);
    glTranslatef(goalPosition[0], goalPosition[1], goalPosition[2] - 0.49);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 36; i++) {
        double cs = 0.5 * cos(i * M_PI / 18);
        double ss = 0.5 * sin(i * M_PI / 18);
        glVertex3f(thick * cs, thick * ss, 0);
        glVertex3f(cs, ss, 0);
    }
    glEnd();
    glPopMatrix();
}

void MyFirstMotionCode::startup() {
    if (isLeader) {
        isMoving = true;
        tryMove();
    }
}

int MyFirstMotionCode::getDistanceToGoal(Cell3DPosition pos) {
    // Manhattan distance to calculate how far a block is from the goal
    return abs(pos[0] - goalPosition[0]) + abs(pos[1] - goalPosition[1]) + abs(pos[2] - goalPosition[2]);
}

bool MyFirstMotionCode::tryMove() {
    Cell3DPosition pos = module->position;
    if (pos == goalPosition) {
        std::cout << "Target Reached!" << std::endl;
        return false;
    }

    // Determine the primary direction of travel (Y-axis first, then X-axis for Question 3)
    int dy = 0, dx = 0;
    if (pos[1] != goalPosition[1]) {
        dy = (pos[1] < goalPosition[1]) ? 1 : -1;
    } else if (pos[0] != goalPosition[0]) {
        dx = (pos[0] < goalPosition[0]) ? 1 : -1;
    }

    // Priority list for movement (Allows climbing over the snake and obstacles for Question 4)
    vector<Cell3DPosition> potentialMoves = {
        Cell3DPosition(pos[0] + dx, pos[1] + dy, pos[2] - 1), // 1. Try to step down forward
        Cell3DPosition(pos[0] + dx, pos[1] + dy, pos[2]),     // 2. Try to move flat forward
        Cell3DPosition(pos[0] + dx, pos[1] + dy, pos[2] + 1), // 3. Try to step up forward (climb)
        Cell3DPosition(pos[0], pos[1], pos[2] + 1)            // 4. Try to move straight up
    };

    for (auto target : potentialMoves) {
        if (module->canMoveTo(target)) {
            module->moveTo(target);
            return true;
        }
    }
    return false; // Blocked or arrived
}

void MyFirstMotionCode::onMotionEnd() {
    // Attempt to keep moving. If blocked, pass the movement token to the tail.
    if (!tryMove()) {
        isMoving = false;
        // The '0' at the end is required by the API!
        sendMessageToAllNeighbors("WAKE", new Message(WAKE_UP_MSG_ID), 100, 10, 0);
    }
}

void MyFirstMotionCode::myBroadcastFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    // If a module receives the wake message, it checks if it is the furthest block back.
    if (isTail()) {
        isMoving = true;
        tryMove();
    } else {
        // Otherwise, forward the message down the snake
        sendMessageToAllNeighbors("WAKE", new Message(WAKE_UP_MSG_ID), 100, 10, 1, sender);
    }
}

bool MyFirstMotionCode::isTail() {
    // A module is the tail if NONE of its neighbors are further away from the goal than it is.
    for (int i = 0; i < 6; i++) {
        // The static_cast is required by the SlidingCubes API!
        P2PNetworkInterface *itf = module->getInterface(static_cast<SCLattice2::Direction>(i));
        if (itf && itf->connectedInterface) {
            Cell3DPosition neighborPos = itf->connectedInterface->hostBlock->position;
            if (getDistanceToGoal(neighborPos) > getDistanceToGoal(module->position)) {
                return false; // Found a neighbor further back, so I am not the tail.
            }
        }
    }
    return true;
}