    ps_3_0
    def c0, 0.5, 1, 0.25, 0
    def c1, 0.001, 0.5, 0.25, 0
    dcl_texcoord v0.xy
    dcl_2d s0
    texld r0, v0, s0
    mov oC0.x, r0.x
    mad oDepth, r0.x, c1.x, c1.y
    mov oC0.yzw, c0.w