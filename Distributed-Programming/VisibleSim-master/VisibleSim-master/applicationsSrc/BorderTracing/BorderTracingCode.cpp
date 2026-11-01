/**
 * @file BorderTracingCode.cpp
 * @author Muhammad Shiraz
 * @date October 18, 2026
 * @brief VisibleSim adapter for the distributed border tracing algorithm.
 */
#include "BorderTracingCode.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <set>

using namespace BlinkyBlocks;

namespace {

typedef std::pair<int, int> Cell;

struct GlobalMetrics {
    std::size_t modules = 0;
    long searchMsgs = 0;
    long doneMsgs = 0;
    uint64_t lastEvent = 0;
    uint64_t lastBorderClosed = 0;
    std::set<Cell> cells;
    std::vector<bt::BorderInfo> borders;
    std::map<Cell, std::size_t> copies;
};

GlobalMetrics g;

uint64_t simTime() { return static_cast<uint64_t>(BaseSimulator::getScheduler()->now()); }

void touchClock() { g.lastEvent = std::max(g.lastEvent, simTime()); }

bool cellOccupied(int x, int y, int z) {
    Lattice *lattice = BaseSimulator::getWorld()->lattice;
    const Cell3DPosition c(x, y, z);
    return lattice->isInGrid(c) && lattice->cellHasBlock(c);
}

Color outerColor() { return Color(0.10f, 0.45f, 1.00f); }

Color holeColor(const bt::Pos &id) {
    static const Color palette[] = {Color(1.00f, 0.55f, 0.10f), Color(0.20f, 0.80f, 0.35f),
                                    Color(0.75f, 0.30f, 0.90f), Color(0.95f, 0.85f, 0.20f),
                                    Color(0.95f, 0.25f, 0.25f)};
    return palette[(id.x * 7 + id.y * 3) % 5];
}

int countExpectedBorders() {
    if (g.cells.empty()) return 0;
    int minx = 1 << 30, miny = 1 << 30, maxx = -(1 << 30), maxy = -(1 << 30);
    for (const Cell &c : g.cells) {
        minx = std::min(minx, c.first);  maxx = std::max(maxx, c.first);
        miny = std::min(miny, c.second); maxy = std::max(maxy, c.second);
    }
    --minx; --miny; ++maxx; ++maxy;
    std::set<Cell> seen;
    int regions = 0;
    for (int x = minx; x <= maxx; ++x) {
        for (int y = miny; y <= maxy; ++y) {
            if (g.cells.count(Cell(x, y)) || seen.count(Cell(x, y))) continue;
            ++regions;
            std::queue<Cell> q;
            q.push(Cell(x, y));
            seen.insert(Cell(x, y));
            while (!q.empty()) {
                const Cell c = q.front();
                q.pop();
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        const Cell n(c.first + dx, c.second + dy);
                        if (n.first < minx || n.first > maxx || n.second < miny || n.second > maxy) continue;
                        if (g.cells.count(n) || seen.count(n)) continue;
                        seen.insert(n);
                        q.push(n);
                    }
                }
            }
        }
    }
    return regions;
}

}  // namespace

BorderTracingCode::BorderTracingCode(BlinkyBlocksBlock *host) : BlinkyBlocksBlockCode(host), module(host) {
    if (not host) return;

    addMessageEventFunc2(BORDER_SEARCH_MSG_ID,
                         std::bind(&BorderTracingCode::onSearchMsg, this, std::placeholders::_1,
                                   std::placeholders::_2));
    addMessageEventFunc2(BORDER_DONE_MSG_ID,
                         std::bind(&BorderTracingCode::onDoneMsg, this, std::placeholders::_1,
                                   std::placeholders::_2));
}

void BorderTracingCode::startup() {
    const Cell3DPosition &p = module->position;
    self = bt::Pos{p.pt[0], p.pt[1]};
    g.cells.insert(Cell(self.x, self.y));
    ++g.modules;

    std::array<bool, 4> neighbor{};
    for (P2PNetworkInterface *itf : module->getP2PNetworkInterfaces()) {        if (itf == nullptr || !itf->isConnected()) continue;
        const Cell3DPosition &q = itf->connectedInterface->hostBlock->position;
        if (q.pt[2] != p.pt[2]) continue;
        const int dx = q.pt[0] - p.pt[0], dy = q.pt[1] - p.pt[1];
        for (int d = 0; d < 4; ++d) {
            if (dx == bt::dirDx(d) && dy == bt::dirDy(d)) {
                neighbor[d] = true;
                nbIf[d] = itf;
            }
        }
    }

    const bool diagBelowRight = cellOccupied(self.x + 1, self.y - 1, p.pt[2]);
    node = bt::BorderNode(self, neighbor, diagBelowRight);

    module->setColor(Color(0.30f, 0.30f, 0.33f));

    if (node.isInitiator()) {
        module->setColor(Color(0.90f, 0.10f, 0.80f));
        const bt::BorderNode::Outcome o = node.start();
        if (o.action == bt::BorderNode::Action::FORWARD) {
            forwardSearch(o.dir, o.msg);
        } else if (o.action == bt::BorderNode::Action::COMPLETE) {
            finishBorder(o.msg);
        }
    }
}

void BorderTracingCode::forwardSearch(int dir, const bt::SearchMsg &msg) {
    P2PNetworkInterface *itf = nbIf[dir];
    if (itf == nullptr) return;
    ++g.searchMsgs;
    sendMessage("BORDER_SEARCH", new MessageOf<bt::SearchMsg>(BORDER_SEARCH_MSG_ID, msg), itf, 0, 0);
}

void BorderTracingCode::onSearchMsg(std::shared_ptr<Message> _msg, P2PNetworkInterface *) {
    touchClock();
    MessageOf<bt::SearchMsg> *m = static_cast<MessageOf<bt::SearchMsg> *>(_msg.get());
    const bt::BorderNode::Outcome o = node.onSearch(*m->getData());
    switch (o.action) {
        case bt::BorderNode::Action::FORWARD:
            forwardSearch(o.dir, o.msg);
            break;
        case bt::BorderNode::Action::COMPLETE:
            finishBorder(o.msg);
            break;
        case bt::BorderNode::Action::DROP:
            break;
    }
}

void BorderTracingCode::finishBorder(const bt::SearchMsg &msg) {
    touchClock();
    g.lastBorderClosed = std::max(g.lastBorderClosed, simTime());
    const bt::BorderInfo info = bt::finalizeBorder(msg.init, msg.corners);
    g.borders.push_back(info);
    winner = true;

    std::cout << "[BORDER] initiator=(" << info.id.x << "," << info.id.y << ") "
              << (info.outer ? "OUTER" : "HOLE ") << " corners=" << info.rawCorners
              << " merged=" << info.mergedCorners << " vertical-edges=" << info.edges
              << " | bytes: corner-list=" << info.rawBytes() << " collinear-merged=" << info.mergedBytes()
              << " packed=" << info.packedBytes() << " | t=" << simTime() / 1000.0 << " ms" << std::endl;

    acceptBorder(info.id, info.outer, info.packed, nullptr, true);
}

bool BorderTracingCode::knowsBorder(const bt::Pos &id) const {
    for (const StoredBorder &b : known)
        if (b.id == id) return true;
    return false;
}

void BorderTracingCode::acceptBorder(const bt::Pos &id, bool outer, const std::vector<uint8_t> &packed,
                                     P2PNetworkInterface *from, bool isWinner) {
    if (knowsBorder(id)) return;
    StoredBorder sb;
    sb.id = id;
    sb.outer = outer;
    sb.packed = packed;
    known.push_back(sb);
    ++g.copies[Cell(id.x, id.y)];

    if (isWinner) {
        module->setColor(Color(1.0f, 1.0f, 1.0f));
    } else if (!winner && node.visitedBy(id)) {
        module->setColor(outer ? outerColor() : holeColor(id));
    }

    for (int d = 0; d < 4; ++d) {
        P2PNetworkInterface *itf = nbIf[d];
        if (itf == nullptr || itf == from) continue;
        BorderDonePayload payload;
        payload.id = id;
        payload.outer = outer;
        payload.packed = packed;
        ++g.doneMsgs;
        sendMessage("BORDER_DONE", new MessageOf<BorderDonePayload>(BORDER_DONE_MSG_ID, payload), itf, 0, 0);
    }
}

void BorderTracingCode::onDoneMsg(std::shared_ptr<Message> _msg, P2PNetworkInterface *sender) {
    touchClock();
    MessageOf<BorderDonePayload> *m = static_cast<MessageOf<BorderDonePayload> *>(_msg.get());
    const BorderDonePayload &d = *m->getData();
    acceptBorder(d.id, d.outer, d.packed, sender, false);
}

void BorderTracingCode::printGlobalReport() {
    if (g.modules == 0) return;

    const int expected = countExpectedBorders();
    std::size_t rawB = 0, mergedB = 0, packedB = 0;
    int outerCount = 0;
    std::vector<std::vector<uint8_t>> packed;
    for (const bt::BorderInfo &b : g.borders) {
        rawB += b.rawBytes();
        mergedB += b.mergedBytes();
        packedB += b.packedBytes();
        outerCount += b.outer ? 1 : 0;
        packed.push_back(b.packed);
    }

    bool coverage = (g.copies.size() == g.borders.size());
    for (const auto &kv : g.copies)
        if (kv.second != g.modules) coverage = false;

    int minx = 1 << 30, miny = 1 << 30, maxx = -(1 << 30), maxy = -(1 << 30);
    for (const Cell &c : g.cells) {
        minx = std::min(minx, c.first);  maxx = std::max(maxx, c.first);
        miny = std::min(miny, c.second); maxy = std::max(maxy, c.second);
    }
    long checked = 0, mismatches = 0;
    Cell firstBad(0, 0);
    for (int x = minx - 3; x <= maxx + 3; ++x) {
        for (int y = miny - 3; y <= maxy + 3; ++y) {
            const bool truth = g.cells.count(Cell(x, y)) > 0;
            const bool computed = bt::insideConfiguration(packed, x, y);
            ++checked;
            if (truth != computed) {
                if (mismatches == 0) firstBad = Cell(x, y);
                ++mismatches;
            }
        }
    }

    const long totalMsgs = g.searchMsgs + g.doneMsgs;
    std::cout << "\n==================== BORDER TRACING REPORT ====================\n"
              << "modules                       : " << g.modules << "\n"
              << "borders found / expected      : " << g.borders.size() << " / " << expected << "  ("
              << outerCount << " outer + " << (static_cast<int>(g.borders.size()) - outerCount)
              << " hole(s))\n"
              << "BORDER_SEARCH messages        : " << g.searchMsgs << "\n"
              << "BORDER_DONE messages (flood)  : " << g.doneMsgs << "\n"
              << "total messages                : " << totalMsgs << "\n"
              << std::fixed << std::setprecision(3)
              << "time, all borders closed      : " << g.lastBorderClosed / 1000.0 << " ms (simulated)\n"
              << "time, broadcast finished      : " << g.lastEvent / 1000.0 << " ms (simulated)\n"
              << "representation, all borders   : corner list = " << rawB << " B, collinear-merged = "
              << mergedB << " B, packed (ours) = " << packedB << " B\n"
              << "every module stores all borders: " << (coverage ? "yes" : "NO") << "\n"
              << "ray-casting validation        : "
              << (mismatches == 0 && g.borders.size() == static_cast<std::size_t>(expected)
                      ? "OK"
                      : "FAILED")
              << " (" << checked << " cells checked, " << mismatches << " mismatch(es)";
    if (mismatches) std::cout << ", first at (" << firstBad.first << "," << firstBad.second << ")";
    std::cout << ")\n===============================================================\n" << std::endl;
}