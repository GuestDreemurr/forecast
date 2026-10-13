#include <revolution/VF.h>

typedef struct PF_UNK {
    u32 unk0;
    u32 unk4;
    PF_VOLUME *unk8;
    u32 unkC;
    u8 unk10[0x14 - 0x10];
    u32 unk14;
    u8 unk18[0x1C - 0x18];
    u32 unk1C;
    u8 unk20[0x444 - 0x20];
    u32 unk444;
} PF_UNK;

void fn_8005BC44(u32 *unk, u32 *unk2, PF_STR* filename);


s32 VFiPFDIR_p_mkdir(PF_VOLUME* p_vol, PF_STR* p_path, PF_UNK *p_unk) {
    u32 cluster;
    PF_DIR_ENT ent;     // 0x560
    PF_DIR_ENT ent2;    // 0x320
    s8 dest[0x200];     // 0x120
    PF_ENT_ITER iter;   // 0xB0
    PF_FFD ffd;         // 0x78
    PF_STR dir_path;    // 0x48
    PF_STR filename;    // 0x38
    PF_FAT_HINT hint;   // 0x28
    u32 is_found;       // 0x18
    s32 err;
    u16 cnt;
    u32 code_mode;
    u32 create_long;

    err = VFiPFPATH_SplitPath(p_path, &dir_path, &filename);
    if (err != 0) {
        return err;
    }

    err = VFiPFENT_ITER_GetEntryOfPath(&iter, &ent, p_vol, &dir_path, 1);
    if (err != 0) {
        return err;
    }

    if ((ent.attr & 0x10) == 0) {
        return 0x14;
    }

    cnt = VFiPFSTR_StrNumChar(&filename, 1);
    if ((u16)cnt > 0xFF) {
        return 1;
    }

    if (ent.path_len > 259) {
        return 2;
    }

    if (VFiPFSTR_GetCodeMode(&filename) == 2) {
        s8 *pos = VFiPFSTR_GetStrPos(&filename, 1);
        VFiPFPATH_transformFromUnicodeToNormal(dest, (u16 *)pos);
    }
    VFiPFSTR_SetLocalStr(&filename, dest);

    if (p_unk) {
        p_unk->unk0 = 0;
        p_unk->unk4 = 0;
        p_unk->unk8 = ent.p_vol;
        p_unk->unkC = ent.start_cluster;
        if (VFiPFSTR_GetCodeMode(&filename) == 1) {
            fn_8005BC44(&p_unk->unk1C, NULL, &filename);
            p_unk->unk14 += 1;
        } else {
            fn_8005BC44(&p_unk->unk1C, &p_unk->unk444, &filename);
            p_unk->unk14 += 2;
        }
    }

    VFiPFFAT_InitFFD(&ffd, &hint, p_vol, &cluster);

    iter.ffd = ffd;

    VFiPFFAT_ResetFFD(&iter.ffd, &cluster);
    err = VFiPFENT_ITER_IteratorInitialize(&iter, 0);
    if (err != 0) {
        return err;
    }

    VFipf_memcpy(&ent2, &ent, 0x240);
    err = VFiPFENT_ITER_FindEntry(&iter, &ent2, &filename, 0x10, 0, &is_found, 1);
    if (err != 0) {
        return err;
    }

    if (is_found) {
        return 8;
    }

    create_long = VFiPFPATH_parseShortName(ent2.short_name, &filename);
    if (create_long != 0 && ent2.short_name[0] == 0) {
        return 1;
    }
    if (create_long != 0) {
        err = VFiPFENT_AdjustSFN(&ent, ent2.short_name);
        if (err != 0) {
            return err;
        }
        if (VFiPFSTR_GetCodeMode(&filename) == 1) {
            cnt = VFiPFPATH_transformInUnicode(ent.long_name, VFiPFSTR_GetStrPos(&filename, 1));
        } else {
            s8 *pos = VFiPFSTR_GetStrPos(&filename, 1);
            VFipf_w_strcpy(ent2.long_name, (u16 *)pos);
        }
    } else {
        ent2.long_name[0] = 0;
    }

    ent.start_cluster = 0;
    ent.file_size = 0;
    ent.p_vol = p_vol;
    ent.small_letter_flag = 0;
    ent.attr = 0x10;

    VFiPFENT_getcurrentDateTimeForEnt(&ent2.create_date, &ent2.create_time);

    return 0;
}

void VFiPFDIR_p_fsnext(u32 *unk, u32 *unk2, PF_STR* filename) {

}

void VFiPFDIR_p_fsfirst() {

}

static void VFiPFDIR_FinalizeSDD(struct PF_SDD* p_sdd) {
    p_sdd->stat = 0;
    VFiPFFAT_FinalizeFFD(&p_sdd->ffd);
}

static void VFiPFDIR_FinalizeUDD(struct PF_DIR* p_dir) {
    p_dir->stat &= ~1u;
}

void VFiPFDIR_FinalizeAllDirs(struct PF_VOLUME* p_vol) {
    u16 i;

    for (i = 0; i < 3u; ++i) {
        VFiPFDIR_FinalizeSDD(&p_vol->sdds[i]);
    }
    for (i = 0; i < 3u; ++i) {
        VFiPFDIR_FinalizeUDD(&p_vol->udds[i]);
    }
    p_vol->num_opened_directories = 0;
}
