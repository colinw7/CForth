256 CONSTANT MAX_ITER

: PSTACK ." Stack> " DEPTH 0 DO DEPTH I - PICK . LOOP CR ;

: SQR DUP * ;

: XXYYXY 
                ( X Y )
  OVER *        ( X Y XX )
  OVER *        ( X Y XX YY )
  >R >R * R> R> ( XY XX YY )
;

: PLOT DROP DROP DROP ;

VARIABLE X
VARIABLE Y

VARIABLE ZR2
VARIABLE ZI2
VARIABLE ZRI

VARIABLE ZR
VARIABLE ZI

VARIABLE ITER

: iterate
  0.0 ZR2 !
  0.0 ZI2 !
  0.0 ZRI !

  MAX_ITER ITER !

  MAX_ITER 0 DO
    ZI2 @ ZR2 @ + 4.0 > IF I ITER ! LEAVE THEN

    ZR2 @ ZI2 @ - X @ + ZR ! ( ZR = ZR2 - ZI2 + X )
    ZRI @ 2.0   * Y @ + ZI ! ( ZI = ZRI + ZRI + Y )

    ZR @ SQR ZR2 ! ( ZR2 = ZR * ZR )
    ZI @ SQR ZI2 ! ( ZI2 = ZI * ZI )

    ZR @ ZI @ * ZRI ! ( ZRI = ZR * ZI )
  LOOP
;

-2.0 CONSTANT XMIN
-1.2 CONSTANT YMIN
 1.2 CONSTANT XMAX
 1.2 CONSTANT YMAX

32 CONSTANT N

VARIABLE DX
VARIABLE DY

: mandelbrot
  XMAX XMIN - N / DX !
  YMAX YMIN - N / DY !

  YMIN Y !

  N 0 DO
    XMIN X !

    N 0 DO
      iterate

      I J ITER @ PLOT

      DX @ X +!
    LOOP

    DY @ Y +!
  LOOP
;

mandelbrot
