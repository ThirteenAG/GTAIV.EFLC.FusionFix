    // Custom shadow filter using a vogel disk and either blue noise or interleaved gradient noise for rotation
    // Vogel disk
    def c130, 0.18993645671348536, 0.027087114076591513, -0.21261242652069953, 0.23391293246949066
    def c131, 0.04771781344140756, -0.3666840644525993, 0.297730981239584, 0.398259878229082
    def c132, -0.509063425827436, -0.06528681462854097, 0.507855152944665, -0.2875976005206389
    def c133, -0.15230616564632418, 0.6426121151781916, -0.30240170651828074, -0.5805072900736001
    def c134, 0.6978019230005561, 0.2771173334141519, -0.6990963248129052, 0.3210960724922725
    def c135, 0.3565142601623699, -0.7066415061851589, 0.266890002328106, 0.8360191043249159
    def c136, -0.7515861305520581, -0.41609876195815027, 0.9102937449894895, -0.17014527555321657
    def c137, -0.5343471434373126, 0.8058593459499529, -0.1133270115046468, -0.9490025827627441
    // def c138, 0.06711056, 0.00583715, 52.9829189, 0
    
    mul r24.xy, c44.xy, v0.xy
    // interleaved gradient noise
    // dp2add r24.z, r24.xy, c138.xy, c138.w
    // frc r24.z, r24.z
    // mul r24.z, r24.z, c138.z
    // frc r24.z, r24.z
    mul r24.xy, r24.xy, c112.z
    texld r24, r24, s10 // blue noise
    mul r24.x, r24.z, c111.z // theta
    sincos r25.xy, r24.x
    mul r25, r25.xyyx, c110.yzyy // cos, -sin, sin, cos
    
    mul r26, c130.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c130.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r28, r26.xy, s15
    texld r27, r26.zw, s15
    mov r28.y, r27.x
    mul r26, c131.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c131.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r27, r26.xy, s15
    mov r28.z, r27.x
    texld r27, r26.zw, s15
    mov r28.w, r27.x
    add r28, r1.z, -r28
    cmp r28, r28, c110.y, c110.w
    dp4 r29.x, r28, -c110.x
    
    mul r26, c132.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c132.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r28, r26.xy, s15
    texld r27, r26.zw, s15
    mov r28.y, r27.x
    mul r26, c133.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c133.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r27, r26.xy, s15
    mov r28.z, r27.x
    texld r27, r26.zw, s15
    mov r28.w, r27.x
    add r28, r1.z, -r28
    cmp r28, r28, c110.y, c110.w
    dp4 r29.y, r28, -c110.x
    
    mul r26, c134.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c134.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r28, r26.xy, s15
    texld r27, r26.zw, s15
    mov r28.y, r27.x
    mul r26, c135.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c135.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r27, r26.xy, s15
    mov r28.z, r27.x
    texld r27, r26.zw, s15
    mov r28.w, r27.x
    add r28, r1.z, -r28
    cmp r28, r28, c110.y, c110.w
    dp4 r29.z, r28, -c110.x
    
    mul r26, c136.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c136.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r28, r26.xy, s15
    texld r27, r26.zw, s15
    mov r28.y, r27.x
    mul r26, c137.xyxy, r25
    add r27.xy, r26.xzxz, r26.ywyw
    mul r26, c137.zwzw, r25
    add r27.zw, r26.xzxz, r26.ywyw
    mad r26, r27, r20.xyxy, r0.zwzw
    texld r27, r26.xy, s15
    mov r28.z, r27.x
    texld r27, r26.zw, s15
    mov r28.w, r27.x
    add r28, r1.z, -r28
    cmp r28, r28, c110.y, c110.w
    dp4 r29.w, r28, -c110.x
    
    dp4 r0.z, r29, -c110.x