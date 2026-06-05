        section code,code

        xref    _mt_install
        xref    _mt_remove
        xref    _mt_init
        xref    _mt_end
        xref    _mt_mastervol
        xref    _mt_musicmask
        xref    _mt_channelmask
        xref    _mt_playfx
        xref    _mt_stopfx
        xref    _mt_Enable

        xdef    _ana_pt_install
        xdef    _ana_pt_remove
        xdef    _ana_pt_init
        xdef    _ana_pt_end
        xdef    _ana_pt_enable
        xdef    _ana_pt_mastervol
        xdef    _ana_pt_musicmask
        xdef    _ana_pt_channelmask
        xdef    _ana_pt_playfx
        xdef    _ana_pt_stopfx

CUSTOM  equ     $dff000

_ana_pt_install:
        movem.l d2-d7/a2-a6,-(sp)
        jsr     _mt_install
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_remove:
        movem.l d2-d7/a2-a6,-(sp)
        jsr     _mt_remove
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_init:
        move.l  4(sp),a0
        movem.l d2-d7/a2-a6,-(sp)
        lea     CUSTOM,a6
        sub.l   a1,a1
        moveq   #0,d0
        jsr     _mt_init
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_end:
        movem.l d2-d7/a2-a6,-(sp)
        lea     CUSTOM,a6
        jsr     _mt_end
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_enable:
        move.l  4(sp),d0
        move.b  d0,_mt_Enable
        rts

_ana_pt_mastervol:
        move.l  4(sp),d0
        movem.l d2-d7/a2-a6,-(sp)
        lea     CUSTOM,a6
        jsr     _mt_mastervol
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_musicmask:
        move.l  4(sp),d0
        movem.l d2-d7/a2-a6,-(sp)
        lea     CUSTOM,a6
        jsr     _mt_musicmask
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_channelmask:
        move.l  4(sp),d0
        movem.l d2-d7/a2-a6,-(sp)
        lea     CUSTOM,a6
        jsr     _mt_channelmask
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_playfx:
        move.l  4(sp),a0
        movem.l d2-d7/a2-a6,-(sp)
        lea     CUSTOM,a6
        jsr     _mt_playfx
        movem.l (sp)+,d2-d7/a2-a6
        rts

_ana_pt_stopfx:
        move.l  4(sp),d0
        movem.l d2-d7/a2-a6,-(sp)
        lea     CUSTOM,a6
        jsr     _mt_stopfx
        movem.l (sp)+,d2-d7/a2-a6
        rts

        end
