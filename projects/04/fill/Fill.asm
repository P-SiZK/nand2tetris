// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/04/Fill.asm

// Runs an infinite loop that listens to the keyboard input.
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel;
// the screen should remain fully black as long as the key is pressed. 
// When no key is pressed, the program clears the screen, i.e. writes
// "white" in every pixel;
// the screen should remain fully clear as long as no key is pressed.

(LOOP)
    @24576
    D=M
    @WHITE
    D;JEQ
    @BLACK
    0;JMP
    (WHITE)
        @color
        M=0
        @ENDIF
        0;JMP
    (BLACK)
        @color
        M=-1
    (ENDIF)
    @24575
    D=A
    @screen
    M=D
    (LOOP2)
        @color
        D=M
        @screen
        A=M
        M=D
        @screen
        D=M
        @16384
        D=D-A
        @END2
        D;JEQ
        @screen
        M=M-1
        @LOOP2
        0;JMP
    (END2)
    @LOOP
    0;JMP
(END)