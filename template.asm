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


/**
8-bit unsiged division from:
http://6502org.wikidot.com/software-math-intdiv
Divides number in tmp by number in tmp+1.
Result in tmp, remainder in A
 */
_udiv8: {
    lda #0
    ldx #8
    asl tmp
L1: 
    rol
    cmp tmp+1
    bcc L2
    sbc tmp+1
L2:
    rol tmp
    dex
    bne L1

    rts
}

/**
8-bit signed division.
Divides number in tmp by number in tmp+1.
Result in tmp, remainder in A
*/
_div8: {
    .label sgn = tmp+6

    stz sgn           // if result is pos, sgn is 0

    bit tmp
    bpl tmp_pos
    
    // negate tmp
    lda #0
    sec
    sbc tmp
    sta tmp

    bit tmp+1
    bmi negate_divisor
    dec sgn           // tmp neg, tmp+1 pos
    jmp divide

tmp_pos:
    bit tmp+1
    bpl divide
    dec sgn           // tmp pos, tmp+1 neg

negate_divisor: 
    lda #0
    sec
    sbc tmp+1
    sta tmp+1

divide:
    jsr _udiv8

    bit sgn
    bpl done            // if result should be positive, done!

    // Negate tmp
    pha
    lda #0
    sec
    sbc tmp
    sta tmp
    pla

done:
    rts
}


/**
16-bit unsigned division from:
https://codebase64.org/doku.php?id=base:16bit_division_16-bit_result
Divides number in tmp,tmp+1 by number in tmp+2,tmp+3.
Stores result in tmp,tmp+1 and remainder in tmp+4,tmp+5
*/
_udiv16: {
    .label dividend = tmp
    .label divisor = tmp+2
    .label remainder = tmp+4
    .label result = dividend

    lda #0	                // preset remainder to 0
	sta remainder
	sta remainder+1
	ldx #16	                // repeat for each bit: ...

divloop:
    asl dividend	        // dividend lb & hb*2, msb -> Carry
	rol dividend+1	
	rol remainder	        // remainder lb & hb * 2 + msb from carry
	rol remainder+1
	lda remainder
	sec
	sbc divisor	            // substract divisor to see if it fits in
	tay	                    // lb result -> Y, for we may need it later
	lda remainder+1
	sbc divisor+1
	bcc skip	            // if carry=0 then divisor didn't fit in yet

	sta remainder+1	        // else save substraction result as new remainder,
	sty remainder	
	inc result	            // and INCrement result cause divisor fit in 1 times

skip:	
    dex
	bne divloop	
	rts
}


/**
16-bit signed division.
Divides number in tmp,tmp+1 by number in tmp+2,tmp+3.
Stores result in tmp,tmp+1 and remainder in tmp+4,tmp+5
*/
_div16: {
    .label sgn = tmp+6

    stz sgn            // if result is pos, sgn is 0
    bit tmp+1
    bpl dividend_pos
    
    // negate dividend
    lda tmp
    eor #$ff
    sta tmp
    lda tmp+1
    eor #$ff
    sta tmp+1
    inc2 tmp

    bit tmp+3
    bmi negate_divisor
    dec sgn             // tmp neg, tmp+1 pos
    jmp divide

dividend_pos:
    bit tmp+3
    bpl divide
    dec sgn            // tmp pos, tmp+1 neg

negate_divisor: 
    lda tmp+2
    eor #$ff
    sta tmp+2
    lda tmp+3
    eor #$ff
    sta tmp+3
    inc2 tmp+2

divide:
    jsr _udiv16

    bit sgn
    bpl done            // if result should be positive, done!

    // Negate tmp
    lda tmp 
    eor #$ff
    sta tmp
    lda tmp+1
    eor #$ff
    sta tmp+1
    inc2 tmp

done:
    rts
}

// -------- END TEMPLATE ---------

