CREATE STRING 6 ALLOT

: GETSTR
  CR ." ?"
  STRING 
  6 0 DO
    KEY
    DUP EMIT
    OVER !
    1 +
  LOOP
  DROP
;

: PRINTSTR
  STRING
  6 0 DO
    DUP @ EMIT
    1 +
  LOOP
  DROP
;

GETSTR

PRINTSTR
