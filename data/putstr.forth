CREATE STRING 40 ALLOT

: PUTSTR STRING 40 32 FILL 32 WORD COUNT STRING SWAP MOVE ;

: PRINTSTR STRING 40 -TRAILING TYPE ;

PUTSTR teststring 

PRINTSTR
