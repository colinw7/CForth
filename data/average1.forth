: *ARRAY
  CREATE
    DUP ,
    0 ,
    0 DO
      0 ,
    LOOP
  DOES>
    DUP DUP @
    SWAP 4 +
    OVER 0 SWAP
    0 DO
      OVER @ +
      SWAP 2+ SWAP
    LOOP
    SWAP DROP SWAP /
    OVER 2+ !
    2+ SWAP 2 * +
;

10 *ARRAY readings

: average
  0
  11 1 DO
    I readings @ +
  LOOP
  10 /
;

10 1 readings !
20 2 readings !
1000 10 readings !

2 readings ?

0 readings ?
