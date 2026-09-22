/**
 * @file tpAppMotionCode4.cpp
 **/

#include "tpAppMotionCode4.hpp"

Cell3DPosition goalPosition;

TpAppMotionCode4::TpAppMotionCode4(SlidingCubesBlock *host) : SlidingCubesBlockCode(host), module(host) {
    if (!host) return;

    addMessageEventFunc2(ELECTFIRST_MSG_ID,
                       std::bind(&TpAppMotionCode4::myElectFirstFunc, this,
                       std::placeholders::_1, std::placeholders::_2));
}

void TpAppMotionCode4::startup() {
    console << "start " << getId() << "\n";

    if (isLeader) {
        setColor(RED);
        moveToNextStep();
    }
}

void TpAppMotionCode4::myElectFirstFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    passBaton();
}

void TpAppMotionCode4::parseUserBlockElements(TiXmlElement *config) {
    const char *attr = config->Attribute("leader");
    isLeader = (attr ? Simulator::extractBoolFromString(attr) : false);
    if (isLeader) {
        std::cout << getId() << " is leader!" << std::endl;
    }
}

void TpAppMotionCode4::parseUserElements(TiXmlDocument *config) {
    TiXmlNode *vs = config->FirstChild("vs");
    if (!vs) return;

    TiXmlNode *node = vs->FirstChild("goal");
    if (node) {
        TiXmlElement *element = node->ToElement();
        const char *attr = element->Attribute("position");
        if (attr) {
            goalPosition = Simulator::extractCell3DPositionFromString(attr);
            std::cout << "goalPosition = " << goalPosition << std::endl;
            return;
        }
    }

    // Auto-detect target center from XML configuration
    if (target) {
        auto b = lattice->getGridUpperBounds();
        for (int iz = 0; iz <= b[2]; iz++) {
            for (int iy = 0; iy <= b[1]; iy++) {
                for (int ix = 0; ix <= b[0]; ix++) {
                    Cell3DPosition testPos(ix, iy, iz);
                    if (target->isInTarget(testPos)) {
                        goalPosition = testPos;
                        return;
                    }
                }
            }
        }
    }
    goalPosition.set(1, 5, 0);
}

void TpAppMotionCode4::onMotionEnd() {
    if (isLeader) {
        moveToNextStep();
    }
}

void TpAppMotionCode4::passBaton() {
    int myDist = abs(module->position[0] - goalPosition[0]) +
                 abs(module->position[1] - goalPosition[1]) +
                 abs(module->position[2] - goalPosition[2]);

    P2PNetworkInterface* furtherNeighbor = nullptr;
    int maxDist = myDist;

    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface *p2p = module->getInterface(static_cast<SCLattice2::Direction>(i));

        if (p2p && p2p->connectedInterface) {
            BuildingBlock *neighbor = p2p->connectedInterface->hostBlock;
            if (neighbor) {
                int nDist = abs(neighbor->position[0] - goalPosition[0]) +
                            abs(neighbor->position[1] - goalPosition[1]) +
                            abs(neighbor->position[2] - goalPosition[2]);

                if (nDist > maxDist) {
                    maxDist = nDist;
                    furtherNeighbor = p2p;
                }
            }
        }
    }

    if (furtherNeighbor) {
        sendMessage("elect", new Message(ELECTFIRST_MSG_ID), furtherNeighbor, 1000, 10);
    } else {
        console << "I am the new tail 🐍 Starting motion...\n";
        isLeader = true;
        setColor(RED);
        moveToNextStep();
    }
}

void TpAppMotionCode4::moveToNextStep() {
    // 1. If we reached a cell in the target zone, dock it and pass the baton
    if (target && target->isInTarget(module->position)) {
        console << "Block docked in target zone!\n";
        setColor(GREEN);
        isLeader = false;
        passBaton();
        return;
    }

    // 2. Find the closest EMPTY red circle target cell
    Cell3DPosition activeTarget = goalPosition;
    int minTgtDist = 10000;

    if (target) {
        auto b = lattice->getGridUpperBounds();
        for (int iz = 0; iz <= b[2]; iz++) {
            for (int iy = 0; iy <= b[1]; iy++) {
                for (int ix = 0; ix <= b[0]; ix++) {
                    Cell3DPosition c(ix, iy, iz);
                    if (target->isInTarget(c)) {
                        if (lattice->getBlock(c) == nullptr) {
                            int d = abs(module->position[0] - c[0]) +
                                    abs(module->position[1] - c[1]) +
                                    abs(module->position[2] - c[2]);
                            if (d < minTgtDist) {
                                minTgtDist = d;
                                activeTarget = c;
                            }
                        }
                    }
                }
            }
        }
    }

    Cell3DPosition bestPos;
    int minDistance = 10000;
    bool foundMove = false;

    // 3. Evaluate all 6 orthogonal directions (Strictly no diagonals to protect physics)
    int dxList[] = {0, 0, 0, 0, 1, -1};
    int dyList[] = {0, 0, 1, -1, 0, 0};
    int dzList[] = {1, -1, 0, 0, 0, 0};

    for (int i = 0; i < 6; i++) {
        Cell3DPosition nextPos(module->position[0] + dxList[i],
                               module->position[1] + dyList[i],
                               module->position[2] + dzList[i]);

        // Prevent immediate backtracking to avoid oscillation
        if (hasPrevious && nextPos == previousPosition) {
            continue;
        }

        if (module->canMoveTo(nextPos)) {
            int dist = abs(nextPos[0] - activeTarget[0]) +
                       abs(nextPos[1] - activeTarget[1]) +
                       abs(nextPos[2] - activeTarget[2]);

            // Allow side-steps and climbs even if distance temporarily increases (essential for un-clumping)
            if (dist < minDistance) {
                minDistance = dist;
                bestPos = nextPos;
                foundMove = true;
            }
        }
    }

    if (foundMove) {
        previousPosition = module->position;
        hasPrevious = true;
        module->moveTo(bestPos);
    } else {
        // If completely cornered, pass the turn back down the chain
        console << "Block is blocked. Passing baton backward...\n";
        hasPrevious = false;
        isLeader = false;
        setColor(GREY);

        bool canPass = false;
        int myDistToGoal = abs(module->position[0] - goalPosition[0]) +
                           abs(module->position[1] - goalPosition[1]) +
                           abs(module->position[2] - goalPosition[2]);

        for (int i = 0; i < 6; i++) {
            P2PNetworkInterface *p2p = module->getInterface(static_cast<SCLattice2::Direction>(i));
            if (p2p && p2p->connectedInterface) {
                BuildingBlock *neighbor = p2p->connectedInterface->hostBlock;
                if (neighbor) {
                    int nDist = abs(neighbor->position[0] - goalPosition[0]) +
                                abs(neighbor->position[1] - goalPosition[1]) +
                                abs(neighbor->position[2] - goalPosition[2]);
                    if (nDist > myDistToGoal) {
                        canPass = true;
                        break;
                    }
                }
            }
        }

        if (canPass) {
            passBaton();
        }
    }
}
void TpAppMotionCode4::onGlDraw() {
    static const float thick = 0.8;
    static const float color[4] = {1.0f, 0.2f, 0.2f, 1.0f};
    const Vector3D gl = lattice->gridScale;

    glDisable(GL_TEXTURE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

    if (target) {
        auto b = lattice->getGridUpperBounds();
        for (int iz = 0; iz <= b[2]; iz++) {
            for (int iy = 0; iy <= b[1]; iy++) {
                for (int ix = 0; ix <= b[0]; ix++) {
                    Cell3DPosition cell(ix, iy, iz);
                    if (target->isInTarget(cell)) {
                        glPushMatrix();
                        glNormal3f(0, 0, 1);
                        glScalef(gl[0], gl[1], gl[2]);
                        glTranslatef(ix, iy, iz - 0.49);
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
                }
            }
        }
    }
}