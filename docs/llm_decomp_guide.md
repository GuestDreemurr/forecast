# Decompiling Wii Channels: a do / do-not guide for LLM agents

This guide comes from matching the Forecast Channel's weather code, around 50 source files of C++ built with CodeWarrior (MWCC GC/3.0a5.2).
The News Channel and other channels from the same team share most of this codebase:

- the scene system (`Scene`, `SceneBase`)
- `System`
- `ButtonGroup` / `LayoutButton`
- `TextTagProcessor`, `LayoutObj`
- the NW4R libraries
- the `ut::Color`/`Rect`/`Vector2` helpers

What you learn in one channel mostly carries over to the others.

---

## 1. Workflow

### Do
- **Check the DOL after every change.** Run `build/tools/dtk shasum -c config/HAFE/build.sha1`. The linked DOL must stay `OK`. Never commit if it isn't.
- **Keep a regression baseline.** Before you start, save a copy of `build/HAFE/report.json`. After each change, compare every function's `fuzzy_match_percent` against it. Anything that got worse is your problem to fix, even if it's in a file you didn't mean to touch. Header changes ripple into other files.
- **Refresh the baseline** after each commit.
- **Split first, then decompile.** Add the file to `config/HAFE/splits.txt` and add an `Object(NonMatching, "X.cpp")` line to `configure.py`. Create an empty `.cpp` file and build it. dtk then writes `build/HAFE/asm/X.s` with proper labels for its data. Read the data from there, not from the `auto_*` files.
- **Find the boundaries between source files from the data, not the code.** These signs all mark a boundary:
  - A `__sinit_*` function is always the last function of its file.
  - Each file has its own `.sdata2` pool of constants. If `0.0f`, `0.5f` or `1.0f` appears a second time, a new file has started.
  - `.ctors` entries map one-to-one to `__sinit`s.
  - `.bss`/`.sbss` objects belong to whichever file's `__sinit` refers to them.
- **Expect tiny files.** A cluster of 3–7 small functions after another file's `__sinit` is its own file. Class methods can be spread across several small files. Split them rather than stuffing them into a neighbour.
- **Use m2c as a map, not as source.** Strip its struct dumps before reading. Its field accesses (`unk7F9`) give you the class layout. Its control flow gives you the structure. Rewrite everything as idiomatic C++.
- **Diff one function at a time** with `objdiff-cli diff -u main/Unit Function`, target on the left and ours on the right. Look at the whole instruction diff, not only the percentage.
- **Script your variant testing.** For each candidate edit: substitute it into the source, rebuild the single `.o`, run objdiff, record the percentage, then restore the file. Brute-forcing declaration orders (24–120 permutations) found fixes in minutes that were impossible to reason out by hand.
- **Commit at every milestone, with 0 regressions.** Push often, because the container is temporary.

### Do not
- Don't trust `build/HAFE/asm/auto_*.s` after you change splits. They can be stale. Rebuild, then read the file-named `.s`.
- Don't edit a file while a variant script is running on it. The script overwrites it on restore.
- Don't read a percentage without rebuilding first. A diff tool reading a stale `.o` will mislead you.
- Don't spend hours on one function stuck at 93–97% because of register numbering. Log it, move on, and come back with fresh evidence from other functions. The same fix usually applies to several functions at once.

---

## 2. Naming and headers

### Do
- **Name every `fn_`/`lbl_` symbol your code calls or refers to.** Add the mangled name to `config/HAFE/symbols.txt` so the linker resolves your C++ names. Get mangled names from your own object with `powerpc-eabi-nm build/HAFE/src/X.o`.
- **Mark file-local statics** with `scope:local` in `symbols.txt`.
- **Rename `lbl_` externs to real names** when you declare them, e.g. `gAmText`, `gUpdatedPrefixes`.
- **Put classes in headers with offset comments** (`// at 0x7F9`). Keep every field at its offset, using `u8 unkXX[...]` padding where needed.
- **Fix wrong signatures as soon as the assembly proves them wrong**, then check every file that uses them. Examples from this project:
  - `SetPaneAlpha` takes an `s32`, not a `u8`. Callers pass an int without a `clrlwi` mask.
  - `SetPosition` takes `const bool&`. A `u8` produced an extra `extrwi` after a compare.
  - A `Setup` that returns nothing is `void`.
  - Return types aren't part of the mangled name, so these changes are free.
- **Check the renamed functions directly.** When a fix changes a mangled name (e.g. `RCUci` → `RCbi`), the regression script won't compare them, because the old name no longer exists.

### Do not
- Don't invent a signature because m2c guessed one. Types come from the instructions: `clrlwi`/`extsb`/`frsp`/`lhz` versus `lwz`.
- Don't leave compiler warnings from your changes, such as "return value expected". Make state functions `void` when nothing reads their result.
- Don't give the same function different declarations across headers by accident. When the assembly *does* show different caller-side types in different files (e.g. a `double` return followed by `frsp`), declare it locally in that file with a comment explaining why.

---

## 3. MWCC codegen: what source produces the target

### Data layout
- **Do** define file-scope tables in the order they appear in `.rodata`/`.data`. They're emitted in definition order.
- **Do** place a table next to the function that first uses it if the target puts it among that function's string literals. Moving two tables of hour labels fixed a 0x90-byte offset that broke every relocation in the file.
- **Do** order literals and conditions the way `.sdata` shows them. `if (lang == 0) strcpy("timeJP") else strcpy("timeWW")` puts the strings in a different order than the reverse test does.
- **Do** order expressions so constants first appear in `.sdata2` order. Writing `(x - 8.0f)` before `2.0f * ...` put 8.0 before 2.0.
- **Do not** build a `GXColor` with a brace initializer when one member isn't constant. MWCC emits a constant template in `.sdata2` and copies it. The target set each byte with `stb`. Assign `.r/.g/.b/.a` one at a time.
- **Do** use `#pragma explicit_zero_data on` when zero-initialized statics live in `.sdata` rather than `.sbss`.
- **Do** make statics file-scope (`static f32 sScaleX = 0.6f;` above the function) when the target loads the static again after each store to a member. Function-local statics let MWCC hoist the loads, and the target didn't.

### Inlining and helpers
- The channel files build with `-inline noauto`. Only functions marked `inline` get inlined.
- **Weak inline functions** are emitted right after the first function that needs them out of line. Array constructors/destructors show up this way through `__construct_array`.
- **Do** use a real `inline` member function, not a macro, when the target's stack slots for temporaries come out in reverse order. Example: a `SetState(StateFunc)` that inlines a ptmf change. Inline parameters are allocated like locals, in call order. Macro blocks weren't.
- **Do** use a tiny inline predicate when the target compares a ptmf through a copy and `cntlzw`. Example: `BOOL IsScrollState(Func f) { return mScrollState == f; }`, used as `if (!IsScrollState(f))`.
- A non-inline function (e.g. `ChangeState`) can still appear inlined elsewhere. That means the source used a macro or a separate inline for those spots. Keep the out-of-line definition where the target has it.
- Functions that take or return structs by value (`ut::Color`, `Vec2F`) are **not** inlined. They become weak copies.

### Control flow
- **Switch lowering depends on the exact set of case values.** An inner `switch (phase) { case 1: case 2: case 3: default: }` pivoted on 2. The target pivoted on 3 because the default arm was also `case 4:`. If the dispatch order differs, add explicit case labels for the final phase.
- **A nested switch that reuses the register** already holding the outer switch's value (no reload) is still a nested switch. Write `switch (x) { case 0: ... case -1: break; default: switch (x) { ... } }`.
- **Dead `b` instructions after a branch tell you the source shape:**
  - `case 1: d = day; break; case 2: d = day + 1; break; default: return;` produces a dead `b body` for case 1 and a dead `b end` for the default.
  - A fall-through `case 2: day++; case 1:` does not.
- **`if (x) {...} else { return; }` differs from `if (!x) return;`.** The first leaves a dead branch.
- **Explicit null checks before `delete`** (`if (p != NULL) delete p;`) show up as an extra `cmpwi` and `beq`, even though `delete` checks for null anyway.
- **Many functions share the same state-machine shape:** `switch (mPhase) { case 0: mPhase++; ...; break; case -1: break; default: ... }` plus `return TRUE`. Recognise it quickly.

### Registers and stack
- **Declaration order decides register numbers:**
  - The first-declared callee-saved local tends to get the highest register (r31, f31).
  - Among stack slots, the first declared gets the higher address.
  - When a loop's registers are wrong, declare the loop variables at function scope and brute-force their order.
- **Scope matters.** Two loops that use different registers for `box`/`i` in the target need their own variables. Use separate blocks or separate declarations.
- **Copy a global into a local before use** when the target loads it early. Examples:
  - `City* city = gCurrentCity;` before any stores to `this` made a function match 100%.
  - `f32 space = gUnkSceneFloat; SetCharSpace(scale * space);` fixes the operand order of a multiply.
  - The same goes for any early `s32 hour = cal.hour;`.
- **A parameter whose address is taken** (e.g. `WrapHour(&hour)`) was copied to a local in the target: `s32 h = hour; ... WrapHour(&h);`.
- **Use `u16` locals** when the target keeps a `lhz` result in a register. Use `u32` when it's stored straight to the stack for a `const u32&` argument.
- **Keep assignments that look dead** if the target keeps them. `box->mX = c.x; box->mY = c.y; box->mX = cx + box->mX * s;` kept both stores. The version without the first stores didn't match.
- **Watch float reuse after a store.** `mDateScaleX = s; SetScale(mDateScaleX, ...)` lets MWCC reuse the stored value. Passing `s` directly may reload it.
- **Compute early when the target multiplies early:** `f32 offset = half * CalcStringWidth(buf);` right after the call. Delaying the multiply keeps the width alive in a different register.
- **Update in place** (`hour += 6; hour %= 24;`) when the target does. `hour = (hour + 6) % 24` goes through a temporary register.

### Types and conversions
- **A `frsp` after a call** means the caller saw a `double` return type.
- **`clrlwi rX, rY, 24` before a call** means the parameter is a `u8`. Its absence with an int argument means it isn't.
- **`extrwi r0, r0, 8, 19` after `cntlzw`** means the compare result was stored to a `u8`. A plain `srwi` means `bool`.
- **`psq_l ... qr3` from a halfword** is `nw4r::math::CosIdx`/`SinIdx` on a `u16` index. Use the nw4r inline, not a hand-written conversion.
- **Byte stores to a colour** usually come from an inline `ut::Color(r,g,b,a)` constructor, or from copying a `ut::Color` by value (`SetTextColor(box->mColor)`).

---

## 4. Codebase-specific knowledge (shared across channels)

- **Layouts.** A `ButtonGroup` owns a `.brlyt` and its `LayoutButton`s. `FindButton(name)` returns a `LayoutButton*`. A missing pane is reported with `OSPanic(file, line, "<name>が見つかりません!\n")`. The line numbers in `OSPanic` calls are real source line numbers, so keep them exact.
- **Screen size.** Width is `gWidescreen ? 832 : 608`. Height is 456 and the centre is `(0.5*w, 228)`. The widescreen x-scale is `1.3684211f`. A side margin of `gWidescreen ? 36 : 28` is common.
- **Pane flags.** In this NW4R version the pane's flag byte is at `0xCF`, and bit 2 means "scale position on widescreen". This differs from the newer header layout.
- **Text drawing.**
  - `gTextWriter` is a `TextWriterBase<wchar_t>`. The usual sequence is `SetFont(*gSysFont)`, `SetDrawFlag`, `SetupGX`, `SetScale`, `SetCharSpace(scale * gUnkSceneFloat)`, `SetCursor`, `Print`.
  - To shrink text to fit: `scale *= maxWidth / CalcStringWidth(text)`.
  - `sTextBuf[0x100]` and `sNameBuf[0x100]` are shared scratch buffers.
- **Pointers and buttons.**
  - Pointer state: `gCursorX[4]`, `gCursorY[4]`, `gKPADLatest[4]`, `gPointerValid[4][16]`, `gTrig[4]`, `gTrigAll`.
  - `WPAD_BUTTON_A` is `0x800`.
  - `IsPointerValid(chan)` is an inline in `System.h`.
- **State machines.** They use pointer-to-member functions (ptmf) plus a phase `s32`. The phase is `-1` on exit and `0` on entry. The ptmf constants are anonymous `.data` objects, in first-use order within each function.
- **Wide strings.** Japanese text in `.sdata` shows up as odd ASCII in dtk (`"fB"` is `L"時"`). Write these as `L"\x6642"` with a comment, and write Shift-JIS char strings with `\x82..` escapes.

---

## 5. Reporting

### Do
- Report per-file and per-function percentages honestly. Name the functions that don't match and say why (register order, one-instruction scheduling).
- Keep the PR description current:
  - one table row per file
  - split findings (where each file's boundaries are and how you found them)
  - signature fixes and their cross-file effect
  - the testing you did (DOL OK, 0 regressions, no warnings)

### Do not
- Don't claim a match you haven't checked in a fresh build.
- Don't hide regressions in other files. Fix them, or state them.
