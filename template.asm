.cpu _65c02
.encoding "ascii"

.label sp = $00
.const stack_top = $5fff

.segment Stack [virtual, start=$5000]
	.fill $FFF, 0

.segment Code [outPrg="out.prg", start=$1000, min=$1000, max=$4fff]	

_enter_main:
	lda #(<stack_top)
	sta sp
	lda #(>stack_top)
	sta sp+1

	lda #(>(_done-1))
	jsr _push
	lda #(<(_done-1))
	jsr _push

	jmp main

_done:
	brk 
	nop
	jmp _enter_main

_push: {
	sta (sp)	
	lda sp
	bne no_carry
	dec sp+1
no_carry:	
	dec sp
	rts
}

_pop: {
	lda (sp)
	inc sp
	bne no_carry
	inc sp+1
no_carry:
	rts
}

