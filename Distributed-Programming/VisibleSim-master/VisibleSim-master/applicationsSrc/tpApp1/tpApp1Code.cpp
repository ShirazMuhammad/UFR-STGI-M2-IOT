/**
 * @file tpApp1Code.cpp
 **/

#include "tpApp1Code.hpp"

TpApp1Code::TpApp1Code(BlinkyBlocksBlock *host) : BlinkyBlocksBlockCode(host), module(host) {
    if (not host) return;

    addMessageEventFunc2(BROADCAST_MSG_ID,
                       std::bind(&TpApp1Code::myBroadcastFunc, this,
                       std::placeholders::_1, std::placeholders::_2));

    addMessageEventFunc2(ACKNOWLEDGE_MSG_ID,
                       std::bind(&TpApp1Code::myAcknowledgeFunc, this,
                       std::placeholders::_1, std::placeholders::_2));
}

void TpApp1Code::parseUserElements(TiXmlDocument *config) {
    TiXmlNode *node = config->FirstChild("parameters");
    if (!node) return;

    TiXmlElement *element = node->ToElement();
    const char *attr = element->Attribute("myColor");

    if (attr) {
        myColor = Simulator::extractColorFromString(attr);
    } else {
        myColor = GREY;
    }
}

void TpApp1Code::parseUserBlockElements(TiXmlElement *config) {
    const char *attr = config->Attribute("leader");
    isLeader = (attr ? Simulator::extractBoolFromString(attr) : false);
}

void TpApp1Code::startup() {
    console << "start " << getId() << "\n";

    if (isLeader) {
        leaderId = getId();
        distance = 0;
        parent = nullptr;
        setColor(BLACK);

        for (int i = 0; i < module->getNbInterfaces(); i++) {
            P2PNetworkInterface *p2p = module->getInterface(i);
            if (p2p->isConnected()) {
                p2p->send(new MessageOf<pair<int, int>>(BROADCAST_MSG_ID, make_pair(distance + 1, leaderId)));
                console << "sends distance(" << distance + 1 << "," << leaderId << ") to " << p2p->getConnectedBlockId() << "\n";
            }
        }
    }
}

void TpApp1Code::onTap(int face) {
    std::cout << ">>> LEADER SHIFTED TO BLOCK " << getId() << " <<<" << std::endl;
    console << "Tapped! Becoming new leader.\n";

    // 1. Make this block the new leader
    isLeader = true;
    leaderId = getId();
    distance = 0;
    parent = nullptr;

    // 2. Turn this block BLACK
    setColor(BLACK);

    // 3. Flood the network with the new leader ID
    for (int i = 0; i < module->getNbInterfaces(); i++) {
        P2PNetworkInterface *p2p = module->getInterface(i);
        if (p2p->isConnected()) {
            p2p->send(new MessageOf<pair<int, int>>(BROADCAST_MSG_ID, make_pair(distance + 1, leaderId)));
            console << "sends distance(" << distance + 1 << "," << leaderId << ") to " << p2p->getConnectedBlockId() << "\n";
        }
    }
}

void TpApp1Code::myBroadcastFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    MessageOf<pair<int, int>>* msg = static_cast<MessageOf<pair<int, int>>*>(_msg.get());
    pair<int, int> msgData = *msg->getData();

    int recDistance = msgData.first;
    int recLeaderId = msgData.second;

    console << "rec. Flood (" << recDistance << "," << recLeaderId << ") from " << sender->getConnectedBlockId() << "\n";

    // Detect if a new leader has taken over
    if (recLeaderId != leaderId) {
        leaderId = recLeaderId;
        isLeader = (recLeaderId == getId());
        distance = 9999; // Reset distance metric for new leader
        parent = nullptr;
    }

    // Process shorter distance metric for current active leader
    if (!isLeader && recDistance < distance) {
        distance = recDistance;
        parent = sender;

        // Dynamic color gradient based on distance from new BLACK leader
        switch (distance) {
            case 1: setColor(CYAN); break;
            case 2: setColor(GREEN); break;
            case 3: setColor(YELLOW); break;
            case 4: setColor(ORANGE); break;
            case 5: setColor(WHITE); break;
            case 6: setColor(MAGENTA); break;
            case 7: setColor(PINK); break;
            default: setColor(BLUE); break;
        }

        // Forward broadcast to connected neighbors
        for (int i = 0; i < module->getNbInterfaces(); i++) {
            P2PNetworkInterface *p2p = module->getInterface(i);
            if (p2p->isConnected() && p2p != sender) {
                p2p->send(new MessageOf<pair<int, int>>(BROADCAST_MSG_ID, make_pair(distance + 1, leaderId)));
                console << "sends distance(" << distance + 1 << "," << leaderId << ") to " << p2p->getConnectedBlockId() << "\n";
            }
        }

        if (parent) {
            parent->send(new MessageOf<int>(ACKNOWLEDGE_MSG_ID, getId()));
            console << "sends ack2parent to " << parent->getConnectedBlockId() << "\n";
        }
    }
}

void TpApp1Code::myAcknowledgeFunc(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    MessageOf<int>* msg = static_cast<MessageOf<int>*>(_msg.get());
    int ackSenderId = *msg->getData();

    console << "rec. Ack(" << ackSenderId << ") from " << sender->getConnectedBlockId() << "\n";
}

void TpApp1Code::onUserArrowKeyPressed(unsigned char c, int x, int y) {
    GlBlock *glBlock = module ? module->getGlBlock() : nullptr;
    BlinkyBlocksWorld *world = BlinkyBlocksWorld::getWorld();

    // Check if THIS block is selected in the 3D view
    if (world && glBlock && world->getselectedGlBlock() == glBlock) {
        switch (c) {
            case GLUT_KEY_LEFT:
                // Schedule tap event on simulator scheduler
                getScheduler()->schedule(new TapEvent(getScheduler()->now(), module, 0));
                break;
            case GLUT_KEY_RIGHT:
                break;
        }
    }
}