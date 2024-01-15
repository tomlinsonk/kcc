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

/* ------------------------------------------------
Create an emulator breakpoint (for debugging)
*/
.pseudocommand @bpt {
    .byte $bb
}


.label sp = $00
// $01
.label fp = $02
// $03
.label ret = $04
// $05

.label tmp = $10
// $10-$1f

.const stack_top = $5fff

.segment Stack [virtual, start=$5000]
	.fill $FFF, 0

.segment Code [outPrg="out.prg", start=$1000, min=$1000, max=$4fff]	

_enter:    
    lda #>stack_top
    sta sp+1
    sta fp+1
    lda #<stack_top
    sta sp
    sta fp

    lda fp+1
    jsr _push
    lda fp
    jsr _push

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

/**
8-bit signed multiplication from:
https://codebase64.org/doku.php?id=base:8bit_multiplication_8bit_product
Multiplies number tmp+1 by number in tmp+2.
Result in A 
*/
_mul8: {
    .label num1 = tmp
    .label num2 = tmp+1

    lda #0
    jmp enterLoop

doAdd:
    clc
    adc num1

loop:
    asl num1

enterLoop:
    lsr num2
    bcs doAdd
    bne loop

    rts
}

/**
16-bit signed multiplication from
https://github.com/TobyLobster/multiply_test/blob/main/tests/smult9.a
which was based on Dr Jefyll's post:
http://forum.6502.org/viewtopic.php?f=9&t=689&start=0#p19958
Multiplies number in tmp,tmp+1 by number in tmp+2,tmp+3,
stores result in (lsb) tmp+4,tmp+5,tmp+6,tmp+7 (msb)
*/
_mul16: {
    .label multiplicand = tmp
    .label multiplier = tmp+2
    .label result = tmp+4

    // Step 1: unsigned multiply
    // copy multiplier into result (multiplier preserved for sign calculation later)
    lda multiplier
    sta result
    lda multiplier+1
    sta result+1

    lda #0              //
    sta result+2        // 16 bits of zero in A, result+2
                        // (think of A as a local cache of result+3)
                        //  Note:    First 8 shifts are  A -> result+2 -> result
                        //           Final 8 shifts are  A -> result+2 -> result+1
    ldx #$fe            // count for outer loop. Loops twice.

    // outer loop (2 times)
outer_loop:
    ldy #8              // count for inner loop
    lsr result+2,x      // think "result" then later "result+1"

    // inner loop (8 times)
inner_loop:
    bcc !+
    sta result+3        // remember A
    lda result+2
    clc
    adc multiplicand
    sta result+2
    lda result+3        // recall A
    adc multiplicand+1

!:
    ror                 // shift
    ror result+2
    ror result+2,x      // think "result" then later "result+1"
    dey
    bne inner_loop      // go back for 1 more shift?

    inx
    bne outer_loop      // go back for 8 more shifts?

    sta result+3        // ms byte of hi-word of result

    // Step 2: apply sign (See C=Hacking16 for details).
    bit multiplier+1
    bpl !+              // skip if multiplier is positive
    sec
    lda result+2
    sbc multiplicand
    sta result+2
    lda result+3
    sbc multiplicand+1
    sta result+3
!:
    bit multiplicand+1
    bpl !+              // skip if multiplicand is positive
    sec
    lda result+2
    sbc multiplier
    sta result+2
    lda result+3
    sbc multiplier+1
    sta result+3
!:
    rts
}

// -------- END TEMPLATE ---------

