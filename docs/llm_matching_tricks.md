# Matching tricks for the MWCC register allocator (for LLM agents)

These notes come from taking `src/SceneBase.cpp` from about 99% to 100% on all 57 functions and linking it as `Matching`.
Read `docs/llm_decomp_guide.md` first. This file adds what that guide does not cover: how MWCC (GC/3.0a5.2) picks registers, and the source-level levers that change the choice.

Use it when the instruction stream is already identical to the target and only register numbers differ.

---

## 1. First, tell what kind of diff you have

- **Instructions differ** (extra/missing/reordered): this is a source-structure problem. Use the guide's workflow.
- **Same instructions, different register numbers**: this is allocation order. Everything below applies.
- **Same instructions, same registers, but the DOL still fails when linked `Matching`**: this is function order or weak-function emission. See section 6.

A diff of the form "the target never reuses the source register" or "ours reuses it" is almost always a coloring-order difference, not a flag, pragma or compiler-version difference. All compiler versions, `-O`/`-ipa`/`-inline` flags and every `#pragma opt_*` were tried and none helped.

---

## 2. The allocator model (validated against the compiler binary)

The compiler is a 32-bit PE under wibo. The register allocator was decompiled with Ghidra and checked with gdb. What matters:

1. **Interference is half-open.** A result does not interfere with a source that dies in the same instruction. So `fmuls f5,f5,f29` (result reuses a dying source) is normal.
2. **Select colors nodes in descending vreg order**, each taking the lowest free register.
   - Volatile registers (r3-r12, f0-f13) are used first.
   - When a node needs a callee-saved register, the next one is handed out in order (`r31`, `r30`, ... / `f31`, `f30`, ...).
   - So the highest vreg gets the lowest volatile register, or the highest callee-saved register.
3. **Nodes of high degree** (values live across calls) are ordered by a spill weight/degree ratio instead of vreg. This decides the order of callee-saved registers for constants and parameters. It is hard to predict; change use counts or the shape of the code and measure.
4. **How vreg numbers are assigned:**
   1. Function parameters, in order (first parameter = lowest).
   2. **Named locals of the function**, first declared = highest vreg (colored first).
   3. All `@N` variables: locals and parameters of **inline expansions**, and optimizer temporaries.
      - Within one inline expansion, locals are created up front in reverse declaration order, so the **last declared local is colored first**.
      - A nested inline expansion is newer, so it has a **lower** vreg (colored later) than the locals of the inline that contains it.
      - The strength-reduced table pointer the optimizer makes is the newest, so it is colored last.
   4. **Expression temporaries** (loads, intermediate results): numbered in IR-generation order, above every variable. Later generated means colored earlier.
   - Consequence: every named local has a lower vreg than every `@` variable, and every expression temp has a higher vreg than every variable.

To see this for yourself, compile a small file and count: which values are variables, which are temps, and in what order. Then ask which value the target colors first.

---

## 3. Levers that actually changed the allocation

Each of these fixed a real function. Try them in this order of cheapness.

### 3.1 Declaration order of locals
Declare locals up front and uninitialised, in the order that gives the coloring order you need, then assign later in the original statement order. This does not change the instructions, only the vregs.

```cpp
// constructor texture loop: i, tex, id before `page` gave the exact register split
int i;
GlyphTexture* tex;
const u32* id;
const char* page;
```

Initialising at the declaration site often changes the code. Only the assign-after-declare form worked for the constructor and for `DrawNumRightAligned`.

### 3.2 The declared type of a local assigned from a load
A local assigned from a load is **merged into the load's expression temp** if its type allows it, so it is colored like a temp (first). A different type keeps it a separate variable.

- `wchar_t c = *p;` merges: `c` is colored first, like an expression temp.
- `u32 c = *p;` stays a variable: `c` is colored where its declaration puts it.

This flipped the order of the character register against the search index. `DrawDateCentered` needs `wchar_t`; `DrawTempCentered` and `DrawNumCentered` need `u32`. Test both.

### 3.3 Put the loop inside one inline expansion
When the search index must be colored before the loop pointer and loop index, all three must be locals of the **same** inline expansion. Nesting a `GetXGlyph()` inline puts the search index in a newer expansion, which is always colored later.

Write the search loop inside the helper and use `goto` to reproduce early-return code:

```cpp
static inline f32 CalcDateWidthImpl(const wchar_t* str, u32 len) {
    f32 width = 0.0f;
    u32 i = 0;
    const wchar_t* p = str;
    int k;
    int glyph;
    wchar_t c;
    for (; i < len; i++, p++) {
        c = *p;
        for (k = 0; k < (int)(sizeof(sDateGlyphs) / sizeof(sDateGlyphs[0])); k++) {
            if (c == sDateGlyphs[k]) {
                glyph = k;
                goto found;
            }
        }
        glyph = -1;
    found:
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }
    return width;
}
```

A `break` plus `glyph = -1` before the loop produced different code. The `goto` form matches inlined early-return code.

### 3.4 A separate pointer for a walked parameter
Incrementing a parameter (`str++`) versus walking a copy (`const wchar_t* p = str; ... p++`) changes vregs. The copy form fixed `CalcDateWidth`, `CalcNumWidth` and `CalcTempWidth` outright. The loop index and pointer can then be stepped together: `for (u32 i = 0; i < len; i++, p++)`.

### 3.5 Non-POD class lvalues change temporaries
MWCC passes a 4-byte struct by value through a hidden pointer to a caller-made stack copy. Normally it recomputes `addi rX,r1,imm` at each call. It hoists that address into a register before the loop if the argument is an lvalue of a **non-POD class** (one with a user constructor, such as `nw4r::ut::Color`):

```cpp
static inline const nw4r::ut::Color& AsColor(const GXColor* c) {
    return *static_cast<const nw4r::ut::Color*>(c);
}
...
GXSetTevColor(GX_TEVREG0, AsColor(shadowColor));
```

The copy is still a single bytewise `lbz`/`stb`. This also changed the argument-evaluation order of a first call in `DrawNumRightAligned`.

### 3.6 Aliasing: reference to a local copy
If the target reloads a value from memory after a store to a stack struct, the compiler thought the store might alias it. Taking a reference to the local does that:

```cpp
Vec2 curStore = *pos;
Vec2& cur = curStore;
```

Plain locals, inline helper references, `Vec2F`, and `DrawGlyph(const Vec2*)` did not. A class with a destructor also does it, but it emits an out-of-line weak destructor that the target does not have, so check the symbol list.

### 3.7 Reuse the carried variable for a derived value
A loop-carried variable that the target keeps live across a block must really be live there. In `DrawTemp`/`DrawDate`/`DrawNum`, using `prevWidth` itself for the advance (`prevWidth = scaleX * (...); cur.x += prevWidth;`) extended its live range and fixed the register. A separate `advance` local did not.

### 3.8 Make a float result a named variable
Expression temps are colored before variables. To make loads of `at.x`/`at.y` colored first, make the results variables and assign in the right order:

```cpp
f32 halfW = 0.5f * sGlyphTextures[index].width;
f32 offX = halfW * scaleX;
f32 halfH = 0.5f * sGlyphTextures[index].height;
f32 offY = halfH * scaleY;
f32 px, py;
py = at.y - offY;
px = at.x - offX;
```

Initialising `px`/`py` at the declaration, assigning x before y, or declaring `pos` first, all failed.

### 3.9 A named local for a hoisted constant
The order of the loop-invariant float constants hoisted into callee-saved registers depends on how they are created. A named local inside the block moved `-1.0f` ahead of the scale parameters (the position of the local mattered):

```cpp
if (i != 0) {
    f32 m1 = -1.0f;
    prevWidth = scaleX * (m1 + (0.5f * prevWidth + 0.5f * sGlyphTextures[index].width));
```

---

## 4. Building a fast harness (do this before anything else)

The real build takes about 4 s per variant. A one-function harness takes about 0.5 s and is more reliable.

1. Write a prelude with the typedefs the function needs (`u8..f32`, `Vec2`, `Vec`, `GXColor`, `extern` arrays) and paste the function plus its static/inline helpers and tables from the real file.
2. Compile it with the unit's exact flags (copy the line from `build.ninja`; `build/tools/wibo build/compilers/GC/3.0a5.2/mwcceppc.exe ...`). Add `-i <your dir>` for your prelude.
3. Disassemble with `build/binutils/powerpc-eabi-objdump -dr -EB -mpowerpc -M broadway -d`.
4. **Score the whole function against the target listing** (from `build/HAFE/obj/<Unit>.o`), with relocation lines dropped and branch targets normalised (`bne 3018` becomes `bne X`). Count differing instructions; 0 is an exact match.
   - Check that the standalone copy reproduces the real file's diff before you search (same instruction count, same diff count).
   - Do **not** score a short window of instructions. A window ending on the last `fsubs` always contains an unrelated `lwzx`, so it can never report a match; several searches were wasted on this.
5. Drive it with a script that generates many variants (declaration orders, statement orders, types, helper forms) and prints any with 0 differences. Then port the winner to the real file and re-verify.

A single run of `permutations()` of the declared locals is cheap: 6-8 locals is 720-40320 variants.

The decomp-permuter (`~/decomp-permuter`) mutates only the named function and not helper inlines defined elsewhere. It found nothing here over hundreds of thousands of iterations, because the needed changes were declaration order and types, which it does not explore well. Use it for expression-level problems.

Useful tool outputs, if you need to look inside the compiler: a gdb script on wibo can dump each variable's name, weight, vreg and final color after allocation, and the interference graph. Anchor points in `mwcceppc.exe` (VAs): `0x596df0` per-class coloring loop, `0x61fae0` graph builder, `0x597300` simplify, `0x5970e0` select, `0x46b4a0` vreg numbering. Headless Ghidra is at `/opt/ghidra` (needs `JAVA_HOME=/usr/lib/jvm/java-26-openjdk`). This is analysis of the toolchain; the clean-room rule is about deriving the channel's own code, not about studying the compiler.

---

## 5. Strategy that worked

- Fix the **shared** problem first. Four draw functions shared one inline (`DrawGlyph`); fixing its float allocation fixed or nearly fixed all of them.
- Work out which values are **variables and which are temporaries**, and which one the target colors first. Then choose declaration order and types to get that order. Guessing expression forms rarely helps.
- If a local seems to have no effect, it was probably merged into a temp. Change its type.
- When one function is stuck at a handful of instructions, look at the **successful sibling**. `DrawTemp` was the key to `DrawDate`: same code shape, one extra constant.
- Parallel agents work well, each on one root cause, in separate git worktrees with a **copy** of `build/tools`, `build/compilers` and `build/binutils` (never symlinks; ninja may rewrite them and "Text file busy" crashes appear). Tell every agent to send one report. Stop an agent that re-sends its report.

---

## 6. Linking a unit as `Matching`

Even with every function at 100%, the DOL can differ:

- **Function order in the source must match the original.** `UpdateDragScroll` came after `DrawTemp` in the source but belongs right after `ToDegrees`. Every function in between was 12 bytes off, which showed up as hundreds of one-byte `bl` differences.
- **Find the first function whose linked address drifts.** Compare `nm build/HAFE/main.elf` addresses against `config/HAFE/symbols.txt` for names that appear once, and look at where the drift starts. A few names that appear in several units (for example `Callback`) are noise.
- **Weak function emission order.** If two weak destructors come out swapped after `__sinit`, add `extra_cflags=["-ipa file"]` for the unit in `configure.py`. `WeatherScene`, `WeatherOther`, `WeatherAddress`, `ForecastData` and `WeatherNormal` need it too.
- Extra weak functions in your object (for example `__dt__Q34nw4r2ut5ColorFv`, `unk44`, `unk48`) are deduplicated by the linker and are harmless if the DOL still matches.
- Always check ninja's **exit status**: a failed link leaves the old DOL, and `dtk shasum` still prints OK.

---

## 7. Practical pitfalls

- The first `ninja build/HAFE/src/X.o` after an edit sometimes fails with a missing `X.d` (`FileNotFoundError` in `transform_dep.py`). The object is usually fine. Run ninja again.
- `build/HAFE/report.json` is not rebuilt by plain `ninja`. Delete it and run `ninja build/HAFE/report.json`.
- Keep a saved copy of `report.json` and compare every function against it before you finish.
- The full build can crash in an unrelated nw4r source under high parallelism (exit 139). Re-run with `ninja -j 4`.
- When a search script edits `src/*.cpp`, do not edit that file by hand at the same time, and always restore it in a `finally` block.
- Do not trust a percentage without rebuilding first, and do not run a variant sweep and a manual edit on the same file together.

---

## 8. What did not help (do not repeat)

- Compiler versions, `-O`/`-ipa`/`-inline`/`-schedule` flags, and every `#pragma opt_*` / `peephole` / `scheduling` setting for these allocation problems.
- Randomised or exhaustive **expression** rewrites (operand order, associativity, named versus unnamed temporaries, helper inlines with value or reference parameters) when the real cause was vreg order.
- `const`/`register` qualifiers on locals.
- Long decomp-permuter runs on whole functions for allocation-order problems.
- Scalar `x`/`y` parameters, VEC3 constructors, pointer versus reference for `DrawGlyph`'s position argument.
