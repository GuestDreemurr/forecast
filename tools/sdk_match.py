#!/usr/bin/env python3
"""
Find RVL_SDK code in the DOL and propose symbols.txt / splits.txt entries.

Compiles each SDK source as a reference object, matches its functions against
the target's functions (masked on relocations, with a fuzzy fallback for
version drift), then uses the reference relocations to name the data each
matched function touches and to derive per-file split ranges.

Nothing in config/ is modified unless --apply is passed. Proposals, diffs and
a report are written to build/<version>/sdk_match/.

Usage:
    python3 tools/sdk_match.py --lib OS           # propose for one library
    python3 tools/sdk_match.py                    # propose for the whole SDK
    python3 tools/sdk_match.py --apply --min-confidence high
"""

import argparse
import bisect
import difflib
import re
import shlex
import shutil
import struct
import subprocess
import sys
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parent.parent

R_PPC_ADDR32 = 1
R_PPC_ADDR16_LO = 4
R_PPC_ADDR16_HI = 5
R_PPC_ADDR16_HA = 6
R_PPC_REL24 = 10
R_PPC_REL14 = 11
R_PPC_EMB_SDA21 = 109

# Instruction bits that a relocation of each type fills in
RELOC_MASKS = {
    R_PPC_ADDR16_LO: 0x0000FFFF,
    R_PPC_ADDR16_HI: 0x0000FFFF,
    R_PPC_ADDR16_HA: 0x0000FFFF,
    R_PPC_REL24: 0x03FFFFFC,
    R_PPC_REL14: 0x0000FFFC,
    R_PPC_EMB_SDA21: 0x001FFFFF,
}

DATA_SECTIONS = (".data", ".bss", ".sdata", ".sbss", ".sdata2", ".sbss2", ".rodata")
SPLIT_SECTIONS = (".init", ".text") + DATA_SECTIONS
FUZZY_THRESHOLD = 0.85
CONFIDENCE_ORDER = {"high": 0, "medium": 1, "low": 2}

SYMBOL_LINE = re.compile(
    r"^(?P<name>\S+) = (?P<section>[.\w]+):0x(?P<addr>[0-9A-Fa-f]+);(?P<rest>.*)$"
)
SPLIT_UNIT = re.compile(r"^(?P<path>\S.*):\s*$")
SPLIT_SECTION = re.compile(
    r"^\s+(?P<section>[.\w]+)\s+start:0x(?P<start>[0-9A-Fa-f]+)\s+end:0x(?P<end>[0-9A-Fa-f]+)"
)
AUTO_NAME = re.compile(r"^(fn|lbl|gap)_[0-9A-Fa-f_]+|^@\d+(_[0-9A-Fa-f]+)?$")


def log(msg):
    print(msg, file=sys.stderr)


# ---------------------------------------------------------------------------
# Target side


@dataclass(eq=False)
class TargetSymbol:
    name: str
    section: str
    addr: int
    size: int
    rest: str
    line: int

    @property
    def is_function(self):
        return "type:function" in self.rest

    @property
    def is_auto(self):
        return bool(AUTO_NAME.match(self.name))


class Target:
    """The linked target ELF plus the current symbols.txt / splits.txt."""

    def __init__(self, version):
        self.version = version
        self.symbols_path = ROOT / "config" / version / "symbols.txt"
        self.splits_path = ROOT / "config" / version / "splits.txt"

        self.sections: Dict[str, Tuple[int, int, bytes]] = {}
        with open(ROOT / "build" / version / "main.elf", "rb") as f:
            elf = ELFFile(f)
            for sec in elf.iter_sections():
                if sec["sh_addr"]:
                    data = b"" if sec["sh_type"] == "SHT_NOBITS" else sec.data()
                    self.sections[sec.name] = (sec["sh_addr"], sec["sh_size"], data)
            symtab = elf.get_section_by_name(".symtab")
            self.sda_base = symtab.get_symbol_by_name("_SDA_BASE_")[0]["st_value"]
            self.sda2_base = symtab.get_symbol_by_name("_SDA2_BASE_")[0]["st_value"]

        self.symbol_lines = self.symbols_path.read_text().splitlines(keepends=True)
        self.symbols: List[TargetSymbol] = []
        for i, line in enumerate(self.symbol_lines):
            m = SYMBOL_LINE.match(line.rstrip("\n"))
            if not m:
                continue
            size = re.search(r"size:0x([0-9A-Fa-f]+)", m["rest"])
            self.symbols.append(
                TargetSymbol(
                    m["name"],
                    m["section"],
                    int(m["addr"], 16),
                    int(size[1], 16) if size else 0,
                    m["rest"],
                    i,
                )
            )
        self.by_addr: Dict[Tuple[str, int], TargetSymbol] = {}
        self.section_starts: Dict[str, List[int]] = defaultdict(list)
        for sym in self.symbols:
            if "type:label" in sym.rest:
                continue
            self.by_addr.setdefault((sym.section, sym.addr), sym)
        for (section, addr) in self.by_addr:
            self.section_starts[section].append(addr)
        for starts in self.section_starts.values():
            starts.sort()

        self.functions = sorted(
            (s for s in self.by_addr.values() if s.is_function and s.size),
            key=lambda s: s.addr,
        )
        self.func_index = {s.addr: i for i, s in enumerate(self.functions)}
        self.funcs_by_size: Dict[int, List[TargetSymbol]] = defaultdict(list)
        for s in self.functions:
            self.funcs_by_size[s.size].append(s)

        self.splits = parse_splits(self.splits_path.read_text())

    def section_of(self, addr) -> Optional[str]:
        for name, (start, size, _) in self.sections.items():
            if start <= addr < start + size:
                return name
        return None

    def read(self, addr, size) -> Optional[bytes]:
        for start, sec_size, data in self.sections.values():
            if start <= addr and addr + size <= start + sec_size:
                return data[addr - start : addr - start + size] if data else None
        return None

    def word(self, addr) -> Optional[int]:
        data = self.read(addr, 4)
        return struct.unpack(">I", data)[0] if data else None

    def containing(self, section, addr) -> Optional[TargetSymbol]:
        starts = self.section_starts.get(section, [])
        i = bisect.bisect_right(starts, addr) - 1
        if i < 0:
            return None
        sym = self.by_addr[(section, starts[i])]
        if sym.addr <= addr < sym.addr + max(sym.size, 1):
            return sym
        return None

    def next_boundary(self, section, addr) -> Optional[int]:
        starts = self.section_starts.get(section, [])
        i = bisect.bisect_right(starts, addr)
        return starts[i] if i < len(starts) else None


def parse_splits(text) -> Dict[str, Dict[str, Tuple[int, int]]]:
    splits: Dict[str, Dict[str, Tuple[int, int]]] = {}
    current = None
    for line in text.splitlines():
        if m := SPLIT_UNIT.match(line):
            current = m["path"]
            if current != "Sections":
                splits[current] = {}
        elif current and current != "Sections" and (m := SPLIT_SECTION.match(line)):
            splits[current][m["section"]] = (int(m["start"], 16), int(m["end"], 16))
    return splits


# ---------------------------------------------------------------------------
# Reference side


@dataclass
class RefSymbol:
    name: str
    section: str
    value: int
    size: int
    kind: str  # "func" / "object" / "section"
    bind: str


@dataclass
class RefReloc:
    offset: int
    type: int
    sym: RefSymbol
    addend: int


@dataclass
class RefObject:
    source: str  # e.g. revolution/OS/OS.c
    sections: Dict[str, Tuple[int, int, bytes]]  # name -> (size, align, data)
    symbols: List[RefSymbol]
    relocs: Dict[str, List[RefReloc]]  # section -> relocs

    @property
    def functions(self) -> List[RefSymbol]:
        return sorted(
            (
                s
                for s in self.symbols
                if s.kind == "func"
                and s.section in (".text", ".init")
                and s.size
                # #pragma force_active dummies, never linked into the game
                and not s.name.startswith("FORCEACTIVE")
            ),
            key=lambda s: (s.section, s.value),
        )


BIND = {"STB_LOCAL": "local", "STB_GLOBAL": "global", "STB_WEAK": "weak"}


def load_ref(source, path) -> RefObject:
    with open(path, "rb") as f:
        elf = ELFFile(f)
        sections = {}
        for sec in elf.iter_sections():
            if sec.name in SPLIT_SECTIONS:
                data = b"" if sec["sh_type"] == "SHT_NOBITS" else sec.data()
                sections[sec.name] = (sec["sh_size"], sec["sh_addralign"], data)
        symtab = elf.get_section_by_name(".symtab")
        raw_syms = list(symtab.iter_symbols())
        by_index: Dict[int, RefSymbol] = {}
        symbols = []
        for i, sym in enumerate(raw_syms):
            shndx = sym["st_shndx"]
            if not isinstance(shndx, int) or shndx == 0:
                continue
            sec_name = elf.get_section(shndx).name
            stype = sym["st_info"]["type"]
            if stype == "STT_SECTION":
                rs = RefSymbol(sec_name, sec_name, 0, 0, "section", "local")
            elif stype in ("STT_FUNC", "STT_OBJECT") and sym.name:
                kind = "func" if stype == "STT_FUNC" else "object"
                rs = RefSymbol(sym.name, sec_name, sym["st_value"], sym["st_size"], kind, BIND.get(sym["st_info"]["bind"], "global"))
                symbols.append(rs)
            else:
                continue
            by_index[i] = rs
        # Undefined symbols still matter for REL24 / data pointers to other files
        for i, sym in enumerate(raw_syms):
            if i not in by_index and sym.name and sym["st_shndx"] == "SHN_UNDEF":
                by_index[i] = RefSymbol(sym.name, "", 0, 0, "extern", "global")
        relocs: Dict[str, List[RefReloc]] = defaultdict(list)
        for sec in elf.iter_sections():
            if not sec.name.startswith(".rela"):
                continue
            target = sec.name[len(".rela"):]
            if target not in sections:
                continue
            for r in sec.iter_relocations():
                rs = by_index.get(r["r_info_sym"])
                if rs is not None:
                    relocs[target].append(RefReloc(r["r_offset"], r["r_info_type"], rs, r["r_addend"]))
        for rl in relocs.values():
            rl.sort(key=lambda r: r.offset)
    return RefObject(source, sections, symbols, relocs)


def ninja_cflags(version):
    """cflags ninja uses for RVL_SDK units, taken from an existing build edge."""
    text = (ROOT / "build.ninja").read_text().replace("$\n", "")
    m = re.search(
        rf"build build/{version}/src/revolution/\S+\.o: mwcc_sjis .*?\n  mw_version = (\S+)\n  cflags = (.*?)\n",
        text,
    )
    if not m:
        sys.exit("Couldn't find an RVL_SDK mwcc_sjis edge in build.ninja; run configure.py and ninja first")
    return m[1], shlex.split(m[2].replace("$ ", " "))


def compile_refs(version, sources) -> Tuple[Dict[str, Path], Dict[str, str]]:
    mw_version, cflags = ninja_cflags(version)
    out_dir = ROOT / "build" / version / "sdk_ref"
    compiler = ROOT / "build" / "compilers" / mw_version / "mwcceppc.exe"

    def build(src: Path):
        rel = src.relative_to(ROOT / "src")
        out = out_dir / rel.with_suffix(".o")
        out.parent.mkdir(parents=True, exist_ok=True)
        if out.exists() and out.stat().st_mtime >= src.stat().st_mtime:
            return str(rel), out, None
        cmd = [str(ROOT / "build/tools/wibo"), str(ROOT / "build/tools/sjiswrap.exe"), str(compiler), *cflags, "-c", str(src.relative_to(ROOT)), "-o", str(out.relative_to(ROOT))]
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, errors="replace")
        if proc.returncode != 0 or not out.exists():
            err = (proc.stdout + proc.stderr).strip().splitlines()
            return str(rel), None, "\n".join(err[:12])
        return str(rel), out, None

    objects, failures = {}, {}
    with ThreadPoolExecutor(max_workers=8) as pool:
        for rel, out, err in pool.map(build, sources):
            if out:
                objects[rel] = out
            else:
                failures[rel] = err
    # wibo occasionally aborts under load ("User break"); retry those serially
    for rel in list(failures):
        _, out, err = build(ROOT / "src" / rel)
        if out:
            objects[rel] = out
            del failures[rel]
        else:
            failures[rel] = err
    return objects, failures


# ---------------------------------------------------------------------------
# Function matching


def reloc_mask_words(obj: RefObject, func: RefSymbol) -> List[int]:
    """Per-word masks for a reference function: 1 bits are compared."""
    count = func.size // 4
    masks = [0xFFFFFFFF] * count
    for r in obj.relocs.get(func.section, []):
        if func.value <= r.offset < func.value + func.size and r.type in RELOC_MASKS:
            masks[(r.offset - func.value) // 4] &= ~RELOC_MASKS[r.type] & 0xFFFFFFFF
    return masks


def words(data: bytes) -> List[int]:
    return list(struct.unpack(f">{len(data) // 4}I", data[: len(data) // 4 * 4]))


def normalize(word: int) -> int:
    """Instruction form used for fuzzy comparison: drop immediates and branch targets."""
    op = word >> 26
    if op in (16, 18):  # bc / b
        return word & 0xFC000003
    if op in (7, 8, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29) or 32 <= op <= 55:
        return word & 0xFFFF0000
    return word


@dataclass
class FuncMatch:
    ref: RefSymbol
    target: TargetSymbol
    kind: str  # exact / fuzzy / call
    score: float = 1.0
    # ref word index -> target word index, for harvesting relocations
    word_map: Dict[int, int] = field(default_factory=dict)


def fuzzy_score(ref_words, tgt_words) -> Tuple[float, Dict[int, int]]:
    a = [normalize(w) for w in ref_words]
    b = [normalize(w) for w in tgt_words]
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    mapping = {}
    for block in sm.get_matching_blocks():
        for k in range(block.size):
            mapping[block.a + k] = block.b + k
    return sm.ratio(), mapping


def best_chain(pairs, sizes, slack=3):
    """
    Pick at most one target function per reference function so the picks are
    in order and nearly contiguous: between consecutive picks the target index
    may advance by at most the reference index gap plus `slack`.

    pairs: (ref_idx, target_idx); sizes: ref_idx -> size. Returns the chain
    with the most picks (ties broken by matched bytes) and whether the best
    score was tied by a different chain.
    """
    pairs = sorted(set(pairs))
    n = len(pairs)
    if not n:
        return [], False
    score = [(1, sizes[r]) for r, _ in pairs]
    prev = [None] * n
    for i, (ri, ti) in enumerate(pairs):
        for j in range(i):
            rj, tj = pairs[j]
            if rj < ri and 0 < ti - tj <= (ri - rj) + slack:
                cand = (score[j][0] + 1, score[j][1] + sizes[ri])
                if cand > score[i]:
                    score[i], prev[i] = cand, j
    best = max(range(n), key=lambda i: score[i])
    ambiguous = sum(1 for i in range(n) if score[i] == score[best] and pairs[i][1] != pairs[best][1]) > 0
    chain, i = [], best
    while i is not None:
        chain.append(pairs[i])
        i = prev[i]
    return chain[::-1], ambiguous


def align_gap(target: Target, obj, ref_funcs, ref_idxs, tgt_idxs) -> List[FuncMatch]:
    """Order-preserving fuzzy alignment of reference functions onto target functions."""
    if not ref_idxs or not tgt_idxs:
        return []
    m, n = len(ref_idxs), len(tgt_idxs)
    scores = {}
    for i, ri in enumerate(ref_idxs):
        rf = ref_funcs[ri]
        rw = words(obj.sections[rf.section][2][rf.value : rf.value + rf.size])
        for j, ti in enumerate(tgt_idxs):
            tf = target.functions[ti]
            if not (0.5 <= tf.size / rf.size <= 2.0):
                continue
            data = target.read(tf.addr, tf.size)
            if not data:
                continue
            score, mapping = fuzzy_score(rw, words(data))
            if score >= FUZZY_THRESHOLD:
                scores[(i, j)] = (score, mapping)
    # DP: maximize total score, order preserving
    best = [[0.0] * (n + 1) for _ in range(m + 1)]
    for i in range(m - 1, -1, -1):
        for j in range(n - 1, -1, -1):
            opt = max(best[i + 1][j], best[i][j + 1])
            if (i, j) in scores:
                opt = max(opt, scores[(i, j)][0] + best[i + 1][j + 1])
            best[i][j] = opt
    out, i, j = [], 0, 0
    while i < m and j < n:
        if (i, j) in scores and abs(best[i][j] - (scores[(i, j)][0] + best[i + 1][j + 1])) < 1e-9:
            score, mapping = scores[(i, j)]
            out.append(FuncMatch(ref_funcs[ref_idxs[i]], target.functions[tgt_idxs[j]], "fuzzy", score, mapping))
            i, j = i + 1, j + 1
        elif best[i][j] == best[i + 1][j]:
            i += 1
        else:
            j += 1
    return out


def match_functions(target: Target, obj: RefObject) -> Tuple[List[FuncMatch], bool]:
    ref_funcs = obj.functions
    candidates: Dict[int, List[FuncMatch]] = defaultdict(list)
    for ri, rf in enumerate(ref_funcs):
        data = obj.sections[rf.section][2][rf.value : rf.value + rf.size]
        rw = words(data)
        masks = reloc_mask_words(obj, rf)
        want = [w & mk for w, mk in zip(rw, masks)]
        for tf in target.funcs_by_size.get(rf.size, []):
            tdata = target.read(tf.addr, tf.size)
            if not tdata:
                continue
            tw = words(tdata)
            if all((t & mk) == w for t, mk, w in zip(tw, masks, want)):
                candidates[ri].append(FuncMatch(rf, tf, "exact", 1.0, {k: k for k in range(len(rw))}))

    # Pick one placement per function: the best nearly-contiguous run
    pairs = []
    for ri, cands in candidates.items():
        for c in cands:
            pairs.append((ri, target.func_index[c.target.addr]))
    chain, ambiguous = best_chain(pairs, {ri: f.size for ri, f in enumerate(ref_funcs)})
    exact = {}
    for ri, ti in chain:
        for c in candidates[ri]:
            if target.func_index[c.target.addr] == ti:
                exact[ri] = c
    if not exact:
        return [], False

    # Require real evidence: one non-trivial function or two in a row
    anchors = sorted(exact)
    if not any(ref_funcs[ri].size >= 0x20 for ri in anchors) and len(anchors) < 2:
        return [], False

    matches = list(exact.values())
    # Fuzzy fill between anchors and at both ends
    tidx = {ri: target.func_index[exact[ri].target.addr] for ri in anchors}
    bounds = [(-1, None)] + [(ri, tidx[ri]) for ri in anchors] + [(len(ref_funcs), None)]
    for (ra, ta), (rb, tb) in zip(bounds, bounds[1:]):
        gap_refs = list(range(ra + 1, rb))
        if not gap_refs:
            continue
        if ta is None:
            lo, hi = max(0, tb - len(gap_refs) - 2), tb
        elif tb is None:
            lo, hi = ta + 1, min(len(target.functions), ta + 1 + len(gap_refs) + 2)
        else:
            lo, hi = ta + 1, tb
        matches += align_gap(target, obj, ref_funcs, gap_refs, list(range(lo, hi)))
    return sorted(matches, key=lambda m: m.target.addr), ambiguous


# ---------------------------------------------------------------------------
# Relocation harvesting


def sext16(v):
    return v - 0x10000 if v & 0x8000 else v


@dataclass
class Placement:
    ref: RefSymbol
    addr: int
    evidence: str  # exact / fuzzy / call / pointer / inferred


def harvest(target: Target, obj: RefObject, matches: List[FuncMatch]) -> List[Placement]:
    """Addresses of reference symbols, from relocations in matched functions."""
    out = []
    for m in matches:
        rf = m.ref
        relocs = [r for r in obj.relocs.get(rf.section, []) if rf.value <= r.offset < rf.value + rf.size]
        ha: Dict[str, List[Tuple[int, int]]] = defaultdict(list)
        pending_lo = []
        for r in relocs:
            ref_word = (r.offset - rf.value) // 4
            if ref_word not in m.word_map:
                continue
            taddr = m.target.addr + m.word_map[ref_word] * 4
            insn = target.word(taddr)
            if insn is None:
                continue
            if r.type == R_PPC_REL24 and r.sym.kind in ("func", "extern"):
                li = insn & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                out.append(Placement(r.sym, taddr + li - r.addend, "call"))
            elif r.type == R_PPC_EMB_SDA21:
                ra = (insn >> 16) & 0x1F
                base = {13: target.sda_base, 2: target.sda2_base, 0: 0}.get(ra)
                if base is not None:
                    out.append(Placement(r.sym, base + sext16(insn & 0xFFFF) - r.addend, m.kind))
            elif r.type == R_PPC_ADDR16_HA:
                ha[r.sym.name].append((insn & 0xFFFF, r.addend))
            elif r.type == R_PPC_ADDR16_LO:
                pending_lo.append((r, insn & 0xFFFF))
        for r, lo in pending_lo:
            for hi, ha_addend in ha.get(r.sym.name, []):
                addr = (hi << 16) + sext16(lo) - r.addend
                if ((addr + ha_addend + 0x8000) >> 16) & 0xFFFF == hi:
                    out.append(Placement(r.sym, addr, m.kind))
                    break
    return out


def section_bases(obj: RefObject, placements: List[Placement]):
    """Vote on where each reference section starts in the target."""
    votes: Dict[str, Counter] = defaultdict(Counter)
    for p in placements:
        if p.ref.kind in ("object", "section") and p.ref.section in DATA_SECTIONS:
            votes[p.ref.section][p.addr - p.ref.value] += 1
    return votes


def pointer_pass(target: Target, obj: RefObject, direct: Dict[str, "Placement"], votes) -> List["Placement"]:
    """
    ADDR32 relocations inside data name what they point to. A pointer is read
    through the data symbol containing it when that symbol was placed from
    code, or through the section base when every reference agreed on it.
    """
    out = []
    for section, relocs in obj.relocs.items():
        if section not in DATA_SECTIONS:
            continue
        holders = sorted((s for s in obj.symbols if s.section == section and s.kind == "object"), key=lambda s: s.value)
        unanimous = len(votes.get(section, {})) == 1
        base = next(iter(votes[section])) if unanimous else None
        for r in relocs:
            if r.type != R_PPC_ADDR32 or r.sym.kind == "section":
                continue
            holder = next((h for h in holders if h.value <= r.offset < h.value + max(h.size, 1)), None)
            if holder and holder.name in direct:
                where = direct[holder.name].addr + (r.offset - holder.value)
            elif base is not None:
                where = base + r.offset
            else:
                continue
            value = target.word(where)
            if value is not None:
                out.append(Placement(r.sym, value - r.addend, "pointer"))
    return out


# ---------------------------------------------------------------------------
# Per-object result


@dataclass
class ObjectResult:
    source: str
    matches: List[FuncMatch] = field(default_factory=list)
    missing: List[str] = field(default_factory=list)
    placements: Dict[str, Placement] = field(default_factory=dict)
    splits: Dict[str, Tuple[int, int]] = field(default_factory=dict)
    notes: List[str] = field(default_factory=list)
    confidence: str = "high"

    def flag(self, level, note):
        self.notes.append(note)
        if CONFIDENCE_ORDER[level] > CONFIDENCE_ORDER[self.confidence]:
            self.confidence = level


def analyze(target: Target, obj: RefObject) -> Optional[ObjectResult]:
    matches, ambiguous = match_functions(target, obj)
    if not matches:
        return None
    res = ObjectResult(obj.source, matches)
    if ambiguous:
        res.flag("low", "placement is ambiguous: another run of functions matches equally well")
    matched = {m.ref.name for m in matches}
    res.missing = [f.name for f in obj.functions if f.name not in matched]
    if res.missing:
        res.flag("medium", f"{len(res.missing)} function(s) not found (inlined, stripped or drifted)")
    if any(m.kind == "fuzzy" for m in matches):
        res.flag("medium", "some functions matched fuzzily")

    placements = harvest(target, obj, matches)
    votes = section_bases(obj, placements)
    bases = {}
    for section, counter in votes.items():
        (base, n), *rest = counter.most_common()
        bases[section] = base
        if rest:
            res.flag("medium", f"{section}: layout differs from reference ({n} refs agree on 0x{base:08X}, {sum(c for _, c in rest)} don't)")
    own = {id(s) for s in obj.symbols}
    rank = {"exact": 0, "fuzzy": 1, "call": 2, "pointer": 2}

    def merge(batch):
        for p in batch:
            # Only name symbols this object defines; other files name their own
            if id(p.ref) not in own or p.ref.kind == "section" or p.ref.name.startswith("..."):
                continue
            prev = res.placements.get(p.ref.name)
            if prev is None or rank[p.evidence] < rank[prev.evidence]:
                if prev and prev.addr != p.addr and rank[prev.evidence] < 2:
                    res.flag("medium", f"{p.ref.name}: {prev.evidence} evidence says 0x{prev.addr:08X}, {p.evidence} says 0x{p.addr:08X}; kept {p.evidence}")
                res.placements[p.ref.name] = p
            elif prev.addr != p.addr:
                if prev.evidence == p.evidence == "exact":
                    res.flag("low", f"{p.ref.name}: exact matches disagree (0x{prev.addr:08X} / 0x{p.addr:08X})")
                elif rank[p.evidence] < 2:
                    res.flag("medium", f"{p.ref.name}: {p.evidence} evidence says 0x{p.addr:08X}, kept {prev.evidence} 0x{prev.addr:08X}")

    merge(placements)
    merge(pointer_pass(target, obj, dict(res.placements), votes))

    # Unreferenced symbols in a data section whose layout agrees exactly
    for section, base in bases.items():
        if len(votes[section]) != 1:
            continue
        for s in obj.symbols:
            if s.section == section and s.kind == "object" and s.name not in res.placements and not s.name.startswith("..."):
                res.placements[s.name] = Placement(s, base + s.value, "inferred")

    derive_splits(target, obj, res, bases)
    return res


def snap_end(target: Target, section, candidate) -> int:
    """First symbol boundary at or after candidate (dtk splits on boundaries)."""
    sym = target.containing(section, candidate - 1) if candidate else None
    if sym and sym.addr + sym.size > candidate:
        return sym.addr + sym.size
    return candidate


def derive_splits(target: Target, obj: RefObject, res: ObjectResult, bases: Dict[str, int]):
    funcs = obj.functions
    by_ref = {m.ref.name: m for m in res.matches}
    for code_section in (".init", ".text"):
        sec_funcs = [f for f in funcs if f.section == code_section]
        if not sec_funcs:
            continue
        first, last = by_ref.get(sec_funcs[0].name), by_ref.get(sec_funcs[-1].name)
        placed = [by_ref[f.name] for f in sec_funcs if f.name in by_ref]
        if not placed:
            continue
        start = placed[0].target.addr
        end = placed[-1].target.addr + placed[-1].target.size
        if not first or not last:
            res.flag("low", f"{code_section}: first/last function not found, range ends at the outermost matched ones")
        tsec = target.section_of(start)
        if tsec != code_section:
            res.flag("low", f"{code_section}: start 0x{start:08X} is in {tsec}")
            continue
        res.splits[code_section] = (start, end)

    for section, base in bases.items():
        size = obj.sections[section][0]
        tsec = target.section_of(base)
        if tsec != section:
            res.flag("low", f"{section}: base 0x{base:08X} lands in {tsec}, skipped")
            continue
        placed_ends = [
            p.addr + max(p.ref.size, 1)
            for p in res.placements.values()
            if p.ref.section == section and p.evidence != "inferred"
        ]
        end = max([base + size] + placed_ends)
        end = snap_end(target, section, (end + 3) & ~3)
        if end - (base + size) > 0x40:
            res.flag("medium", f"{section}: target is 0x{end - base:X} bytes vs reference 0x{size:X}")
        res.splits[section] = (base, end)


# ---------------------------------------------------------------------------
# Combining results into proposals


def absorb_padding(target: Target, results: List[ObjectResult]):
    """
    Extend a split over the zero padding before the next split. Left as its own
    gap the linker fills it with junk, and dtk rejects small unsplit holes.
    """
    for sec in SPLIT_SECTIONS:
        starts = sorted(
            [rng[0] for secs in target.splits.values() for s, rng in secs.items() if s == sec]
            + [r.splits[sec][0] for r in results if sec in r.splits and r.source not in target.splits]
        )
        for res in results:
            if sec not in res.splits or res.source in target.splits:
                continue
            start, end = res.splits[sec]
            i = bisect.bisect_right(starts, start)
            if i >= len(starts):
                continue
            nxt = starts[i]
            gap = nxt - end
            if 0 < gap < 0x20:
                data = target.read(end, gap)
                if data is None or not any(data):  # NOBITS or all zero
                    res.splits[sec] = (start, nxt)


def resolve_overlaps(target: Target, results: List[ObjectResult]):
    """Drop split sections that overlap existing splits or each other."""
    existing = [(src, sec, rng) for src, secs in target.splits.items() for sec, rng in secs.items()]
    claimed = []
    for res in sorted(results, key=lambda r: CONFIDENCE_ORDER[r.confidence]):
        if res.source in target.splits:
            continue
        for sec, (start, end) in list(res.splits.items()):
            for other_src, other_sec, (os_, oe) in existing + claimed:
                if other_sec == sec and start < oe and os_ < end:
                    res.flag("low", f"{sec} 0x{start:08X}-0x{end:08X} overlaps {other_src}, dropped")
                    del res.splits[sec]
                    break
            else:
                claimed.append((res.source, sec, (start, end)))


def claim_symbols(target: Target, results: List[ObjectResult], include_anon: bool, override_names: bool):
    """
    Turn placements into symbols.txt edits.

    Returns (renames, creates, conflicts):
      renames: TargetSymbol -> (new_name, scope, source)
      creates: (section, addr) -> (name, size, scope, source)
    """
    owners: Dict[Tuple[str, int], List[Tuple[ObjectResult, Placement]]] = defaultdict(list)

    def in_split(res, section, addr):
        # A file only names what lies inside its own (surviving) split ranges
        rng = res.splits.get(section)
        return rng is not None and rng[0] <= addr < rng[1]

    for res in results:
        for m in res.matches:
            if in_split(res, m.target.section, m.target.addr):
                owners[(m.target.section, m.target.addr)].append((res, Placement(m.ref, m.target.addr, m.kind)))
        for p in res.placements.values():
            if p.ref.kind == "func":
                continue
            section = target.section_of(p.addr)
            if section and section == p.ref.section and in_split(res, section, p.addr):
                owners[(section, p.addr)].append((res, p))

    rank = {"exact": 0, "fuzzy": 1, "call": 2, "pointer": 2, "inferred": 3}
    renames, creates, conflicts = {}, {}, []
    for (section, addr), claims in owners.items():
        names = {p.ref.name for _, p in claims}
        claims.sort(key=lambda c: (rank[c[1].evidence], CONFIDENCE_ORDER[c[0].confidence]))
        res, p = claims[0]
        if len(names) > 1:
            conflicts.append(f"{section}:0x{addr:08X} claimed as {', '.join(sorted(names))}; kept {p.ref.name} ({res.source})")
        name = p.ref.name
        if name.startswith("@") and not include_anon:
            name = None
        existing = target.by_addr.get((section, addr))
        if existing:
            if name and existing.name != name:
                if not existing.is_auto and not (override_names and p.evidence == "exact"):
                    conflicts.append(
                        f"{section}:0x{addr:08X} already named `{existing.name}`, reference says `{name}` "
                        f"({p.evidence} match, {res.source})"
                    )
                    continue
                renames[existing] = (name, p.ref.bind, res.source)
            continue
        # Address inside an existing (merged) symbol, or in a gap: create
        size = p.ref.size or 4
        data = target.read(addr, size)
        if data and b"\0" in data and p.ref.kind == "object":
            size = min(size, max(data.index(b"\0") + 1, 1)) if data.index(b"\0") + 1 >= size else size
        nb = target.next_boundary(section, addr)
        if nb is not None:
            size = min(size, nb - addr)
        creates[(section, addr)] = (name or f"lbl_{addr:08X}", size, p.ref.bind, res.source)
    # Never introduce a duplicate name: dtk/the linker need unique non-local names
    final: Dict[str, List[Tuple[str, int, bool]]] = defaultdict(list)
    for sym in target.symbols:
        if sym not in renames and "type:label" not in sym.rest:
            final[sym.name].append((sym.section, sym.addr, "scope:local" in sym.rest))
    proposed = [(sym.section, sym.addr, name, scope, src, ("rename", sym)) for sym, (name, scope, src) in renames.items()]
    proposed += [(sec, addr, name, scope, src, ("create", (sec, addr))) for (sec, addr), (name, _, scope, src) in creates.items()]
    for sec, addr, name, scope, src, (kind, key) in sorted(proposed, key=lambda x: x[1]):
        others = [o for o in final.get(name, []) if (o[0], o[1]) != (sec, addr)]
        if name.startswith("lbl_") or not others or (scope == "local" and all(local for *_, local in others)):
            final[name].append((sec, addr, scope == "local"))
            continue
        where = ", ".join(f"{o[0]}:0x{o[1]:08X}" for o in others)
        conflicts.append(f"{sec}:0x{addr:08X} would be named `{name}`, which already exists at {where} ({src})")
        if kind == "rename":
            del renames[key]
        else:
            del creates[key]
    return renames, creates, conflicts


def build_symbols_text(target: Target, renames, creates) -> str:
    lines = list(target.symbol_lines)
    for sym, (name, scope, _) in renames.items():
        rest = sym.rest
        if "scope:" in rest:
            rest = re.sub(r"scope:\w+", f"scope:{scope}", rest)
        else:
            rest = re.sub(r"(size:0x[0-9A-Fa-f]+)", rf"\1 scope:{scope}", rest, count=1)
        lines[sym.line] = f"{name} = {sym.section}:0x{sym.addr:08X};{rest}\n"

    # Shrink symbols that created symbols split, fill what's left of them, and
    # insert the new lines in address order
    inserts: Dict[int, List[str]] = defaultdict(list)
    ordered = sorted(creates.items())
    groups: Dict[Optional[int], List] = defaultdict(list)  # outer symbol line -> creates inside it
    for (section, addr), info in ordered:
        outer = target.containing(section, addr)
        groups[outer.line if outer and outer.addr < addr else None].append(((section, addr), info))

    def emit(anchor, section, addr, size, name, scope):
        suffix = f" scope:{scope}" if scope else ""
        inserts[anchor].append(f"{name} = {section}:0x{addr:08X}; // type:object size:0x{size:X}{suffix}\n")

    for outer_line, items in groups.items():
        if outer_line is not None:
            m = SYMBOL_LINE.match(lines[outer_line].rstrip("\n"))
            section = m["section"]
            outer_addr = int(m["addr"], 16)
            outer_end = outer_addr + int(re.search(r"size:0x([0-9A-Fa-f]+)", m["rest"])[1], 16)
            first = items[0][0][1]
            rest = re.sub(r"size:0x[0-9A-Fa-f]+", f"size:0x{first - outer_addr:X}", m["rest"])
            lines[outer_line] = f"{m['name']} = {section}:0x{m['addr']};{rest}\n"
            for k, ((section, addr), (name, size, scope, _)) in enumerate(items):
                limit = items[k + 1][0][1] if k + 1 < len(items) else outer_end
                size = min(size, limit - addr)
                emit(outer_line, section, addr, size, name, scope)
                if addr + size < limit:
                    emit(outer_line, section, addr + size, limit - addr - size, f"lbl_{addr + size:08X}", None)
        else:
            for k, ((section, addr), (name, size, scope, _)) in enumerate(items):
                nxt = next((a for (sec, a), _ in ordered if sec == section and a > addr), None)
                if nxt is not None:
                    size = min(size, nxt - addr)
                prev = target.containing(section, addr - 1)
                anchor = prev.line if prev else next((s.line for s in target.symbols if s.section == section and s.addr > addr), len(lines)) - 1
                emit(anchor, section, addr, size, name, scope)
    out = []
    for i, line in enumerate(lines):
        out.append(line)
        # created lines sort by address after the anchor
        extra = sorted(inserts.get(i, []), key=lambda l: int(l.split(":0x")[1].split(";")[0], 16))
        out += extra
    return "".join(out)


def validate_splits(target: Target, results: List[ObjectResult], symbols_text: str):
    """Split boundaries dtk would reject: ones that fall inside a symbol."""
    spans: Dict[str, List[Tuple[int, int]]] = defaultdict(list)
    for line in symbols_text.splitlines():
        m = SYMBOL_LINE.match(line)
        if not m or "type:label" in m["rest"]:
            continue
        size = re.search(r"size:0x([0-9A-Fa-f]+)", m["rest"])
        if size and int(size[1], 16):
            addr = int(m["addr"], 16)
            spans[m["section"]].append((addr, addr + int(size[1], 16)))
    for lst in spans.values():
        lst.sort()
    starts = {sec: [a for a, _ in lst] for sec, lst in spans.items()}

    def inside(sec, addr):
        lst = spans.get(sec, [])
        i = bisect.bisect_left(starts[sec], addr) - 1 if lst else -1
        # check a few predecessors, symbols can nest (labels excluded above)
        for a, e in lst[max(0, i - 3) : i + 1]:
            if a < addr < e:
                return (a, e)
        return None

    bad = []
    for res in results:
        if res.source in target.splits:
            continue
        for sec, (start, end) in res.splits.items():
            for edge in (start, end):
                hit = inside(sec, edge)
                if hit:
                    bad.append((res, sec, f"{sec} split edge 0x{edge:08X} falls inside symbol 0x{hit[0]:08X}-0x{hit[1]:08X}, dropped"))
                    break
    return bad


def build_splits_text(target: Target, results: List[ObjectResult]) -> str:
    text = target.splits_path.read_text()
    header, _, _ = text.partition("\n\n")
    units = {src: dict(secs) for src, secs in target.splits.items()}
    for res in results:
        if res.source not in units and res.splits:
            units[res.source] = res.splits

    def key(item):
        secs = item[1]
        return min((secs[s][0] for s in (".init", ".text") if s in secs), default=min(s[0] for s in secs.values()))

    blocks = [header.rstrip("\n")]
    for src, secs in sorted(units.items(), key=key):
        lines = [f"{src}:"]
        for sec in SPLIT_SECTIONS:
            if sec in secs:
                s, e = secs[sec]
                lines.append(f"\t{sec:<11} start:0x{s:08X} end:0x{e:08X}")
        blocks.append("\n".join(lines))
    return "\n\n".join(blocks) + "\n"


def write_report(path: Path, results, failures, not_found, conflicts, renames, creates, target, min_confidence, included):
    by_source_renames = defaultdict(list)
    for sym, (name, _, src) in renames.items():
        by_source_renames[src].append((sym, name))
    by_source_creates = defaultdict(list)
    for (sec, addr), (name, size, _, src) in creates.items():
        by_source_creates[src].append((sec, addr, name, size))

    counts = Counter(r.confidence for r in results)
    out = ["# RVL_SDK match report", ""]
    out.append(
        f"{len(results)} files found in the DOL ({counts['high']} high, {counts['medium']} medium, "
        f"{counts['low']} low confidence), {len(not_found)} not found, {len(failures)} failed to compile."
    )
    out.append(
        f"The staged diffs include the {len(included)} files at `{min_confidence}` confidence or better "
        f"(change with `--min-confidence`): {len(renames)} symbols renamed, {len(creates)} created, "
        f"{len(conflicts)} conflicts."
    )
    out.append("")
    if conflicts:
        out += ["## Conflicts (not applied)", ""] + [f"- {c}" for c in sorted(conflicts)] + [""]
    out += ["## Files", ""]
    for res in sorted(results, key=lambda r: min((a for a, _ in r.splits.values()), default=0)):
        exact = sum(m.kind == "exact" for m in res.matches)
        fuzzy = [m for m in res.matches if m.kind == "fuzzy"]
        excluded = "" if id(res) in included else " (not in staged diffs)"
        out.append(f"### {res.source} — {res.confidence}{excluded}")
        out.append("")
        out.append(f"- functions: {exact} exact, {len(fuzzy)} fuzzy, {len(res.missing)} missing")
        for m in fuzzy:
            out.append(f"  - fuzzy `{m.ref.name}` → 0x{m.target.addr:08X} ({m.score:.0%})")
        if res.missing:
            out.append(f"  - missing: {', '.join(f'`{n}`' for n in res.missing)}")
        for sec in SPLIT_SECTIONS:
            if sec in res.splits:
                s, e = res.splits[sec]
                out.append(f"- `{sec}` 0x{s:08X}–0x{e:08X}")
        existing = target.splits.get(res.source)
        if existing:
            for sec, rng in res.splits.items():
                if existing.get(sec) != rng:
                    cur = existing.get(sec)
                    cur_s = f"0x{cur[0]:08X}–0x{cur[1]:08X}" if cur else "none"
                    out.append(f"  - ⚠ existing split `{sec}` is {cur_s} (kept existing)")
        renamed = sorted(by_source_renames.get(res.source, []), key=lambda x: x[0].addr)
        if renamed:
            out.append(f"- renamed: " + ", ".join(f"`{s.name}`→`{n}`" for s, n in renamed))
        created = sorted(by_source_creates.get(res.source, []), key=lambda x: x[1])
        if created:
            out.append(f"- created: " + ", ".join(f"`{n}` {sec}:0x{a:08X}" for sec, a, n, _ in created))
        for note in res.notes:
            out.append(f"- note: {note}")
        out.append("")
    if not_found:
        out += ["## Not found in the DOL", "", ", ".join(f"`{s}`" for s in sorted(not_found)), ""]
    if failures:
        out += ["## Failed to compile", ""]
        for src, err in sorted(failures.items()):
            lines = [l.strip("# ").strip() for l in (err or "").splitlines()]
            reason = next((l for l in lines if l and not l.startswith(("Error", "User break", "Too many", "mwcceppc", "File:", "---", "^")) and not l[:1].isdigit()), lines[-1] if lines else "")
            out.append(f"- `{src}`: {reason[:160]}")
        out.append("")
    path.write_text("\n".join(out))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--version", default="HAFE")
    parser.add_argument("--lib", action="append", help="only these libraries (e.g. OS, DVD); repeatable")
    parser.add_argument("--include-anon", action="store_true", help="also propose compiler @NNN names")
    parser.add_argument(
        "--override-names",
        action="store_true",
        help="let exact matches replace existing hand-made names (listed as conflicts otherwise)",
    )
    parser.add_argument("--apply", action="store_true", help="copy the staged proposals into config/")
    parser.add_argument(
        "--min-confidence",
        choices=list(CONFIDENCE_ORDER),
        default="high",
        help="lowest confidence level to stage/apply (default: high)",
    )
    args = parser.parse_args()

    target = Target(args.version)
    sdk_root = ROOT / "src" / "revolution"
    sources = sorted(sdk_root.rglob("*.c"))
    if args.lib:
        sources = [s for s in sources if s.relative_to(sdk_root).parts[0] in args.lib]
    log(f"Compiling {len(sources)} reference files...")
    objects, failures = compile_refs(args.version, sources)
    log(f"  {len(objects)} compiled, {len(failures)} failed")

    results, not_found = [], []
    for source, path in sorted(objects.items()):
        obj = load_ref(source, path)
        if not obj.functions:
            not_found.append(source)
            continue
        res = analyze(target, obj)
        if res:
            results.append(res)
        else:
            not_found.append(source)
    log(f"  {len(results)} found in the DOL")

    resolve_overlaps(target, results)
    all_results = list(results)
    allowed = CONFIDENCE_ORDER[args.min_confidence]
    while True:
        # Filter after overlap resolution / validation, which can downgrade a file
        results = [r for r in results if CONFIDENCE_ORDER[r.confidence] <= allowed]
        renames, creates, conflicts = claim_symbols(target, results, args.include_anon, args.override_names)
        symbols_text = build_symbols_text(target, renames, creates)
        bad = validate_splits(target, results, symbols_text)
        if not bad:
            break
        for res, sec, msg in bad:
            res.splits.pop(sec, None)
            res.flag("low", msg)
    absorb_padding(target, results)
    splits_text = build_splits_text(target, results)

    out_dir = ROOT / "build" / args.version / "sdk_match"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "symbols.proposed.txt").write_text(symbols_text)
    (out_dir / "splits.proposed.txt").write_text(splits_text)
    for name, old, new in (
        ("symbols", target.symbols_path, symbols_text),
        ("splits", target.splits_path, splits_text),
    ):
        diff = difflib.unified_diff(
            old.read_text().splitlines(keepends=True),
            new.splitlines(keepends=True),
            fromfile=str(old.relative_to(ROOT)),
            tofile=str(old.relative_to(ROOT)),
        )
        (out_dir / f"{name}.diff").write_text("".join(diff))
    write_report(out_dir / "report.md", all_results, failures, not_found, conflicts, renames, creates, target, args.min_confidence, {id(r) for r in results})
    log(f"Wrote proposals to {out_dir.relative_to(ROOT)}/ (report.md, symbols.diff, splits.diff)")

    if args.apply:
        shutil.copyfile(out_dir / "symbols.proposed.txt", target.symbols_path)
        shutil.copyfile(out_dir / "splits.proposed.txt", target.splits_path)
        log(f"Applied {args.min_confidence}+ confidence proposals to config/{args.version}/")


if __name__ == "__main__":
    main()
