.set noreorder
.text
.align 4
.globl Hyena_GSScaleLightmap
.ent Hyena_GSScaleLightmap
Hyena_GSScaleLightmap:
    move    $11,$0
    b       lightmap_setup
    nop
.end Hyena_GSScaleLightmap

.align 4
.globl Hyena_GSAddLightmap
.ent Hyena_GSAddLightmap
Hyena_GSAddLightmap:
    li      $11,1
lightmap_setup:
    pextlw  $9,$7,$7
    pcpyld  $9,$9,$9
    bnez    $11,lightmap_add
    nop
lightmap_scale:
    sltiu   $3,$6,4
    bnez    $3,lightmap_tail
    nop
    lwl     $8,3($5)
    lwr     $8,0($5)
    pextlb  $8,$0,$8
    pextlh  $8,$0,$8
    pmulth  $8,$8,$9
    addiu   $5,$5,4
    addiu   $6,$6,-4
    sq      $8,0($4)
    b       lightmap_scale
    addiu   $4,$4,16
lightmap_add:
    sltiu   $3,$6,4
    bnez    $3,lightmap_tail
    nop
    lwl     $8,3($5)
    lwr     $8,0($5)
    lq      $10,0($4)
    pextlb  $8,$0,$8
    pextlh  $8,$0,$8
    pmulth  $8,$8,$9
    addiu   $5,$5,4
    addiu   $6,$6,-4
    paddw   $8,$8,$10
    sq      $8,0($4)
    b       lightmap_add
    addiu   $4,$4,16
lightmap_tail:
    beqz    $6,lightmap_done
    nop
    lbu     $8,0($5)
    mult    $8,$8,$7
    beqz    $11,lightmap_store
    nop
    lw      $10,0($4)
    addu    $8,$8,$10
lightmap_store:
    sw      $8,0($4)
    addiu   $5,$5,1
    addiu   $4,$4,4
    b       lightmap_tail
    addiu   $6,$6,-1
lightmap_done:
    jr      $31
    nop
.end Hyena_GSAddLightmap
