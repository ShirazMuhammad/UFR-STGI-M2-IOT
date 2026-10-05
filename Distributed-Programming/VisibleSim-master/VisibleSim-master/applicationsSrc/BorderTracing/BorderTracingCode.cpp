#include "BorderTracingCode.hpp"
#include <iostream>

using namespace std;
using namespace BlinkyBlocks;

BorderTracingCode::BorderTracingCode(BlinkyBlocksBlock *host) : BlinkyBlocksBlockCode(host), module(host) {
    addMessageEventFunc2(BORDER_SEARCH_MSG_ID,
                         std::bind(&BorderTracingCode::myBorderSearchFunc, this,
                                   std::placeholders::_1, std::placeholders::_2));

    addMessageEventFunc2(BORDER_BROADCAST_MSG_ID,
                         std::bind(&BorderTracingCode::myBorderBroadcastFunc, this,
                                   std::placeholders::_1, std::placeholders::_2));
}

BlockCode* BorderTracingCode::buildNewBlockCode(BuildingBlock *host) {
    return(new BorderTracingCode((BlinkyBlocksBlock*)host));
}

P2PNetworkInterface* BorderTracingCode::getInterfaceForDir(int dir) {
    int targetX = module->position.pt[0];
    int targetY = module->position.pt[1];
    if (dir == 0) targetX++; else if (dir == 1) targetY++; else if (dir == 2) targetX--; else if (dir == 3) targetY--;

    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface *interface = module->getInterface(i);
        if (interface && interface->connectedInterface) {
            BlinkyBlocksBlock* neighbor = (BlinkyBlocksBlock*)interface->connectedInterface->hostBlock;
            if (neighbor->position.pt[0] == targetX && neighbor->position.pt[1] == targetY) return interface;
        }
    }
    return nullptr;
}

bool BorderTracingCode::isEmpty(int x, int y) {
    auto lattice = BlinkyBlocks::getWorld()->lattice;
    if (x < 0 || y < 0 || x >= lattice->gridSize[0] || y >= lattice->gridSize[1]) return true;
    return !lattice->cellHasBlock(Cell3DPosition(x, y, 0));
}

bool BorderTracingCode::isInitiator() {
    int x = module->position.pt[0];
    int y = module->position.pt[1];
    return isEmpty(x, y - 1) && (isEmpty(x - 1, y) || !isEmpty(x + 1, y - 1));
}

bool BorderTracingCode::isPosLess(Cell3DPosition p1, Cell3DPosition p2) {
    return (p1[0] < p2[0]) || (p1[0] == p2[0] && p1[1] < p2[1]);
}

bool BorderTracingCode::isOnBorder() {
    int x = module->position.pt[0];
    int y = module->position.pt[1];
    int neighbors = (!isEmpty(x+1, y)) + (!isEmpty(x, y+1)) + (!isEmpty(x-1, y)) + (!isEmpty(x, y-1));
    return neighbors < 4;
}

int BorderTracingCode::getNextDir(int prevDir) {
    int k = (prevDir + 3) % 4;
    int x = module->position.pt[0];
    int y = module->position.pt[1];

    for (int i = 0; i < 4; i++) {
        int nx = x, ny = y;
        if (k == 0) nx++; else if (k == 1) ny++; else if (k == 2) nx--; else if (k == 3) ny--;

        if (!isEmpty(nx, ny)) return k;
        k = (k + 1) % 4;
    }
    return prevDir;
}

void BorderTracingCode::startup() {
    if (isInitiator()) {
        isBorderOrInitiator = true;
        module->setColor(YELLOW);
        int FRONT = 0;
        int nextDir = getNextDir(FRONT);

        std::vector<Cell3DPosition> border;
        border.push_back(module->position);

        BorderSearchMsg *msg = new BorderSearchMsg(nextDir, border, module->position);
        P2PNetworkInterface* outFace = getInterfaceForDir(nextDir);

        if (outFace) {
            sendMessage(msg, outFace, BaseSimulator::getScheduler()->now() + 50, 50);
        } else {
            delete msg;
        }
    }
}

void BorderTracingCode::startBroadcast(Cell3DPosition initPos) {
    if (broadcastStarted.count(initPos)) return;
    broadcastStarted.insert(initPos);

    P2PNetworkInterface* firstInterface = nullptr;
    int receiverId = -1;

    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface *interface = module->getInterface(i);
        if (interface && interface->connectedInterface) {
            BlinkyBlocksBlock* neighbor = (BlinkyBlocksBlock*)interface->connectedInterface->hostBlock;
            firstInterface = interface;
            receiverId = neighbor->blockId;
            break;
        }
    }

    if (firstInterface && firstInterface->connectedInterface) {
        console << "Initiator [";
        console << module->blockId;
        console << "] starting broadcast to Receiver [";
        console << receiverId;
        console << "]\n";

        BorderBroadcastMsg *bMsg = new BorderBroadcastMsg(initPos);
        sendMessage(bMsg, firstInterface, BaseSimulator::getScheduler()->now() + 500, 500);
    }
}

void BorderTracingCode::myBorderSearchFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    std::shared_ptr<BorderSearchMsg> msg = std::static_pointer_cast<BorderSearchMsg>(_msg);
    Cell3DPosition P = module->position;

    if (P == msg->initiatorPos) {
        isBorderOrInitiator = true;
        module->setColor(BLUE);

        console << "Loop completed by Block [";
        console << module->blockId;
        console << "]\n";

        startBroadcast(msg->initiatorPos);
        return;
    }

    if (dirInit.count(msg->prevDir) && isPosLess(dirInit[msg->prevDir], msg->initiatorPos)) {
        return;
    }

    isBorderOrInitiator = true;
    int nextDir = getNextDir(msg->prevDir);
    std::vector<Cell3DPosition> newBorder = msg->border;

    if (nextDir != msg->prevDir && isOnBorder()) {
        newBorder.push_back(P);
        module->setColor(YELLOW);
    } else {
        module->setColor(RED);
    }

    dirInit[msg->prevDir] = msg->initiatorPos;

    BorderSearchMsg *forwardMsg = new BorderSearchMsg(nextDir, newBorder, msg->initiatorPos);
    P2PNetworkInterface* outFace = getInterfaceForDir(nextDir);

    if (outFace) {
        sendMessage(forwardMsg, outFace, BaseSimulator::getScheduler()->now() + 50, 50);
    } else {
        delete forwardMsg;
    }
}

void BorderTracingCode::myBorderBroadcastFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    std::shared_ptr<BorderBroadcastMsg> msg = std::static_pointer_cast<BorderBroadcastMsg>(_msg);
    Cell3DPosition initPos = msg->initiatorPos;

    if (broadcastReceived) return;
    broadcastReceived = true;

    if (!isBorderOrInitiator) {
        module->setColor(CYAN);
    }

    if (module->position == initPos) {
        console << "Broadcast loop completed back at Initiator [";
        console << module->blockId;
        console << "]\n";
        return;
    }

    P2PNetworkInterface* nextInterface = nullptr;
    int nextNeighborId = -1;

    for (int i = 0; i < 6; i++) {
        P2PNetworkInterface *interface = module->getInterface(i);
        if (interface && interface->connectedInterface && interface != sender) {
            BlinkyBlocksBlock* neighbor = (BlinkyBlocksBlock*)interface->connectedInterface->hostBlock;
            nextInterface = interface;
            nextNeighborId = neighbor->blockId;
            break;
        }
    }

    if (nextInterface && nextInterface->connectedInterface) {
        console << "Block [";
        console << module->blockId;
        console << "] forwarding to Receiver [";
        console << nextNeighborId;
        console << "]\n";

        BorderBroadcastMsg *fMsg = new BorderBroadcastMsg(initPos);
        sendMessage(fMsg, nextInterface, BaseSimulator::getScheduler()->now() + 500, 500);
    }
}