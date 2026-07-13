.segment "HEADER"
    .byte $4E, $45, $53, $1A
    .byte $02
    .byte $01
    .byte $00
    .byte $00, $00, $00, $00, $00, $00, $00, $00

.segment "STARTUP"

.segment "CODE"

RESET:
    SEI
    CLD

    CLC
    LDA #$02
    ADC #$02

Loop:
    JMP Loop

.segment "VECTORS"
    .word 0
    .word RESET
    .word 0

.segment "CHARS"
