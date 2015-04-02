.arm
.align 4
.code 32
.text

.global arm9_start
@ insert your funky stuff here
@ this code will not be executed if "/arm9payload.bin" is present
arm9_start:
	b skipvars

	@ offs 4, to be filled during runtime
	pa_arm9_entrypoint_backup: .long 0
	
skipvars:
	stmfd sp!, {r0-r12,lr}

	@ delay execution. just for "fun"
	mov r1, #0x10
outer:
	
	mov r0, #0
	add r0, r0, #0xFFFFFFFF
inner:
	subs r0, r0, #1
	nop
	bgt inner
	subs r1, r1, #1
	bgt outer

	ldmfd sp!, {r0-r12,lr}
	
	ldr pc, pa_arm9_entrypoint_backup
	
	
.global arm9_end
arm9_end:
