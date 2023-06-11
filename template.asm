.cpu _65c02
.encoding "ascii"

/* ------------------------------------------------
Get the high byte for a two byte argument
*/
.function _2byteNextArgument(arg) {
    .if (arg.getType() == AT_IMMEDIATE) 
        .return CmdArgument(arg.getType(), >arg.getValue())
    .return CmdArgument(arg.getType(), arg.getValue() + 1)
}

/* ------------------------------------------------
Increment a two-byte number stored at arg (little endian)
*/
.pseudocommand @inc2 arg {
    inc arg
    bne done
    inc _2byteNextArgument(arg)
done:
}

/* ------------------------------------------------
Decrement a two-byte number stored at arg (little endian)
*/
.pseudocommand @dec2 arg {
    lda arg
    bne no_carry
    dec _2byteNextArgument(arg)
no_carry:
    dec arg
}

/* ------------------------------------------------
Move one byte from address source to address target
*/
.pseudocommand @mov source:target {
    lda source
    sta target
}

/* ------------------------------------------------
Move the two bytes at source and source+1 to target
*/
.pseudocommand @mov2 source:target {
    lda source
    sta target
    lda _2byteNextArgument(source)
    sta _2byteNextArgument(target)
}

/* ------------------------------------------------
Add two byte number stored at arg1 to two byte number stored at arg2
Store the result in target, if specified
If no target is given, store the result in arg1 
*/
.pseudocommand @add2 arg1 : arg2 : target {
    .if (target.getType()==AT_NONE) 
    	.eval target = arg1
    clc
    lda arg1
    adc arg2
    sta target
    lda _2byteNextArgument(arg1)
    adc _2byteNextArgument(arg2)
    sta _2byteNextArgument(target)
}

.label sp = $00
.const stack_top = $5fff

.segment Stack [virtual, start=$5000]
	.fill $FFF, 0

.segment Code [outPrg="out.prg", start=$1000, min=$1000, max=$4fff]	

_enter:
    mov2 #stack_top : sp

	lda #(<(_exit-1))
	jsr _push
	lda #(>(_exit-1))
	jsr _push

	jmp main

_exit:
    brk 
    nop

	jmp _enter

_push:
	sta (sp)	
	dec2 sp
	rts

_pop:
	inc2 sp
	lda (sp)
	rts
