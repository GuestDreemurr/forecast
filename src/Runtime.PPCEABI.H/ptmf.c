// Pointer-to-member-function helpers. A pointer to member function is
// {this adjustment, vtable offset (-1 for a non-virtual function), function address or vtable entry offset}

typedef struct __ptmf {
    long this_delta;
    long v_offset;
    union {
        void* f_addr;
        long ve_offset;
    } f_data;
} __ptmf;

const __ptmf __ptmf_null = {0, 0, 0};

#ifdef __cplusplus
extern "C" {
#endif

// Returns whether the pointer is not null
asm long __ptmf_test(register __ptmf* ptmf) {
    nofralloc
    lwz r5, 0x0(r3)
    lwz r6, 0x4(r3)
    lwz r7, 0x8(r3)
    li r3, 1
    cmpwi r5, 0
    cmpwi cr6, r6, 0
    cmpwi cr7, r7, 0
    bnelr
    bnelr cr6
    bnelr cr7
    li r3, 0
    blr
}

// Returns whether the two pointers differ
asm long __ptmf_cmpr(register __ptmf* ptmf1, register __ptmf* ptmf2) {
    nofralloc
    lwz r5, 0x0(r3)
    lwz r6, 0x0(r4)
    lwz r7, 0x4(r3)
    lwz r8, 0x4(r4)
    lwz r9, 0x8(r3)
    lwz r10, 0x8(r4)
    li r3, 1
    cmpw r5, r6
    cmpw cr6, r7, r8
    cmpw cr7, r9, r10
    bnelr
    bnelr cr6
    bnelr cr7
    li r3, 0
    blr
}

// Calls through the pointer in r12 with the object in r3
asm void __ptmf_scall(...) {
    nofralloc
    lwz r0, 0x0(r12)
    lwz r11, 0x4(r12)
    lwz r12, 0x8(r12)
    add r3, r3, r0
    cmpwi r11, 0
    blt nonvirtual
    lwzx r12, r3, r12
    lwzx r12, r12, r11
nonvirtual:
    mtctr r12
    bctr
}

// Same, with the object in r4 (r3 holds a hidden struct return pointer)
asm void __ptmf_scall4(...) {
    nofralloc
    lwz r0, 0x0(r12)
    lwz r11, 0x4(r12)
    lwz r12, 0x8(r12)
    add r4, r4, r0
    cmpwi r11, 0
    blt nonvirtual
    lwzx r12, r4, r12
    lwzx r12, r12, r11
nonvirtual:
    mtctr r12
    bctr
}

#ifdef __cplusplus
}
#endif
