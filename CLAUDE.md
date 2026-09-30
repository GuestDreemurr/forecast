# Instructions for AI agents

This repository is a decompilation of the Wii Forecast Channel (USA/NTSC v7).

## Read these first

Before doing any work in this repository, read every file in `docs/` whose name contains `llm`.
Start with this one:

- `docs/llm_decomp_guide.md`: a do / do-not guide to decompiling Wii channels with this toolchain.

The other files in `docs/` explain the build setup, splits and symbols. Read them when you need them.

## Rules that always apply

- After every change, the DOL must still match the original.
  Check it with `build/tools/dtk shasum -c config/HAFE/build.sha1`.
- No function in any unit may get worse. Compare `build/HAFE/report.json` against a saved baseline before you commit.
- Name every `fn_`/`lbl_` symbol your code uses in `config/HAFE/symbols.txt`.
