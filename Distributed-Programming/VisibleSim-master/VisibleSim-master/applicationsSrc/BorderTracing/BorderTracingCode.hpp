/**
 * @file BorderTracingCode.hpp
 * @author Muhammad Shiraz
 * @date October 18, 2026
 * @brief VisibleSim / Blinky Blocks block code for distributed border tracing.
 *        Includes the core bit-packed vertical edge geometry logic and simulator adapters.
 */
#ifndef BORDERTRACINGCODE_HPP_
#define BORDERTRACINGCODE_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "blinkyBlocksBlockCode.h"
#include "blinkyBlocksSimulator.h"
#include "network.h"

// =====================================================================
// PART 1 - Platform-independent core of the algorithm
// =====================================================================
namespace bt {

struct Pos {
    int x = 0, y = 0;
    bool operator==(const Pos &o) const { return x == o.x && y == o.y; }
    bool operator!=(const Pos &o) const { return !(*this == o); }
};

inline bool posLess(const Pos &a, const Pos &b) {
    return a.x < b.x || (a.x == b.x && a.y < b.y);
}

enum Dir : int { EAST = 0, NORTH = 1, WEST = 2, SOUTH = 3, NO_DIR = -1 };

inline int dirDx(int d) { static const int t[4] = {1, 0, -1, 0}; return t[d & 3]; }
inline int dirDy(int d) { static const int t[4] = {0, 1, 0, -1}; return t[d & 3]; }
inline int rightOf(int d) { return (d + 3) & 3; }
inline int leftOf(int d) { return (d + 1) & 3; }
inline int opposite(int d) { return (d + 2) & 3; }

inline int signOf(int v) { return (v > 0) - (v < 0); }

inline int dirBetween(const Pos &a, const Pos &b) {
    const int sx = signOf(b.x - a.x), sy = signOf(b.y - a.y);
    if (sx == 1 && sy == 0) return EAST;
    if (sx == 0 && sy == 1) return NORTH;
    if (sx == -1 && sy == 0) return WEST;
    if (sx == 0 && sy == -1) return SOUTH;
    return NO_DIR;
}

struct SearchMsg {
    Pos init;
    int dir = NO_DIR;
    std::vector<Pos> corners;
};

class BorderNode {
public:
    enum class Action { DROP, FORWARD, COMPLETE };

    struct Outcome {
        Action action = Action::DROP;
        int dir = NO_DIR;
        SearchMsg msg;
    };

    BorderNode() = default;

    BorderNode(Pos self, const std::array<bool, 4> &neighbor, bool diagBelowRight)
        : self_(self), nb_(neighbor), diagBR_(diagBelowRight) {}

    bool isInitiator() const { return !nb_[SOUTH] && (!nb_[WEST] || diagBR_); }

    int getNextDir(int prevDir) const {
        int k = (prevDir + 3) & 3;
        for (int i = 0; i < 4; ++i) {
            if (nb_[k]) return k;
            k = (k + 1) & 3;
        }
        return NO_DIR;
    }

    Outcome start() {
        Outcome o;
        startDir_ = getNextDir(EAST);
        for (int a = 0; a < 4; ++a) {
            if (nb_[opposite(a)] && getNextDir(a) == startDir_) {
                seen_[a] = true;
                seenInit_[a] = self_;
            }
        }
        o.msg.init = self_;
        o.msg.corners.push_back(self_);
        if (startDir_ == NO_DIR) {
            o.action = Action::COMPLETE;
            return o;
        }
        o.action = Action::FORWARD;
        o.dir = startDir_;
        o.msg.dir = startDir_;
        return o;
    }

    Outcome onSearch(SearchMsg m) {
        Outcome o;
        const int prev = m.dir;
        if (seen_[prev] && posLess(seenInit_[prev], m.init)) return o;

        const int next = getNextDir(prev);
        seen_[prev] = true;
        seenInit_[prev] = m.init;

        if (m.init == self_ && next == startDir_) {
            o.action = Action::COMPLETE;
            o.msg = std::move(m);
            return o;
        }
        if (next != prev) m.corners.push_back(self_);
        m.dir = next;
        o.action = Action::FORWARD;
        o.dir = next;
        o.msg = std::move(m);
        return o;
    }

    bool visitedBy(const Pos &id) const {
        for (int d = 0; d < 4; ++d)
            if (seen_[d] && seenInit_[d] == id) return true;
        return false;
    }

    const Pos &position() const { return self_; }

private:
    Pos self_;
    std::array<bool, 4> nb_{};
    bool diagBR_ = false;
    int startDir_ = NO_DIR;
    std::array<bool, 4> seen_{};
    std::array<Pos, 4> seenInit_{};
};

struct Vtx { int X, Y; };
struct Edge { int k, ylo, yhi; };

inline std::vector<Vtx> buildOutline(const std::vector<Pos> &c) {
    std::vector<Vtx> out;
    const std::size_t m = c.size();
    if (m == 0) return out;
    if (m == 1) {
        const int X = 2 * c[0].x, Y = 2 * c[0].y;
        return {{X - 1, Y - 1}, {X + 1, Y - 1}, {X + 1, Y + 1}, {X - 1, Y + 1}};
    }
    for (std::size_t i = 0; i < m; ++i) {
        const Pos &p = c[(i + m - 1) % m], &q = c[i], &n = c[(i + 1) % m];
        const int din = dirBetween(p, q), dout = dirBetween(q, n);
        if (din == NO_DIR || dout == NO_DIR || din == dout) continue;
        const int r = rightOf(din);
        const int X = 2 * q.x, Y = 2 * q.y;
        if (dout == opposite(din)) {
            out.push_back({X + dirDx(din) + dirDx(r), Y + dirDy(din) + dirDy(r)});
            out.push_back({X + dirDx(din) - dirDx(r), Y + dirDy(din) - dirDy(r)});
        } else if (dout == r) {
            out.push_back({X - dirDx(din) + dirDx(r), Y - dirDy(din) + dirDy(r)});
        } else {
            out.push_back({X + dirDx(din) + dirDx(r), Y + dirDy(din) + dirDy(r)});
        }
    }
    return out;
}

inline long long signedArea2(const std::vector<Vtx> &v) {
    long long s = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const Vtx &a = v[i], &b = v[(i + 1) % v.size()];
        s += (long long)a.X * b.Y - (long long)b.X * a.Y;
    }
    return s;
}

inline std::vector<Edge> verticalEdges(const std::vector<Vtx> &v) {
    std::vector<Edge> e;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const Vtx &a = v[i], &b = v[(i + 1) % v.size()];
        if (a.X == b.X && a.Y != b.Y) {
            const int lo = std::min(a.Y, b.Y), hi = std::max(a.Y, b.Y);
            e.push_back({(a.X + 1) / 2, (lo + 1) / 2, (hi - 1) / 2});
        }
    }
    return e;
}

inline std::size_t countMergedCorners(const std::vector<Pos> &c) {
    const std::size_t m = c.size();
    if (m <= 1) return m;
    std::size_t n = 0;
    for (std::size_t i = 0; i < m; ++i) {
        const int din = dirBetween(c[(i + m - 1) % m], c[i]);
        const int dout = dirBetween(c[i], c[(i + 1) % m]);
        if (din != dout) ++n;
    }
    return n;
}

namespace detail {
inline int bitsFor(unsigned v) { int b = 0; while (v) { ++b; v >>= 1; } return b; }

struct BitWriter {
    std::vector<uint8_t> &buf;
    int used = 8;
    explicit BitWriter(std::vector<uint8_t> &b) : buf(b) {}
    void put(unsigned value, int nbits) {
        for (int i = nbits - 1; i >= 0; --i) {
            if (used == 8) { buf.push_back(0); used = 0; }
            if ((value >> i) & 1u) buf.back() |= (uint8_t)(0x80u >> used);
            ++used;
        }
    }
};

struct BitReader {
    const std::vector<uint8_t> &buf;
    std::size_t pos;
    int used = 0;
    BitReader(const std::vector<uint8_t> &b, std::size_t start) : buf(b), pos(start) {}
    unsigned get(int nbits) {
        unsigned v = 0;
        for (int i = 0; i < nbits; ++i) {
            if (used == 8) { ++pos; used = 0; }
            v = (v << 1) | ((buf[pos] >> (7 - used)) & 1u);
            ++used;
        }
        return v;
    }
};
}  // namespace detail

inline std::vector<uint8_t> packEdges(const std::vector<Edge> &e) {
    std::vector<uint8_t> out;
    if (e.empty()) return out;
    int kMin = e[0].k, kMax = e[0].k, rMin = e[0].ylo, rMax = e[0].yhi;
    for (const Edge &x : e) {
        kMin = std::min(kMin, x.k);       kMax = std::max(kMax, x.k);
        rMin = std::min(rMin, x.ylo);     rMax = std::max(rMax, x.yhi);
    }
    const int bk = detail::bitsFor((unsigned)(kMax - kMin));
    const int br = detail::bitsFor((unsigned)(rMax - rMin));
    out.push_back((uint8_t)kMin);
    out.push_back((uint8_t)rMin);
    out.push_back((uint8_t)((bk << 4) | br));
    std::size_t n = e.size();
    do {
        uint8_t b = (uint8_t)(n & 0x7F);
        n >>= 7;
        if (n) b |= 0x80;
        out.push_back(b);
    } while (n);
    detail::BitWriter w(out);
    for (const Edge &x : e) {
        w.put((unsigned)(x.k - kMin), bk);
        w.put((unsigned)(x.ylo - rMin), br);
        w.put((unsigned)(x.yhi - x.ylo), br);
    }
    return out;
}

template <class F>
inline void forEachEdge(const std::vector<uint8_t> &p, F f) {
    if (p.size() < 4) return;
    const int kMin = p[0], rMin = p[1], bk = p[2] >> 4, br = p[2] & 0x0F;
    std::size_t i = 3, n = 0;
    int shift = 0;
    for (;; ++i) {
        n |= (std::size_t)(p[i] & 0x7F) << shift;
        shift += 7;
        if (!(p[i] & 0x80)) { ++i; break; }
    }
    detail::BitReader r(p, i);
    for (std::size_t j = 0; j < n; ++j) {
        const int k = kMin + (int)r.get(bk);
        const int lo = rMin + (int)r.get(br);
        const int hi = lo + (int)r.get(br);
        f(Edge{k, lo, hi});
    }
}

inline int rayCrossings(const std::vector<uint8_t> &packed, int x, int y) {
    int c = 0;
    forEachEdge(packed, [&](const Edge &e) {
        if (e.k > x && e.ylo <= y && y <= e.yhi) ++c;
    });
    return c;
}

inline bool insideConfiguration(const std::vector<std::vector<uint8_t>> &borders, int x, int y) {
    int total = 0;
    for (const auto &b : borders) total += rayCrossings(b, x, y);
    return (total & 1) != 0;
}

struct BorderInfo {
    Pos id;
    bool outer = true;
    std::size_t rawCorners = 0;
    std::size_t mergedCorners = 0;
    std::size_t outlineVertices = 0;
    std::size_t edges = 0;
    std::vector<uint8_t> packed;

    std::size_t rawBytes() const { return 2 * rawCorners; }
    std::size_t mergedBytes() const { return 2 * mergedCorners; }
    std::size_t packedBytes() const { return packed.size(); }
};

inline BorderInfo finalizeBorder(const Pos &id, const std::vector<Pos> &corners) {
    BorderInfo b;
    b.id = id;
    b.rawCorners = corners.size();
    b.mergedCorners = countMergedCorners(corners);
    const std::vector<Vtx> outline = buildOutline(corners);
    b.outlineVertices = outline.size();
    b.outer = signedArea2(outline) > 0;
    const std::vector<Edge> e = verticalEdges(outline);
    b.edges = e.size();
    b.packed = packEdges(e);
    return b;
}

}  // namespace bt


// =====================================================================
// PART 2 - VisibleSim block code
// =====================================================================
static const int BORDER_SEARCH_MSG_ID = 9001;
static const int BORDER_DONE_MSG_ID = 9002;

struct BorderDonePayload {
    bt::Pos id;
    bool outer = true;
    std::vector<uint8_t> packed;
};

struct StoredBorder {
    bt::Pos id;
    bool outer = true;
    std::vector<uint8_t> packed;
};

class BorderTracingCode : public BlinkyBlocks::BlinkyBlocksBlockCode {
public:
    explicit BorderTracingCode(BlinkyBlocks::BlinkyBlocksBlock *host);
    ~BorderTracingCode() override = default;

    void startup() override;
    void onSearchMsg(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);
    void onDoneMsg(std::shared_ptr<Message> msg, P2PNetworkInterface *sender);

    static BlockCode *buildNewBlockCode(BuildingBlock *host) {
        return new BorderTracingCode(static_cast<BlinkyBlocks::BlinkyBlocksBlock *>(host));
    }

    static void printGlobalReport();

private:
    void forwardSearch(int dir, const bt::SearchMsg &msg);
    void finishBorder(const bt::SearchMsg &msg);
    void acceptBorder(const bt::Pos &id, bool outer, const std::vector<uint8_t> &packed,
                      P2PNetworkInterface *from, bool isWinner);
    bool knowsBorder(const bt::Pos &id) const;

    BlinkyBlocks::BlinkyBlocksBlock *module = nullptr;
    bt::Pos self;
    bt::BorderNode node;
    std::array<P2PNetworkInterface *, 4> nbIf{};
    std::vector<StoredBorder> known;
    bool winner = false;
};

#endif  // BORDERTRACINGCODE_HPP_