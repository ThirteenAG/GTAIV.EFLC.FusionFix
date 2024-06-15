// Depth of field downsampled blur pass
// Generated by Microsoft (R) HLSL Shader Compiler 9.26.952.2844
//
// Parameters:
//
//   sampler2D DOFBlurSampler;
//   float4 TexelSize;
//   float4 globalScreenSize;
//
//
// Registers:
//
//   Name                         Reg   Size
//   ---------------------------- ----- ----
//   globalScreenSize             c44      1
//   TexelSize                    c76      1
//   DOFBlurSampler               s8       1
//

    ps_3_0
	def c14, 0.000076, 0.0833333, 1, 0
	def c15, 0.1722689015274074, -0.03890781798837625, -0.2925545106670724, 0.19991406586063373
	def c16, 0.008048957573572442, -0.4935956098342653, 0.29673929703202895, 0.3896855726102194
	def c17, -0.6348666391696282, -0.14557204319639935, 0.5393697912263803, -0.40227442735351937
	def c18, -0.2229188387677916, 0.6718392321695309, -0.39623424062096485, -0.7404974035768233
	def c19, 0.7587014288733177, 0.24980221089178709, -0.8542977299379194, 0.3005844845825336
	def c20, 0.36461638085186754, -0.8861446508288848, 0.26112720207880213, 0.8951663866635635
	
    dcl_texcoord v0.xy
    dcl_2d s8
	
	rsq r20.x, c44.y
	rcp r20.x, r20.x
	mul r20.x, r20.x, c44.y
	mul r20.x, r20.x, c14.x
	mul r20, r20.x, c76.xyxy
	
	mad r22, r20, c15, v0.xyxy
	texld r21.xyz, r22.xy, s8
	texld r23.xyz, r22.zw, s8
	add r21.xyz, r21, r23
	
	mad r22, r20, c16, v0.xyxy
	texld r23.xyz, r22.xy, s8
	add r21.xyz, r21, r23
	texld r23.xyz, r22.zw, s8
	add r21.xyz, r21, r23
	
	mad r22, r20, c17, v0.xyxy
	texld r23.xyz, r22.xy, s8
	add r21.xyz, r21, r23
	texld r23.xyz, r22.zw, s8
	add r21.xyz, r21, r23
	
	mad r22, r20, c18, v0.xyxy
	texld r23.xyz, r22.xy, s8
	add r21.xyz, r21, r23
	texld r23.xyz, r22.zw, s8
	add r21.xyz, r21, r23
	
	mad r22, r20, c19, v0.xyxy
	texld r23.xyz, r22.xy, s8
	add r21.xyz, r21, r23
	texld r23.xyz, r22.zw, s8
	add r21.xyz, r21, r23
	
	mad r22, r20, c20, v0.xyxy
	texld r23.xyz, r22.xy, s8
	add r21.xyz, r21, r23
	texld r23.xyz, r22.zw, s8
	add r21.xyz, r21, r23
	
	mul oC0.xyz, r21, c14.y
    mov oC0.w, c14.z

// approximately 176 instruction slots used (14 texture, 162 arithmetic)