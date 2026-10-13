#ifndef RVL_SDK_VF_PF_ENTRY_ITERATOR_H
#define RVL_SDK_VF_PF_ENTRY_ITERATOR_H
#include <types.h>

#include "revolution/VF/vf_struct.h"
#ifdef __cplusplus
extern "C" {
#endif

static s32 VFiPFENT_RecalcEntryIterator(PF_ENT_ITER* p_iter, u32 may_allocate);
static s32 VFiPFENT_ITER_FindCluster(PF_ENT_ITER* p_iter, PF_DIR_ENT* p_ent, u32 cluster, u32* p_is_found);
static s32 VFiPFENT_ITER_DoFindEntry(PF_ENT_ITER* p_iter, PF_DIR_ENT* p_ent, PF_STR* p_pattern, u8 attr_required, u8 attr_unwanted, u32* p_is_found, u32 is_skip);
static s32 VFiPFENT_ITER_DoGetEntry(PF_ENT_ITER* p_iter, PF_DIR_ENT* p_ent, PF_VOLUME* p_vol, PF_STR* p_path, u32 wildcard, u32 no_look_last_token);
s32 VFiPFENT_ITER_IteratorInitialize(PF_ENT_ITER* p_iter, u32 index_start_from);
u32 VFiPFENT_ITER_IsAtLogicalEnd(PF_ENT_ITER* p_iter);
s32 VFiPFENT_ITER_Advance(PF_ENT_ITER* p_iter, u32 may_allocate);
s32 VFiPFENT_ITER_Retreat(PF_ENT_ITER* p_iter, u32 may_allocate);
s32 VFiPFENT_ITER_FindEntry(PF_ENT_ITER* p_iter, PF_DIR_ENT* p_ent, PF_STR* p_pattern, u8 attr_required, u8 attr_unwanted, u32* p_is_found, u32 is_skip);
s32 VFiPFENT_ITER_GetEntryOfPath(PF_ENT_ITER* p_iter, PF_DIR_ENT* p_ent, PF_VOLUME* p_vol, PF_STR* p_path, u32 no_look_last_token);
s32 VFiPFENT_ITER_GetEntryOfPattern(PF_ENT_ITER* p_iter, PF_DIR_ENT* p_ent, PF_VOLUME* p_vol, PF_STR* p_path);

#ifdef __cplusplus
}
#endif
#endif
