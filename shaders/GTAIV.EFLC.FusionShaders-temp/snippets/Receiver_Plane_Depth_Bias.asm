// Receiver Plane Depth Bias
// Adapted from: http://web.archive.org/web/20170830044834/http://developer.amd.com/wordpress/media/2012/10/Isidoro-ShadowMapping.pdf

    mov r22.xy, r0.zw // UV
    mov r22.z, r1.z // light depth
    dsx r20.xyz, r22.xyz
    dsy r21.xyz, r22.xyz
    
    mul r22.xyz, r20.yzy, r21.zxx
    mad r22.xyz, r21.yzy, r20.zxx, -r22.xyz
    rcp r22.z, r22.z
    mul r22.xy, r22.xy, r22.z
    
    // lightdepth += dot(r22.xy, offset.xy);