    // Shadow filter ported from 1.0.4.0
    
    def c110, -0.25, 1, -1, 0
    def c111, 0.159154937, 0.5, 6.28318548, -3.14159274
    def c112, 3, 7.13800001, 0, 0
    def c113, 0.75, -0.5, 0.5, 0
    
    mov r21.xy, c112.xy
    mul r21.xy, r21.xy, c44.xy // r21.xy * screen dimensions
    dp2add r21.y, v0, r21, c112.w // v0.x * r21.x + v0.y * r21.y
    mad r21.y, r21.y, c111.x, c111.y
    frc r21.y, r21.y
    mad r21.y, r21.y, c111.z, c111.w // r21.y * 2pi - pi
    sincos r22.xy, r21.y // cosine & sine of r21.y
    mul r23, r22.yxxy, c110.xxyz // offsets for 1st and 4th samples, respectively
    mul r21, r22.yxxy, c113.xxyz        // offsets for 3rd and 2nd samples, respectively
    mov r20.xy, c53.y // copy texel size
    mul r20.xy, r20.xy, c112.x // blur factor
    mad r23, r23, r20.xyxy, r0.zwzw // offset * texel size + UV
    mad r21, r21, r20.xyxy, r0.zwzw // offset * texel size + UV
    texld r25, r23.xy, s15 // 1st sample
    texld r24, r21.zw, s15 // 2nd sample
    mov r25.y, r24.x // copy to r25
    texld r24, r21.xy, s15 // 3rd sample
    mov r25.z, r24.x // copy to r25
    texld r24, r23.zw, s15 // 4th sample
    mov r25.w, r24.x // copy to r25
    add r25, r1.z, -r25 // depth bias
    cmp r25, r25, c110.y, c110.w
    dp4 r0.z, r25, -c110.x // average