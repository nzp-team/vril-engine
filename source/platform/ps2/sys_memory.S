.set noreorder
.text
.align 4
.globl memcpy
.ent memcpy
memcpy:
    move    $2,$4
    sltiu   $3,$6,16
    bnez    $3,copy_double
    or      $8,$4,$5
    andi    $8,$8,15
    bnez    $8,copy_double
    nop
copy_quad:
    sltiu   $3,$6,64
    bnez    $3,copy_quad_tail
    nop
    lq      $8,0($5)
    lq      $9,16($5)
    lq      $10,32($5)
    lq      $11,48($5)
    sq      $8,0($4)
    sq      $9,16($4)
    sq      $10,32($4)
    sq      $11,48($4)
    addiu   $5,$5,64
    addiu   $4,$4,64
    b       copy_quad
    addiu   $6,$6,-64
copy_quad_tail:
    sltiu   $3,$6,16
    bnez    $3,copy_double
    nop
    lq      $8,0($5)
    addiu   $5,$5,16
    sq      $8,0($4)
    addiu   $4,$4,16
    b       copy_quad_tail
    addiu   $6,$6,-16
copy_double:
    sltiu   $3,$6,8
    bnez    $3,copy_word
    nop
    ldl     $8,7($5)
    ldr     $8,0($5)
    sdl     $8,7($4)
    sdr     $8,0($4)
    addiu   $5,$5,8
    addiu   $4,$4,8
    b       copy_double
    addiu   $6,$6,-8
copy_word:
    sltiu   $3,$6,4
    bnez    $3,copy_byte
    nop
    lwl     $8,3($5)
    lwr     $8,0($5)
    swl     $8,3($4)
    swr     $8,0($4)
    addiu   $5,$5,4
    addiu   $4,$4,4
    addiu   $6,$6,-4
copy_byte:
    beqz    $6,copy_done
    nop
    lbu     $8,0($5)
    addiu   $5,$5,1
    sb      $8,0($4)
    addiu   $4,$4,1
    b       copy_byte
    addiu   $6,$6,-1
copy_done:
    jr      $31
    nop
.end memcpy

.align 4
.globl memcmp
.ent memcmp
memcmp:
    beq     $4,$5,compare_equal
    or      $8,$4,$5
    andi    $8,$8,15
    bnez    $8,compare_double
    nop
compare_quad:
    sltiu   $3,$6,16
    bnez    $3,compare_double
    nop
    lq      $8,0($4)
    lq      $9,0($5)
    pxor    $10,$8,$9
    pcpyud  $11,$10,$10
    or      $10,$10,$11
    bnez    $10,compare_byte
    nop
    addiu   $4,$4,16
    addiu   $5,$5,16
    b       compare_quad
    addiu   $6,$6,-16
compare_double:
    sltiu   $3,$6,8
    bnez    $3,compare_word
    nop
    ldl     $8,7($4)
    ldr     $8,0($4)
    ldl     $9,7($5)
    ldr     $9,0($5)
    bne     $8,$9,compare_byte
    nop
    addiu   $4,$4,8
    addiu   $5,$5,8
    b       compare_double
    addiu   $6,$6,-8
compare_word:
    sltiu   $3,$6,4
    bnez    $3,compare_byte
    nop
    lwl     $8,3($4)
    lwr     $8,0($4)
    lwl     $9,3($5)
    lwr     $9,0($5)
    bne     $8,$9,compare_byte
    nop
    addiu   $4,$4,4
    addiu   $5,$5,4
    addiu   $6,$6,-4
compare_byte:
    beqz    $6,compare_equal
    nop
    lbu     $8,0($4)
    lbu     $9,0($5)
    bne     $8,$9,compare_done
    subu    $2,$8,$9
    addiu   $4,$4,1
    addiu   $5,$5,1
    b       compare_byte
    addiu   $6,$6,-1
compare_equal:
    move    $2,$0
compare_done:
    jr      $31
    nop
.end memcmp

.align 4
.globl memset
.ent memset
memset:
    move    $2,$4
    andi    $5,$5,255
    sltiu   $3,$6,16
    bnez    $3,set_byte
    nop
set_align:
    andi    $3,$4,15
    beqz    $3,set_pattern
    nop
    sb      $5,0($4)
    addiu   $4,$4,1
    b       set_align
    addiu   $6,$6,-1
set_pattern:
    sll     $8,$5,8
    or      $8,$8,$5
    sll     $9,$8,16
    or      $8,$8,$9
    dsll32  $8,$8,0
    dsrl32  $8,$8,0
    dsll32  $9,$8,0
    or      $8,$8,$9
    pcpyld  $8,$8,$8
set_quad:
    sltiu   $3,$6,64
    bnez    $3,set_quad_tail
    nop
    sq      $8,0($4)
    sq      $8,16($4)
    sq      $8,32($4)
    sq      $8,48($4)
    addiu   $4,$4,64
    b       set_quad
    addiu   $6,$6,-64
set_quad_tail:
    sltiu   $3,$6,16
    bnez    $3,set_byte
    nop
    sq      $8,0($4)
    addiu   $4,$4,16
    b       set_quad_tail
    addiu   $6,$6,-16
set_byte:
    beqz    $6,set_done
    nop
    sb      $5,0($4)
    addiu   $4,$4,1
    b       set_byte
    addiu   $6,$6,-1
set_done:
    jr      $31
    nop
.end memset

.align 4
.globl memmove
.ent memmove
memmove:
    sltu    $3,$5,$4
    beqz    $3,memcpy
    subu    $8,$4,$5
    sltu    $3,$8,$6
    beqz    $3,memcpy
    move    $2,$4
    addu    $4,$4,$6
    addu    $5,$5,$6
    or      $8,$4,$5
    andi    $8,$8,15
    bnez    $8,move_double
    nop
move_quad:
    sltiu   $3,$6,16
    bnez    $3,move_double
    nop
    addiu   $5,$5,-16
    lq      $8,0($5)
    addiu   $4,$4,-16
    sq      $8,0($4)
    b       move_quad
    addiu   $6,$6,-16
move_double:
    sltiu   $3,$6,8
    bnez    $3,move_byte
    nop
    addiu   $5,$5,-8
    ldl     $8,7($5)
    ldr     $8,0($5)
    addiu   $4,$4,-8
    sdl     $8,7($4)
    sdr     $8,0($4)
    b       move_double
    addiu   $6,$6,-8
move_byte:
    beqz    $6,move_done
    nop
    addiu   $5,$5,-1
    lbu     $8,0($5)
    addiu   $4,$4,-1
    sb      $8,0($4)
    b       move_byte
    addiu   $6,$6,-1
move_done:
    jr      $31
    nop
.end memmove

