# Runtime.PPCEABI.H/runtime.s
#
# The CodeWarrior runtime primitives. Everything in this unit is assembly in
# the original library, because the compiler compiles the corresponding C
# operators into calls to these functions:
#
#   __div2u, __div2i, __mod2u, __mod2i   unsigned/signed 64-bit / and %
#   __shl2i                              unsigned 64-bit <<
#   __cvt_fp2unsigned                    unsigned long <- double
#   __cvt_sll_dbl, __cvt_dbl_ull         double <-> 64-bit integer
#
# __save_fpr .. __restore_gpr are the FPR/GPR save and restore ranges the
# exception unwinder enters in the middle of, at the entry point of the first
# register it needs (r11 points at the end of the register save area).

.include "macros.inc"
.file "runtime.s"

.text
.balign 4


# Defines the entry point _<name>_<reg> and saves/restores one register.
# r11 is the pointer the unwinder passes to these ranges.
.macro save_fpr reg, offset
.sym _savefpr_\reg, global
	stfd f\reg, \offset(r11)
.endm

.macro restore_fpr reg, offset
.sym _restfpr_\reg, global
	lfd f\reg, \offset(r11)
.endm

.macro save_gpr reg, offset
.sym _savegpr_\reg, global
	stw r\reg, \offset(r11)
.endm

.macro restore_gpr reg, offset
.sym _restgpr_\reg, global
	lwz r\reg, \offset(r11)
.endm

# 0x8016189C size:0x5C  unsigned long <- double, saturating
.fn __cvt_fp2unsigned, global
	stwu r1, -0x10(r1)
	lis r4, __constants@ha
	addi r4, r4, __constants@l
	li r3, 0x0
	lfd f0, 0x0(r4)
	lfd f3, 0x8(r4)
	lfd f4, 0x10(r4)
	fcmpu cr0, f1, f0
	fcmpu cr6, f1, f3
	blt .L_801618F0
	subi r3, r3, 0x1
	bge cr6, .L_801618F0
	fcmpu cr7, f1, f4
	fmr f2, f1
	blt cr7, .L_801618DC
	fsub f2, f1, f4
.L_801618DC:
	fctiwz f2, f2
	stfd f2, 0x8(r1)
	lwz r3, 0xc(r1)
	blt cr7, .L_801618F0
	addis r3, r3, 0x8000
.L_801618F0:
	addi r1, r1, 0x10
	blr
.endfn __cvt_fp2unsigned


# 0x801618F8 size:0x4C  save f14..f31
.fn __save_fpr, global
	save_fpr 14, -0x90
	save_fpr 15, -0x88
	save_fpr 16, -0x80
	save_fpr 17, -0x78
	save_fpr 18, -0x70
	save_fpr 19, -0x68
	save_fpr 20, -0x60
	save_fpr 21, -0x58
	save_fpr 22, -0x50
	save_fpr 23, -0x48
	save_fpr 24, -0x40
	save_fpr 25, -0x38
	save_fpr 26, -0x30
	save_fpr 27, -0x28
	save_fpr 28, -0x20
	save_fpr 29, -0x18
	save_fpr 30, -0x10
	save_fpr 31, -0x8
	blr
.endfn __save_fpr


# 0x80161944 size:0x4C  restore f14..f31
.fn __restore_fpr, global
	restore_fpr 14, -0x90
	restore_fpr 15, -0x88
	restore_fpr 16, -0x80
	restore_fpr 17, -0x78
	restore_fpr 18, -0x70
	restore_fpr 19, -0x68
	restore_fpr 20, -0x60
	restore_fpr 21, -0x58
	restore_fpr 22, -0x50
	restore_fpr 23, -0x48
	restore_fpr 24, -0x40
	restore_fpr 25, -0x38
	restore_fpr 26, -0x30
	restore_fpr 27, -0x28
	restore_fpr 28, -0x20
	restore_fpr 29, -0x18
	restore_fpr 30, -0x10
	restore_fpr 31, -0x8
	blr
.endfn __restore_fpr


# 0x80161990 size:0x4C  save r14..r31
.fn __save_gpr, global
	save_gpr 14, -0x48
	save_gpr 15, -0x44
	save_gpr 16, -0x40
	save_gpr 17, -0x3c
	save_gpr 18, -0x38
	save_gpr 19, -0x34
	save_gpr 20, -0x30
	save_gpr 21, -0x2c
	save_gpr 22, -0x28
	save_gpr 23, -0x24
	save_gpr 24, -0x20
	save_gpr 25, -0x1c
	save_gpr 26, -0x18
	save_gpr 27, -0x14
	save_gpr 28, -0x10
	save_gpr 29, -0xc
	save_gpr 30, -0x8
	save_gpr 31, -0x4
	blr
.endfn __save_gpr


# 0x801619DC size:0x4C  restore r14..r31
.fn __restore_gpr, global
	restore_gpr 14, -0x48
	restore_gpr 15, -0x44
	restore_gpr 16, -0x40
	restore_gpr 17, -0x3c
	restore_gpr 18, -0x38
	restore_gpr 19, -0x34
	restore_gpr 20, -0x30
	restore_gpr 21, -0x2c
	restore_gpr 22, -0x28
	restore_gpr 23, -0x24
	restore_gpr 24, -0x20
	restore_gpr 25, -0x1c
	restore_gpr 26, -0x18
	restore_gpr 27, -0x14
	restore_gpr 28, -0x10
	restore_gpr 29, -0xc
	restore_gpr 30, -0x8
	restore_gpr 31, -0x4
	blr
.endfn __restore_gpr


# 0x80161A28 size:0xEC  unsigned 64-bit /
.fn __div2u, global
	cmpwi r3, 0x0
	cntlzw r0, r3
	cntlzw r9, r4
	bne .L_80161A3C
	addi r0, r9, 0x20
.L_80161A3C:
	cmpwi r5, 0x0
	cntlzw r9, r5
	cntlzw r10, r6
	bne .L_80161A50
	addi r9, r10, 0x20
.L_80161A50:
	cmpw r0, r9
	subfic r10, r0, 0x40
	bgt .L_80161B08
	addi r9, r9, 0x1
	subfic r9, r9, 0x40
	add r0, r0, r9
	subf r9, r9, r10
	mtctr r9
	cmpwi r9, 0x20
	subi r7, r9, 0x20
	blt .L_80161A88
	srw r8, r3, r7
	li r7, 0x0
	b .L_80161A9C
.L_80161A88:
	srw r8, r4, r9
	subfic r7, r9, 0x20
	slw r7, r3, r7
	or r8, r8, r7
	srw r7, r3, r9
.L_80161A9C:
	cmpwi r0, 0x20
	subic r9, r0, 0x20
	blt .L_80161AB4
	slw r3, r4, r9
	li r4, 0x0
	b .L_80161AC8
.L_80161AB4:
	slw r3, r3, r0
	subfic r9, r0, 0x20
	srw r9, r4, r9
	or r3, r3, r9
	slw r4, r4, r0
.L_80161AC8:
	li r10, -0x1
	addic r7, r7, 0x0
.L_80161AD0:
	adde r4, r4, r4
	adde r3, r3, r3
	adde r8, r8, r8
	adde r7, r7, r7
	subfc r0, r6, r8
	subfe. r9, r5, r7
	blt .L_80161AF8
	mr r8, r0
	mr r7, r9
	addic r0, r10, 0x1
.L_80161AF8:
	bdnz .L_80161AD0
	adde r4, r4, r4
	adde r3, r3, r3
	blr
.L_80161B08:
	li r4, 0x0
	li r3, 0x0
	blr
.endfn __div2u


# 0x80161B14 size:0x138 signed 64-bit /
.fn __div2i, global
	stwu r1, -0x10(r1)
	clrrwi. r9, r3, 31
	beq .L_80161B28
	subfic r4, r4, 0x0
	subfze r3, r3
.L_80161B28:
	stw r9, 0x8(r1)
	clrrwi. r10, r5, 31
	beq .L_80161B3C
	subfic r6, r6, 0x0
	subfze r5, r5
.L_80161B3C:
	stw r10, 0xc(r1)
	cmpwi r3, 0x0
	cntlzw r0, r3
	cntlzw r9, r4
	bne .L_80161B54
	addi r0, r9, 0x20
.L_80161B54:
	cmpwi r5, 0x0
	cntlzw r9, r5
	cntlzw r10, r6
	bne .L_80161B68
	addi r9, r10, 0x20
.L_80161B68:
	cmpw r0, r9
	subfic r10, r0, 0x40
	bgt .L_80161C3C
	addi r9, r9, 0x1
	subfic r9, r9, 0x40
	add r0, r0, r9
	subf r9, r9, r10
	mtctr r9
	cmpwi r9, 0x20
	subi r7, r9, 0x20
	blt .L_80161BA0
	srw r8, r3, r7
	li r7, 0x0
	b .L_80161BB4
.L_80161BA0:
	srw r8, r4, r9
	subfic r7, r9, 0x20
	slw r7, r3, r7
	or r8, r8, r7
	srw r7, r3, r9
.L_80161BB4:
	cmpwi r0, 0x20
	subic r9, r0, 0x20
	blt .L_80161BCC
	slw r3, r4, r9
	li r4, 0x0
	b .L_80161BE0
.L_80161BCC:
	slw r3, r3, r0
	subfic r9, r0, 0x20
	srw r9, r4, r9
	or r3, r3, r9
	slw r4, r4, r0
.L_80161BE0:
	li r10, -0x1
	addic r7, r7, 0x0
.L_80161BE8:
	adde r4, r4, r4
	adde r3, r3, r3
	adde r8, r8, r8
	adde r7, r7, r7
	subfc r0, r6, r8
	subfe. r9, r5, r7
	blt .L_80161C10
	mr r8, r0
	mr r7, r9
	addic r0, r10, 0x1
.L_80161C10:
	bdnz .L_80161BE8
	adde r4, r4, r4
	adde r3, r3, r3
	lwz r9, 0x8(r1)
	lwz r10, 0xc(r1)
	xor. r7, r9, r10
	beq .L_80161C38
	cmpwi r9, 0x0
	subfic r4, r4, 0x0
	subfze r3, r3
.L_80161C38:
	b .L_80161C44
.L_80161C3C:
	li r4, 0x0
	li r3, 0x0
.L_80161C44:
	addi r1, r1, 0x10
	blr
.endfn __div2i


# 0x80161C4C size:0xE4  unsigned 64-bit %
.fn __mod2u, global
	cmpwi r3, 0x0
	cntlzw r0, r3
	cntlzw r9, r4
	bne .L_80161C60
	addi r0, r9, 0x20
.L_80161C60:
	cmpwi r5, 0x0
	cntlzw r9, r5
	cntlzw r10, r6
	bne .L_80161C74
	addi r9, r10, 0x20
.L_80161C74:
	cmpw r0, r9
	subfic r10, r0, 0x40
	bgt .L_80161D2C
	addi r9, r9, 0x1
	subfic r9, r9, 0x40
	add r0, r0, r9
	subf r9, r9, r10
	mtctr r9
	cmpwi r9, 0x20
	subi r7, r9, 0x20
	blt .L_80161CAC
	srw r8, r3, r7
	li r7, 0x0
	b .L_80161CC0
.L_80161CAC:
	srw r8, r4, r9
	subfic r7, r9, 0x20
	slw r7, r3, r7
	or r8, r8, r7
	srw r7, r3, r9
.L_80161CC0:
	cmpwi r0, 0x20
	subic r9, r0, 0x20
	blt .L_80161CD8
	slw r3, r4, r9
	li r4, 0x0
	b .L_80161CEC
.L_80161CD8:
	slw r3, r3, r0
	subfic r9, r0, 0x20
	srw r9, r4, r9
	or r3, r3, r9
	slw r4, r4, r0
.L_80161CEC:
	li r10, -0x1
	addic r7, r7, 0x0
.L_80161CF4:
	adde r4, r4, r4
	adde r3, r3, r3
	adde r8, r8, r8
	adde r7, r7, r7
	subfc r0, r6, r8
	subfe. r9, r5, r7
	blt .L_80161D1C
	mr r8, r0
	mr r7, r9
	addic r0, r10, 0x1
.L_80161D1C:
	bdnz .L_80161CF4
	mr r4, r8
	mr r3, r7
	blr
.L_80161D2C:
	blr
.endfn __mod2u


# 0x80161D30 size:0x10C signed 64-bit %
.fn __mod2i, global
	cmpwi cr7, r3, 0x0
	bge cr7, .L_80161D40
	subfic r4, r4, 0x0
	subfze r3, r3
.L_80161D40:
	cmpwi r5, 0x0
	bge .L_80161D50
	subfic r6, r6, 0x0
	subfze r5, r5
.L_80161D50:
	cmpwi r3, 0x0
	cntlzw r0, r3
	cntlzw r9, r4
	bne .L_80161D64
	addi r0, r9, 0x20
.L_80161D64:
	cmpwi r5, 0x0
	cntlzw r9, r5
	cntlzw r10, r6
	bne .L_80161D78
	addi r9, r10, 0x20
.L_80161D78:
	cmpw r0, r9
	subfic r10, r0, 0x40
	bgt .L_80161E2C
	addi r9, r9, 0x1
	subfic r9, r9, 0x40
	add r0, r0, r9
	subf r9, r9, r10
	mtctr r9
	cmpwi r9, 0x20
	subi r7, r9, 0x20
	blt .L_80161DB0
	srw r8, r3, r7
	li r7, 0x0
	b .L_80161DC4
.L_80161DB0:
	srw r8, r4, r9
	subfic r7, r9, 0x20
	slw r7, r3, r7
	or r8, r8, r7
	srw r7, r3, r9
.L_80161DC4:
	cmpwi r0, 0x20
	subic r9, r0, 0x20
	blt .L_80161DDC
	slw r3, r4, r9
	li r4, 0x0
	b .L_80161DF0
.L_80161DDC:
	slw r3, r3, r0
	subfic r9, r0, 0x20
	srw r9, r4, r9
	or r3, r3, r9
	slw r4, r4, r0
.L_80161DF0:
	li r10, -0x1
	addic r7, r7, 0x0
.L_80161DF8:
	adde r4, r4, r4
	adde r3, r3, r3
	adde r8, r8, r8
	adde r7, r7, r7
	subfc r0, r6, r8
	subfe. r9, r5, r7
	blt .L_80161E20
	mr r8, r0
	mr r7, r9
	addic r0, r10, 0x1
.L_80161E20:
	bdnz .L_80161DF8
	mr r4, r8
	mr r3, r7
.L_80161E2C:
	bge cr7, .L_80161E38
	subfic r4, r4, 0x0
	subfze r3, r3
.L_80161E38:
	blr
.endfn __mod2i


# 0x80161E3C size:0x24  unsigned 64-bit <<
.fn __shl2i, global
	subfic r8, r5, 0x20
	subic r9, r5, 0x20
	slw r3, r3, r5
	srw r10, r4, r8
	or r3, r3, r10
	slw r10, r4, r9
	or r3, r3, r10
	slw r4, r4, r5
	blr
.endfn __shl2i


# 0x80161E60 size:0xB0  double <- signed 64-bit
.fn __cvt_sll_dbl, global
	stwu r1, -0x10(r1)
	clrrwi. r5, r3, 31
	beq .L_80161E74
	subfic r4, r4, 0x0
	subfze r3, r3
.L_80161E74:
	or. r7, r3, r4
	li r6, 0x0
	beq .L_80161EFC
	cntlzw r7, r3
	cntlzw r8, r4
	extlwi r9, r7, 5, 26
	srawi r9, r9, 31
	and r9, r9, r8
	add r7, r7, r9
	subfic r8, r7, 0x20
	subic r9, r7, 0x20
	slw r3, r3, r7
	srw r10, r4, r8
	or r3, r3, r10
	slw r10, r4, r9
	or r3, r3, r10
	slw r4, r4, r7
	subf r6, r7, r6
	clrlwi r7, r4, 21
	cmpwi r7, 0x400
	addi r6, r6, 0x43e
	blt .L_80161EE4
	bgt .L_80161ED8
	rlwinm. r7, r4, 0, 20, 20
	beq .L_80161EE4
.L_80161ED8:
	addic r4, r4, 0x800
	addze r3, r3
	addze r6, r6
.L_80161EE4:
	rotrwi r4, r4, 11
	rlwimi r4, r3, 21, 0, 10
	extrwi r3, r3, 20, 1
	slwi r6, r6, 20
	or r3, r6, r3
	or r3, r5, r3
.L_80161EFC:
	stw r3, 0x8(r1)
	stw r4, 0xc(r1)
	lfd f1, 0x8(r1)
	addi r1, r1, 0x10
	blr
.endfn __cvt_sll_dbl


# 0x80161F10 size:0xA8  unsigned 64-bit <- double
.fn __cvt_dbl_ull, global
	stwu r1, -0x10(r1)
	stfd f1, 0x8(r1)
	lwz r3, 0x8(r1)
	lwz r4, 0xc(r1)
	extrwi r5, r3, 11, 1
	cmplwi r5, 0x3ff
	bge .L_80161F38
.L_80161F2C:
	li r3, 0x0
	li r4, 0x0
	b .L_80161FB0
.L_80161F38:
	clrrwi. r6, r3, 31
	bne .L_80161F2C
	clrlwi r3, r3, 12
	oris r3, r3, 0x10
	subi r5, r5, 0x433
	cmpwi r5, 0x0
	bge .L_80161F7C
	neg r5, r5
	subfic r8, r5, 0x20
	subic r9, r5, 0x20
	srw r4, r4, r5
	slw r10, r3, r8
	or r4, r4, r10
	srw r10, r3, r9
	or r4, r4, r10
	srw r3, r3, r5
	b .L_80161FB0
.L_80161F7C:
	cmpwi r5, 0xb
	ble+ .L_80161F90
	li r3, -0x1
	li r4, -0x1
	b .L_80161FB0
.L_80161F90:
	subfic r8, r5, 0x20
	subic r9, r5, 0x20
	slw r3, r3, r5
	srw r10, r4, r8
	or r3, r3, r10
	slw r10, r4, r9
	or r3, r3, r10
	slw r4, r4, r5
.L_80161FB0:
	addi r1, r1, 0x10
	blr
.endfn __cvt_dbl_ull


# 0x801979B0 size:0x18
# The saturating bounds used by __cvt_fp2unsigned.
.rodata
.balign 8

.obj __constants, local
	.double 0
	.double 4294967296
	.double 2147483648
.endobj __constants
