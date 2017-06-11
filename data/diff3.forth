: DIFF3
                    ( ZR2 ZI2 ZRI )
  2.0 * >R          ( ZR2 ZI2 RET=2*ZRI )
  OVER OVER - >R    ( ZR2 ZI2 RET=2*ZRI ZR2-ZI2 )
  + R> R> ROT       ( ZR2-ZI2 2*ZRI ZR2+ZI2)
;
