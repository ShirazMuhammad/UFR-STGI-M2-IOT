/**
 * @file tpApp2Code.cpp
 **/

#include "tpApp2Code.hpp"

// Global variables required for HUD drawing and evaluation
BlinkyBlocksBlock *moduleA = nullptr;
BlinkyBlocksBlock *moduleB = nullptr;
BlinkyBlocksBlock *moduleC = nullptr;
BlinkyBlocksBlock *moduleCenter = nullptr;
int distAB = 0;
int distBC = 0;
int distBcenter = 0;
int distCcenter = 0;

TpApp2Code::TpApp2Code(BlinkyBlocksBlock *host) {} // placeholder if needed, use correct constructor below:

TpApp2Code::TpApp2Code(BlinkyBlocksBlock *host):BlinkyBlocksBlockCode(host),module(host) {
  if (not host) return;

  addMessageEventFunc2(BROADCAST_MSG_ID,
                       std::bind(&TpApp2Code::myBroadcastFunc,
                                 this,
                                 std::placeholders::_1,
                                 std::placeholders::_2)
                      );

  addMessageEventFunc2(ACKNOWLEDGE_MSG_ID,
                       std::bind(&TpApp2Code::myAcknowledgeFunc,
                                 this,
                                 std::placeholders::_1,
                                 std::placeholders::_2)
                      );

  addMessageEventFunc2(START_B_MSG_ID,
                       std::bind(&TpApp2Code::startBFunc,
                                 this,
                                 std::placeholders::_1,
                                 std::placeholders::_2)
                      );

  addMessageEventFunc2(START_C_MSG_ID,
                       std::bind(&TpApp2Code::startCFunc,
                                 this,
                                 std::placeholders::_1,
                                 std::placeholders::_2)
                      );
}

void TpApp2Code::startup() {
  console << "start " << getId() << "\n";
  distance = 0;
  currentRound = 0;
  parent = nullptr;
  maxSubtreeDist = 0;
  furthestNodeId = getId();
  bestChild = nullptr;

  if (isA) {
    moduleA = module;
    if (module) module->setColor(RED);
    startElection();
  }
}

void TpApp2Code::startElection() {
  currentRound++;
  distance = 0;
  parent = nullptr;
  maxSubtreeDist = 0;
  furthestNodeId = getId();
  bestChild = nullptr;

  console << getId() << " starting election flood for round " << currentRound << "\n";

  string str = "distance(1," + to_string(currentRound) + ")";
  nbWaitedAnswers = sendMessageToAllNeighbors(
                       str.c_str(),
                       new MessageOf<pair<int,int>>(BROADCAST_MSG_ID, make_pair(distance + 1, currentRound)),
                       1000, 100, 0);
}

void TpApp2Code::myBroadcastFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
  MessageOf<pair<int,int>>* msg = static_cast<MessageOf<pair<int,int>>*>(_msg.get());
  pair<int,int> msgData = *msg->getData();
  int newDistance = msgData.first;
  int newRound = msgData.second;

  console << "rec. Flood (" << newDistance << "," << newRound << ") from " << sender->getConnectedBlockId() << "\n";

  if (newRound > currentRound) {
    currentRound = newRound;
    distance = newDistance;
    parent = sender;
    maxSubtreeDist = 0;
    furthestNodeId = getId();
    bestChild = nullptr;

    if (currentRound == 2) {
      distanceToB = distance;
    } else if (currentRound == 3) {
      distanceToC = distance;
      // Center D condition: middle of BC diameter
      if (abs(distanceToB - distanceToC) <= 1 && (distanceToB + distanceToC == distBC)) {
        moduleCenter = module;
        distBcenter = distanceToB;
        distCcenter = distanceToC;
        if (module) module->setColor(ORANGE);
        std::cout << "Center D elected at ID " << getId() << std::endl;
      }
    }

    string str = "distance(" + to_string(distance + 1) + "," + to_string(currentRound) + ")";
    nbWaitedAnswers = sendMessageToAllNeighbors(
                         str.c_str(),
                         new MessageOf<pair<int,int>>(BROADCAST_MSG_ID, make_pair(distance + 1, currentRound)),
                         1000, 100, 1, sender);
    if (nbWaitedAnswers == 0) {
      sendMessage("ack2parent", new MessageOf<pair<int,int>>(ACKNOWLEDGE_MSG_ID, make_pair(distance, getId())), parent, 1000, 100);
    }
  } else {
    sendMessage("ack2sender", new MessageOf<pair<int,int>>(ACKNOWLEDGE_MSG_ID, make_pair(0, 0)), sender, 1000, 100);
  }
}

void TpApp2Code::myAcknowledgeFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
  MessageOf<pair<int,int>>* msg = static_cast<MessageOf<pair<int,int>>*>(_msg.get());
  if (msg && msg->getData()) {
    pair<int,int> childData = *msg->getData();
    int childDist = childData.first;
    bID childId = childData.second;

    if (childDist > maxSubtreeDist) {
      maxSubtreeDist = childDist;
      furthestNodeId = childId;
      bestChild = sender;
    }
  }

  nbWaitedAnswers--;
  console << "rec. Ack from " << sender->getConnectedBlockId() << ", waited: " << nbWaitedAnswers << "\n";

  if (nbWaitedAnswers == 0) {
    if (parent == nullptr) {
      if (currentRound == 1) {
        distAB = maxSubtreeDist;
        std::cout << "Phase 1 complete. distAB = " << distAB << ", B ID = " << furthestNodeId << std::endl;
        if (bestChild) {
          sendMessage("startB", new MessageOf<bID>(START_B_MSG_ID, furthestNodeId), bestChild, 1000, 100);
        }
      } else if (currentRound == 2) {
        distBC = maxSubtreeDist;
        std::cout << "Phase 2 complete. distBC = " << distBC << ", C ID = " << furthestNodeId << std::endl;
        if (bestChild) {
          sendMessage("startC", new MessageOf<bID>(START_C_MSG_ID, furthestNodeId), bestChild, 1000, 100);
        }
      }
    } else {
      sendMessage("ack2parent", new MessageOf<pair<int,int>>(ACKNOWLEDGE_MSG_ID, make_pair(maxSubtreeDist, furthestNodeId)), parent, 1000, 100);
    }
  }
}

void TpApp2Code::startBFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
  MessageOf<bID>* msg = static_cast<MessageOf<bID>*>(_msg.get());
  bID targetB = *msg->getData();

  if (getId() == targetB) {
    moduleB = module;
    if (module) module->setColor(BLUE);
    std::cout << "Node " << getId() << " elected as B (Blue)" << std::endl;
    startElection();
  } else {
    if (bestChild) {
      sendMessage("startB", new MessageOf<bID>(START_B_MSG_ID, targetB), bestChild, 1000, 100);
    }
  }
}

void TpApp2Code::startCFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
  MessageOf<bID>* msg = static_cast<MessageOf<bID>*>(_msg.get());
  bID targetC = *msg->getData();

  if (getId() == targetC) {
    moduleC = module;
    if (module) module->setColor(GREEN);
    std::cout << "Node " << getId() << " elected as C (Green)" << std::endl;
    startElection();
  } else {
    if (bestChild) {
      sendMessage("startC", new MessageOf<bID>(START_C_MSG_ID, targetC), bestChild, 1000, 100);
    }
  }
}

void TpApp2Code::parseUserBlockElements(TiXmlElement *config) {
  const char *attr = config->Attribute("isA");
  isA = (attr ? Simulator::extractBoolFromString(attr) : false);
  if (isA) {
    std::cout << getId() << " is A!" << std::endl;
  }
}

void TpApp2Code::onUserKeyPressed(unsigned char c, int x, int y) {
  switch (c) {
    case 'a': break;
    case 'd': break;
  }
}

void TpApp2Code::onTap(int face) {
  std::cout << "Block tapped: " << getId() << std::endl;
}

string TpApp2Code::onInterfaceDraw() {
    string str = "Press 'r' to run the simulation.";
    if (moduleA != nullptr) {
        str = "A=(" + moduleA->position.to_string() + ")";
    }
    if (moduleB != nullptr) {
        str += "\nB=(" + moduleB->position.to_string() + "), distAB=" + to_string(distAB);
    }
    if (moduleC != nullptr) {
        str += "\nC=(" + moduleC->position.to_string() + "), distBC=" + to_string(distBC);
    }
    if (moduleCenter != nullptr) {
        str += "\nCenter=(" + moduleCenter->position.to_string() + "), dist to B=" +
            to_string(distBcenter) + ", dist to C=" + to_string(distCcenter);
    }
    return str;
}