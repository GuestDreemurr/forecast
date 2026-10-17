# Runtime.PPCEABI.H/ptmf.s
#
# The pointer-to-member-function helpers. All four routines read the registers
# the caller passes (r12 holds the ptmf for the scall thunks) and are assembly
# in the original library; the compiler emits calls to __ptmf_test/__ptmf_cmpr
# and jumps to __ptmf_scall/__ptmf_scall4 for pointer-to-member calls.

.include "macros.inc"
.file "ptmf.s"

# 0x801617E0..0x8016189C | size: 0xBC
.text
.balign 4

# 0x801617E0 size:0x30
.fn __ptmf_test, global
	lwz r5, 0x0(r3)
	lwz r6, 0x4(r3)
	lwz r7, 0x8(r3)
	li r3, 0x1
	cmpwi r5, 0x0
	cmpwi cr6, r6, 0x0
	cmpwi cr7, r7, 0x0
	bnelr
	bnelr cr6
	bnelr cr7
	li r3, 0x0
	blr
.endfn __ptmf_test

# 0x80161810 size:0x3C
.fn __ptmf_cmpr, global
	lwz r5, 0x0(r3)
	lwz r6, 0x0(r4)
	lwz r7, 0x4(r3)
	lwz r8, 0x4(r4)
	lwz r9, 0x8(r3)
	lwz r10, 0x8(r4)
	li r3, 0x1
	cmpw r5, r6
	cmpw cr6, r7, r8
	cmpw cr7, r9, r10
	bnelr
	bnelr cr6
	bnelr cr7
	li r3, 0x0
	blr
.endfn __ptmf_cmpr

# 0x8016184C size:0x28
.fn __ptmf_scall, global
	lwz r0, 0x0(r12)
	lwz r11, 0x4(r12)
	lwz r12, 0x8(r12)
	add r3, r3, r0
	cmpwi r11, 0x0
	blt .L_8016186C
	lwzx r12, r3, r12
	lwzx r12, r12, r11
.L_8016186C:
	mtctr r12
	bctr
.endfn __ptmf_scall

# 0x80161874 size:0x28
.fn __ptmf_scall4, global
	lwz r0, 0x0(r12)
	lwz r11, 0x4(r12)
	lwz r12, 0x8(r12)
	add r4, r4, r0
	cmpwi r11, 0x0
	blt .L_80161894
	lwzx r12, r4, r12
	lwzx r12, r12, r11
.L_80161894:
	mtctr r12
	bctr
.endfn __ptmf_scall4

# 0x801979A0..0x801979AC | size: 0xC
.rodata
.balign 8

.obj __ptmf_null, global
	.4byte 0x00000000
	.4byte 0x00000000
	.4byte 0x00000000
.endobj __ptmf_null
