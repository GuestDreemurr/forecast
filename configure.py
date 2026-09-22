#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import sys
from pathlib import Path
from typing import Any, Dict, List

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "HAFE",  # Forecast Channel USA/NTSC
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-2"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.3"
config.objdiff_tag = "v3.6.1"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.0.3"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
]
config.ldflags = [
    "-fp hardware",
    "-nodefaults",
]
if args.debug:
    config.ldflags.append("-g")  # Or -gdwarf-2 for Wii linkers
if args.map:
    config.ldflags.append("-mapunused")
    # config.ldflags.append("-listclosure") # For Wii linkers

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = []

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Base flags, common to most GC/Wii games.
# Generally leave untouched, with overrides added below.
cflags_base = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse",
    "-enc SJIS",
    "-i include",
    "-i include/MSL",
    "-i include/MSL/internal",
    "-i include/decomp",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# Debug flags
if args.debug:
    # Or -sym dwarf-2 for Wii compilers
    cflags_base.extend(["-sym dwarf-2", "-DDEBUG=1"])
else:
    cflags_base.append("-DNDEBUG=1")

# Warning flags
if args.warn == "all":
    cflags_base.append("-W all")
elif args.warn == "off":
    cflags_base.append("-W off")
elif args.warn == "error":
    cflags_base.append("-W error")

# Metrowerks library flags
cflags_runtime = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-common off",
    "-inline auto",
]

# RVL SDK flags
cflags_rvl = [
    *cflags_base,
    "-enc SJIS",
    "-fp_contract off",
    "-ipa file",
]

# REL flags
cflags_rel = [
    *cflags_base,
    "-sdata 0",
    "-sdata2 0",
]

config.linker_version = "GC/1.3.2"


# Helper function for Dolphin libraries
def DolphinLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_base,
        "progress_category": "sdk",
        "objects": objects,
    }


# Helper function for REL script objects
def Rel(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.3.2",
        "cflags": cflags_rel,
        "progress_category": "game",
        "objects": objects,
    }


Matching = True                   # Object matches and should be linked
NonMatching = False               # Object does not match and should not be linked
Equivalent = config.non_matching  # Object should be linked when configured with --non-matching


# Object is only matching for specific versions
def MatchingFor(*versions):
    return config.version in versions


config.warn_missing_config = True
config.warn_missing_source = False
config.libs = [
    {
        "lib": "Runtime.PPCEABI.H",
        "mw_version": config.linker_version,
        "cflags": cflags_runtime,
        "progress_category": "sdk",  # str | List[str]
        "objects": [
            Object(NonMatching, "Runtime.PPCEABI.H/global_destructor_chain.c"),
            Object(NonMatching, "Runtime.PPCEABI.H/__init_cpp_exceptions.cpp"),
        ],
    },
    {
        "lib": "RVL_SDK",
        "mw_version": "GC/3.0a5",
        "cflags": cflags_rvl,
        "progress_category": "sdk",  # str | List[str]
        "objects": [
            # AI
            Object(NonMatching, "revolution/AI/ai.c"),

            # ARC
            Object(NonMatching, "revolution/ARC/arc.c"),

            # AX
            Object(NonMatching, "revolution/AX/AX.c"),
            Object(NonMatching, "revolution/AX/AXAlloc.c"),
            Object(NonMatching, "revolution/AX/AXAux.c"),
            Object(NonMatching, "revolution/AX/AXCL.c"),
            Object(NonMatching, "revolution/AX/AXComp.c"),
            Object(NonMatching, "revolution/AX/AXOut.c"),
            Object(NonMatching, "revolution/AX/AXProf.c"),
            Object(NonMatching, "revolution/AX/AXSPB.c"),
            Object(NonMatching, "revolution/AX/AXVPB.c"),
            Object(NonMatching, "revolution/AX/DSPCode.c"),

            # AXFX
            Object(NonMatching, "revolution/AXFX/AXFXChorus.c"),
            Object(NonMatching, "revolution/AXFX/AXFXDelay.c"),
            Object(NonMatching, "revolution/AXFX/AXFXHooks.c"),
            Object(NonMatching, "revolution/AXFX/AXFXLfoTable.c"),
            Object(NonMatching, "revolution/AXFX/AXFXReverbHi.c"),
            Object(NonMatching, "revolution/AXFX/AXFXReverbHiDpl2.c"),
            Object(NonMatching, "revolution/AXFX/AXFXSrcCoef.c"),

            # BASE
            Object(NonMatching, "revolution/BASE/PPCArch.c"),

            # BTE/audio_a2dp_hw
            Object(NonMatching, "revolution/BTE/audio_a2dp_hw/audio_a2dp_hw.c"),

            # BTE/bta/ag
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_act.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_api.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_at.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_cfg.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_ci.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_cmd.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_main.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_rfc.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_sco.c"),
            Object(NonMatching, "revolution/BTE/bta/ag/bta_ag_sdp.c"),

            # BTE/bta/ar
            Object(NonMatching, "revolution/BTE/bta/ar/bta_ar.c"),

            # BTE/bta/av
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_aact.c"),
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_act.c"),
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_api.c"),
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_cfg.c"),
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_ci.c"),
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_main.c"),
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_sbc.c"),
            Object(NonMatching, "revolution/BTE/bta/av/bta_av_ssm.c"),

            # BTE/bta/dm
            Object(NonMatching, "revolution/BTE/bta/dm/bta_dm_act.c"),
            Object(NonMatching, "revolution/BTE/bta/dm/bta_dm_api.c"),
            Object(NonMatching, "revolution/BTE/bta/dm/bta_dm_cfg.c"),
            Object(NonMatching, "revolution/BTE/bta/dm/bta_dm_ci.c"),
            Object(NonMatching, "revolution/BTE/bta/dm/bta_dm_main.c"),
            Object(NonMatching, "revolution/BTE/bta/dm/bta_dm_pm.c"),
            Object(NonMatching, "revolution/BTE/bta/dm/bta_dm_sco.c"),

            # BTE/bta/fs
            Object(NonMatching, "revolution/BTE/bta/fs/bta_fs_cfg.c"),
            Object(NonMatching, "revolution/BTE/bta/fs/bta_fs_ci.c"),

            # BTE/bta/gatt
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gattc_act.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gattc_api.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gattc_cache.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gattc_ci.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gattc_main.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gattc_utils.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gatts_act.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gatts_api.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gatts_main.c"),
            Object(NonMatching, "revolution/BTE/bta/gatt/bta_gatts_utils.c"),

            # BTE/bta/hh
            Object(NonMatching, "revolution/BTE/bta/hh/bta_hh_act.c"),
            Object(NonMatching, "revolution/BTE/bta/hh/bta_hh_api.c"),
            Object(NonMatching, "revolution/BTE/bta/hh/bta_hh_cfg.c"),
            Object(NonMatching, "revolution/BTE/bta/hh/bta_hh_main.c"),
            Object(NonMatching, "revolution/BTE/bta/hh/bta_hh_utils.c"),

            # BTE/bta/hl
            Object(NonMatching, "revolution/BTE/bta/hl/bta_hl_act.c"),
            Object(NonMatching, "revolution/BTE/bta/hl/bta_hl_api.c"),
            Object(NonMatching, "revolution/BTE/bta/hl/bta_hl_ci.c"),
            Object(NonMatching, "revolution/BTE/bta/hl/bta_hl_main.c"),
            Object(NonMatching, "revolution/BTE/bta/hl/bta_hl_sdp.c"),
            Object(NonMatching, "revolution/BTE/bta/hl/bta_hl_utils.c"),

            # BTE/bta/jv
            Object(NonMatching, "revolution/BTE/bta/jv/bta_jv_act.c"),
            Object(NonMatching, "revolution/BTE/bta/jv/bta_jv_api.c"),
            Object(NonMatching, "revolution/BTE/bta/jv/bta_jv_cfg.c"),
            Object(NonMatching, "revolution/BTE/bta/jv/bta_jv_main.c"),

            # BTE/bta/pan
            Object(NonMatching, "revolution/BTE/bta/pan/bta_pan_act.c"),
            Object(NonMatching, "revolution/BTE/bta/pan/bta_pan_api.c"),
            Object(NonMatching, "revolution/BTE/bta/pan/bta_pan_ci.c"),
            Object(NonMatching, "revolution/BTE/bta/pan/bta_pan_main.c"),

            # BTE/bta/pb
            Object(NonMatching, "revolution/BTE/bta/pb/bta_pbs_cfg.c"),

            # BTE/bta/sys
            Object(NonMatching, "revolution/BTE/bta/sys/bd.c"),
            Object(NonMatching, "revolution/BTE/bta/sys/bta_sys_cfg.c"),
            Object(NonMatching, "revolution/BTE/bta/sys/bta_sys_ci.c"),
            Object(NonMatching, "revolution/BTE/bta/sys/bta_sys_conn.c"),
            Object(NonMatching, "revolution/BTE/bta/sys/bta_sys_main.c"),
            Object(NonMatching, "revolution/BTE/bta/sys/ptim.c"),
            Object(NonMatching, "revolution/BTE/bta/sys/utl.c"),

            # BTE/btif/co
            Object(NonMatching, "revolution/BTE/btif/co/bta_ag_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_av_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_dm_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_fs_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_hh_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_hl_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_pan_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_sys_co.c"),

            # BTE/btif/src
            Object(NonMatching, "revolution/BTE/btif/src/bluetooth.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_av.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_config.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_config_util.cpp"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_core.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_dm.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_hf.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_hh.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_hl.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_media_task.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_pan.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_profile_queue.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_rc.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_sm.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_sock.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_sock_rfc.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_sock_sdp.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_sock_thread.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_sock_util.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_storage.c"),
            Object(NonMatching, "revolution/BTE/btif/src/btif_util.c"),

            # BTE/embdrv/sbc/encoder/srce
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_analysis.c"),
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_dct.c"),
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_dct_coeffs.c"),
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_enc_bit_alloc_mono.c"),
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_enc_bit_alloc_ste.c"),
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_enc_coeffs.c"),
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_encoder.c"),
            Object(NonMatching, "revolution/BTE/embdrv/sbc/encoder/srce/sbc_packing.c"),

            # BTE/gki/common
            Object(NonMatching, "revolution/BTE/gki/common/gki_buffer.c"),
            Object(NonMatching, "revolution/BTE/gki/common/gki_debug.c"),
            Object(NonMatching, "revolution/BTE/gki/common/gki_time.c"),

            # BTE/gki/ulinux
            Object(NonMatching, "revolution/BTE/gki/ulinux/gki_ulinux.c"),

            # BTE/hci/src
            Object(NonMatching, "revolution/BTE/hci/src/bt_hci_bdroid.c"),
            Object(NonMatching, "revolution/BTE/hci/src/bt_hw.c"),
            Object(NonMatching, "revolution/BTE/hci/src/btsnoop.c"),
            Object(NonMatching, "revolution/BTE/hci/src/hci_h4.c"),
            Object(NonMatching, "revolution/BTE/hci/src/hci_mct.c"),
            Object(NonMatching, "revolution/BTE/hci/src/hcisu_h2.c"),
            Object(NonMatching, "revolution/BTE/hci/src/lpm.c"),
            Object(NonMatching, "revolution/BTE/hci/src/userial.c"),
            Object(NonMatching, "revolution/BTE/hci/src/userial_mct.c"),
            Object(NonMatching, "revolution/BTE/hci/src/utils.c"),

            # BTE/main
            Object(NonMatching, "revolution/BTE/main/bte_conf.c"),
            Object(NonMatching, "revolution/BTE/main/bte_init.c"),
            Object(NonMatching, "revolution/BTE/main/bte_logmsg.c"),
            Object(NonMatching, "revolution/BTE/main/bte_main.c"),
            Object(NonMatching, "revolution/BTE/main/bte_version.c"),

            # BTE/rvl
            Object(NonMatching, "revolution/BTE/rvl/gki_ppc.c"),

            # BTE/stack/a2dp
            Object(NonMatching, "revolution/BTE/stack/a2dp/a2d_api.c"),
            Object(NonMatching, "revolution/BTE/stack/a2dp/a2d_sbc.c"),

            # BTE/stack/avct
            Object(NonMatching, "revolution/BTE/stack/avct/avct_api.c"),
            Object(NonMatching, "revolution/BTE/stack/avct/avct_ccb.c"),
            Object(NonMatching, "revolution/BTE/stack/avct/avct_l2c.c"),
            Object(NonMatching, "revolution/BTE/stack/avct/avct_lcb.c"),
            Object(NonMatching, "revolution/BTE/stack/avct/avct_lcb_act.c"),

            # BTE/stack/avdt
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_ad.c"),
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_api.c"),
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_ccb.c"),
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_ccb_act.c"),
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_l2c.c"),
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_msg.c"),
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_scb.c"),
            Object(NonMatching, "revolution/BTE/stack/avdt/avdt_scb_act.c"),

            # BTE/stack/avrc
            Object(NonMatching, "revolution/BTE/stack/avrc/avrc_api.c"),
            Object(NonMatching, "revolution/BTE/stack/avrc/avrc_opt.c"),
            Object(NonMatching, "revolution/BTE/stack/avrc/avrc_sdp.c"),

            # BTE/stack/bnep
            Object(NonMatching, "revolution/BTE/stack/bnep/bnep_api.c"),
            Object(NonMatching, "revolution/BTE/stack/bnep/bnep_main.c"),
            Object(NonMatching, "revolution/BTE/stack/bnep/bnep_utils.c"),

            # BTE/stack/btm
            Object(NonMatching, "revolution/BTE/stack/btm/btm_acl.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_ble.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_ble_addr.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_ble_bgconn.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_ble_gap.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_dev.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_devctl.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_inq.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_main.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_pm.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_sco.c"),
            Object(NonMatching, "revolution/BTE/stack/btm/btm_sec.c"),

            # BTE/stack/btu
            Object(NonMatching, "revolution/BTE/stack/btu/btu_hcif.c"),
            Object(NonMatching, "revolution/BTE/stack/btu/btu_init.c"),
            Object(NonMatching, "revolution/BTE/stack/btu/btu_task.c"),

            # BTE/stack/gatt
            Object(NonMatching, "revolution/BTE/stack/gatt/att_protocol.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_api.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_attr.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_auth.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_cl.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_db.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_main.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_sr.c"),
            Object(NonMatching, "revolution/BTE/stack/gatt/gatt_utils.c"),

            # BTE/stack/hcic
            Object(NonMatching, "revolution/BTE/stack/hcic/hciblecmds.c"),
            Object(NonMatching, "revolution/BTE/stack/hcic/hcicmds.c"),

            # BTE/stack/hid
            Object(NonMatching, "revolution/BTE/stack/hid/hidh_api.c"),
            Object(NonMatching, "revolution/BTE/stack/hid/hidh_conn.c"),

            # BTE/stack/l2cap
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_api.c"),
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_ble.c"),
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_csm.c"),
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_fcr.c"),
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_link.c"),
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_main.c"),
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_ucd.c"),
            Object(NonMatching, "revolution/BTE/stack/l2cap/l2c_utils.c"),

            # BTE/stack/mcap
            Object(NonMatching, "revolution/BTE/stack/mcap/mca_api.c"),
            Object(NonMatching, "revolution/BTE/stack/mcap/mca_cact.c"),
            Object(NonMatching, "revolution/BTE/stack/mcap/mca_csm.c"),
            Object(NonMatching, "revolution/BTE/stack/mcap/mca_dact.c"),
            Object(NonMatching, "revolution/BTE/stack/mcap/mca_dsm.c"),
            Object(NonMatching, "revolution/BTE/stack/mcap/mca_l2c.c"),
            Object(NonMatching, "revolution/BTE/stack/mcap/mca_main.c"),

            # BTE/stack/pan
            Object(NonMatching, "revolution/BTE/stack/pan/pan_api.c"),
            Object(NonMatching, "revolution/BTE/stack/pan/pan_main.c"),
            Object(NonMatching, "revolution/BTE/stack/pan/pan_utils.c"),

            # BTE/stack/rfcomm
            Object(NonMatching, "revolution/BTE/stack/rfcomm/port_api.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/port_rfc.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/port_utils.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/rfc_l2cap_if.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/rfc_mx_fsm.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/rfc_port_fsm.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/rfc_port_if.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/rfc_ts_frames.c"),
            Object(NonMatching, "revolution/BTE/stack/rfcomm/rfc_utils.c"),

            # BTE/stack/sdp
            Object(NonMatching, "revolution/BTE/stack/sdp/sdp_api.c"),
            Object(NonMatching, "revolution/BTE/stack/sdp/sdp_db.c"),
            Object(NonMatching, "revolution/BTE/stack/sdp/sdp_discovery.c"),
            Object(NonMatching, "revolution/BTE/stack/sdp/sdp_main.c"),
            Object(NonMatching, "revolution/BTE/stack/sdp/sdp_server.c"),
            Object(NonMatching, "revolution/BTE/stack/sdp/sdp_utils.c"),

            # BTE/stack/smp
            Object(NonMatching, "revolution/BTE/stack/smp/aes.c"),
            Object(NonMatching, "revolution/BTE/stack/smp/smp_act.c"),
            Object(NonMatching, "revolution/BTE/stack/smp/smp_api.c"),
            Object(NonMatching, "revolution/BTE/stack/smp/smp_cmac.c"),
            Object(NonMatching, "revolution/BTE/stack/smp/smp_keys.c"),
            Object(NonMatching, "revolution/BTE/stack/smp/smp_l2c.c"),
            Object(NonMatching, "revolution/BTE/stack/smp/smp_main.c"),
            Object(NonMatching, "revolution/BTE/stack/smp/smp_utils.c"),

            # BTE/udrv/ulinux
            Object(NonMatching, "revolution/BTE/udrv/ulinux/uipc.c"),

            # BTE/utils/src
            Object(NonMatching, "revolution/BTE/utils/src/bt_utils.c"),

            # CNT
            Object(NonMatching, "revolution/CNT/cnt.c"),

            # DB
            Object(NonMatching, "revolution/DB/db.c"),

            # DSP
            Object(NonMatching, "revolution/DSP/dsp.c"),
            Object(NonMatching, "revolution/DSP/dsp_debug.c"),
            Object(NonMatching, "revolution/DSP/dsp_task.c"),

            # DVD
            Object(NonMatching, "revolution/DVD/dvd.c"),
            Object(NonMatching, "revolution/DVD/dvd_broadway.c"),
            Object(NonMatching, "revolution/DVD/dvderror.c"),
            Object(NonMatching, "revolution/DVD/dvdFatal.c"),
            Object(NonMatching, "revolution/DVD/dvdfs.c"),
            Object(NonMatching, "revolution/DVD/dvdidutils.c"),
            Object(NonMatching, "revolution/DVD/dvdqueue.c"),

            # ESP
            Object(NonMatching, "revolution/ESP/esp.c"),

            # EUART
            Object(NonMatching, "revolution/EUART/euart.c"),

            # EXI
            Object(NonMatching, "revolution/EXI/EXIBios.c"),
            Object(NonMatching, "revolution/EXI/EXICommon.c"),
            Object(NonMatching, "revolution/EXI/EXIUart.c"),

            # FS
            Object(NonMatching, "revolution/FS/fs.c"),

            # GX
            Object(NonMatching, "revolution/GX/GXAttr.c"),
            Object(NonMatching, "revolution/GX/GXBump.c"),
            Object(NonMatching, "revolution/GX/GXDisplayList.c"),
            Object(NonMatching, "revolution/GX/GXDraw.c"),
            Object(NonMatching, "revolution/GX/GXGeometry.c"),
            Object(NonMatching, "revolution/GX/GXLight.c"),
            Object(NonMatching, "revolution/GX/GXPixel.c"),
            Object(NonMatching, "revolution/GX/GXTransform.c"),

            # IPC
            Object(NonMatching, "revolution/IPC/ipcclt.c"),
            Object(NonMatching, "revolution/IPC/ipcMain.c"),
            Object(NonMatching, "revolution/IPC/ipcProfile.c"),
            Object(NonMatching, "revolution/IPC/memory.c"),

            # MEM
            Object(NonMatching, "revolution/MEM/mem_allocator.c"),
            Object(NonMatching, "revolution/MEM/mem_expHeap.c"),
            Object(NonMatching, "revolution/MEM/mem_frameHeap.c"),
            Object(NonMatching, "revolution/MEM/mem_heapCommon.c"),
            Object(NonMatching, "revolution/MEM/mem_list.c"),

            # MTX
            Object(NonMatching, "revolution/MTX/mtx44.c"),
            Object(NonMatching, "revolution/MTX/mtxvec.c"),
            Object(NonMatching, "revolution/MTX/quat.c"),
            Object(NonMatching, "revolution/MTX/vec.c"),

            # NAND
            Object(NonMatching, "revolution/NAND/nand.c"),
            Object(NonMatching, "revolution/NAND/NANDCheck.c"),
            Object(NonMatching, "revolution/NAND/NANDCore.c"),
            Object(NonMatching, "revolution/NAND/NANDOpenClose.c"),

            # NdevExi2AD
            Object(NonMatching, "revolution/NdevExi2AD/DebuggerDriver.c"),
            Object(NonMatching, "revolution/NdevExi2AD/exi2.c"),

            # NET
            Object(NonMatching, "revolution/NET/nettime.c"),
            Object(NonMatching, "revolution/NET/NETVersion.c"),

            # NWC24
            Object(NonMatching, "revolution/NWC24/NWC24Config.c"),
            Object(NonMatching, "revolution/NWC24/NWC24DateParser.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Download.c"),
            Object(NonMatching, "revolution/NWC24/NWC24FileApi.c"),
            Object(NonMatching, "revolution/NWC24/NWC24FriendList.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Ipc.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Manage.c"),
            Object(NonMatching, "revolution/NWC24/NWC24MBoxCtrl.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Mime.c"),
            Object(NonMatching, "revolution/NWC24/NWC24MsgCommit.c"),
            Object(NonMatching, "revolution/NWC24/NWC24MsgObj.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Parser.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Schedule.c"),
            Object(NonMatching, "revolution/NWC24/NWC24SecretFList.c"),
            Object(NonMatching, "revolution/NWC24/NWC24StdApi.c"),
            Object(NonMatching, "revolution/NWC24/NWC24System.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Time.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Utils.c"),

            # OS
            Object(NonMatching, "revolution/OS/__ppc_eabi_init.c"),
            Object(NonMatching, "revolution/OS/__start.c"),
            Object(NonMatching, "revolution/OS/OS.c"),
            Object(NonMatching, "revolution/OS/OSAlarm.c"),
            Object(NonMatching, "revolution/OS/OSAlloc.c"),
            Object(NonMatching, "revolution/OS/OSArena.c"),
            Object(NonMatching, "revolution/OS/OSAudioSystem.c"),
            Object(NonMatching, "revolution/OS/OSCache.c"),
            Object(NonMatching, "revolution/OS/OSContext.c"),
            Object(Matching, "revolution/OS/OSError.c"),
            Object(NonMatching, "revolution/OS/OSExec.c"),
            Object(NonMatching, "revolution/OS/OSFatal.c"),
            Object(NonMatching, "revolution/OS/OSFont.c"),
            Object(NonMatching, "revolution/OS/OSInterrupt.c"),
            Object(NonMatching, "revolution/OS/OSIpc.c"),
            Object(NonMatching, "revolution/OS/OSLink.c"),
            Object(NonMatching, "revolution/OS/OSMemory.c"),
            Object(NonMatching, "revolution/OS/OSMessage.c"),
            Object(NonMatching, "revolution/OS/OSMutex.c"),
            Object(NonMatching, "revolution/OS/OSNet.c"),
            Object(NonMatching, "revolution/OS/OSPlayRecord.c"),
            Object(NonMatching, "revolution/OS/OSReset.c"),
            Object(NonMatching, "revolution/OS/OSRtc.c"),
            Object(NonMatching, "revolution/OS/OSStateFlags.c"),
            Object(NonMatching, "revolution/OS/OSStateTM.c"),
            Object(NonMatching, "revolution/OS/OSSync.c"),
            Object(NonMatching, "revolution/OS/OSThread.c"),
            Object(NonMatching, "revolution/OS/OSTime.c"),
            Object(NonMatching, "revolution/OS/OSUtf.c"),

            # PAD
            Object(NonMatching, "revolution/PAD/Pad.c"),

            # SC
            Object(NonMatching, "revolution/SC/scapi.c"),
            Object(NonMatching, "revolution/SC/scapi_prdinfo.c"),
            Object(NonMatching, "revolution/SC/scsystem.c"),

            # SI
            Object(NonMatching, "revolution/SI/SIBios.c"),
            Object(NonMatching, "revolution/SI/SISamplingRate.c"),

            # TPL
            Object(NonMatching, "revolution/TPL/TPL.c"),

            # USB
            Object(NonMatching, "revolution/USB/usb.c"),

            # VF
            Object(NonMatching, "revolution/VF/d_common.c"),
            Object(NonMatching, "revolution/VF/d_hash.c"),
            Object(NonMatching, "revolution/VF/d_time.c"),
            Object(NonMatching, "revolution/VF/d_vf.c"),
            Object(NonMatching, "revolution/VF/d_vf_sys.c"),
            Object(NonMatching, "revolution/VF/nand_drv.c"),
            Object(NonMatching, "revolution/VF/pdm_bpb.c"),
            Object(NonMatching, "revolution/VF/pdm_disk.c"),
            Object(NonMatching, "revolution/VF/pdm_dskmng.c"),
            Object(NonMatching, "revolution/VF/pdm_mbr.c"),
            Object(NonMatching, "revolution/VF/pdm_partition.c"),
            Object(NonMatching, "revolution/VF/pf_api_util.c"),
            Object(NonMatching, "revolution/VF/pf_attach.c"),
            Object(NonMatching, "revolution/VF/pf_cache.c"),
            Object(NonMatching, "revolution/VF/pf_clib.c"),
            Object(NonMatching, "revolution/VF/pf_cluster.c"),
            Object(NonMatching, "revolution/VF/pf_code.c"),
            Object(NonMatching, "revolution/VF/pf_cp932.c"),
            Object(NonMatching, "revolution/VF/pf_detach.c"),
            Object(NonMatching, "revolution/VF/pf_dir.c"),
            Object(NonMatching, "revolution/VF/pf_driver.c"),
            Object(NonMatching, "revolution/VF/pf_entry.c"),
            Object(NonMatching, "revolution/VF/pf_entry_iterator.c"),
            Object(NonMatching, "revolution/VF/pf_errnum.c"),
            Object(NonMatching, "revolution/VF/pf_fat.c"),
            Object(NonMatching, "revolution/VF/pf_fat12.c"),
            Object(NonMatching, "revolution/VF/pf_fat16.c"),
            Object(NonMatching, "revolution/VF/pf_fat32.c"),
            Object(NonMatching, "revolution/VF/pf_fatfs.c"),
            Object(NonMatching, "revolution/VF/pf_fclose.c"),
            Object(NonMatching, "revolution/VF/pf_file.c"),
            Object(NonMatching, "revolution/VF/pf_filelock.c"),
            Object(NonMatching, "revolution/VF/pf_finfo.c"),
            Object(NonMatching, "revolution/VF/pf_fopen.c"),
            Object(NonMatching, "revolution/VF/pf_fread.c"),
            Object(NonMatching, "revolution/VF/pf_fseek.c"),
            Object(NonMatching, "revolution/VF/pf_fwrite.c"),
            Object(NonMatching, "revolution/VF/pf_getdev.c"),
            Object(NonMatching, "revolution/VF/pf_init_prfile2.c"),
            Object(NonMatching, "revolution/VF/pf_path.c"),
            Object(NonMatching, "revolution/VF/pf_remove.c"),
            Object(NonMatching, "revolution/VF/pf_sector.c"),
            Object(NonMatching, "revolution/VF/pf_service.c"),
            Object(NonMatching, "revolution/VF/pf_str.c"),
            Object(NonMatching, "revolution/VF/pf_system.c"),
            Object(NonMatching, "revolution/VF/pf_unmount.c"),
            Object(NonMatching, "revolution/VF/pf_volume.c"),
            Object(NonMatching, "revolution/VF/pf_w_clib.c"),
            Object(NonMatching, "revolution/VF/sd_drv.c"),

            # VI
            Object(NonMatching, "revolution/VI/vi.c"),

            # WENC
            Object(NonMatching, "revolution/WENC/wenc.c"),

            # WPAD
            Object(NonMatching, "revolution/WPAD/debug_msg.c"),
            Object(NonMatching, "revolution/WPAD/WPAD.c"),

            # WUD
            Object(NonMatching, "revolution/WUD/debug_msg.c"),
            Object(NonMatching, "revolution/WUD/WUD.c"),
            Object(NonMatching, "revolution/WUD/WUDHidHost.c"),
        ],
    },
    {
            "lib": "Channel", # throwing shit at the wall here, im guessing this is what we should put forecast code as? not sure how forecast is structured yet.. -guestd
            "mw_version": config.linker_version,
            "cflags": cflags_base,
            "progress_category": "channel",
            "objects": [
                Object(NonMatching, "main.cpp")
            ],    
    },
]


# Optional callback to adjust link order. This can be used to add, remove, or reorder objects.
# This is called once per module, with the module ID and the current link order.
#
# For example, this adds "dummy.c" to the end of the DOL link order if configured with --non-matching.
# "dummy.c" *must* be configured as a Matching (or Equivalent) object in order to be linked.
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    if module_id == 0:  # DOL
        return objects + ["dummy.c"]
    return objects


# Uncomment to enable the link order callback.
# config.link_order_callback = link_order_callback


# Optional extra categories for progress tracking
# Adjust as desired for your project
config.progress_categories = [
    ProgressCategory("channel", "Channel Code"),
    ProgressCategory("sdk", "RVL_SDK"),
]
config.progress_each_module = args.verbose
# Optional extra arguments to `objdiff-cli report generate`
config.progress_report_args = [
    # Marks relocations as mismatching if the target value is different
    # Default is "functionRelocDiffs=none", which is most lenient
    # "--config functionRelocDiffs=data_value",
]

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
