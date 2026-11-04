"""Lays out Lint's signal path for every order and loop: which port of each scope
each line leaves and enters by, and whether it runs straight along the middle,
over the scopes or under them, so that no two lines cross.

Ports are a rim's four compass points: L (180 deg), R (0), T (270, the top) and
B (90, the bottom). Every distortion of a rim vanishes there, so a line meets a
rim at the same point whatever the rim is doing.

Elements:
  straight  between neighbours, R of the left one to L of the right one
  arc       over (side +1) or under (side -1) the scopes between, from a T/B port
            or leaving an L/R port horizontally and curving away
  stub      the way in at the first scope, the way out at the last
  tear      the tape loop: hangs from the echo and comes back to the same port
  lens      the echo's loop through a patched section: out and back between them
"""
import itertools

ORDERS = ["DPES", "DEPS", "PDES", "DSPE", "SDEP", "EDPS"]
COL = {"D": 0, "P": 1, "E": 2, "S": 3}
PX = {"L": -0.4, "R": 0.4, "T": 0.0, "B": 0.0}


class El:
    def __init__(self, kind, side, ends, gaps=(), cost=0.0, dirn=0):
        self.kind, self.side, self.ends, self.gaps, self.cost, self.dirn = kind, side, ends, gaps, cost, dirn
        xs = [c + PX[p] for c, p in ends]
        self.lo, self.hi = min(xs), max(xs)
        self.level = 0

    def ports(self):
        return list(self.ends)

    def __repr__(self):
        e = " ".join(f"{'DPES'[c]}.{p}" for c, p in self.ends)
        return f"{self.kind}{'+' if self.side > 0 else '-' if self.side < 0 else '='}{self.level} {e}"


def connection_options(u, v):
    out = []
    if abs(u - v) == 1:
        a, b = min(u, v), max(u, v)
        out.append(El("straight", 0, [(u, "R" if u == a else "L"), (v, "L" if v == b else "R")], gaps=[(a, 0)], cost=0, dirn=v - u))
    fwd = v > u
    for side in (1, -1):
        vert = "T" if side > 0 else "B"
        pu = [vert, "R" if fwd else "L"]
        pv = [vert, "L" if fwd else "R"]
        for a, b in itertools.product(pu, pv):
            if abs(u - v) == 1 and a != vert and b != vert:
                continue      # neighbours joined by a bent line: never better than straight
            gaps = []
            c = 2.0 + 0.4 * abs(u - v)
            for col, p in ((u, a), (v, b)):
                if p in "LR":
                    gaps.append((col - 1 if p == "L" else col, side))
                    c += 1.2
            # forward over, back under, reads most naturally
            c += 0.6 if (fwd and side < 0) or (not fwd and side > 0) else 0
            out.append(El("arc", side, [(u, a), (v, b)], gaps=gaps, cost=c, dirn=v - u))
    return out


def stub_options(col, way):
    out = []
    # (port, side, gaps, cost)
    pref = "L" if way == "in" else "R"
    for p in "LRTB":
        if p in "LR":
            g = col - 1 if p == "L" else col
            c = 0 if p == pref else 6
            out.append(El("stub_" + way, 0, [(col, p)], gaps=[(g, 0)], cost=c))
        else:
            out.append(El("stub_" + way, 1 if p == "T" else -1, [(col, p)], cost=1.6))
    return out


def loop_options(loop):
    if loop == 0:
        return [None]
    if loop == 1:
        return [El("tear", -1, [(2, "B")], cost=0), El("tear", 1, [(2, "T")], cost=1.5)]
    x = 1 if loop == 2 else 3
    out = []
    if x == 1:
        out.append(El("lens", 0, [(2, "L"), (1, "R")], gaps=[(1, 0)], cost=0.2))
    else:
        out.append(El("lens", 0, [(2, "R"), (3, "L")], gaps=[(2, 0)], cost=0.2))
    out.append(El("lens", -1, [(2, "B"), (x, "B")], cost=0.6))
    out.append(El("lens", 1, [(2, "T"), (x, "T")], cost=1.8))
    return out


def interleave(a, b):
    return (a.lo < b.lo < a.hi < b.hi) or (b.lo < a.lo < b.hi < a.hi)


def ok(els):
    used = set()
    gaps = set()
    for e in els:
        for cp in e.ports():
            if cp in used:
                return False
            used.add(cp)
    for e in els:
        for g in e.gaps:
            # a line element in a gap takes the whole gap; arcs leaving into a gap take its side
            if g in gaps:
                return False
            gaps.add(g)
    for e in els:
        for g in e.gaps:
            if g[1] == 0 and ((g[0], 1) in gaps or (g[0], -1) in gaps):
                return False
    for side in (1, -1):
        s = [e for e in els if e.side == side]
        for a, b in itertools.combinations(s, 2):
            if interleave(a, b):
                return False
            # two elements meeting at one scope from the same side: only through different ports (checked)
    return True


def levels(els):
    for side in (1, -1):
        s = sorted([e for e in els if e.side == side], key=lambda e: e.hi - e.lo)
        for e in s:
            inner = [f for f in s if f is not e and e.lo <= f.lo and f.hi <= e.hi and (f.hi - f.lo) < (e.hi - e.lo)]
            e.level = 1 + max([f.level for f in inner], default=0)
    return max([e.level for e in els], default=0)


def solve(order, loop):
    chain = [COL[c] for c in order]
    if loop == 2:
        chain = [c for c in chain if c != 1]
    if loop == 3:
        chain = [c for c in chain if c != 3]
    conns = [connection_options(a, b) for a, b in zip(chain, chain[1:])]
    best = None
    for combo in itertools.product(*conns, stub_options(chain[0], "in"), stub_options(chain[-1], "out"), loop_options(loop)):
        els = [e for e in combo if e is not None]
        if not ok(els):
            continue
        # fresh copies for levels
        lv = levels(els)
        cost = sum(e.cost for e in els) + {0: 0, 1: 0, 2: 1.5, 3: 6}.get(lv, 20) + 0.8 * sum(e.level - 1 for e in els if e.side)
        if best is None or cost < best[0]:
            best = (cost, [El(e.kind, e.side, e.ends, e.gaps, e.cost, e.dirn) for e in els])
    cost, els = best
    levels(els)
    return chain, els, cost


if __name__ == "__main__":
    for loop in range(4):
        for o in ORDERS:
            chain, els, cost = solve(o, loop)
            print(f"{o} loop{loop} {''.join('DPES'[c] for c in chain):5s} cost {cost:4.1f}: " + ", ".join(map(repr, els)))
