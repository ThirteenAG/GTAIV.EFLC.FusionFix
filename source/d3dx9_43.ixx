module;

#include <common.hxx>
#include <d3d9.h>
#include <d3dx9.h>
#include <D3DX9Mesh.h>
#include <D3DX9Shader.h>

export module d3dx9_43;

import common;

export HMODULE GetD3DX9_43DLL()
{
    constexpr auto dll = L"d3dx9_43.dll";
    auto hm = GetModuleHandleW(dll);
    return (hm ? hm : LoadLibraryW(dll));
}

decltype(D3DXAssembleShader)* pD3DXAssembleShader;
decltype(D3DXAssembleShaderFromFileA)* pD3DXAssembleShaderFromFileA;
decltype(D3DXAssembleShaderFromFileW)* pD3DXAssembleShaderFromFileW;
decltype(D3DXAssembleShaderFromResourceA)* pD3DXAssembleShaderFromResourceA;
decltype(D3DXAssembleShaderFromResourceW)* pD3DXAssembleShaderFromResourceW;
decltype(D3DXBoxBoundProbe)* pD3DXBoxBoundProbe;
decltype(D3DXCheckCubeTextureRequirements)* pD3DXCheckCubeTextureRequirements;
decltype(D3DXCheckTextureRequirements)* pD3DXCheckTextureRequirements;
decltype(D3DXCheckVersion)* pD3DXCheckVersion;
decltype(D3DXCheckVolumeTextureRequirements)* pD3DXCheckVolumeTextureRequirements;
decltype(D3DXCleanMesh)* pD3DXCleanMesh;
decltype(D3DXColorAdjustContrast)* pD3DXColorAdjustContrast;
decltype(D3DXColorAdjustSaturation)* pD3DXColorAdjustSaturation;
decltype(D3DXCompileShader)* pD3DXCompileShader;
decltype(D3DXCompileShaderFromFileA)* pD3DXCompileShaderFromFileA;
decltype(D3DXCompileShaderFromFileW)* pD3DXCompileShaderFromFileW;
decltype(D3DXCompileShaderFromResourceA)* pD3DXCompileShaderFromResourceA;
decltype(D3DXCompileShaderFromResourceW)* pD3DXCompileShaderFromResourceW;
decltype(D3DXComputeBoundingBox)* pD3DXComputeBoundingBox;
decltype(D3DXComputeBoundingSphere)* pD3DXComputeBoundingSphere;
decltype(D3DXComputeIMTFromPerTexelSignal)* pD3DXComputeIMTFromPerTexelSignal;
decltype(D3DXComputeIMTFromPerVertexSignal)* pD3DXComputeIMTFromPerVertexSignal;
decltype(D3DXComputeIMTFromSignal)* pD3DXComputeIMTFromSignal;
decltype(D3DXComputeIMTFromTexture)* pD3DXComputeIMTFromTexture;
decltype(D3DXComputeNormalMap)* pD3DXComputeNormalMap;
decltype(D3DXComputeNormals)* pD3DXComputeNormals;
decltype(D3DXComputeTangentFrame)* pD3DXComputeTangentFrame;
decltype(D3DXComputeTangentFrameEx)* pD3DXComputeTangentFrameEx;
decltype(D3DXConcatenateMeshes)* pD3DXConcatenateMeshes;
decltype(D3DXConvertMeshSubsetToSingleStrip)* pD3DXConvertMeshSubsetToSingleStrip;
decltype(D3DXConvertMeshSubsetToStrips)* pD3DXConvertMeshSubsetToStrips;
decltype(D3DXCreateAnimationController)* pD3DXCreateAnimationController;
decltype(D3DXCreateBox)* pD3DXCreateBox;
decltype(D3DXCreateBuffer)* pD3DXCreateBuffer;
decltype(D3DXCreateCompressedAnimationSet)* pD3DXCreateCompressedAnimationSet;
decltype(D3DXCreateCubeTexture)* pD3DXCreateCubeTexture;
decltype(D3DXCreateCubeTextureFromFileA)* pD3DXCreateCubeTextureFromFileA;
decltype(D3DXCreateCubeTextureFromFileExA)* pD3DXCreateCubeTextureFromFileExA;
decltype(D3DXCreateCubeTextureFromFileExW)* pD3DXCreateCubeTextureFromFileExW;
decltype(D3DXCreateCubeTextureFromFileInMemory)* pD3DXCreateCubeTextureFromFileInMemory;
decltype(D3DXCreateCubeTextureFromFileInMemoryEx)* pD3DXCreateCubeTextureFromFileInMemoryEx;
decltype(D3DXCreateCubeTextureFromFileW)* pD3DXCreateCubeTextureFromFileW;
decltype(D3DXCreateCubeTextureFromResourceA)* pD3DXCreateCubeTextureFromResourceA;
decltype(D3DXCreateCubeTextureFromResourceExA)* pD3DXCreateCubeTextureFromResourceExA;
decltype(D3DXCreateCubeTextureFromResourceExW)* pD3DXCreateCubeTextureFromResourceExW;
decltype(D3DXCreateCubeTextureFromResourceW)* pD3DXCreateCubeTextureFromResourceW;
decltype(D3DXCreateCylinder)* pD3DXCreateCylinder;
decltype(D3DXCreateEffect)* pD3DXCreateEffect;
decltype(D3DXCreateEffectCompiler)* pD3DXCreateEffectCompiler;
decltype(D3DXCreateEffectCompilerFromFileA)* pD3DXCreateEffectCompilerFromFileA;
decltype(D3DXCreateEffectCompilerFromFileW)* pD3DXCreateEffectCompilerFromFileW;
decltype(D3DXCreateEffectCompilerFromResourceA)* pD3DXCreateEffectCompilerFromResourceA;
decltype(D3DXCreateEffectCompilerFromResourceW)* pD3DXCreateEffectCompilerFromResourceW;
decltype(D3DXCreateEffectEx)* pD3DXCreateEffectEx;
decltype(D3DXCreateEffectFromFileA)* pD3DXCreateEffectFromFileA;
decltype(D3DXCreateEffectFromFileExA)* pD3DXCreateEffectFromFileExA;
decltype(D3DXCreateEffectFromFileExW)* pD3DXCreateEffectFromFileExW;
decltype(D3DXCreateEffectFromFileW)* pD3DXCreateEffectFromFileW;
decltype(D3DXCreateEffectFromResourceA)* pD3DXCreateEffectFromResourceA;
decltype(D3DXCreateEffectFromResourceExA)* pD3DXCreateEffectFromResourceExA;
decltype(D3DXCreateEffectFromResourceExW)* pD3DXCreateEffectFromResourceExW;
decltype(D3DXCreateEffectFromResourceW)* pD3DXCreateEffectFromResourceW;
decltype(D3DXCreateEffectPool)* pD3DXCreateEffectPool;
decltype(D3DXCreateFontA)* pD3DXCreateFontA;
decltype(D3DXCreateFontIndirectA)* pD3DXCreateFontIndirectA;
decltype(D3DXCreateFontIndirectW)* pD3DXCreateFontIndirectW;
decltype(D3DXCreateFontW)* pD3DXCreateFontW;
decltype(D3DXCreateKeyframedAnimationSet)* pD3DXCreateKeyframedAnimationSet;
decltype(D3DXCreateLine)* pD3DXCreateLine;
decltype(D3DXCreateMatrixStack)* pD3DXCreateMatrixStack;
decltype(D3DXCreateMesh)* pD3DXCreateMesh;
decltype(D3DXCreateMeshFVF)* pD3DXCreateMeshFVF;
decltype(D3DXCreateNPatchMesh)* pD3DXCreateNPatchMesh;
decltype(D3DXCreatePMeshFromStream)* pD3DXCreatePMeshFromStream;
decltype(D3DXCreatePRTBuffer)* pD3DXCreatePRTBuffer;
decltype(D3DXCreatePRTBufferTex)* pD3DXCreatePRTBufferTex;
decltype(D3DXCreatePRTCompBuffer)* pD3DXCreatePRTCompBuffer;
decltype(D3DXCreatePRTEngine)* pD3DXCreatePRTEngine;
decltype(D3DXCreatePatchMesh)* pD3DXCreatePatchMesh;
decltype(D3DXCreatePolygon)* pD3DXCreatePolygon;
decltype(D3DXCreateRenderToEnvMap)* pD3DXCreateRenderToEnvMap;
decltype(D3DXCreateRenderToSurface)* pD3DXCreateRenderToSurface;
decltype(D3DXCreateSPMesh)* pD3DXCreateSPMesh;
decltype(D3DXCreateSkinInfo)* pD3DXCreateSkinInfo;
decltype(D3DXCreateSkinInfoFVF)* pD3DXCreateSkinInfoFVF;
decltype(D3DXCreateSkinInfoFromBlendedMesh)* pD3DXCreateSkinInfoFromBlendedMesh;
decltype(D3DXCreateSphere)* pD3DXCreateSphere;
decltype(D3DXCreateSprite)* pD3DXCreateSprite;
decltype(D3DXCreateTeapot)* pD3DXCreateTeapot;
decltype(D3DXCreateTextA)* pD3DXCreateTextA;
decltype(D3DXCreateTextW)* pD3DXCreateTextW;
decltype(D3DXCreateTexture)* pD3DXCreateTexture;
decltype(D3DXCreateTextureFromFileA)* pD3DXCreateTextureFromFileA;
decltype(D3DXCreateTextureFromFileExA)* pD3DXCreateTextureFromFileExA;
decltype(D3DXCreateTextureFromFileExW)* pD3DXCreateTextureFromFileExW;
decltype(D3DXCreateTextureFromFileInMemory)* pD3DXCreateTextureFromFileInMemory;
decltype(D3DXCreateTextureFromFileInMemoryEx)* pD3DXCreateTextureFromFileInMemoryEx;
decltype(D3DXCreateTextureFromFileW)* pD3DXCreateTextureFromFileW;
decltype(D3DXCreateTextureFromResourceA)* pD3DXCreateTextureFromResourceA;
decltype(D3DXCreateTextureFromResourceExA)* pD3DXCreateTextureFromResourceExA;
decltype(D3DXCreateTextureFromResourceExW)* pD3DXCreateTextureFromResourceExW;
decltype(D3DXCreateTextureFromResourceW)* pD3DXCreateTextureFromResourceW;
decltype(D3DXCreateTextureGutterHelper)* pD3DXCreateTextureGutterHelper;
decltype(D3DXCreateTextureShader)* pD3DXCreateTextureShader;
decltype(D3DXCreateTorus)* pD3DXCreateTorus;
decltype(D3DXCreateVolumeTexture)* pD3DXCreateVolumeTexture;
decltype(D3DXCreateVolumeTextureFromFileA)* pD3DXCreateVolumeTextureFromFileA;
decltype(D3DXCreateVolumeTextureFromFileExA)* pD3DXCreateVolumeTextureFromFileExA;
decltype(D3DXCreateVolumeTextureFromFileExW)* pD3DXCreateVolumeTextureFromFileExW;
decltype(D3DXCreateVolumeTextureFromFileInMemory)* pD3DXCreateVolumeTextureFromFileInMemory;
decltype(D3DXCreateVolumeTextureFromFileInMemoryEx)* pD3DXCreateVolumeTextureFromFileInMemoryEx;
decltype(D3DXCreateVolumeTextureFromFileW)* pD3DXCreateVolumeTextureFromFileW;
decltype(D3DXCreateVolumeTextureFromResourceA)* pD3DXCreateVolumeTextureFromResourceA;
decltype(D3DXCreateVolumeTextureFromResourceExA)* pD3DXCreateVolumeTextureFromResourceExA;
decltype(D3DXCreateVolumeTextureFromResourceExW)* pD3DXCreateVolumeTextureFromResourceExW;
decltype(D3DXCreateVolumeTextureFromResourceW)* pD3DXCreateVolumeTextureFromResourceW;
decltype(D3DXDebugMute)* pD3DXDebugMute;
decltype(D3DXDeclaratorFromFVF)* pD3DXDeclaratorFromFVF;
decltype(D3DXDisassembleEffect)* pD3DXDisassembleEffect;
decltype(D3DXDisassembleShader)* pD3DXDisassembleShader;
decltype(D3DXFVFFromDeclarator)* pD3DXFVFFromDeclarator;
decltype(D3DXFillCubeTexture)* pD3DXFillCubeTexture;
decltype(D3DXFillCubeTextureTX)* pD3DXFillCubeTextureTX;
decltype(D3DXFillTexture)* pD3DXFillTexture;
decltype(D3DXFillTextureTX)* pD3DXFillTextureTX;
decltype(D3DXFillVolumeTexture)* pD3DXFillVolumeTexture;
decltype(D3DXFillVolumeTextureTX)* pD3DXFillVolumeTextureTX;
decltype(D3DXFilterTexture)* pD3DXFilterTexture;
decltype(D3DXFindShaderComment)* pD3DXFindShaderComment;
decltype(D3DXFloat16To32Array)* pD3DXFloat16To32Array;
decltype(D3DXFloat32To16Array)* pD3DXFloat32To16Array;
decltype(D3DXFrameAppendChild)* pD3DXFrameAppendChild;
decltype(D3DXFrameCalculateBoundingSphere)* pD3DXFrameCalculateBoundingSphere;
decltype(D3DXFrameDestroy)* pD3DXFrameDestroy;
decltype(D3DXFrameFind)* pD3DXFrameFind;
decltype(D3DXFrameNumNamedMatrices)* pD3DXFrameNumNamedMatrices;
decltype(D3DXFrameRegisterNamedMatrices)* pD3DXFrameRegisterNamedMatrices;
decltype(D3DXFresnelTerm)* pD3DXFresnelTerm;
decltype(D3DXGenerateOutputDecl)* pD3DXGenerateOutputDecl;
decltype(D3DXGeneratePMesh)* pD3DXGeneratePMesh;
decltype(D3DXGetDeclLength)* pD3DXGetDeclLength;
decltype(D3DXGetDeclVertexSize)* pD3DXGetDeclVertexSize;
decltype(D3DXGetDriverLevel)* pD3DXGetDriverLevel;
decltype(D3DXGetFVFVertexSize)* pD3DXGetFVFVertexSize;
decltype(D3DXGetImageInfoFromFileA)* pD3DXGetImageInfoFromFileA;
decltype(D3DXGetImageInfoFromFileInMemory)* pD3DXGetImageInfoFromFileInMemory;
decltype(D3DXGetImageInfoFromFileW)* pD3DXGetImageInfoFromFileW;
decltype(D3DXGetImageInfoFromResourceA)* pD3DXGetImageInfoFromResourceA;
decltype(D3DXGetImageInfoFromResourceW)* pD3DXGetImageInfoFromResourceW;
decltype(D3DXGetPixelShaderProfile)* pD3DXGetPixelShaderProfile;
decltype(D3DXGetShaderConstantTable)* pD3DXGetShaderConstantTable;
decltype(D3DXGetShaderInputSemantics)* pD3DXGetShaderInputSemantics;
decltype(D3DXGetShaderOutputSemantics)* pD3DXGetShaderOutputSemantics;
decltype(D3DXGetShaderSamplers)* pD3DXGetShaderSamplers;
decltype(D3DXGetShaderSize)* pD3DXGetShaderSize;
decltype(D3DXGetShaderVersion)* pD3DXGetShaderVersion;
decltype(D3DXGetVertexShaderProfile)* pD3DXGetVertexShaderProfile;
decltype(D3DXIntersect)* pD3DXIntersect;
decltype(D3DXIntersectSubset)* pD3DXIntersectSubset;
decltype(D3DXIntersectTri)* pD3DXIntersectTri;
decltype(D3DXLoadMeshFromXA)* pD3DXLoadMeshFromXA;
decltype(D3DXLoadMeshFromXInMemory)* pD3DXLoadMeshFromXInMemory;
decltype(D3DXLoadMeshFromXResource)* pD3DXLoadMeshFromXResource;
decltype(D3DXLoadMeshFromXW)* pD3DXLoadMeshFromXW;
decltype(D3DXLoadMeshFromXof)* pD3DXLoadMeshFromXof;
decltype(D3DXLoadMeshHierarchyFromXA)* pD3DXLoadMeshHierarchyFromXA;
decltype(D3DXLoadMeshHierarchyFromXInMemory)* pD3DXLoadMeshHierarchyFromXInMemory;
decltype(D3DXLoadMeshHierarchyFromXW)* pD3DXLoadMeshHierarchyFromXW;
decltype(D3DXLoadPRTBufferFromFileA)* pD3DXLoadPRTBufferFromFileA;
decltype(D3DXLoadPRTBufferFromFileW)* pD3DXLoadPRTBufferFromFileW;
decltype(D3DXLoadPRTCompBufferFromFileA)* pD3DXLoadPRTCompBufferFromFileA;
decltype(D3DXLoadPRTCompBufferFromFileW)* pD3DXLoadPRTCompBufferFromFileW;
decltype(D3DXLoadPatchMeshFromXof)* pD3DXLoadPatchMeshFromXof;
decltype(D3DXLoadSkinMeshFromXof)* pD3DXLoadSkinMeshFromXof;
decltype(D3DXLoadSurfaceFromFileA)* pD3DXLoadSurfaceFromFileA;
decltype(D3DXLoadSurfaceFromFileInMemory)* pD3DXLoadSurfaceFromFileInMemory;
decltype(D3DXLoadSurfaceFromFileW)* pD3DXLoadSurfaceFromFileW;
decltype(D3DXLoadSurfaceFromMemory)* pD3DXLoadSurfaceFromMemory;
decltype(D3DXLoadSurfaceFromResourceA)* pD3DXLoadSurfaceFromResourceA;
decltype(D3DXLoadSurfaceFromResourceW)* pD3DXLoadSurfaceFromResourceW;
decltype(D3DXLoadSurfaceFromSurface)* pD3DXLoadSurfaceFromSurface;
decltype(D3DXLoadVolumeFromFileA)* pD3DXLoadVolumeFromFileA;
decltype(D3DXLoadVolumeFromFileInMemory)* pD3DXLoadVolumeFromFileInMemory;
decltype(D3DXLoadVolumeFromFileW)* pD3DXLoadVolumeFromFileW;
decltype(D3DXLoadVolumeFromMemory)* pD3DXLoadVolumeFromMemory;
decltype(D3DXLoadVolumeFromResourceA)* pD3DXLoadVolumeFromResourceA;
decltype(D3DXLoadVolumeFromResourceW)* pD3DXLoadVolumeFromResourceW;
decltype(D3DXLoadVolumeFromVolume)* pD3DXLoadVolumeFromVolume;
decltype(D3DXMatrixAffineTransformation)* pD3DXMatrixAffineTransformation;
decltype(D3DXMatrixAffineTransformation2D)* pD3DXMatrixAffineTransformation2D;
decltype(D3DXMatrixDecompose)* pD3DXMatrixDecompose;
decltype(D3DXMatrixDeterminant)* pD3DXMatrixDeterminant;
decltype(D3DXMatrixInverse)* pD3DXMatrixInverse;
decltype(D3DXMatrixLookAtLH)* pD3DXMatrixLookAtLH;
decltype(D3DXMatrixLookAtRH)* pD3DXMatrixLookAtRH;
decltype(D3DXMatrixMultiply)* pD3DXMatrixMultiply;
decltype(D3DXMatrixMultiplyTranspose)* pD3DXMatrixMultiplyTranspose;
decltype(D3DXMatrixOrthoLH)* pD3DXMatrixOrthoLH;
decltype(D3DXMatrixOrthoOffCenterLH)* pD3DXMatrixOrthoOffCenterLH;
decltype(D3DXMatrixOrthoOffCenterRH)* pD3DXMatrixOrthoOffCenterRH;
decltype(D3DXMatrixOrthoRH)* pD3DXMatrixOrthoRH;
decltype(D3DXMatrixPerspectiveFovLH)* pD3DXMatrixPerspectiveFovLH;
decltype(D3DXMatrixPerspectiveFovRH)* pD3DXMatrixPerspectiveFovRH;
decltype(D3DXMatrixPerspectiveLH)* pD3DXMatrixPerspectiveLH;
decltype(D3DXMatrixPerspectiveOffCenterLH)* pD3DXMatrixPerspectiveOffCenterLH;
decltype(D3DXMatrixPerspectiveOffCenterRH)* pD3DXMatrixPerspectiveOffCenterRH;
decltype(D3DXMatrixPerspectiveRH)* pD3DXMatrixPerspectiveRH;
decltype(D3DXMatrixReflect)* pD3DXMatrixReflect;
decltype(D3DXMatrixRotationAxis)* pD3DXMatrixRotationAxis;
decltype(D3DXMatrixRotationQuaternion)* pD3DXMatrixRotationQuaternion;
decltype(D3DXMatrixRotationX)* pD3DXMatrixRotationX;
decltype(D3DXMatrixRotationY)* pD3DXMatrixRotationY;
decltype(D3DXMatrixRotationYawPitchRoll)* pD3DXMatrixRotationYawPitchRoll;
decltype(D3DXMatrixRotationZ)* pD3DXMatrixRotationZ;
decltype(D3DXMatrixScaling)* pD3DXMatrixScaling;
decltype(D3DXMatrixShadow)* pD3DXMatrixShadow;
decltype(D3DXMatrixTransformation)* pD3DXMatrixTransformation;
decltype(D3DXMatrixTransformation2D)* pD3DXMatrixTransformation2D;
decltype(D3DXMatrixTranslation)* pD3DXMatrixTranslation;
decltype(D3DXMatrixTranspose)* pD3DXMatrixTranspose;
decltype(D3DXOptimizeFaces)* pD3DXOptimizeFaces;
decltype(D3DXOptimizeVertices)* pD3DXOptimizeVertices;
decltype(D3DXPlaneFromPointNormal)* pD3DXPlaneFromPointNormal;
decltype(D3DXPlaneFromPoints)* pD3DXPlaneFromPoints;
decltype(D3DXPlaneIntersectLine)* pD3DXPlaneIntersectLine;
decltype(D3DXPlaneNormalize)* pD3DXPlaneNormalize;
decltype(D3DXPlaneTransform)* pD3DXPlaneTransform;
decltype(D3DXPlaneTransformArray)* pD3DXPlaneTransformArray;
decltype(D3DXPreprocessShader)* pD3DXPreprocessShader;
decltype(D3DXPreprocessShaderFromFileA)* pD3DXPreprocessShaderFromFileA;
decltype(D3DXPreprocessShaderFromFileW)* pD3DXPreprocessShaderFromFileW;
decltype(D3DXPreprocessShaderFromResourceA)* pD3DXPreprocessShaderFromResourceA;
decltype(D3DXPreprocessShaderFromResourceW)* pD3DXPreprocessShaderFromResourceW;
decltype(D3DXQuaternionBaryCentric)* pD3DXQuaternionBaryCentric;
decltype(D3DXQuaternionExp)* pD3DXQuaternionExp;
decltype(D3DXQuaternionInverse)* pD3DXQuaternionInverse;
decltype(D3DXQuaternionLn)* pD3DXQuaternionLn;
decltype(D3DXQuaternionMultiply)* pD3DXQuaternionMultiply;
decltype(D3DXQuaternionNormalize)* pD3DXQuaternionNormalize;
decltype(D3DXQuaternionRotationAxis)* pD3DXQuaternionRotationAxis;
decltype(D3DXQuaternionRotationMatrix)* pD3DXQuaternionRotationMatrix;
decltype(D3DXQuaternionRotationYawPitchRoll)* pD3DXQuaternionRotationYawPitchRoll;
decltype(D3DXQuaternionSlerp)* pD3DXQuaternionSlerp;
decltype(D3DXQuaternionSquad)* pD3DXQuaternionSquad;
decltype(D3DXQuaternionSquadSetup)* pD3DXQuaternionSquadSetup;
decltype(D3DXQuaternionToAxisAngle)* pD3DXQuaternionToAxisAngle;
decltype(D3DXRectPatchSize)* pD3DXRectPatchSize;
decltype(D3DXSHAdd)* pD3DXSHAdd;
decltype(D3DXSHDot)* pD3DXSHDot;
decltype(D3DXSHEvalConeLight)* pD3DXSHEvalConeLight;
decltype(D3DXSHEvalDirection)* pD3DXSHEvalDirection;
decltype(D3DXSHEvalDirectionalLight)* pD3DXSHEvalDirectionalLight;
decltype(D3DXSHEvalHemisphereLight)* pD3DXSHEvalHemisphereLight;
decltype(D3DXSHEvalSphericalLight)* pD3DXSHEvalSphericalLight;
decltype(D3DXSHMultiply2)* pD3DXSHMultiply2;
decltype(D3DXSHMultiply3)* pD3DXSHMultiply3;
decltype(D3DXSHMultiply4)* pD3DXSHMultiply4;
decltype(D3DXSHMultiply5)* pD3DXSHMultiply5;
decltype(D3DXSHMultiply6)* pD3DXSHMultiply6;
decltype(D3DXSHPRTCompSplitMeshSC)* pD3DXSHPRTCompSplitMeshSC;
decltype(D3DXSHPRTCompSuperCluster)* pD3DXSHPRTCompSuperCluster;
decltype(D3DXSHProjectCubeMap)* pD3DXSHProjectCubeMap;
decltype(D3DXSHRotate)* pD3DXSHRotate;
decltype(D3DXSHRotateZ)* pD3DXSHRotateZ;
decltype(D3DXSHScale)* pD3DXSHScale;
decltype(D3DXSaveMeshHierarchyToFileA)* pD3DXSaveMeshHierarchyToFileA;
decltype(D3DXSaveMeshHierarchyToFileW)* pD3DXSaveMeshHierarchyToFileW;
decltype(D3DXSaveMeshToXA)* pD3DXSaveMeshToXA;
decltype(D3DXSaveMeshToXW)* pD3DXSaveMeshToXW;
decltype(D3DXSavePRTBufferToFileA)* pD3DXSavePRTBufferToFileA;
decltype(D3DXSavePRTBufferToFileW)* pD3DXSavePRTBufferToFileW;
decltype(D3DXSavePRTCompBufferToFileA)* pD3DXSavePRTCompBufferToFileA;
decltype(D3DXSavePRTCompBufferToFileW)* pD3DXSavePRTCompBufferToFileW;
decltype(D3DXSaveSurfaceToFileA)* pD3DXSaveSurfaceToFileA;
decltype(D3DXSaveSurfaceToFileInMemory)* pD3DXSaveSurfaceToFileInMemory;
decltype(D3DXSaveSurfaceToFileW)* pD3DXSaveSurfaceToFileW;
decltype(D3DXSaveTextureToFileA)* pD3DXSaveTextureToFileA;
decltype(D3DXSaveTextureToFileInMemory)* pD3DXSaveTextureToFileInMemory;
decltype(D3DXSaveTextureToFileW)* pD3DXSaveTextureToFileW;
decltype(D3DXSaveVolumeToFileA)* pD3DXSaveVolumeToFileA;
decltype(D3DXSaveVolumeToFileInMemory)* pD3DXSaveVolumeToFileInMemory;
decltype(D3DXSaveVolumeToFileW)* pD3DXSaveVolumeToFileW;
decltype(D3DXSimplifyMesh)* pD3DXSimplifyMesh;
decltype(D3DXSphereBoundProbe)* pD3DXSphereBoundProbe;
decltype(D3DXSplitMesh)* pD3DXSplitMesh;
decltype(D3DXTessellateNPatches)* pD3DXTessellateNPatches;
decltype(D3DXTessellateRectPatch)* pD3DXTessellateRectPatch;
decltype(D3DXTessellateTriPatch)* pD3DXTessellateTriPatch;
decltype(D3DXTriPatchSize)* pD3DXTriPatchSize;
decltype(D3DXUVAtlasCreate)* pD3DXUVAtlasCreate;
decltype(D3DXUVAtlasPack)* pD3DXUVAtlasPack;
decltype(D3DXUVAtlasPartition)* pD3DXUVAtlasPartition;
decltype(D3DXValidMesh)* pD3DXValidMesh;
decltype(D3DXValidPatchMesh)* pD3DXValidPatchMesh;
decltype(D3DXVec2BaryCentric)* pD3DXVec2BaryCentric;
decltype(D3DXVec2CatmullRom)* pD3DXVec2CatmullRom;
decltype(D3DXVec2Hermite)* pD3DXVec2Hermite;
decltype(D3DXVec2Normalize)* pD3DXVec2Normalize;
decltype(D3DXVec2Transform)* pD3DXVec2Transform;
decltype(D3DXVec2TransformArray)* pD3DXVec2TransformArray;
decltype(D3DXVec2TransformCoord)* pD3DXVec2TransformCoord;
decltype(D3DXVec2TransformCoordArray)* pD3DXVec2TransformCoordArray;
decltype(D3DXVec2TransformNormal)* pD3DXVec2TransformNormal;
decltype(D3DXVec2TransformNormalArray)* pD3DXVec2TransformNormalArray;
decltype(D3DXVec3BaryCentric)* pD3DXVec3BaryCentric;
decltype(D3DXVec3CatmullRom)* pD3DXVec3CatmullRom;
decltype(D3DXVec3Hermite)* pD3DXVec3Hermite;
decltype(D3DXVec3Normalize)* pD3DXVec3Normalize;
decltype(D3DXVec3Project)* pD3DXVec3Project;
decltype(D3DXVec3ProjectArray)* pD3DXVec3ProjectArray;
decltype(D3DXVec3Transform)* pD3DXVec3Transform;
decltype(D3DXVec3TransformArray)* pD3DXVec3TransformArray;
decltype(D3DXVec3TransformCoord)* pD3DXVec3TransformCoord;
decltype(D3DXVec3TransformCoordArray)* pD3DXVec3TransformCoordArray;
decltype(D3DXVec3TransformNormal)* pD3DXVec3TransformNormal;
decltype(D3DXVec3TransformNormalArray)* pD3DXVec3TransformNormalArray;
decltype(D3DXVec3Unproject)* pD3DXVec3Unproject;
decltype(D3DXVec3UnprojectArray)* pD3DXVec3UnprojectArray;
decltype(D3DXVec4BaryCentric)* pD3DXVec4BaryCentric;
decltype(D3DXVec4CatmullRom)* pD3DXVec4CatmullRom;
decltype(D3DXVec4Cross)* pD3DXVec4Cross;
decltype(D3DXVec4Hermite)* pD3DXVec4Hermite;
decltype(D3DXVec4Normalize)* pD3DXVec4Normalize;
decltype(D3DXVec4Transform)* pD3DXVec4Transform;
decltype(D3DXVec4TransformArray)* pD3DXVec4TransformArray;
decltype(D3DXWeldVertices)* pD3DXWeldVertices;

HRESULT WINAPI D3DXAssembleShader
(
    LPCSTR                          pSrcData,
    UINT                            SrcDataLen,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXAssembleShader)(pSrcData, SrcDataLen, pDefines, pInclude, Flags, ppShader, ppErrorMsgs);
}

HRESULT WINAPI D3DXAssembleShaderFromFileA(
    LPCSTR                          pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXAssembleShaderFromFileA)(pSrcFile, pDefines, pInclude, Flags, ppShader, ppErrorMsgs);
}

HRESULT WINAPI D3DXAssembleShaderFromFileW(
    LPCWSTR                         pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXAssembleShaderFromFileW)(pSrcFile, pDefines, pInclude, Flags, ppShader, ppErrorMsgs);
}

HRESULT WINAPI D3DXAssembleShaderFromResourceA(
    HMODULE                         hSrcModule,
    LPCSTR                          pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXAssembleShaderFromResourceA)(hSrcModule, pSrcResource, pDefines, pInclude, Flags, ppShader, ppErrorMsgs);
}

HRESULT WINAPI D3DXAssembleShaderFromResourceW(
    HMODULE                         hSrcModule,
    LPCWSTR                         pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXAssembleShaderFromResourceW)(hSrcModule, pSrcResource, pDefines, pInclude, Flags, ppShader, ppErrorMsgs);
}

BOOL WINAPI D3DXBoxBoundProbe(
    CONST D3DXVECTOR3* pMin,
    CONST D3DXVECTOR3* pMax,
    CONST D3DXVECTOR3* pRayPosition,
    CONST D3DXVECTOR3* pRayDirection)
{
    return (*pD3DXBoxBoundProbe)(pMin, pMax, pRayPosition, pRayDirection);
}

HRESULT WINAPI D3DXCheckCubeTextureRequirements(
    LPDIRECT3DDEVICE9         pDevice,
    UINT* pSize,
    UINT* pNumMipLevels,
    DWORD                     Usage,
    D3DFORMAT* pFormat,
    D3DPOOL                   Pool)
{
    return (*pD3DXCheckCubeTextureRequirements)(pDevice, pSize, pNumMipLevels, Usage, pFormat, Pool);
}

HRESULT WINAPI D3DXCheckTextureRequirements(
    LPDIRECT3DDEVICE9         pDevice,
    UINT* pWidth,
    UINT* pHeight,
    UINT* pNumMipLevels,
    DWORD                     Usage,
    D3DFORMAT* pFormat,
    D3DPOOL                   Pool)
{
    return (*pD3DXCheckTextureRequirements)(pDevice, pWidth, pHeight, pNumMipLevels, Usage, pFormat, Pool);
}

BOOL WINAPI D3DXCheckVersion(
    UINT D3DSdkVersion,
    UINT D3DXSdkVersion)
{
    return (*pD3DXCheckVersion)(D3DSdkVersion, D3DXSdkVersion);
}

HRESULT WINAPI D3DXCheckVolumeTextureRequirements(
    LPDIRECT3DDEVICE9         pDevice,
    UINT* pWidth,
    UINT* pHeight,
    UINT* pDepth,
    UINT* pNumMipLevels,
    DWORD                     Usage,
    D3DFORMAT* pFormat,
    D3DPOOL                   Pool)
{
    return (*pD3DXCheckVolumeTextureRequirements)(pDevice, pWidth, pHeight, pDepth, pNumMipLevels, Usage, pFormat, Pool);
}

HRESULT WINAPI D3DXCleanMesh(
    D3DXCLEANTYPE CleanType,
    LPD3DXMESH pMeshIn,
    CONST DWORD* pAdjacencyIn,
    LPD3DXMESH* ppMeshOut,
    DWORD* pAdjacencyOut,
    LPD3DXBUFFER* ppErrorsAndWarnings)
{
    return (*pD3DXCleanMesh)(CleanType, pMeshIn, pAdjacencyIn, ppMeshOut, pAdjacencyOut, ppErrorsAndWarnings);
}

D3DXCOLOR* WINAPI D3DXColorAdjustContrast(
    D3DXCOLOR* pOut,
    CONST D3DXCOLOR* pC,
    FLOAT c)
{
    return (*pD3DXColorAdjustContrast)(pOut, pC, c);
}

D3DXCOLOR* WINAPI D3DXColorAdjustSaturation(
    D3DXCOLOR* pOut,
    CONST D3DXCOLOR* pC,
    FLOAT s)
{
    return (*pD3DXColorAdjustSaturation)(pOut, pC, s);
}

HRESULT WINAPI D3DXCompileShader(
    LPCSTR                          pSrcData,
    UINT                            SrcDataLen,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    LPCSTR                          pFunctionName,
    LPCSTR                          pProfile,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs,
    LPD3DXCONSTANTTABLE* ppConstantTable)
{
    return (*pD3DXCompileShader)(pSrcData, SrcDataLen, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable);
}

HRESULT WINAPI D3DXCompileShaderFromFileA(
    LPCSTR                          pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    LPCSTR                          pFunctionName,
    LPCSTR                          pProfile,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs,
    LPD3DXCONSTANTTABLE* ppConstantTable)
{
    return (*pD3DXCompileShaderFromFileA)(pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable);
}

HRESULT WINAPI D3DXCompileShaderFromFileW(
    LPCWSTR                         pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    LPCSTR                          pFunctionName,
    LPCSTR                          pProfile,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs,
    LPD3DXCONSTANTTABLE* ppConstantTable)
{
    return (*pD3DXCompileShaderFromFileW)(pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable);
}

HRESULT WINAPI D3DXCompileShaderFromResourceA(
    HMODULE                         hSrcModule,
    LPCSTR                          pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    LPCSTR                          pFunctionName,
    LPCSTR                          pProfile,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs,
    LPD3DXCONSTANTTABLE* ppConstantTable)
{
    return (*pD3DXCompileShaderFromResourceA)(hSrcModule, pSrcResource, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable);
}

HRESULT WINAPI D3DXCompileShaderFromResourceW(
    HMODULE                         hSrcModule,
    LPCWSTR                         pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE                   pInclude,
    LPCSTR                          pFunctionName,
    LPCSTR                          pProfile,
    DWORD                           Flags,
    LPD3DXBUFFER* ppShader,
    LPD3DXBUFFER* ppErrorMsgs,
    LPD3DXCONSTANTTABLE* ppConstantTable)
{
    return (*pD3DXCompileShaderFromResourceW)(hSrcModule, pSrcResource, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable);
}

HRESULT WINAPI D3DXComputeBoundingBox(
    CONST D3DXVECTOR3* pFirstPosition,
    DWORD NumVertices,
    DWORD dwStride,
    D3DXVECTOR3* pMin,
    D3DXVECTOR3* pMax)
{
    return (*pD3DXComputeBoundingBox)(pFirstPosition, NumVertices, dwStride, pMin, pMax);
}

HRESULT WINAPI D3DXComputeBoundingSphere(
    CONST D3DXVECTOR3* pFirstPosition,
    DWORD NumVertices,
    DWORD dwStride,
    D3DXVECTOR3* pCenter,
    FLOAT* pRadius)
{
    return (*pD3DXComputeBoundingSphere)(pFirstPosition, NumVertices, dwStride, pCenter, pRadius);
}

HRESULT WINAPI D3DXComputeIMTFromPerTexelSignal(
    LPD3DXMESH pMesh,
    DWORD dwTextureIndex,
    FLOAT* pfTexelSignal,
    UINT uWidth,
    UINT uHeight,
    UINT uSignalDimension,
    UINT uComponents,
    DWORD dwOptions,
    LPD3DXUVATLASCB pStatusCallback,
    LPVOID pUserContext,
    LPD3DXBUFFER* ppIMTData)
{
    return (*pD3DXComputeIMTFromPerTexelSignal)(pMesh, dwTextureIndex, pfTexelSignal, uWidth, uHeight, uSignalDimension, uComponents, dwOptions, pStatusCallback, pUserContext, ppIMTData);
}

HRESULT WINAPI D3DXComputeIMTFromPerVertexSignal(
    LPD3DXMESH pMesh,
    CONST FLOAT* pfVertexSignal,
    UINT uSignalDimension,
    UINT uSignalStride,
    DWORD dwOptions,
    LPD3DXUVATLASCB pStatusCallback,
    LPVOID pUserContext,
    LPD3DXBUFFER* ppIMTData)
{
    return (*pD3DXComputeIMTFromPerVertexSignal)(pMesh, pfVertexSignal, uSignalDimension, uSignalStride, dwOptions, pStatusCallback, pUserContext, ppIMTData);
}

HRESULT WINAPI D3DXComputeIMTFromSignal(
    LPD3DXMESH pMesh,
    DWORD dwTextureIndex,
    UINT uSignalDimension,
    FLOAT fMaxUVDistance,
    DWORD dwOptions,
    LPD3DXIMTSIGNALCALLBACK pSignalCallback,
    VOID* pUserData,
    LPD3DXUVATLASCB pStatusCallback,
    LPVOID pUserContext,
    LPD3DXBUFFER* ppIMTData)
{
    return (*pD3DXComputeIMTFromSignal)(pMesh, dwTextureIndex, uSignalDimension, fMaxUVDistance, dwOptions,
        pSignalCallback, pUserData, pStatusCallback, pUserContext, ppIMTData);
}

HRESULT WINAPI D3DXComputeIMTFromTexture(
    LPD3DXMESH pMesh,
    LPDIRECT3DTEXTURE9 pTexture,
    DWORD dwTextureIndex,
    DWORD dwOptions,
    LPD3DXUVATLASCB pStatusCallback,
    LPVOID pUserContext,
    LPD3DXBUFFER* ppIMTData)
{
    return (*pD3DXComputeIMTFromTexture)(pMesh, pTexture, dwTextureIndex, dwOptions, pStatusCallback, pUserContext, ppIMTData);
}

HRESULT WINAPI D3DXComputeNormalMap(
    LPDIRECT3DTEXTURE9        pTexture,
    LPDIRECT3DTEXTURE9        pSrcTexture,
    CONST PALETTEENTRY* pSrcPalette,
    DWORD                     Flags,
    DWORD                     Channel,
    FLOAT                     Amplitude)
{
    return (*pD3DXComputeNormalMap)(pTexture, pSrcTexture, pSrcPalette, Flags, Channel, Amplitude);
}

HRESULT WINAPI D3DXComputeNormals(
    LPD3DXBASEMESH pMesh,
    CONST DWORD* pAdjacency)
{
    return (*pD3DXComputeNormals)(pMesh, pAdjacency);
}

HRESULT WINAPI D3DXComputeTangentFrame(
    ID3DXMesh* pMesh,
    DWORD dwOptions)
{
    return (*pD3DXComputeTangentFrame)(pMesh, dwOptions);
}

HRESULT WINAPI D3DXComputeTangentFrameEx(
    ID3DXMesh* pMesh,
    DWORD dwTextureInSemantic,
    DWORD dwTextureInIndex,
    DWORD dwUPartialOutSemantic,
    DWORD dwUPartialOutIndex,
    DWORD dwVPartialOutSemantic,
    DWORD dwVPartialOutIndex,
    DWORD dwNormalOutSemantic,
    DWORD dwNormalOutIndex,
    DWORD dwOptions,
    CONST DWORD* pdwAdjacency,
    FLOAT fPartialEdgeThreshold,
    FLOAT fSingularPointThreshold,
    FLOAT fNormalEdgeThreshold,
    ID3DXMesh** ppMeshOut,
    ID3DXBuffer** ppVertexMapping)
{
    return (*pD3DXComputeTangentFrameEx)(pMesh, dwTextureInSemantic, dwTextureInIndex, dwUPartialOutSemantic, dwUPartialOutIndex, dwVPartialOutSemantic, dwVPartialOutIndex, dwNormalOutSemantic, dwNormalOutIndex, dwOptions, pdwAdjacency, fPartialEdgeThreshold, fSingularPointThreshold, fNormalEdgeThreshold, ppMeshOut, ppVertexMapping);
}

HRESULT WINAPI D3DXConcatenateMeshes(
    LPD3DXMESH* ppMeshes,
    UINT NumMeshes,
    DWORD Options,
    CONST D3DXMATRIX* pGeomXForms,
    CONST D3DXMATRIX* pTextureXForms,
    CONST D3DVERTEXELEMENT9* pDecl,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXMESH* ppMeshOut)
{
    return (*pD3DXConcatenateMeshes)(ppMeshes, NumMeshes, Options, pGeomXForms, pTextureXForms, pDecl, pD3DDevice, ppMeshOut);
}

HRESULT WINAPI D3DXConvertMeshSubsetToSingleStrip(
    LPD3DXBASEMESH MeshIn,
    DWORD AttribId,
    DWORD IBOptions,
    LPDIRECT3DINDEXBUFFER9* ppIndexBuffer,
    DWORD* pNumIndices)
{
    return (*pD3DXConvertMeshSubsetToSingleStrip)(MeshIn, AttribId, IBOptions, ppIndexBuffer, pNumIndices);
}

HRESULT WINAPI D3DXConvertMeshSubsetToStrips(
    LPD3DXBASEMESH MeshIn,
    DWORD AttribId,
    DWORD IBOptions,
    LPDIRECT3DINDEXBUFFER9* ppIndexBuffer,
    DWORD* pNumIndices,
    LPD3DXBUFFER* ppStripLengths,
    DWORD* pNumStrips)
{
    return (*pD3DXConvertMeshSubsetToStrips)(MeshIn, AttribId, IBOptions, ppIndexBuffer, pNumIndices, ppStripLengths, pNumStrips);
}

HRESULT WINAPI D3DXCreateAnimationController(
    UINT MaxNumMatrices,
    UINT MaxNumAnimationSets,
    UINT MaxNumTracks,
    UINT MaxNumEvents,
    LPD3DXANIMATIONCONTROLLER* ppAnimController)
{
    return (*pD3DXCreateAnimationController)(MaxNumMatrices, MaxNumAnimationSets, MaxNumTracks, MaxNumEvents, ppAnimController);
}

HRESULT WINAPI D3DXCreateBox(
    LPDIRECT3DDEVICE9   pDevice,
    FLOAT               Width,
    FLOAT               Height,
    FLOAT               Depth,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency)
{
    return (*pD3DXCreateBox)(pDevice, Width, Height, Depth, ppMesh, ppAdjacency);
}

HRESULT WINAPI D3DXCreateBuffer(
    DWORD NumBytes,
    LPD3DXBUFFER* ppBuffer)
{
    return (*pD3DXCreateBuffer)(NumBytes, ppBuffer);
}

HRESULT WINAPI D3DXCreateCompressedAnimationSet(
    LPCSTR pName,
    DOUBLE TicksPerSecond,
    D3DXPLAYBACK_TYPE Playback,
    LPD3DXBUFFER pCompressedData,
    UINT NumCallbackKeys,
    CONST D3DXKEY_CALLBACK* pCallbackKeys,
    LPD3DXCOMPRESSEDANIMATIONSET* ppAnimationSet)
{
    return (*pD3DXCreateCompressedAnimationSet)(pName, TicksPerSecond, Playback, pCompressedData, NumCallbackKeys, pCallbackKeys, ppAnimationSet);
}

HRESULT WINAPI D3DXCreateCubeTexture(
    LPDIRECT3DDEVICE9         pDevice,
    UINT                      Size,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTexture)(pDevice, Size, MipLevels, Usage, Format, Pool, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileA(
    LPDIRECT3DDEVICE9         pDevice,
    LPCSTR                    pSrcFile,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromFileA)(pDevice, pSrcFile, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileExA(
    LPDIRECT3DDEVICE9         pDevice,
    LPCSTR                    pSrcFile,
    UINT                      Size,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    DWORD                     Filter,
    DWORD                     MipFilter,
    D3DCOLOR                  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromFileExA)(pDevice, pSrcFile, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileExW(
    LPDIRECT3DDEVICE9         pDevice,
    LPCWSTR                   pSrcFile,
    UINT                      Size,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    DWORD                     Filter,
    DWORD                     MipFilter,
    D3DCOLOR                  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromFileExW)(pDevice, pSrcFile, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileInMemory(
    LPDIRECT3DDEVICE9         pDevice,
    LPCVOID                   pSrcData,
    UINT                      SrcDataSize,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromFileInMemory)(pDevice, pSrcData, SrcDataSize, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileInMemoryEx(
    LPDIRECT3DDEVICE9         pDevice,
    LPCVOID                   pSrcData,
    UINT                      SrcDataSize,
    UINT                      Size,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    DWORD                     Filter,
    DWORD                     MipFilter,
    D3DCOLOR                  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromFileInMemoryEx)(pDevice, pSrcData, SrcDataSize, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileW(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCWSTR				pSrcFile,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromFileW)(pDevice, pSrcFile, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromResourceA(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromResourceA)(pDevice, hSrcModule, pSrcResource, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromResourceExA(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    UINT				   Size,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromResourceExA)(pDevice, hSrcModule, pSrcResource, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromResourceExW(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    UINT				   Size,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromResourceExW)(pDevice, hSrcModule, pSrcResource, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCubeTextureFromResourceW(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    return (*pD3DXCreateCubeTextureFromResourceW)(pDevice, hSrcModule, pSrcResource, ppCubeTexture);
}

HRESULT WINAPI D3DXCreateCylinder(
    LPDIRECT3DDEVICE9   pDevice,
    FLOAT		    Radius1,
    FLOAT		    Radius2,
    FLOAT		    Length,
    UINT				Slices,
    UINT				Stacks,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency)
{
    return (*pD3DXCreateCylinder)(pDevice, Radius1, Radius2, Length, Slices, Stacks, ppMesh, ppAdjacency);
}

HRESULT WINAPI D3DXCreateEffect(
    LPDIRECT3DDEVICE9		    pDevice,
    LPCVOID						 pSrcData,
    UINT						 SrcDataLen,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{


    return (*pD3DXCreateEffect)(pDevice, pSrcData, SrcDataLen, pDefines, pInclude, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectCompiler(
    LPCSTR						  pSrcData,
    UINT						 SrcDataLen,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTCOMPILER* ppCompiler,
    LPD3DXBUFFER* ppParseErrors)
{
    return (*pD3DXCreateEffectCompiler)(pSrcData, SrcDataLen, pDefines, pInclude, Flags, ppCompiler, ppParseErrors);
}

HRESULT WINAPI D3DXCreateEffectCompilerFromFileA(
    LPCSTR						  pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTCOMPILER* ppCompiler,
    LPD3DXBUFFER* ppParseErrors)
{
    return (*pD3DXCreateEffectCompilerFromFileA)(pSrcFile, pDefines, pInclude, Flags, ppCompiler, ppParseErrors);
}

HRESULT WINAPI D3DXCreateEffectCompilerFromFileW(
    LPCWSTR						 pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTCOMPILER* ppCompiler,
    LPD3DXBUFFER* ppParseErrors)
{
    return (*pD3DXCreateEffectCompilerFromFileW)(pSrcFile, pDefines, pInclude, Flags, ppCompiler, ppParseErrors);
}

HRESULT WINAPI D3DXCreateEffectCompilerFromResourceA(
    HMODULE						 hSrcModule,
    LPCSTR						  pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTCOMPILER* ppCompiler,
    LPD3DXBUFFER* ppParseErrors)
{
    return (*pD3DXCreateEffectCompilerFromResourceA)(hSrcModule, pSrcResource, pDefines, pInclude, Flags, ppCompiler, ppParseErrors);
}

HRESULT WINAPI D3DXCreateEffectCompilerFromResourceW(
    HMODULE						 hSrcModule,
    LPCWSTR						 pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTCOMPILER* ppCompiler,
    LPD3DXBUFFER* ppParseErrors)
{
    return (*pD3DXCreateEffectCompilerFromResourceW)(hSrcModule, pSrcResource, pDefines, pInclude, Flags, ppCompiler, ppParseErrors);
}

HRESULT WINAPI D3DXCreateEffectEx(
    LPDIRECT3DDEVICE9		    pDevice,
    LPCVOID						 pSrcData,
    UINT						 SrcDataLen,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPCSTR						  pSkipConstants,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectEx)(pDevice, pSrcData, SrcDataLen, pDefines, pInclude, pSkipConstants, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromFileA(
    LPDIRECT3DDEVICE9		    pDevice,
    LPCSTR						  pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromFileA)(pDevice, pSrcFile, pDefines, pInclude, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromFileExA(
    LPDIRECT3DDEVICE9		    pDevice,
    LPCSTR						  pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPCSTR						  pSkipConstants,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromFileExA)(pDevice, pSrcFile, pDefines, pInclude, pSkipConstants, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromFileExW(
    LPDIRECT3DDEVICE9		    pDevice,
    LPCWSTR						 pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPCSTR						  pSkipConstants,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromFileExW)(pDevice, pSrcFile, pDefines, pInclude, pSkipConstants, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromFileW(
    LPDIRECT3DDEVICE9		    pDevice,
    LPCWSTR						 pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromFileW)(pDevice, pSrcFile, pDefines, pInclude, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromResourceA(
    LPDIRECT3DDEVICE9		    pDevice,
    HMODULE						 hSrcModule,
    LPCSTR						  pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromResourceA)(pDevice, hSrcModule, pSrcResource, pDefines, pInclude, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromResourceExA(
    LPDIRECT3DDEVICE9		    pDevice,
    HMODULE						 hSrcModule,
    LPCSTR						  pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPCSTR						  pSkipConstants,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromResourceExA)(pDevice, hSrcModule, pSrcResource, pDefines, pInclude, pSkipConstants, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromResourceExW(
    LPDIRECT3DDEVICE9		    pDevice,
    HMODULE						 hSrcModule,
    LPCWSTR						 pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPCSTR						  pSkipConstants,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromResourceExW)(pDevice, hSrcModule, pSrcResource, pDefines, pInclude, pSkipConstants, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectFromResourceW(
    LPDIRECT3DDEVICE9		    pDevice,
    HMODULE						 hSrcModule,
    LPCWSTR						 pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    DWORD						Flags,
    LPD3DXEFFECTPOOL				pPool,
    LPD3DXEFFECT* ppEffect,
    LPD3DXBUFFER* ppCompilationErrors)
{
    return (*pD3DXCreateEffectFromResourceW)(pDevice, hSrcModule, pSrcResource, pDefines, pInclude, Flags, pPool, ppEffect, ppCompilationErrors);
}

HRESULT WINAPI D3DXCreateEffectPool(
    LPD3DXEFFECTPOOL* ppPool)
{
    return (*pD3DXCreateEffectPool)(ppPool);
}

HRESULT WINAPI D3DXCreateFontA(
    LPDIRECT3DDEVICE9       pDevice,
    INT				  Height,
    UINT				 Width,
    UINT				 Weight,
    UINT				 MipLevels,
    BOOL				 Italic,
    DWORD				CharSet,
    DWORD				OutputPrecision,
    DWORD				Quality,
    DWORD				PitchAndFamily,
    LPCSTR				  pFaceName,
    LPD3DXFONT* ppFont)
{
    return (*pD3DXCreateFontA)(pDevice, Height, Width, Weight, MipLevels, Italic, CharSet, OutputPrecision, Quality, PitchAndFamily, pFaceName, ppFont);
}

HRESULT WINAPI D3DXCreateFontIndirectA(
    LPDIRECT3DDEVICE9       pDevice,
    CONST D3DXFONT_DESCA* pDesc,
    LPD3DXFONT* ppFont)
{
    return (*pD3DXCreateFontIndirectA)(pDevice, pDesc, ppFont);
}

HRESULT WINAPI D3DXCreateFontIndirectW(
    LPDIRECT3DDEVICE9       pDevice,
    CONST D3DXFONT_DESCW* pDesc,
    LPD3DXFONT* ppFont)
{
    return (*pD3DXCreateFontIndirectW)(pDevice, pDesc, ppFont);
}

HRESULT WINAPI D3DXCreateFontW(
    LPDIRECT3DDEVICE9       pDevice,
    INT				  Height,
    UINT				 Width,
    UINT				 Weight,
    UINT				 MipLevels,
    BOOL				 Italic,
    DWORD				CharSet,
    DWORD				OutputPrecision,
    DWORD				Quality,
    DWORD				PitchAndFamily,
    LPCWSTR				 pFaceName,
    LPD3DXFONT* ppFont)
{
    return (*pD3DXCreateFontW)(pDevice, Height, Width, Weight, MipLevels, Italic, CharSet, OutputPrecision, Quality, PitchAndFamily, pFaceName, ppFont);
}

HRESULT WINAPI D3DXCreateKeyframedAnimationSet(
    LPCSTR pName,
    DOUBLE TicksPerSecond,
    D3DXPLAYBACK_TYPE Playback,
    UINT NumAnimations,
    UINT NumCallbackKeys,
    CONST D3DXKEY_CALLBACK* pCallbackKeys,
    LPD3DXKEYFRAMEDANIMATIONSET* ppAnimationSet)
{
    return (*pD3DXCreateKeyframedAnimationSet)(pName, TicksPerSecond, Playback, NumAnimations, NumCallbackKeys, pCallbackKeys, ppAnimationSet);
}

HRESULT WINAPI D3DXCreateLine(
    LPDIRECT3DDEVICE9   pDevice,
    LPD3DXLINE* ppLine)
{
    return (*pD3DXCreateLine)(pDevice, ppLine);
}

HRESULT WINAPI D3DXCreateMatrixStack(
    DWORD		    Flags,
    LPD3DXMATRIXSTACK* ppStack)
{
    return (*pD3DXCreateMatrixStack)(Flags, ppStack);
}

HRESULT WINAPI D3DXCreateMesh(
    DWORD NumFaces,
    DWORD NumVertices,
    DWORD Options,
    CONST D3DVERTEXELEMENT9* pDeclaration,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXCreateMesh)(NumFaces, NumVertices, Options, pDeclaration, pD3DDevice, ppMesh);
}

HRESULT WINAPI D3DXCreateMeshFVF(
    DWORD NumFaces,
    DWORD NumVertices,
    DWORD Options,
    DWORD FVF,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXCreateMeshFVF)(NumFaces, NumVertices, Options, FVF, pD3DDevice, ppMesh);
}

HRESULT WINAPI D3DXCreateNPatchMesh(
    LPD3DXMESH pMeshSysMem,
    LPD3DXPATCHMESH* pPatchMesh)
{
    return (*pD3DXCreateNPatchMesh)(pMeshSysMem, pPatchMesh);
}

HRESULT WINAPI D3DXCreatePMeshFromStream(
    IStream* pStream,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    DWORD* pNumMaterials,
    LPD3DXPMESH* ppPMesh)
{
    return (*pD3DXCreatePMeshFromStream)(pStream, Options, pD3DDevice, ppMaterials, ppEffectInstances, pNumMaterials, ppPMesh);
}

HRESULT WINAPI D3DXCreatePRTBuffer(
    UINT NumSamples,
    UINT NumCoeffs,
    UINT NumChannels,
    LPD3DXPRTBUFFER* ppBuffer)
{
    return (*pD3DXCreatePRTBuffer)(NumSamples, NumCoeffs, NumChannels, ppBuffer);
}

HRESULT WINAPI D3DXCreatePRTBufferTex(
    UINT Width,
    UINT Height,
    UINT NumCoeffs,
    UINT NumChannels,
    LPD3DXPRTBUFFER* ppBuffer)
{
    return (*pD3DXCreatePRTBufferTex)(Width, Height, NumCoeffs, NumChannels, ppBuffer);
}

HRESULT WINAPI D3DXCreatePRTCompBuffer(
    D3DXSHCOMPRESSQUALITYTYPE Quality,
    UINT NumClusters,
    UINT NumPCA,
    LPD3DXSHPRTSIMCB pCB,
    LPVOID lpUserContext,
    LPD3DXPRTBUFFER  pBufferIn,
    LPD3DXPRTCOMPBUFFER* ppBufferOut)
{
    return (*pD3DXCreatePRTCompBuffer)(Quality, NumClusters, NumPCA, pCB, lpUserContext, pBufferIn, ppBufferOut);
}

HRESULT WINAPI D3DXCreatePRTEngine(
    LPD3DXMESH pMesh,
    DWORD* pAdjacency,
    BOOL ExtractUVs,
    LPD3DXMESH pBlockerMesh,
    LPD3DXPRTENGINE* ppEngine)
{
    return (*pD3DXCreatePRTEngine)(pMesh, pAdjacency, ExtractUVs, pBlockerMesh, ppEngine);
}

HRESULT WINAPI D3DXCreatePatchMesh(
    CONST D3DXPATCHINFO* pInfo,
    DWORD dwNumPatches,
    DWORD dwNumVertices,
    DWORD dwOptions,
    CONST D3DVERTEXELEMENT9* pDecl,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXPATCHMESH* pPatchMesh)
{
    return (*pD3DXCreatePatchMesh)(pInfo, dwNumPatches, dwNumVertices, dwOptions, pDecl, pD3DDevice, pPatchMesh);
}

HRESULT WINAPI D3DXCreatePolygon(
    LPDIRECT3DDEVICE9   pDevice,
    FLOAT		    Length,
    UINT				Sides,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency)
{
    return (*pD3DXCreatePolygon)(pDevice, Length, Sides, ppMesh, ppAdjacency);
}

HRESULT WINAPI D3DXCreateRenderToEnvMap(
    LPDIRECT3DDEVICE9       pDevice,
    UINT				 Size,
    UINT				 MipLevels,
    D3DFORMAT		    Format,
    BOOL				 DepthStencil,
    D3DFORMAT		    DepthStencilFormat,
    LPD3DXRenderToEnvMap* ppRenderToEnvMap)
{
    return (*pD3DXCreateRenderToEnvMap)(pDevice, Size, MipLevels, Format, DepthStencil, DepthStencilFormat, ppRenderToEnvMap);
}

HRESULT WINAPI D3DXCreateRenderToSurface(
    LPDIRECT3DDEVICE9       pDevice,
    UINT				 Width,
    UINT				 Height,
    D3DFORMAT		    Format,
    BOOL				 DepthStencil,
    D3DFORMAT		    DepthStencilFormat,
    LPD3DXRENDERTOSURFACE* ppRenderToSurface)
{
    return (*pD3DXCreateRenderToSurface)(pDevice, Width, Height, Format, DepthStencil, DepthStencilFormat, ppRenderToSurface);
}

HRESULT WINAPI D3DXCreateSPMesh(
    LPD3DXMESH pMesh,
    CONST DWORD* pAdjacency,
    CONST D3DXATTRIBUTEWEIGHTS* pVertexAttributeWeights,
    CONST FLOAT* pVertexWeights,
    LPD3DXSPMESH* ppSMesh)
{
    return (*pD3DXCreateSPMesh)(pMesh, pAdjacency, pVertexAttributeWeights, pVertexWeights, ppSMesh);
}

HRESULT WINAPI D3DXCreateSkinInfo(
    DWORD NumVertices,
    CONST D3DVERTEXELEMENT9* pDeclaration,
    DWORD NumBones,
    LPD3DXSKININFO* ppSkinInfo)
{
    return (*pD3DXCreateSkinInfo)(NumVertices, pDeclaration, NumBones, ppSkinInfo);
}

HRESULT WINAPI D3DXCreateSkinInfoFVF(
    DWORD NumVertices,
    DWORD FVF,
    DWORD NumBones,
    LPD3DXSKININFO* ppSkinInfo)
{
    return (*pD3DXCreateSkinInfoFVF)(NumVertices, FVF, NumBones, ppSkinInfo);
}

HRESULT WINAPI D3DXCreateSkinInfoFromBlendedMesh(
    LPD3DXBASEMESH pMesh,
    DWORD NumBones,
    CONST D3DXBONECOMBINATION* pBoneCombinationTable,
    LPD3DXSKININFO* ppSkinInfo)
{
    return (*pD3DXCreateSkinInfoFromBlendedMesh)(pMesh, NumBones, pBoneCombinationTable, ppSkinInfo);
}

HRESULT WINAPI D3DXCreateSphere(
    LPDIRECT3DDEVICE9  pDevice,
    FLOAT		   Radius,
    UINT		    Slices,
    UINT		    Stacks,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency)
{
    return (*pD3DXCreateSphere)(pDevice, Radius, Slices, Stacks, ppMesh, ppAdjacency);
}

HRESULT WINAPI D3DXCreateSprite(
    LPDIRECT3DDEVICE9   pDevice,
    LPD3DXSPRITE* ppSprite)
{
    return (*pD3DXCreateSprite)(pDevice, ppSprite);
}

HRESULT WINAPI D3DXCreateTeapot(
    LPDIRECT3DDEVICE9   pDevice,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency)
{
    return (*pD3DXCreateTeapot)(pDevice, ppMesh, ppAdjacency);
}

HRESULT WINAPI D3DXCreateTextA(
    LPDIRECT3DDEVICE9   pDevice,
    HDC				 hDC,
    LPCSTR		   pText,
    FLOAT		    Deviation,
    FLOAT		    Extrusion,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency,
    LPGLYPHMETRICSFLOAT pGlyphMetrics)
{
    return (*pD3DXCreateTextA)(pDevice, hDC, pText, Deviation, Extrusion, ppMesh, ppAdjacency, pGlyphMetrics);
}

HRESULT WINAPI D3DXCreateTextW(
    LPDIRECT3DDEVICE9   pDevice,
    HDC				 hDC,
    LPCWSTR		  pText,
    FLOAT		    Deviation,
    FLOAT		    Extrusion,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency,
    LPGLYPHMETRICSFLOAT pGlyphMetrics)
{
    return (*pD3DXCreateTextW)(pDevice, hDC, pText, Deviation, Extrusion, ppMesh, ppAdjacency, pGlyphMetrics);
}

HRESULT WINAPI D3DXCreateTexture(
    LPDIRECT3DDEVICE9		 pDevice,
    UINT				   Width,
    UINT				   Height,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTexture)(pDevice, Width, Height, MipLevels, Usage, Format, Pool, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileA(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCSTR				 pSrcFile,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromFileA)(pDevice, pSrcFile, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileExA(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCSTR				 pSrcFile,
    UINT				   Width,
    UINT				   Height,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromFileExA)(pDevice, pSrcFile, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileExW(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCWSTR				pSrcFile,
    UINT				   Width,
    UINT				   Height,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromFileExW)(pDevice, pSrcFile, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileInMemory(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCVOID				pSrcData,
    UINT				   SrcDataSize,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromFileInMemory)(pDevice, pSrcData, SrcDataSize, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCVOID				pSrcData,
    UINT				   SrcDataSize,
    UINT				   Width,
    UINT				   Height,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromFileInMemoryEx)(pDevice, pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromFileW(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCWSTR				pSrcFile,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromFileW)(pDevice, pSrcFile, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromResourceA(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromResourceA)(pDevice, hSrcModule, pSrcResource, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromResourceExA(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    UINT				   Width,
    UINT				   Height,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromResourceExA)(pDevice, hSrcModule, pSrcResource, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromResourceExW(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    UINT				   Width,
    UINT				   Height,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromResourceExW)(pDevice, hSrcModule, pSrcResource, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromResourceW(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    LPDIRECT3DTEXTURE9* ppTexture)
{
    return (*pD3DXCreateTextureFromResourceW)(pDevice, hSrcModule, pSrcResource, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureGutterHelper(
    UINT Width,
    UINT Height,
    LPD3DXMESH pMesh,
    FLOAT GutterSize,
    LPD3DXTEXTUREGUTTERHELPER* ppBuffer)
{
    return (*pD3DXCreateTextureGutterHelper)(Width, Height, pMesh, GutterSize, ppBuffer);
}

HRESULT WINAPI D3DXCreateTextureShader(
    CONST DWORD* pFunction,
    LPD3DXTEXTURESHADER* ppTextureShader)
{
    return (*pD3DXCreateTextureShader)(pFunction, ppTextureShader);
}

HRESULT WINAPI D3DXCreateTorus(
    LPDIRECT3DDEVICE9   pDevice,
    FLOAT		    InnerRadius,
    FLOAT		    OuterRadius,
    UINT				Sides,
    UINT				Rings,
    LPD3DXMESH* ppMesh,
    LPD3DXBUFFER* ppAdjacency)
{
    return (*pD3DXCreateTorus)(pDevice, InnerRadius, OuterRadius, Sides, Rings, ppMesh, ppAdjacency);
}

HRESULT WINAPI D3DXCreateVolumeTexture(
    LPDIRECT3DDEVICE9		 pDevice,
    UINT				   Width,
    UINT				   Height,
    UINT				   Depth,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTexture)(pDevice, Width, Height, Depth, MipLevels, Usage, Format, Pool, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromFileA(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCSTR				 pSrcFile,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromFileA)(pDevice, pSrcFile, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromFileExA(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCSTR				 pSrcFile,
    UINT				   Width,
    UINT				   Height,
    UINT				   Depth,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromFileExA)(pDevice, pSrcFile, Width, Height, Depth, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromFileExW(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCWSTR				pSrcFile,
    UINT				   Width,
    UINT				   Height,
    UINT				   Depth,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromFileExW)(pDevice, pSrcFile, Width, Height, Depth, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromFileInMemory(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCVOID				pSrcData,
    UINT				   SrcDataSize,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromFileInMemory)(pDevice, pSrcData, SrcDataSize, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromFileInMemoryEx(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCVOID				pSrcData,
    UINT				   SrcDataSize,
    UINT				   Width,
    UINT				   Height,
    UINT				   Depth,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromFileInMemoryEx)(pDevice, pSrcData, SrcDataSize, Width, Height, Depth, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromFileW(
    LPDIRECT3DDEVICE9		 pDevice,
    LPCWSTR				pSrcFile,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromFileW)(pDevice, pSrcFile, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromResourceA(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromResourceA)(pDevice, hSrcModule, pSrcResource, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromResourceExA(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    UINT				   Width,
    UINT				   Height,
    UINT				   Depth,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromResourceExA)(pDevice, hSrcModule, pSrcResource, Width, Height, Depth, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromResourceExW(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    UINT				   Width,
    UINT				   Height,
    UINT				   Depth,
    UINT				   MipLevels,
    DWORD				  Usage,
    D3DFORMAT				 Format,
    D3DPOOL				Pool,
    DWORD				  Filter,
    DWORD				  MipFilter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromResourceExW)(pDevice, hSrcModule, pSrcResource, Width, Height, Depth, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppVolumeTexture);
}

HRESULT WINAPI D3DXCreateVolumeTextureFromResourceW(
    LPDIRECT3DDEVICE9		 pDevice,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    return (*pD3DXCreateVolumeTextureFromResourceW)(pDevice, hSrcModule, pSrcResource, ppVolumeTexture);
}

BOOL WINAPI D3DXDebugMute(
    BOOL Mute)
{
    return (*pD3DXDebugMute)(Mute);
}

HRESULT WINAPI D3DXDeclaratorFromFVF(
    DWORD FVF,
    D3DVERTEXELEMENT9 pDeclarator[MAX_FVF_DECL_SIZE]
)
{
    return (*pD3DXDeclaratorFromFVF)(FVF, pDeclarator);
}

HRESULT WINAPI D3DXDisassembleEffect(
    LPD3DXEFFECT pEffect,
    BOOL EnableColorCode,
    LPD3DXBUFFER* ppDisassembly)
{
    return (*pD3DXDisassembleEffect)(pEffect, EnableColorCode, ppDisassembly);
}

HRESULT WINAPI D3DXDisassembleShader(
    CONST DWORD* pShader,
    BOOL						 EnableColorCode,
    LPCSTR						  pComments,
    LPD3DXBUFFER* ppDisassembly)
{
    return (*pD3DXDisassembleShader)(pShader, EnableColorCode, pComments, ppDisassembly);
}

HRESULT WINAPI D3DXFVFFromDeclarator(
    CONST D3DVERTEXELEMENT9* pDeclarator,
    DWORD* pFVF)
{
    return (*pD3DXFVFFromDeclarator)(pDeclarator, pFVF);
}

HRESULT WINAPI D3DXFillCubeTexture(
    LPDIRECT3DCUBETEXTURE9    pCubeTexture,
    LPD3DXFILL3D		   pFunction,
    LPVOID				 pData)
{
    return (*pD3DXFillCubeTexture)(pCubeTexture, pFunction, pData);
}

HRESULT WINAPI D3DXFillCubeTextureTX(
    LPDIRECT3DCUBETEXTURE9    pCubeTexture,
    LPD3DXTEXTURESHADER       pTextureShader)
{
    return (*pD3DXFillCubeTextureTX)(pCubeTexture, pTextureShader);
}

HRESULT WINAPI D3DXFillTexture(
    LPDIRECT3DTEXTURE9		pTexture,
    LPD3DXFILL2D		   pFunction,
    LPVOID				 pData)
{
    return (*pD3DXFillTexture)(pTexture, pFunction, pData);
}

HRESULT WINAPI D3DXFillTextureTX(
    LPDIRECT3DTEXTURE9		pTexture,
    LPD3DXTEXTURESHADER       pTextureShader)
{
    return (*pD3DXFillTextureTX)(pTexture, pTextureShader);
}

HRESULT WINAPI D3DXFillVolumeTexture(
    LPDIRECT3DVOLUMETEXTURE9  pVolumeTexture,
    LPD3DXFILL3D		   pFunction,
    LPVOID				 pData)
{
    return (*pD3DXFillVolumeTexture)(pVolumeTexture, pFunction, pData);
}

HRESULT WINAPI D3DXFillVolumeTextureTX(
    LPDIRECT3DVOLUMETEXTURE9  pVolumeTexture,
    LPD3DXTEXTURESHADER       pTextureShader)
{
    return (*pD3DXFillVolumeTextureTX)(pVolumeTexture, pTextureShader);
}

HRESULT WINAPI D3DXFilterTexture(
    LPDIRECT3DBASETEXTURE9    pBaseTexture,
    CONST PALETTEENTRY* pPalette,
    UINT				   SrcLevel,
    DWORD				  Filter)
{
    return (*pD3DXFilterTexture)(pBaseTexture, pPalette, SrcLevel, Filter);
}

HRESULT WINAPI D3DXFindShaderComment(
    CONST DWORD* pFunction,
    DWORD						FourCC,
    LPCVOID* ppData,
    UINT* pSizeInBytes)
{
    return (*pD3DXFindShaderComment)(pFunction, FourCC, ppData, pSizeInBytes);
}

FLOAT* WINAPI D3DXFloat16To32Array(
    FLOAT* pOut,
    CONST D3DXFLOAT16* pIn,
    UINT n)
{
    return (*pD3DXFloat16To32Array)(pOut, pIn, n);
}

D3DXFLOAT16* WINAPI D3DXFloat32To16Array(
    D3DXFLOAT16* pOut,
    CONST FLOAT* pIn,
    UINT n)
{
    return (*pD3DXFloat32To16Array)(pOut, pIn, n);
}

HRESULT WINAPI D3DXFrameAppendChild(
    LPD3DXFRAME pFrameParent,
    CONST D3DXFRAME* pFrameChild)
{
    return (*pD3DXFrameAppendChild)(pFrameParent, pFrameChild);
}

HRESULT WINAPI D3DXFrameCalculateBoundingSphere(
    CONST D3DXFRAME* pFrameRoot,
    LPD3DXVECTOR3 pObjectCenter,
    FLOAT* pObjectRadius)
{
    return (*pD3DXFrameCalculateBoundingSphere)(pFrameRoot, pObjectCenter, pObjectRadius);
}

HRESULT WINAPI D3DXFrameDestroy(
    LPD3DXFRAME pFrameRoot,
    LPD3DXALLOCATEHIERARCHY pAlloc)
{
    return (*pD3DXFrameDestroy)(pFrameRoot, pAlloc);
}

LPD3DXFRAME WINAPI D3DXFrameFind(
    CONST D3DXFRAME* pFrameRoot,
    LPCSTR Name)
{
    return (*pD3DXFrameFind)(pFrameRoot, Name);
}

UINT WINAPI D3DXFrameNumNamedMatrices(
    CONST D3DXFRAME* pFrameRoot)
{
    return (*pD3DXFrameNumNamedMatrices)(pFrameRoot);
}

HRESULT WINAPI D3DXFrameRegisterNamedMatrices(
    LPD3DXFRAME pFrameRoot,
    LPD3DXANIMATIONCONTROLLER pAnimController)
{
    return (*pD3DXFrameRegisterNamedMatrices)(pFrameRoot, pAnimController);
}

FLOAT WINAPI D3DXFresnelTerm(
    FLOAT CosTheta,
    FLOAT RefractionIndex)
{
    return (*pD3DXFresnelTerm)(CosTheta, RefractionIndex);
}

HRESULT WINAPI D3DXGenerateOutputDecl(
    D3DVERTEXELEMENT9* pOutput,
    CONST D3DVERTEXELEMENT9* pInput)
{
    return (*pD3DXGenerateOutputDecl)(pOutput, pInput);
}

HRESULT WINAPI D3DXGeneratePMesh(
    LPD3DXMESH pMesh,
    CONST DWORD* pAdjacency,
    CONST D3DXATTRIBUTEWEIGHTS* pVertexAttributeWeights,
    CONST FLOAT* pVertexWeights,
    DWORD MinValue,
    DWORD Options,
    LPD3DXPMESH* ppPMesh)
{
    return (*pD3DXGeneratePMesh)(pMesh, pAdjacency, pVertexAttributeWeights, pVertexWeights, MinValue, Options, ppPMesh);
}

UINT WINAPI D3DXGetDeclLength(
    CONST D3DVERTEXELEMENT9* pDecl)
{
    return (*pD3DXGetDeclLength)(pDecl);
}

UINT WINAPI D3DXGetDeclVertexSize(
    CONST D3DVERTEXELEMENT9* pDecl, DWORD Stream)
{
    return (*pD3DXGetDeclVertexSize)(pDecl, Stream);
}

UINT WINAPI D3DXGetDriverLevel(
    LPDIRECT3DDEVICE9 pDevice)
{
    return (*pD3DXGetDriverLevel)(pDevice);
}

UINT WINAPI D3DXGetFVFVertexSize(
    DWORD FVF)
{
    return (*pD3DXGetFVFVertexSize)(FVF);
}

HRESULT WINAPI D3DXGetImageInfoFromFileA(
    LPCSTR				 pSrcFile,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXGetImageInfoFromFileA)(pSrcFile, pSrcInfo);
}

HRESULT WINAPI D3DXGetImageInfoFromFileInMemory(
    LPCVOID				pSrcData,
    UINT				   SrcDataSize,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXGetImageInfoFromFileInMemory)(pSrcData, SrcDataSize, pSrcInfo);
}

HRESULT WINAPI D3DXGetImageInfoFromFileW(
    LPCWSTR				pSrcFile,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXGetImageInfoFromFileW)(pSrcFile, pSrcInfo);
}

HRESULT WINAPI D3DXGetImageInfoFromResourceA(
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXGetImageInfoFromResourceA)(hSrcModule, pSrcResource, pSrcInfo);
}

HRESULT WINAPI D3DXGetImageInfoFromResourceW(
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXGetImageInfoFromResourceW)(hSrcModule, pSrcResource, pSrcInfo);
}

LPCSTR WINAPI D3DXGetPixelShaderProfile(
    LPDIRECT3DDEVICE9		    pDevice)
{
    return (*pD3DXGetPixelShaderProfile)(pDevice);
}

HRESULT WINAPI D3DXGetShaderConstantTable(
    CONST DWORD* pFunction,
    LPD3DXCONSTANTTABLE* ppConstantTable)
{
    return (*pD3DXGetShaderConstantTable)(pFunction, ppConstantTable);
}

HRESULT WINAPI D3DXGetShaderInputSemantics(
    CONST DWORD* pFunction,
    D3DXSEMANTIC* pSemantics,
    UINT* pCount)
{
    return (*pD3DXGetShaderInputSemantics)(pFunction, pSemantics, pCount);
}

HRESULT WINAPI D3DXGetShaderOutputSemantics(
    CONST DWORD* pFunction,
    D3DXSEMANTIC* pSemantics,
    UINT* pCount)
{
    return (*pD3DXGetShaderOutputSemantics)(pFunction, pSemantics, pCount);
}

HRESULT WINAPI D3DXGetShaderSamplers(
    CONST DWORD* pFunction,
    LPCSTR* pSamplers,
    UINT* pCount)
{
    return (*pD3DXGetShaderSamplers)(pFunction, pSamplers, pCount);
}

UINT WINAPI D3DXGetShaderSize(
    CONST DWORD* pFunction)
{
    return (*pD3DXGetShaderSize)(pFunction);
}

DWORD WINAPI D3DXGetShaderVersion(
    CONST DWORD* pFunction)
{
    return (*pD3DXGetShaderVersion)(pFunction);
}

LPCSTR WINAPI D3DXGetVertexShaderProfile(
    LPDIRECT3DDEVICE9		    pDevice)
{
    return (*pD3DXGetVertexShaderProfile)(pDevice);
}

HRESULT WINAPI D3DXIntersect(
    LPD3DXBASEMESH pMesh,
    CONST D3DXVECTOR3* pRayPos,
    CONST D3DXVECTOR3* pRayDir,
    BOOL* pHit,
    DWORD* pFaceIndex,
    FLOAT* pU,
    FLOAT* pV,
    FLOAT* pDist,
    LPD3DXBUFFER* ppAllHits,
    DWORD* pCountOfHits)
{
    return (*pD3DXIntersect)(pMesh, pRayPos, pRayDir, pHit, pFaceIndex, pU, pV, pDist, ppAllHits, pCountOfHits);
}

HRESULT WINAPI D3DXIntersectSubset(
    LPD3DXBASEMESH pMesh,
    DWORD AttribId,
    CONST D3DXVECTOR3* pRayPos,
    CONST D3DXVECTOR3* pRayDir,
    BOOL* pHit,
    DWORD* pFaceIndex,
    FLOAT* pU,
    FLOAT* pV,
    FLOAT* pDist,
    LPD3DXBUFFER* ppAllHits,
    DWORD* pCountOfHits)
{
    return (*pD3DXIntersectSubset)(pMesh, AttribId, pRayPos, pRayDir, pHit, pFaceIndex, pU, pV, pDist, ppAllHits, pCountOfHits);
}

BOOL WINAPI D3DXIntersectTri(
    CONST D3DXVECTOR3* p0,
    CONST D3DXVECTOR3* p1,
    CONST D3DXVECTOR3* p2,
    CONST D3DXVECTOR3* pRayPos,
    CONST D3DXVECTOR3* pRayDir,
    FLOAT* pU,
    FLOAT* pV,
    FLOAT* pDist)
{
    return (*pD3DXIntersectTri)(p0, p1, p2, pRayPos, pRayDir, pU, pV, pDist);
}

HRESULT WINAPI D3DXLoadMeshFromXA(
    LPCSTR pFilename,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppAdjacency,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    DWORD* pNumMaterials,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXLoadMeshFromXA)(pFilename, Options, pD3DDevice, ppAdjacency, ppMaterials, ppEffectInstances, pNumMaterials, ppMesh);
}

HRESULT WINAPI D3DXLoadMeshFromXInMemory(
    LPCVOID Memory,
    DWORD SizeOfMemory,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppAdjacency,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    DWORD* pNumMaterials,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXLoadMeshFromXInMemory)(Memory, SizeOfMemory, Options, pD3DDevice, ppAdjacency, ppMaterials, ppEffectInstances, pNumMaterials, ppMesh);
}

HRESULT WINAPI D3DXLoadMeshFromXResource(
    HMODULE Module,
    LPCSTR Name,
    LPCSTR Type,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppAdjacency,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    DWORD* pNumMaterials,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXLoadMeshFromXResource)(Module, Name, Type, Options, pD3DDevice, ppAdjacency, ppMaterials, ppEffectInstances, pNumMaterials, ppMesh);
}

HRESULT WINAPI D3DXLoadMeshFromXW(
    LPCWSTR pFilename,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppAdjacency,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    DWORD* pNumMaterials,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXLoadMeshFromXW)(pFilename, Options, pD3DDevice, ppAdjacency, ppMaterials, ppEffectInstances, pNumMaterials, ppMesh);
}

HRESULT WINAPI D3DXLoadMeshFromXof(
    LPD3DXFILEDATA pxofMesh,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppAdjacency,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    DWORD* pNumMaterials,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXLoadMeshFromXof)(pxofMesh, Options, pD3DDevice, ppAdjacency, ppMaterials, ppEffectInstances, pNumMaterials, ppMesh);
}

HRESULT WINAPI D3DXLoadMeshHierarchyFromXA(
    LPCSTR Filename,
    DWORD MeshOptions,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXALLOCATEHIERARCHY pAlloc,
    LPD3DXLOADUSERDATA pUserDataLoader,
    LPD3DXFRAME* ppFrameHierarchy,
    LPD3DXANIMATIONCONTROLLER* ppAnimController)
{
    return (*pD3DXLoadMeshHierarchyFromXA)(Filename, MeshOptions, pD3DDevice, pAlloc, pUserDataLoader, ppFrameHierarchy, ppAnimController);
}

HRESULT WINAPI D3DXLoadMeshHierarchyFromXInMemory(
    LPCVOID Memory,
    DWORD SizeOfMemory,
    DWORD MeshOptions,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXALLOCATEHIERARCHY pAlloc,
    LPD3DXLOADUSERDATA pUserDataLoader,
    LPD3DXFRAME* ppFrameHierarchy,
    LPD3DXANIMATIONCONTROLLER* ppAnimController)
{
    return (*pD3DXLoadMeshHierarchyFromXInMemory)(Memory, SizeOfMemory, MeshOptions, pD3DDevice, pAlloc, pUserDataLoader, ppFrameHierarchy, ppAnimController);
}

HRESULT WINAPI D3DXLoadMeshHierarchyFromXW(
    LPCWSTR Filename,
    DWORD MeshOptions,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXALLOCATEHIERARCHY pAlloc,
    LPD3DXLOADUSERDATA pUserDataLoader,
    LPD3DXFRAME* ppFrameHierarchy,
    LPD3DXANIMATIONCONTROLLER* ppAnimController)
{
    return (*pD3DXLoadMeshHierarchyFromXW)(Filename, MeshOptions, pD3DDevice, pAlloc, pUserDataLoader, ppFrameHierarchy, ppAnimController);
}

HRESULT WINAPI D3DXLoadPRTBufferFromFileA(
    LPCSTR pFilename,
    LPD3DXPRTBUFFER* ppBuffer)
{
    return (*pD3DXLoadPRTBufferFromFileA)(pFilename, ppBuffer);
}

HRESULT WINAPI D3DXLoadPRTBufferFromFileW(
    LPCWSTR pFilename,
    LPD3DXPRTBUFFER* ppBuffer)
{
    return (*pD3DXLoadPRTBufferFromFileW)(pFilename, ppBuffer);
}

HRESULT WINAPI D3DXLoadPRTCompBufferFromFileA
(
    LPCSTR pFilename,
    LPD3DXPRTCOMPBUFFER* ppBuffer)
{
    return (*pD3DXLoadPRTCompBufferFromFileA)(pFilename, ppBuffer);
}

HRESULT WINAPI D3DXLoadPRTCompBufferFromFileW(
    LPCWSTR pFilename,
    LPD3DXPRTCOMPBUFFER* ppBuffer)
{
    return (*pD3DXLoadPRTCompBufferFromFileW)(pFilename, ppBuffer);
}

HRESULT WINAPI D3DXLoadPatchMeshFromXof(
    LPD3DXFILEDATA pXofObjMesh,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    PDWORD pNumMaterials,
    LPD3DXPATCHMESH* ppMesh)
{
    return (*pD3DXLoadPatchMeshFromXof)(pXofObjMesh, Options, pD3DDevice, ppMaterials, ppEffectInstances, pNumMaterials, ppMesh);
}

HRESULT WINAPI D3DXLoadSkinMeshFromXof(
    LPD3DXFILEDATA pxofMesh,
    DWORD Options,
    LPDIRECT3DDEVICE9 pD3DDevice,
    LPD3DXBUFFER* ppAdjacency,
    LPD3DXBUFFER* ppMaterials,
    LPD3DXBUFFER* ppEffectInstances,
    DWORD* pMatOut,
    LPD3DXSKININFO* ppSkinInfo,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXLoadSkinMeshFromXof)(pxofMesh, Options, pD3DDevice, ppAdjacency, ppMaterials, ppEffectInstances, pMatOut, ppSkinInfo, ppMesh);
}

HRESULT WINAPI D3DXLoadSurfaceFromFileA(
    LPDIRECT3DSURFACE9		pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    LPCSTR				 pSrcFile,
    CONST RECT* pSrcRect,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadSurfaceFromFileA)(pDestSurface, pDestPalette, pDestRect, pSrcFile, pSrcRect, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadSurfaceFromFileInMemory(
    LPDIRECT3DSURFACE9		pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    LPCVOID				pSrcData,
    UINT				   SrcDataSize,
    CONST RECT* pSrcRect,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadSurfaceFromFileInMemory)(pDestSurface, pDestPalette, pDestRect, pSrcData, SrcDataSize, pSrcRect, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadSurfaceFromFileW(
    LPDIRECT3DSURFACE9		pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    LPCWSTR				pSrcFile,
    CONST RECT* pSrcRect,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadSurfaceFromFileW)(pDestSurface, pDestPalette, pDestRect, pSrcFile, pSrcRect, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadSurfaceFromMemory(
    LPDIRECT3DSURFACE9		pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    LPCVOID				pSrcMemory,
    D3DFORMAT				 SrcFormat,
    UINT				   SrcPitch,
    CONST PALETTEENTRY* pSrcPalette,
    CONST RECT* pSrcRect,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey)
{
    return (*pD3DXLoadSurfaceFromMemory)(pDestSurface, pDestPalette, pDestRect, pSrcMemory, SrcFormat, SrcPitch, pSrcPalette, pSrcRect, Filter, ColorKey);
}

HRESULT WINAPI D3DXLoadSurfaceFromResourceA(
    LPDIRECT3DSURFACE9		pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    CONST RECT* pSrcRect,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadSurfaceFromResourceA)(pDestSurface, pDestPalette, pDestRect, hSrcModule, pSrcResource, pSrcRect, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadSurfaceFromResourceW(
    LPDIRECT3DSURFACE9		pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    CONST RECT* pSrcRect,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadSurfaceFromResourceW)(pDestSurface, pDestPalette, pDestRect, hSrcModule, pSrcResource, pSrcRect, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadSurfaceFromSurface(
    LPDIRECT3DSURFACE9		pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    LPDIRECT3DSURFACE9		pSrcSurface,
    CONST PALETTEENTRY* pSrcPalette,
    CONST RECT* pSrcRect,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey)
{
    return (*pD3DXLoadSurfaceFromSurface)(pDestSurface, pDestPalette, pDestRect, pSrcSurface, pSrcPalette, pSrcRect, Filter, ColorKey);
}

HRESULT WINAPI D3DXLoadVolumeFromFileA(
    LPDIRECT3DVOLUME9		 pDestVolume,
    CONST PALETTEENTRY* pDestPalette,
    CONST D3DBOX* pDestBox,
    LPCSTR				 pSrcFile,
    CONST D3DBOX* pSrcBox,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadVolumeFromFileA)(pDestVolume, pDestPalette, pDestBox, pSrcFile, pSrcBox, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadVolumeFromFileInMemory(
    LPDIRECT3DVOLUME9		 pDestVolume,
    CONST PALETTEENTRY* pDestPalette,
    CONST D3DBOX* pDestBox,
    LPCVOID				pSrcData,
    UINT				   SrcDataSize,
    CONST D3DBOX* pSrcBox,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadVolumeFromFileInMemory)(pDestVolume, pDestPalette, pDestBox, pSrcData, SrcDataSize, pSrcBox, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadVolumeFromFileW(
    LPDIRECT3DVOLUME9		 pDestVolume,
    CONST PALETTEENTRY* pDestPalette,
    CONST D3DBOX* pDestBox,
    LPCWSTR				pSrcFile,
    CONST D3DBOX* pSrcBox,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadVolumeFromFileW)(pDestVolume, pDestPalette, pDestBox, pSrcFile, pSrcBox, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadVolumeFromMemory(
    LPDIRECT3DVOLUME9		 pDestVolume,
    CONST PALETTEENTRY* pDestPalette,
    CONST D3DBOX* pDestBox,
    LPCVOID				pSrcMemory,
    D3DFORMAT				 SrcFormat,
    UINT				   SrcRowPitch,
    UINT				   SrcSlicePitch,
    CONST PALETTEENTRY* pSrcPalette,
    CONST D3DBOX* pSrcBox,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey)
{
    return (*pD3DXLoadVolumeFromMemory)(pDestVolume, pDestPalette, pDestBox, pSrcMemory, SrcFormat, SrcRowPitch, SrcSlicePitch, pSrcPalette, pSrcBox, Filter, ColorKey);
}

HRESULT WINAPI D3DXLoadVolumeFromResourceA(
    LPDIRECT3DVOLUME9		 pDestVolume,
    CONST PALETTEENTRY* pDestPalette,
    CONST D3DBOX* pDestBox,
    HMODULE				hSrcModule,
    LPCSTR				 pSrcResource,
    CONST D3DBOX* pSrcBox,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadVolumeFromResourceA)(pDestVolume, pDestPalette, pDestBox, hSrcModule, pSrcResource, pSrcBox, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadVolumeFromResourceW(
    LPDIRECT3DVOLUME9		 pDestVolume,
    CONST PALETTEENTRY* pDestPalette,
    CONST D3DBOX* pDestBox,
    HMODULE				hSrcModule,
    LPCWSTR				pSrcResource,
    CONST D3DBOX* pSrcBox,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo)
{
    return (*pD3DXLoadVolumeFromResourceW)(pDestVolume, pDestPalette, pDestBox, hSrcModule, pSrcResource, pSrcBox, Filter, ColorKey, pSrcInfo);
}

HRESULT WINAPI D3DXLoadVolumeFromVolume(
    LPDIRECT3DVOLUME9		 pDestVolume,
    CONST PALETTEENTRY* pDestPalette,
    CONST D3DBOX* pDestBox,
    LPDIRECT3DVOLUME9		 pSrcVolume,
    CONST PALETTEENTRY* pSrcPalette,
    CONST D3DBOX* pSrcBox,
    DWORD				  Filter,
    D3DCOLOR				  ColorKey)
{
    return (*pD3DXLoadVolumeFromVolume)(pDestVolume, pDestPalette, pDestBox, pSrcVolume, pSrcPalette, pSrcBox, Filter, ColorKey);
}

D3DXMATRIX* WINAPI D3DXMatrixAffineTransformation
(
    D3DXMATRIX* pOut,
    FLOAT Scaling,
    CONST D3DXVECTOR3* pRotationCenter,
    CONST D3DXQUATERNION* pRotation,
    CONST D3DXVECTOR3* pTranslation)
{
    return (*pD3DXMatrixAffineTransformation)(pOut, Scaling, pRotationCenter, pRotation, pTranslation);
}

D3DXMATRIX* WINAPI D3DXMatrixAffineTransformation2D(
    D3DXMATRIX* pOut,
    FLOAT Scaling,
    CONST D3DXVECTOR2* pRotationCenter,
    FLOAT Rotation,
    CONST D3DXVECTOR2* pTranslation)
{
    return (*pD3DXMatrixAffineTransformation2D)(pOut, Scaling, pRotationCenter, Rotation, pTranslation);
}

HRESULT WINAPI D3DXMatrixDecompose(
    D3DXVECTOR3* pOutScale,
    D3DXQUATERNION* pOutRotation,
    D3DXVECTOR3* pOutTranslation,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXMatrixDecompose)(pOutScale, pOutRotation, pOutTranslation, pM);
}

FLOAT WINAPI D3DXMatrixDeterminant(
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXMatrixDeterminant)(pM);
}

D3DXMATRIX* WINAPI D3DXMatrixInverse(
    D3DXMATRIX* pOut,
    FLOAT* pDeterminant,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXMatrixInverse)(pOut, pDeterminant, pM);
}


D3DXMATRIX* WINAPI D3DXMatrixLookAtLH(
    D3DXMATRIX* pOut,
    CONST D3DXVECTOR3* pEye,
    CONST D3DXVECTOR3* pAt,
    CONST D3DXVECTOR3* pUp)
{
    return (*pD3DXMatrixLookAtLH)(pOut, pEye, pAt, pUp);
}

D3DXMATRIX* WINAPI D3DXMatrixLookAtRH(
    D3DXMATRIX* pOut,
    CONST D3DXVECTOR3* pEye,
    CONST D3DXVECTOR3* pAt,
    CONST D3DXVECTOR3* pUp)
{
    return (*pD3DXMatrixLookAtRH)(pOut, pEye, pAt, pUp);
}

D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX* pOut,
    CONST D3DXMATRIX* pM1,
    CONST D3DXMATRIX* pM2)
{
    return (*pD3DXMatrixMultiply)(pOut, pM1, pM2);
}

D3DXMATRIX* WINAPI D3DXMatrixMultiplyTranspose(
    D3DXMATRIX* pOut,
    CONST D3DXMATRIX* pM1,
    CONST D3DXMATRIX* pM2)
{
    return (*pD3DXMatrixMultiplyTranspose)(pOut, pM1, pM2);
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoLH(
    D3DXMATRIX* pOut,
    FLOAT w,
    FLOAT h,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixOrthoLH)(pOut, w, h, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoOffCenterLH(
    D3DXMATRIX* pOut,
    FLOAT l,
    FLOAT r,
    FLOAT b,
    FLOAT t,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixOrthoOffCenterLH)(pOut, l, r, b, t, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoOffCenterRH(
    D3DXMATRIX* pOut,
    FLOAT l,
    FLOAT r,
    FLOAT b,
    FLOAT t,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixOrthoOffCenterRH)(pOut, l, r, b, t, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoRH(
    D3DXMATRIX* pOut,
    FLOAT w,
    FLOAT h,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixOrthoRH)(pOut, w, h, zn, zf);
}



D3DXMATRIX* WINAPI D3DXMatrixPerspectiveFovLH(
    D3DXMATRIX* pOut,
    FLOAT fovy,
    FLOAT Aspect,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixPerspectiveFovLH)(pOut, fovy, Aspect, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveFovRH(
    D3DXMATRIX* pOut,
    FLOAT fovy,
    FLOAT Aspect,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixPerspectiveFovRH)(pOut, fovy, Aspect, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveLH(
    D3DXMATRIX* pOut,
    FLOAT w,
    FLOAT h,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixPerspectiveLH)(pOut, w, h, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveOffCenterLH(
    D3DXMATRIX* pOut,
    FLOAT l,
    FLOAT r,
    FLOAT b,
    FLOAT t,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixPerspectiveOffCenterLH)(pOut, l, r, b, t, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveOffCenterRH(
    D3DXMATRIX* pOut,
    FLOAT l,
    FLOAT r,
    FLOAT b,
    FLOAT t,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixPerspectiveOffCenterRH)(pOut, l, r, b, t, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveRH(
    D3DXMATRIX* pOut,
    FLOAT w,
    FLOAT h,
    FLOAT zn,
    FLOAT zf)
{
    return (*pD3DXMatrixPerspectiveRH)(pOut, w, h, zn, zf);
}

D3DXMATRIX* WINAPI D3DXMatrixReflect(
    D3DXMATRIX* pOut,
    CONST D3DXPLANE* pPlane)
{
    return (*pD3DXMatrixReflect)(pOut, pPlane);
}

D3DXMATRIX* WINAPI D3DXMatrixRotationAxis(
    D3DXMATRIX* pOut,
    CONST D3DXVECTOR3* pV,
    FLOAT Angle)
{
    return (*pD3DXMatrixRotationAxis)(pOut, pV, Angle);
}

D3DXMATRIX* WINAPI D3DXMatrixRotationQuaternion(
    D3DXMATRIX* pOut,
    CONST D3DXQUATERNION* pQ)
{
    return (*pD3DXMatrixRotationQuaternion)(pOut, pQ);
}

D3DXMATRIX* WINAPI D3DXMatrixRotationX(
    D3DXMATRIX* pOut,
    FLOAT Angle)
{
    return (*pD3DXMatrixRotationX)(pOut, Angle);
}

D3DXMATRIX* WINAPI D3DXMatrixRotationY(
    D3DXMATRIX* pOut,
    FLOAT Angle)
{
    return (*pD3DXMatrixRotationY)(pOut, Angle);
}

D3DXMATRIX* WINAPI D3DXMatrixRotationYawPitchRoll(
    D3DXMATRIX* pOut,
    FLOAT Yaw,
    FLOAT Pitch,
    FLOAT Roll)
{
    return (*pD3DXMatrixRotationYawPitchRoll)(pOut, Yaw, Pitch, Roll);
}

D3DXMATRIX* WINAPI D3DXMatrixRotationZ(
    D3DXMATRIX* pOut,
    FLOAT Angle)
{
    return (*pD3DXMatrixRotationZ)(pOut, Angle);
}

D3DXMATRIX* WINAPI D3DXMatrixScaling(
    D3DXMATRIX* pOut,
    FLOAT sx,
    FLOAT sy,
    FLOAT sz)
{
    return (*pD3DXMatrixScaling)(pOut, sx, sy, sz);
}

D3DXMATRIX* WINAPI D3DXMatrixShadow(
    D3DXMATRIX* pOut,
    CONST D3DXVECTOR4* pLight,
    CONST D3DXPLANE* pPlane)
{
    return (*pD3DXMatrixShadow)(pOut, pLight, pPlane);
}

D3DXMATRIX* WINAPI D3DXMatrixTransformation(
    D3DXMATRIX* pOut,
    CONST D3DXVECTOR3* pScalingCenter,
    CONST D3DXQUATERNION* pScalingRotation,
    CONST D3DXVECTOR3* pScaling,
    CONST D3DXVECTOR3* pRotationCenter,
    CONST D3DXQUATERNION* pRotation,
    CONST D3DXVECTOR3* pTranslation)
{
    return (*pD3DXMatrixTransformation)(pOut, pScalingCenter, pScalingRotation, pScaling, pRotationCenter, pRotation, pTranslation);
}

D3DXMATRIX* WINAPI D3DXMatrixTransformation2D(
    D3DXMATRIX* pOut,
    CONST D3DXVECTOR2* pScalingCenter,
    FLOAT ScalingRotation,
    CONST D3DXVECTOR2* pScaling,
    CONST  D3DXVECTOR2* pRotationCenter,
    FLOAT Rotation,
    CONST  D3DXVECTOR2* pTranslation)
{
    return (*pD3DXMatrixTransformation2D)(pOut, pScalingCenter, ScalingRotation, pScaling, pRotationCenter, Rotation, pTranslation);
}

D3DXMATRIX* WINAPI D3DXMatrixTranslation(
    D3DXMATRIX* pOut,
    FLOAT x,
    FLOAT y,
    FLOAT z)
{
    return (*pD3DXMatrixTranslation)(pOut, x, y, z);
}

D3DXMATRIX* WINAPI D3DXMatrixTranspose(
    D3DXMATRIX* pOut,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXMatrixTranspose)(pOut, pM);
}

HRESULT WINAPI D3DXOptimizeFaces(
    LPCVOID pbIndices,
    UINT cFaces,
    UINT cVertices,
    BOOL b32BitIndices,
    DWORD* pFaceRemap)
{
    return (*pD3DXOptimizeFaces)(pbIndices, cFaces, cVertices, b32BitIndices, pFaceRemap);
}

HRESULT WINAPI D3DXOptimizeVertices(
    LPCVOID pbIndices,
    UINT cFaces,
    UINT cVertices,
    BOOL b32BitIndices,
    DWORD* pVertexRemap)
{
    return (*pD3DXOptimizeVertices)(pbIndices, cFaces, cVertices, b32BitIndices, pVertexRemap);
}

D3DXPLANE* WINAPI D3DXPlaneFromPointNormal(
    D3DXPLANE* pOut,
    CONST D3DXVECTOR3* pPoint,
    CONST D3DXVECTOR3* pNormal)
{
    return (*pD3DXPlaneFromPointNormal)(pOut, pPoint, pNormal);
}

D3DXPLANE* WINAPI D3DXPlaneFromPoints(
    D3DXPLANE* pOut,
    CONST D3DXVECTOR3* pV1,
    CONST D3DXVECTOR3* pV2,
    CONST D3DXVECTOR3* pV3)
{
    return (*pD3DXPlaneFromPoints)(pOut, pV1, pV2, pV3);
}

D3DXVECTOR3* WINAPI D3DXPlaneIntersectLine(
    D3DXVECTOR3* pOut,
    CONST D3DXPLANE* pP,
    CONST D3DXVECTOR3* pV1,
    CONST D3DXVECTOR3* pV2)
{
    return (*pD3DXPlaneIntersectLine)(pOut, pP, pV1, pV2);
}

D3DXPLANE* WINAPI D3DXPlaneNormalize(
    D3DXPLANE* pOut,
    CONST D3DXPLANE* pP)
{
    return (*pD3DXPlaneNormalize)(pOut, pP);
}

D3DXPLANE* WINAPI D3DXPlaneTransform(
    D3DXPLANE* pOut,
    CONST D3DXPLANE* pP,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXPlaneTransform)(pOut, pP, pM);
}

D3DXPLANE* WINAPI D3DXPlaneTransformArray(
    D3DXPLANE* pOut,
    UINT OutStride,
    CONST D3DXPLANE* pP,
    UINT PStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXPlaneTransformArray)(pOut, OutStride, pP, PStride, pM, n);
}

HRESULT WINAPI D3DXPreprocessShader(
    LPCSTR				    pSrcData,
    UINT						 SrcDataSize,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPD3DXBUFFER* ppShaderText,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXPreprocessShader)(pSrcData, SrcDataSize, pDefines, pInclude, ppShaderText, ppErrorMsgs);
}

HRESULT WINAPI D3DXPreprocessShaderFromFileA(
    LPCSTR				    pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPD3DXBUFFER* ppShaderText,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXPreprocessShaderFromFileA)(pSrcFile, pDefines, pInclude, ppShaderText, ppErrorMsgs);
}

HRESULT WINAPI D3DXPreprocessShaderFromFileW(
    LPCWSTR				   pSrcFile,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPD3DXBUFFER* ppShaderText,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXPreprocessShaderFromFileW)(pSrcFile, pDefines, pInclude, ppShaderText, ppErrorMsgs);
}

HRESULT WINAPI D3DXPreprocessShaderFromResourceA(
    HMODULE				   hSrcModule,
    LPCSTR				    pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPD3DXBUFFER* ppShaderText,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXPreprocessShaderFromResourceA)(hSrcModule, pSrcResource, pDefines, pInclude, ppShaderText, ppErrorMsgs);
}

HRESULT WINAPI D3DXPreprocessShaderFromResourceW(
    HMODULE				   hSrcModule,
    LPCWSTR				   pSrcResource,
    CONST D3DXMACRO* pDefines,
    LPD3DXINCLUDE				pInclude,
    LPD3DXBUFFER* ppShaderText,
    LPD3DXBUFFER* ppErrorMsgs)
{
    return (*pD3DXPreprocessShaderFromResourceW)(hSrcModule, pSrcResource, pDefines, pInclude, ppShaderText, ppErrorMsgs);
}

D3DXQUATERNION* WINAPI D3DXQuaternionBaryCentric(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ1,
    CONST D3DXQUATERNION* pQ2,
    CONST D3DXQUATERNION* pQ3,
    FLOAT f,
    FLOAT g)
{
    return (*pD3DXQuaternionBaryCentric)(pOut, pQ1, pQ2, pQ3, f, g);
}

D3DXQUATERNION* WINAPI D3DXQuaternionExp(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ)
{
    return (*pD3DXQuaternionExp)(pOut, pQ);
}

D3DXQUATERNION* WINAPI D3DXQuaternionInverse(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ)
{
    return (*pD3DXQuaternionInverse)(pOut, pQ);
}

D3DXQUATERNION* WINAPI D3DXQuaternionLn(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ)
{
    return (*pD3DXQuaternionLn)(pOut, pQ);
}

D3DXQUATERNION* WINAPI D3DXQuaternionMultiply(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ1,
    CONST D3DXQUATERNION* pQ2)
{
    return (*pD3DXQuaternionMultiply)(pOut, pQ1, pQ2);
}

D3DXQUATERNION* WINAPI D3DXQuaternionNormalize(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ)
{
    return (*pD3DXQuaternionNormalize)(pOut, pQ);
}

D3DXQUATERNION* WINAPI D3DXQuaternionRotationAxis(
    D3DXQUATERNION* pOut,
    CONST D3DXVECTOR3* pV,
    FLOAT Angle)
{
    return (*pD3DXQuaternionRotationAxis)(pOut, pV, Angle);
}

D3DXQUATERNION* WINAPI D3DXQuaternionRotationMatrix(
    D3DXQUATERNION* pOut,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXQuaternionRotationMatrix)(pOut, pM);
}

D3DXQUATERNION* WINAPI D3DXQuaternionRotationYawPitchRoll(
    D3DXQUATERNION* pOut,
    FLOAT Yaw,
    FLOAT Pitch,
    FLOAT Roll)
{
    return (*pD3DXQuaternionRotationYawPitchRoll)(pOut, Yaw, Pitch, Roll);
}

D3DXQUATERNION* WINAPI D3DXQuaternionSlerp(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ1,
    CONST D3DXQUATERNION* pQ2,
    FLOAT t)
{
    return (*pD3DXQuaternionSlerp)(pOut, pQ1, pQ2, t);
}

D3DXQUATERNION* WINAPI D3DXQuaternionSquad(
    D3DXQUATERNION* pOut,
    CONST D3DXQUATERNION* pQ1,
    CONST D3DXQUATERNION* pA,
    CONST D3DXQUATERNION* pB,
    CONST D3DXQUATERNION* pC,
    FLOAT t)
{
    return (*pD3DXQuaternionSquad)(pOut, pQ1, pA, pB, pC, t);
}

void WINAPI D3DXQuaternionSquadSetup(
    D3DXQUATERNION* pAOut,
    D3DXQUATERNION* pBOut,
    D3DXQUATERNION* pCOut,
    CONST D3DXQUATERNION* pQ0,
    CONST D3DXQUATERNION* pQ1,
    CONST D3DXQUATERNION* pQ2,
    CONST D3DXQUATERNION* pQ3)
{
    return (*pD3DXQuaternionSquadSetup)(pAOut, pBOut, pCOut, pQ0, pQ1, pQ2, pQ3);
}

void WINAPI D3DXQuaternionToAxisAngle(
    CONST D3DXQUATERNION* pQ,
    D3DXVECTOR3* pAxis,
    FLOAT* pAngle)
{
    return (*pD3DXQuaternionToAxisAngle)(pQ, pAxis, pAngle);
}

HRESULT WINAPI D3DXRectPatchSize(
    CONST FLOAT* pfNumSegs,
    DWORD* pdwTriangles,
    DWORD* pdwVertices)
{
    return (*pD3DXRectPatchSize)(pfNumSegs, pdwTriangles, pdwVertices);
}

FLOAT* WINAPI D3DXSHAdd(
    FLOAT* pOut,
    UINT Order,
    CONST FLOAT* pA,
    CONST FLOAT* pB)
{
    return (*pD3DXSHAdd)(pOut, Order, pA, pB);
}

FLOAT WINAPI D3DXSHDot(
    UINT Order,
    CONST FLOAT* pA,
    CONST FLOAT* pB)
{
    return (*pD3DXSHDot)(Order, pA, pB);
}

HRESULT WINAPI D3DXSHEvalConeLight(
    UINT Order,
    CONST D3DXVECTOR3* pDir,
    FLOAT Radius,
    FLOAT RIntensity,
    FLOAT GIntensity,
    FLOAT BIntensity,
    FLOAT* pROut,
    FLOAT* pGOut,
    FLOAT* pBOut)
{
    return (*pD3DXSHEvalConeLight)(Order, pDir, Radius, RIntensity, GIntensity, BIntensity, pROut, pGOut, pBOut);
}

FLOAT* WINAPI D3DXSHEvalDirection(
    FLOAT* pOut,
    UINT Order,
    CONST D3DXVECTOR3* pDir)
{
    return (*pD3DXSHEvalDirection)(pOut, Order, pDir);
}

HRESULT WINAPI D3DXSHEvalDirectionalLight(
    UINT Order,
    CONST D3DXVECTOR3* pDir,
    FLOAT RIntensity,
    FLOAT GIntensity,
    FLOAT BIntensity,
    FLOAT* pROut,
    FLOAT* pGOut,
    FLOAT* pBOut)
{
    return (*pD3DXSHEvalDirectionalLight)(Order, pDir, RIntensity, GIntensity, BIntensity, pROut, pGOut, pBOut);
}

HRESULT WINAPI D3DXSHEvalHemisphereLight(
    UINT Order,
    CONST D3DXVECTOR3* pDir,
    D3DXCOLOR Top,
    D3DXCOLOR Bottom,
    FLOAT* pROut,
    FLOAT* pGOut,
    FLOAT* pBOut)
{
    return (*pD3DXSHEvalHemisphereLight)(Order, pDir, Top, Bottom, pROut, pGOut, pBOut);
}

HRESULT WINAPI D3DXSHEvalSphericalLight(
    UINT Order,
    CONST D3DXVECTOR3* pPos,
    FLOAT Radius,
    FLOAT RIntensity,
    FLOAT GIntensity,
    FLOAT BIntensity,
    FLOAT* pROut,
    FLOAT* pGOut,
    FLOAT* pBOut)
{
    return (*pD3DXSHEvalSphericalLight)(Order, pPos, Radius, RIntensity, GIntensity, BIntensity, pROut, pGOut, pBOut);
}

FLOAT* WINAPI D3DXSHMultiply2(
    FLOAT* pOut,
    CONST FLOAT* pF,
    CONST FLOAT* pG)
{
    return (*pD3DXSHMultiply2)(pOut, pF, pG);
}

FLOAT* WINAPI D3DXSHMultiply3(
    FLOAT* pOut,
    CONST FLOAT* pF,
    CONST FLOAT* pG)
{
    return (*pD3DXSHMultiply3)(pOut, pF, pG);
}

FLOAT* WINAPI D3DXSHMultiply4(
    FLOAT* pOut,
    CONST FLOAT* pF,
    CONST FLOAT* pG)
{
    return (*pD3DXSHMultiply4)(pOut, pF, pG);
}

FLOAT* WINAPI D3DXSHMultiply5(


    FLOAT* pOut,
    CONST FLOAT* pF,
    CONST FLOAT* pG)
{
    return (*pD3DXSHMultiply5)(pOut, pF, pG);
}

FLOAT* WINAPI D3DXSHMultiply6(


    FLOAT* pOut,
    CONST FLOAT* pF,
    CONST FLOAT* pG)
{
    return (*pD3DXSHMultiply6)(pOut, pF, pG);
}

HRESULT WINAPI D3DXSHPRTCompSplitMeshSC(
    UINT* pClusterIDs,
    UINT NumVertices,
    UINT NumClusters,
    UINT* pSuperClusterIDs,
    UINT NumSuperClusters,
    LPVOID pInputIB,
    BOOL InputIBIs32Bit,
    UINT NumFaces,
    LPD3DXBUFFER* ppIBData,
    UINT* pIBDataLength,
    BOOL OutputIBIs32Bit,
    LPD3DXBUFFER* ppFaceRemap,
    LPD3DXBUFFER* ppVertData,
    UINT* pVertDataLength,
    UINT* pSCClusterList,
    D3DXSHPRTSPLITMESHCLUSTERDATA* pSCData)
{
    return (*pD3DXSHPRTCompSplitMeshSC)(pClusterIDs, NumVertices, NumClusters, pSuperClusterIDs, NumSuperClusters, pInputIB, InputIBIs32Bit, NumFaces, ppIBData, pIBDataLength, OutputIBIs32Bit, ppFaceRemap, ppVertData, pVertDataLength, pSCClusterList, pSCData);
}

HRESULT WINAPI D3DXSHPRTCompSuperCluster(
    UINT* pClusterIDs,
    LPD3DXMESH pScene,
    UINT MaxNumClusters,
    UINT NumClusters,
    UINT* pSuperClusterIDs,
    UINT* pNumSuperClusters)
{
    return (*pD3DXSHPRTCompSuperCluster)(pClusterIDs, pScene, MaxNumClusters, NumClusters, pSuperClusterIDs, pNumSuperClusters);
}

HRESULT WINAPI D3DXSHProjectCubeMap(
    UINT uOrder,
    LPDIRECT3DCUBETEXTURE9 pCubeMap,
    FLOAT* pROut,
    FLOAT* pGOut,
    FLOAT* pBOut)
{
    return (*pD3DXSHProjectCubeMap)(uOrder, pCubeMap, pROut, pGOut, pBOut);
}

FLOAT* WINAPI D3DXSHRotate(
    FLOAT* pOut,
    UINT Order,
    CONST D3DXMATRIX* pMatrix,
    CONST FLOAT* pIn)
{
    return (*pD3DXSHRotate)(pOut, Order, pMatrix, pIn);
}

FLOAT* WINAPI D3DXSHRotateZ(
    FLOAT* pOut,
    UINT Order,
    FLOAT Angle,
    CONST FLOAT* pIn)
{
    return (*pD3DXSHRotateZ)(pOut, Order, Angle, pIn);
}

FLOAT* WINAPI D3DXSHScale(
    FLOAT* pOut,
    UINT Order,
    CONST FLOAT* pIn,
    CONST FLOAT Scale)
{
    return (*pD3DXSHScale)(pOut, Order, pIn, Scale);
}

HRESULT WINAPI D3DXSaveMeshHierarchyToFileA(
    LPCSTR Filename,
    DWORD XFormat,
    CONST D3DXFRAME* pFrameRoot,
    LPD3DXANIMATIONCONTROLLER pAnimcontroller,
    LPD3DXSAVEUSERDATA pUserDataSaver)
{
    return (*pD3DXSaveMeshHierarchyToFileA)(Filename, XFormat, pFrameRoot, pAnimcontroller, pUserDataSaver);
}

HRESULT WINAPI D3DXSaveMeshHierarchyToFileW(
    LPCWSTR Filename,
    DWORD XFormat,
    CONST D3DXFRAME* pFrameRoot,
    LPD3DXANIMATIONCONTROLLER pAnimController,
    LPD3DXSAVEUSERDATA pUserDataSaver)
{
    return (*pD3DXSaveMeshHierarchyToFileW)(Filename, XFormat, pFrameRoot, pAnimController, pUserDataSaver);
}

HRESULT WINAPI D3DXSaveMeshToXA(
    LPCSTR pFilename,
    LPD3DXMESH pMesh,
    CONST DWORD* pAdjacency,
    CONST D3DXMATERIAL* pMaterials,
    CONST D3DXEFFECTINSTANCE* pEffectInstances,
    DWORD NumMaterials,
    DWORD Format)
{
    return (*pD3DXSaveMeshToXA)(pFilename, pMesh, pAdjacency, pMaterials, pEffectInstances, NumMaterials, Format);
}

HRESULT WINAPI D3DXSaveMeshToXW(
    LPCWSTR pFilename,
    LPD3DXMESH pMesh,
    CONST DWORD* pAdjacency,
    CONST D3DXMATERIAL* pMaterials,
    CONST D3DXEFFECTINSTANCE* pEffectInstances,
    DWORD NumMaterials,
    DWORD Format)
{
    return (*pD3DXSaveMeshToXW)(pFilename, pMesh, pAdjacency, pMaterials, pEffectInstances, NumMaterials, Format);
}

HRESULT WINAPI D3DXSavePRTBufferToFileA(
    LPCSTR pFileName,
    LPD3DXPRTBUFFER pBuffer)
{
    return (*pD3DXSavePRTBufferToFileA)(pFileName, pBuffer);
}

HRESULT WINAPI D3DXSavePRTBufferToFileW(
    LPCWSTR pFileName,
    LPD3DXPRTBUFFER pBuffer)
{
    return (*pD3DXSavePRTBufferToFileW)(pFileName, pBuffer);
}

HRESULT WINAPI D3DXSavePRTCompBufferToFileA
(
    LPCSTR pFileName,
    LPD3DXPRTCOMPBUFFER pBuffer)
{
    return (*pD3DXSavePRTCompBufferToFileA)(pFileName, pBuffer);
}

HRESULT WINAPI D3DXSavePRTCompBufferToFileW(
    LPCWSTR pFileName,
    LPD3DXPRTCOMPBUFFER pBuffer)
{
    return (*pD3DXSavePRTCompBufferToFileW)(pFileName, pBuffer);
}

HRESULT WINAPI D3DXSaveSurfaceToFileA(
    LPCSTR				 pDestFile,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DSURFACE9		pSrcSurface,
    CONST PALETTEENTRY* pSrcPalette,
    CONST RECT* pSrcRect)
{
    return (*pD3DXSaveSurfaceToFileA)(pDestFile, DestFormat, pSrcSurface, pSrcPalette, pSrcRect);
}

HRESULT WINAPI D3DXSaveSurfaceToFileInMemory(
    LPD3DXBUFFER* ppDestBuf,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DSURFACE9		pSrcSurface,
    CONST PALETTEENTRY* pSrcPalette,
    CONST RECT* pSrcRect)
{
    return (*pD3DXSaveSurfaceToFileInMemory)(ppDestBuf, DestFormat, pSrcSurface, pSrcPalette, pSrcRect);
}

HRESULT WINAPI D3DXSaveSurfaceToFileW(
    LPCWSTR				pDestFile,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DSURFACE9		pSrcSurface,
    CONST PALETTEENTRY* pSrcPalette,
    CONST RECT* pSrcRect)
{
    return (*pD3DXSaveSurfaceToFileW)(pDestFile, DestFormat, pSrcSurface, pSrcPalette, pSrcRect);
}

HRESULT WINAPI D3DXSaveTextureToFileA(
    LPCSTR				 pDestFile,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DBASETEXTURE9    pSrcTexture,
    CONST PALETTEENTRY* pSrcPalette)
{
    return (*pD3DXSaveTextureToFileA)(pDestFile, DestFormat, pSrcTexture, pSrcPalette);
}

HRESULT WINAPI D3DXSaveTextureToFileInMemory(
    LPD3DXBUFFER* ppDestBuf,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DBASETEXTURE9    pSrcTexture,
    CONST PALETTEENTRY* pSrcPalette)
{
    return (*pD3DXSaveTextureToFileInMemory)(ppDestBuf, DestFormat, pSrcTexture, pSrcPalette);
}

HRESULT WINAPI D3DXSaveTextureToFileW(
    LPCWSTR				pDestFile,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DBASETEXTURE9    pSrcTexture,
    CONST PALETTEENTRY* pSrcPalette)
{
    return (*pD3DXSaveTextureToFileW)(pDestFile, DestFormat, pSrcTexture, pSrcPalette);
}

HRESULT WINAPI D3DXSaveVolumeToFileA(
    LPCSTR				 pDestFile,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DVOLUME9		 pSrcVolume,
    CONST PALETTEENTRY* pSrcPalette,
    CONST D3DBOX* pSrcBox)
{
    return (*pD3DXSaveVolumeToFileA)(pDestFile, DestFormat, pSrcVolume, pSrcPalette, pSrcBox);
}

HRESULT WINAPI D3DXSaveVolumeToFileInMemory(
    LPD3DXBUFFER* ppDestBuf,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DVOLUME9		 pSrcVolume,
    CONST PALETTEENTRY* pSrcPalette,
    CONST D3DBOX* pSrcBox)
{
    return (*pD3DXSaveVolumeToFileInMemory)(ppDestBuf, DestFormat, pSrcVolume, pSrcPalette, pSrcBox);
}

HRESULT WINAPI D3DXSaveVolumeToFileW(
    LPCWSTR				pDestFile,
    D3DXIMAGE_FILEFORMAT      DestFormat,
    LPDIRECT3DVOLUME9		 pSrcVolume,
    CONST PALETTEENTRY* pSrcPalette,
    CONST D3DBOX* pSrcBox)
{
    return (*pD3DXSaveVolumeToFileW)(pDestFile, DestFormat, pSrcVolume, pSrcPalette, pSrcBox);
}

HRESULT WINAPI D3DXSimplifyMesh(
    LPD3DXMESH pMesh,
    CONST DWORD* pAdjacency,
    CONST D3DXATTRIBUTEWEIGHTS* pVertexAttributeWeights,
    CONST FLOAT* pVertexWeights,
    DWORD MinValue,
    DWORD Options,
    LPD3DXMESH* ppMesh)
{
    return (*pD3DXSimplifyMesh)(pMesh, pAdjacency, pVertexAttributeWeights, pVertexWeights, MinValue, Options, ppMesh);
}

BOOL WINAPI D3DXSphereBoundProbe(
    CONST D3DXVECTOR3* pCenter,
    FLOAT Radius,
    CONST D3DXVECTOR3* pRayPosition,
    CONST D3DXVECTOR3* pRayDirection)
{
    return (*pD3DXSphereBoundProbe)(pCenter, Radius, pRayPosition, pRayDirection);
}

HRESULT WINAPI D3DXSplitMesh(
    LPD3DXMESH pMeshIn,
    CONST DWORD* pAdjacencyIn,
    CONST DWORD MaxSize,
    CONST DWORD Options,
    DWORD* pMeshesOut,
    LPD3DXBUFFER* ppMeshArrayOut,
    LPD3DXBUFFER* ppAdjacencyArrayOut,
    LPD3DXBUFFER* ppFaceRemapArrayOut,
    LPD3DXBUFFER* ppVertRemapArrayOut)
{
    return (*pD3DXSplitMesh)(pMeshIn, pAdjacencyIn, MaxSize, Options, pMeshesOut, ppMeshArrayOut, ppAdjacencyArrayOut, ppFaceRemapArrayOut, ppVertRemapArrayOut);
}

HRESULT WINAPI D3DXTessellateNPatches(
    LPD3DXMESH pMeshIn,
    CONST DWORD* pAdjacencyIn,
    FLOAT NumSegs,
    BOOL  QuadraticInterpNormals,
    LPD3DXMESH* ppMeshOut,
    LPD3DXBUFFER* ppAdjacencyOut)
{
    return (*pD3DXTessellateNPatches)(pMeshIn, pAdjacencyIn, NumSegs, QuadraticInterpNormals, ppMeshOut, ppAdjacencyOut);
}

HRESULT WINAPI D3DXTessellateRectPatch(
    LPDIRECT3DVERTEXBUFFER9 pVB,
    CONST FLOAT* pNumSegs,
    CONST D3DVERTEXELEMENT9* pdwInDecl,
    CONST D3DRECTPATCH_INFO* pRectPatchInfo,
    LPD3DXMESH pMesh)
{
    return (*pD3DXTessellateRectPatch)(pVB, pNumSegs, pdwInDecl, pRectPatchInfo, pMesh);
}

HRESULT WINAPI D3DXTessellateTriPatch(
    LPDIRECT3DVERTEXBUFFER9 pVB,
    CONST FLOAT* pNumSegs,
    CONST D3DVERTEXELEMENT9* pInDecl,
    CONST D3DTRIPATCH_INFO* pTriPatchInfo,
    LPD3DXMESH pMesh)
{
    return (*pD3DXTessellateTriPatch)(pVB, pNumSegs, pInDecl, pTriPatchInfo, pMesh);
}

HRESULT WINAPI D3DXTriPatchSize(
    CONST FLOAT* pfNumSegs,
    DWORD* pdwTriangles,
    DWORD* pdwVertices)
{
    return (*pD3DXTriPatchSize)(pfNumSegs, pdwTriangles, pdwVertices);
}

HRESULT WINAPI D3DXValidMesh(
    LPD3DXMESH pMeshIn,
    CONST DWORD* pAdjacency,
    LPD3DXBUFFER* ppErrorsAndWarnings)
{
    return (*pD3DXValidMesh)(pMeshIn, pAdjacency, ppErrorsAndWarnings);
}

HRESULT WINAPI D3DXValidPatchMesh(
    LPD3DXPATCHMESH pMesh,
    DWORD* dwcDegenerateVertices,
    DWORD* dwcDegeneratePatches,
    LPD3DXBUFFER* ppErrorsAndWarnings)
{
    return (*pD3DXValidPatchMesh)(pMesh, dwcDegenerateVertices, dwcDegeneratePatches, ppErrorsAndWarnings);
}

D3DXVECTOR2* WINAPI D3DXVec2BaryCentric(
    D3DXVECTOR2* pOut,
    CONST D3DXVECTOR2* pV1,
    CONST D3DXVECTOR2* pV2,
    CONST D3DXVECTOR2* pV3,
    FLOAT f,
    FLOAT g)
{
    return (*pD3DXVec2BaryCentric)(pOut, pV1, pV2, pV3, f, g);
}

D3DXVECTOR2* WINAPI D3DXVec2CatmullRom(
    D3DXVECTOR2* pOut,
    CONST D3DXVECTOR2* pV0,
    CONST D3DXVECTOR2* pV1,
    CONST D3DXVECTOR2* pV2,
    CONST D3DXVECTOR2* pV3,
    FLOAT s)
{
    return (*pD3DXVec2CatmullRom)(pOut, pV0, pV1, pV2, pV3, s);
}

D3DXVECTOR2* WINAPI D3DXVec2Hermite(
    D3DXVECTOR2* pOut,
    CONST D3DXVECTOR2* pV1,
    CONST D3DXVECTOR2* pT1,
    CONST D3DXVECTOR2* pV2,
    CONST D3DXVECTOR2* pT2,
    FLOAT s)
{
    return (*pD3DXVec2Hermite)(pOut, pV1, pT1, pV2, pT2, s);
}

D3DXVECTOR2* WINAPI D3DXVec2Normalize(
    D3DXVECTOR2* pOut,
    CONST D3DXVECTOR2* pV)
{
    return (*pD3DXVec2Normalize)(pOut, pV);
}

D3DXVECTOR4* WINAPI D3DXVec2Transform(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR2* pV,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXVec2Transform)(pOut, pV, pM);
}

D3DXVECTOR4* WINAPI D3DXVec2TransformArray(
    D3DXVECTOR4* pOut,
    UINT OutStride,
    CONST D3DXVECTOR2* pV,
    UINT VStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXVec2TransformArray)(pOut, OutStride, pV, VStride, pM, n);
}

D3DXVECTOR2* WINAPI D3DXVec2TransformCoord(
    D3DXVECTOR2* pOut,
    CONST D3DXVECTOR2* pV,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXVec2TransformCoord)(pOut, pV, pM);
}

D3DXVECTOR2* WINAPI D3DXVec2TransformCoordArray(
    D3DXVECTOR2* pOut,
    UINT OutStride,
    CONST D3DXVECTOR2* pV,
    UINT VStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXVec2TransformCoordArray)(pOut, OutStride, pV, VStride, pM, n);
}

D3DXVECTOR2* WINAPI D3DXVec2TransformNormal(
    D3DXVECTOR2* pOut,
    CONST D3DXVECTOR2* pV,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXVec2TransformNormal)(pOut, pV, pM);
}

D3DXVECTOR2* WINAPI D3DXVec2TransformNormalArray(
    D3DXVECTOR2* pOut,
    UINT OutStride,
    CONST D3DXVECTOR2* pV,
    UINT VStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXVec2TransformNormalArray)(pOut, OutStride, pV, VStride, pM, n);
}

D3DXVECTOR3* WINAPI D3DXVec3BaryCentric(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV1,
    CONST D3DXVECTOR3* pV2,
    CONST D3DXVECTOR3* pV3,
    FLOAT f,
    FLOAT g)
{
    return (*pD3DXVec3BaryCentric)(pOut, pV1, pV2, pV3, f, g);
}

D3DXVECTOR3* WINAPI D3DXVec3CatmullRom(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV0,
    CONST D3DXVECTOR3* pV1,
    CONST D3DXVECTOR3* pV2,
    CONST D3DXVECTOR3* pV3,
    FLOAT s)
{
    return (*pD3DXVec3CatmullRom)(pOut, pV0, pV1, pV2, pV3, s);
}

D3DXVECTOR3* WINAPI D3DXVec3Hermite(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV1,
    CONST D3DXVECTOR3* pT1,
    CONST D3DXVECTOR3* pV2,
    CONST D3DXVECTOR3* pT2,
    FLOAT s)
{
    return (*pD3DXVec3Hermite)(pOut, pV1, pT1, pV2, pT2, s);
}

D3DXVECTOR3* WINAPI D3DXVec3Normalize(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV)
{
    return (*pD3DXVec3Normalize)(pOut, pV);
}

D3DXVECTOR3* WINAPI D3DXVec3Project(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV,
    CONST D3DVIEWPORT9* pViewport,
    CONST D3DXMATRIX* pProjection,
    CONST D3DXMATRIX* pView,
    CONST D3DXMATRIX* pWorld)
{
    return (*pD3DXVec3Project)(pOut, pV, pViewport, pProjection, pView, pWorld);
}

D3DXVECTOR3* WINAPI D3DXVec3ProjectArray(
    D3DXVECTOR3* pOut,
    UINT OutStride,
    CONST D3DXVECTOR3* pV,
    UINT VStride,
    CONST D3DVIEWPORT9* pViewport,
    CONST D3DXMATRIX* pProjection,
    CONST D3DXMATRIX* pView,
    CONST D3DXMATRIX* pWorld,
    UINT n)
{
    return (*pD3DXVec3ProjectArray)(pOut, OutStride, pV, VStride, pViewport, pProjection, pView, pWorld, n);
}

D3DXVECTOR4* WINAPI D3DXVec3Transform(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR3* pV,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXVec3Transform)(pOut, pV, pM);
}

D3DXVECTOR4* WINAPI D3DXVec3TransformArray(
    D3DXVECTOR4* pOut,
    UINT OutStride,
    CONST D3DXVECTOR3* pV,
    UINT VStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXVec3TransformArray)(pOut, OutStride, pV, VStride, pM, n);
}

D3DXVECTOR3* WINAPI D3DXVec3TransformCoord(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXVec3TransformCoord)(pOut, pV, pM);
}

D3DXVECTOR3* WINAPI D3DXVec3TransformCoordArray(
    D3DXVECTOR3* pOut,
    UINT OutStride,
    CONST D3DXVECTOR3* pV,
    UINT VStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXVec3TransformCoordArray)(pOut, OutStride, pV, VStride, pM, n);
}

D3DXVECTOR3* WINAPI D3DXVec3TransformNormal(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXVec3TransformNormal)(pOut, pV, pM);
}

D3DXVECTOR3* WINAPI D3DXVec3TransformNormalArray(
    D3DXVECTOR3* pOut,
    UINT OutStride,
    CONST D3DXVECTOR3* pV,
    UINT VStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXVec3TransformNormalArray)(pOut, OutStride, pV, VStride, pM, n);
}

D3DXVECTOR3* WINAPI D3DXVec3Unproject(
    D3DXVECTOR3* pOut,
    CONST D3DXVECTOR3* pV,
    CONST D3DVIEWPORT9* pViewport,
    CONST D3DXMATRIX* pProjection,
    CONST D3DXMATRIX* pView,
    CONST D3DXMATRIX* pWorld)
{
    return (*pD3DXVec3Unproject)(pOut, pV, pViewport, pProjection, pView, pWorld);
}

D3DXVECTOR3* WINAPI D3DXVec3UnprojectArray(
    D3DXVECTOR3* pOut,
    UINT OutStride,
    CONST D3DXVECTOR3* pV,
    UINT VStride,
    CONST D3DVIEWPORT9* pViewport,
    CONST D3DXMATRIX* pProjection,
    CONST D3DXMATRIX* pView,
    CONST D3DXMATRIX* pWorld,
    UINT n)
{
    return (*pD3DXVec3UnprojectArray)(pOut, OutStride, pV, VStride, pViewport, pProjection, pView, pWorld, n);
}

D3DXVECTOR4* WINAPI D3DXVec4BaryCentric(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR4* pV1,
    CONST D3DXVECTOR4* pV2,
    CONST  D3DXVECTOR4* pV3,
    FLOAT f,
    FLOAT g)
{
    return (*pD3DXVec4BaryCentric)(pOut, pV1, pV2, pV3, f, g);
}

D3DXVECTOR4* WINAPI D3DXVec4CatmullRom(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR4* pV0,
    CONST D3DXVECTOR4* pV1,
    CONST D3DXVECTOR4* pV2,
    CONST D3DXVECTOR4* pV3,
    FLOAT s)
{
    return (*pD3DXVec4CatmullRom)(pOut, pV0, pV1, pV2, pV3, s);
}

D3DXVECTOR4* WINAPI D3DXVec4Cross(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR4* pV1,
    CONST D3DXVECTOR4* pV2,
    CONST D3DXVECTOR4* pV3)
{
    return (*pD3DXVec4Cross)(pOut, pV1, pV2, pV3);
}

D3DXVECTOR4* WINAPI D3DXVec4Hermite(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR4* pV1,
    CONST D3DXVECTOR4* pT1,
    CONST D3DXVECTOR4* pV2,
    CONST D3DXVECTOR4* pT2,
    FLOAT s)
{
    return (*pD3DXVec4Hermite)(pOut, pV1, pT1, pV2, pT2, s);
}

D3DXVECTOR4* WINAPI D3DXVec4Normalize(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR4* pV)
{
    return (*pD3DXVec4Normalize)(pOut, pV);
}

D3DXVECTOR4* WINAPI D3DXVec4Transform(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR4* pV,
    CONST D3DXMATRIX* pM)
{
    return (*pD3DXVec4Transform)(pOut, pV, pM);
}

D3DXVECTOR4* WINAPI D3DXVec4TransformArray(
    D3DXVECTOR4* pOut,
    UINT OutStride,
    CONST D3DXVECTOR4* pV,
    UINT VStride,
    CONST D3DXMATRIX* pM,
    UINT n)
{
    return (*pD3DXVec4TransformArray)(pOut, OutStride, pV, VStride, pM, n);
}

HRESULT WINAPI D3DXWeldVertices(
    LPD3DXMESH pMesh,
    DWORD Flags,
    CONST D3DXWELDEPSILONS* pEpsilons,
    CONST DWORD* pAdjacencyIn,
    DWORD* pAdjacencyOut,
    DWORD* pFaceRemap,
    LPD3DXBUFFER* ppVertexRemap)
{
    return (*pD3DXWeldVertices)(pMesh, Flags, pEpsilons, pAdjacencyIn, pAdjacencyOut, pFaceRemap, ppVertexRemap);
}

class d3dx9_43
{
public:
    d3dx9_43()
    {
        auto dll = GetD3DX9_43DLL();

        pD3DXAssembleShader = (decltype(D3DXAssembleShader)*)(GetProcAddress(dll, "D3DXAssembleShader"));
        pD3DXAssembleShaderFromFileA = (decltype(D3DXAssembleShaderFromFileA)*)(GetProcAddress(dll, "D3DXAssembleShaderFromFileA"));
        pD3DXAssembleShaderFromFileW = (decltype(D3DXAssembleShaderFromFileW)*)(GetProcAddress(dll, "D3DXAssembleShaderFromFileW"));
        pD3DXAssembleShaderFromResourceA = (decltype(D3DXAssembleShaderFromResourceA)*)(GetProcAddress(dll, "D3DXAssembleShaderFromResourceA"));
        pD3DXAssembleShaderFromResourceW = (decltype(D3DXAssembleShaderFromResourceW)*)(GetProcAddress(dll, "D3DXAssembleShaderFromResourceW"));
        pD3DXBoxBoundProbe = (decltype(D3DXBoxBoundProbe)*)(GetProcAddress(dll, "D3DXBoxBoundProbe"));
        pD3DXCheckCubeTextureRequirements = (decltype(D3DXCheckCubeTextureRequirements)*)(GetProcAddress(dll, "D3DXCheckCubeTextureRequirements"));
        pD3DXCheckTextureRequirements = (decltype(D3DXCheckTextureRequirements)*)(GetProcAddress(dll, "D3DXCheckTextureRequirements"));
        pD3DXCheckVersion = (decltype(D3DXCheckVersion)*)(GetProcAddress(dll, "D3DXCheckVersion"));
        pD3DXCheckVolumeTextureRequirements = (decltype(D3DXCheckVolumeTextureRequirements)*)(GetProcAddress(dll, "D3DXCheckVolumeTextureRequirements"));
        pD3DXCleanMesh = (decltype(D3DXCleanMesh)*)(GetProcAddress(dll, "D3DXCleanMesh"));
        pD3DXColorAdjustContrast = (decltype(D3DXColorAdjustContrast)*)(GetProcAddress(dll, "D3DXColorAdjustContrast"));
        pD3DXColorAdjustSaturation = (decltype(D3DXColorAdjustSaturation)*)(GetProcAddress(dll, "D3DXColorAdjustSaturation"));
        pD3DXCompileShader = (decltype(D3DXCompileShader)*)(GetProcAddress(dll, "D3DXCompileShader"));
        pD3DXCompileShaderFromFileA = (decltype(D3DXCompileShaderFromFileA)*)(GetProcAddress(dll, "D3DXCompileShaderFromFileA"));
        pD3DXCompileShaderFromFileW = (decltype(D3DXCompileShaderFromFileW)*)(GetProcAddress(dll, "D3DXCompileShaderFromFileW"));
        pD3DXCompileShaderFromResourceA = (decltype(D3DXCompileShaderFromResourceA)*)(GetProcAddress(dll, "D3DXCompileShaderFromResourceA"));
        pD3DXCompileShaderFromResourceW = (decltype(D3DXCompileShaderFromResourceW)*)(GetProcAddress(dll, "D3DXCompileShaderFromResourceW"));
        pD3DXComputeBoundingBox = (decltype(D3DXComputeBoundingBox)*)(GetProcAddress(dll, "D3DXComputeBoundingBox"));
        pD3DXComputeBoundingSphere = (decltype(D3DXComputeBoundingSphere)*)(GetProcAddress(dll, "D3DXComputeBoundingSphere"));
        pD3DXComputeIMTFromPerTexelSignal = (decltype(D3DXComputeIMTFromPerTexelSignal)*)(GetProcAddress(dll, "D3DXComputeIMTFromPerTexelSignal"));
        pD3DXComputeIMTFromPerVertexSignal = (decltype(D3DXComputeIMTFromPerVertexSignal)*)(GetProcAddress(dll, "D3DXComputeIMTFromPerVertexSignal"));
        pD3DXComputeIMTFromSignal = (decltype(D3DXComputeIMTFromSignal)*)(GetProcAddress(dll, "D3DXComputeIMTFromSignal("));
        pD3DXComputeIMTFromTexture = (decltype(D3DXComputeIMTFromTexture)*)(GetProcAddress(dll, "D3DXComputeIMTFromTexture"));
        pD3DXComputeNormalMap = (decltype(D3DXComputeNormalMap)*)(GetProcAddress(dll, "D3DXComputeNormalMap"));
        pD3DXComputeNormals = (decltype(D3DXComputeNormals)*)(GetProcAddress(dll, "D3DXComputeNormals"));
        pD3DXComputeTangentFrame = (decltype(D3DXComputeTangentFrame)*)(GetProcAddress(dll, "D3DXComputeTangentFrame"));
        pD3DXComputeTangentFrameEx = (decltype(D3DXComputeTangentFrameEx)*)(GetProcAddress(dll, "D3DXComputeTangentFrameEx"));
        pD3DXConcatenateMeshes = (decltype(D3DXConcatenateMeshes)*)(GetProcAddress(dll, "D3DXConcatenateMeshes"));
        pD3DXConvertMeshSubsetToSingleStrip = (decltype(D3DXConvertMeshSubsetToSingleStrip)*)(GetProcAddress(dll, "D3DXConvertMeshSubsetToSingleStrip"));
        pD3DXConvertMeshSubsetToStrips = (decltype(D3DXConvertMeshSubsetToStrips)*)(GetProcAddress(dll, "D3DXConvertMeshSubsetToStrips"));
        pD3DXCreateAnimationController = (decltype(D3DXCreateAnimationController)*)(GetProcAddress(dll, "D3DXCreateAnimationController"));
        pD3DXCreateBox = (decltype(D3DXCreateBox)*)(GetProcAddress(dll, "D3DXCreateBox"));
        pD3DXCreateBuffer = (decltype(D3DXCreateBuffer)*)(GetProcAddress(dll, "D3DXCreateBuffer"));
        pD3DXCreateCompressedAnimationSet = (decltype(D3DXCreateCompressedAnimationSet)*)(GetProcAddress(dll, "D3DXCreateCompressedAnimationSet"));
        pD3DXCreateCubeTexture = (decltype(D3DXCreateCubeTexture)*)(GetProcAddress(dll, "D3DXCreateCubeTexture"));
        pD3DXCreateCubeTextureFromFileA = (decltype(D3DXCreateCubeTextureFromFileA)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromFileA"));
        pD3DXCreateCubeTextureFromFileExA = (decltype(D3DXCreateCubeTextureFromFileExA)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromFileExA"));
        pD3DXCreateCubeTextureFromFileExW = (decltype(D3DXCreateCubeTextureFromFileExW)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromFileExW"));
        pD3DXCreateCubeTextureFromFileInMemory = (decltype(D3DXCreateCubeTextureFromFileInMemory)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromFileInMemory"));
        pD3DXCreateCubeTextureFromFileInMemoryEx = (decltype(D3DXCreateCubeTextureFromFileInMemoryEx)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromFileInMemoryEx"));
        pD3DXCreateCubeTextureFromFileW = (decltype(D3DXCreateCubeTextureFromFileW)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromFileW"));
        pD3DXCreateCubeTextureFromResourceA = (decltype(D3DXCreateCubeTextureFromResourceA)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromResourceA"));
        pD3DXCreateCubeTextureFromResourceExA = (decltype(D3DXCreateCubeTextureFromResourceExA)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromResourceExA"));
        pD3DXCreateCubeTextureFromResourceExW = (decltype(D3DXCreateCubeTextureFromResourceExW)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromResourceExW"));
        pD3DXCreateCubeTextureFromResourceW = (decltype(D3DXCreateCubeTextureFromResourceW)*)(GetProcAddress(dll, "D3DXCreateCubeTextureFromResourceW"));
        pD3DXCreateCylinder = (decltype(D3DXCreateCylinder)*)(GetProcAddress(dll, "D3DXCreateCylinder"));
        pD3DXCreateEffect = (decltype(D3DXCreateEffect)*)(GetProcAddress(dll, "D3DXCreateEffect"));
        pD3DXCreateEffectCompiler = (decltype(D3DXCreateEffectCompiler)*)(GetProcAddress(dll, "D3DXCreateEffectCompiler"));
        pD3DXCreateEffectCompilerFromFileA = (decltype(D3DXCreateEffectCompilerFromFileA)*)(GetProcAddress(dll, "D3DXCreateEffectCompilerFromFileA"));
        pD3DXCreateEffectCompilerFromFileW = (decltype(D3DXCreateEffectCompilerFromFileW)*)(GetProcAddress(dll, "D3DXCreateEffectCompilerFromFileW"));
        pD3DXCreateEffectCompilerFromResourceA = (decltype(D3DXCreateEffectCompilerFromResourceA)*)(GetProcAddress(dll, "D3DXCreateEffectCompilerFromResourceA"));
        pD3DXCreateEffectCompilerFromResourceW = (decltype(D3DXCreateEffectCompilerFromResourceW)*)(GetProcAddress(dll, "D3DXCreateEffectCompilerFromResourceW"));
        pD3DXCreateEffectEx = (decltype(D3DXCreateEffectEx)*)(GetProcAddress(dll, "D3DXCreateEffectEx"));
        pD3DXCreateEffectFromFileA = (decltype(D3DXCreateEffectFromFileA)*)(GetProcAddress(dll, "D3DXCreateEffectFromFileA"));
        pD3DXCreateEffectFromFileExA = (decltype(D3DXCreateEffectFromFileExA)*)(GetProcAddress(dll, "D3DXCreateEffectFromFileExA"));
        pD3DXCreateEffectFromFileExW = (decltype(D3DXCreateEffectFromFileExW)*)(GetProcAddress(dll, "D3DXCreateEffectFromFileExW"));
        pD3DXCreateEffectFromFileW = (decltype(D3DXCreateEffectFromFileW)*)(GetProcAddress(dll, "D3DXCreateEffectFromFileW"));
        pD3DXCreateEffectFromResourceA = (decltype(D3DXCreateEffectFromResourceA)*)(GetProcAddress(dll, "D3DXCreateEffectFromResourceA"));
        pD3DXCreateEffectFromResourceExA = (decltype(D3DXCreateEffectFromResourceExA)*)(GetProcAddress(dll, "D3DXCreateEffectFromResourceExA"));
        pD3DXCreateEffectFromResourceExW = (decltype(D3DXCreateEffectFromResourceExW)*)(GetProcAddress(dll, "D3DXCreateEffectFromResourceExW"));
        pD3DXCreateEffectFromResourceW = (decltype(D3DXCreateEffectFromResourceW)*)(GetProcAddress(dll, "D3DXCreateEffectFromResourceW"));
        pD3DXCreateEffectPool = (decltype(D3DXCreateEffectPool)*)(GetProcAddress(dll, "D3DXCreateEffectPool"));
        pD3DXCreateFontA = (decltype(D3DXCreateFontA)*)(GetProcAddress(dll, "D3DXCreateFontA"));
        pD3DXCreateFontIndirectA = (decltype(D3DXCreateFontIndirectA)*)(GetProcAddress(dll, "D3DXCreateFontIndirectA"));
        pD3DXCreateFontIndirectW = (decltype(D3DXCreateFontIndirectW)*)(GetProcAddress(dll, "D3DXCreateFontIndirectW"));
        pD3DXCreateFontW = (decltype(D3DXCreateFontW)*)(GetProcAddress(dll, "D3DXCreateFontW"));
        pD3DXCreateKeyframedAnimationSet = (decltype(D3DXCreateKeyframedAnimationSet)*)(GetProcAddress(dll, "D3DXCreateKeyframedAnimationSet"));
        pD3DXCreateLine = (decltype(D3DXCreateLine)*)(GetProcAddress(dll, "D3DXCreateLine"));
        pD3DXCreateMatrixStack = (decltype(D3DXCreateMatrixStack)*)(GetProcAddress(dll, "D3DXCreateMatrixStack"));
        pD3DXCreateMesh = (decltype(D3DXCreateMesh)*)(GetProcAddress(dll, "D3DXCreateMesh"));
        pD3DXCreateMeshFVF = (decltype(D3DXCreateMeshFVF)*)(GetProcAddress(dll, "D3DXCreateMeshFVF"));
        pD3DXCreateNPatchMesh = (decltype(D3DXCreateNPatchMesh)*)(GetProcAddress(dll, "D3DXCreateNPatchMesh"));
        pD3DXCreatePMeshFromStream = (decltype(D3DXCreatePMeshFromStream)*)(GetProcAddress(dll, "D3DXCreatePMeshFromStream"));
        pD3DXCreatePRTBuffer = (decltype(D3DXCreatePRTBuffer)*)(GetProcAddress(dll, "D3DXCreatePRTBuffer"));
        pD3DXCreatePRTBufferTex = (decltype(D3DXCreatePRTBufferTex)*)(GetProcAddress(dll, "D3DXCreatePRTBufferTex"));
        pD3DXCreatePRTCompBuffer = (decltype(D3DXCreatePRTCompBuffer)*)(GetProcAddress(dll, "D3DXCreatePRTCompBuffer"));
        pD3DXCreatePRTEngine = (decltype(D3DXCreatePRTEngine)*)(GetProcAddress(dll, "D3DXCreatePRTEngine"));
        pD3DXCreatePatchMesh = (decltype(D3DXCreatePatchMesh)*)(GetProcAddress(dll, "D3DXCreatePatchMesh"));
        pD3DXCreatePolygon = (decltype(D3DXCreatePolygon)*)(GetProcAddress(dll, "D3DXCreatePolygon"));
        pD3DXCreateRenderToEnvMap = (decltype(D3DXCreateRenderToEnvMap)*)(GetProcAddress(dll, "D3DXCreateRenderToEnvMap"));
        pD3DXCreateRenderToSurface = (decltype(D3DXCreateRenderToSurface)*)(GetProcAddress(dll, "D3DXCreateRenderToSurface"));
        pD3DXCreateSPMesh = (decltype(D3DXCreateSPMesh)*)(GetProcAddress(dll, "D3DXCreateSPMesh"));
        pD3DXCreateSkinInfo = (decltype(D3DXCreateSkinInfo)*)(GetProcAddress(dll, "D3DXCreateSkinInfo"));
        pD3DXCreateSkinInfoFVF = (decltype(D3DXCreateSkinInfoFVF)*)(GetProcAddress(dll, "D3DXCreateSkinInfoFVF"));
        pD3DXCreateSkinInfoFromBlendedMesh = (decltype(D3DXCreateSkinInfoFromBlendedMesh)*)(GetProcAddress(dll, "D3DXCreateSkinInfoFromBlendedMesh"));
        pD3DXCreateSphere = (decltype(D3DXCreateSphere)*)(GetProcAddress(dll, "D3DXCreateSphere"));
        pD3DXCreateSprite = (decltype(D3DXCreateSprite)*)(GetProcAddress(dll, "D3DXCreateSprite"));
        pD3DXCreateTeapot = (decltype(D3DXCreateTeapot)*)(GetProcAddress(dll, "D3DXCreateTeapot"));
        pD3DXCreateTextA = (decltype(D3DXCreateTextA)*)(GetProcAddress(dll, "D3DXCreateTextA"));
        pD3DXCreateTextW = (decltype(D3DXCreateTextW)*)(GetProcAddress(dll, "D3DXCreateTextW"));
        pD3DXCreateTexture = (decltype(D3DXCreateTexture)*)(GetProcAddress(dll, "D3DXCreateTexture"));
        pD3DXCreateTextureFromFileA = (decltype(D3DXCreateTextureFromFileA)*)(GetProcAddress(dll, "D3DXCreateTextureFromFileA"));
        pD3DXCreateTextureFromFileExA = (decltype(D3DXCreateTextureFromFileExA)*)(GetProcAddress(dll, "D3DXCreateTextureFromFileExA"));
        pD3DXCreateTextureFromFileExW = (decltype(D3DXCreateTextureFromFileExW)*)(GetProcAddress(dll, "D3DXCreateTextureFromFileExW"));
        pD3DXCreateTextureFromFileInMemory = (decltype(D3DXCreateTextureFromFileInMemory)*)(GetProcAddress(dll, "D3DXCreateTextureFromFileInMemory"));
        pD3DXCreateTextureFromFileInMemoryEx = (decltype(D3DXCreateTextureFromFileInMemoryEx)*)(GetProcAddress(dll, "D3DXCreateTextureFromFileInMemoryEx"));
        pD3DXCreateTextureFromFileW = (decltype(D3DXCreateTextureFromFileW)*)(GetProcAddress(dll, "D3DXCreateTextureFromFileW"));
        pD3DXCreateTextureFromResourceA = (decltype(D3DXCreateTextureFromResourceA)*)(GetProcAddress(dll, "D3DXCreateTextureFromResourceA"));
        pD3DXCreateTextureFromResourceExA = (decltype(D3DXCreateTextureFromResourceExA)*)(GetProcAddress(dll, "D3DXCreateTextureFromResourceExA"));
        pD3DXCreateTextureFromResourceExW = (decltype(D3DXCreateTextureFromResourceExW)*)(GetProcAddress(dll, "D3DXCreateTextureFromResourceExW"));
        pD3DXCreateTextureFromResourceW = (decltype(D3DXCreateTextureFromResourceW)*)(GetProcAddress(dll, "D3DXCreateTextureFromResourceW"));
        pD3DXCreateTextureGutterHelper = (decltype(D3DXCreateTextureGutterHelper)*)(GetProcAddress(dll, "D3DXCreateTextureGutterHelper"));
        pD3DXCreateTextureShader = (decltype(D3DXCreateTextureShader)*)(GetProcAddress(dll, "D3DXCreateTextureShader"));
        pD3DXCreateTorus = (decltype(D3DXCreateTorus)*)(GetProcAddress(dll, "D3DXCreateTorus"));
        pD3DXCreateVolumeTexture = (decltype(D3DXCreateVolumeTexture)*)(GetProcAddress(dll, "D3DXCreateVolumeTexture"));
        pD3DXCreateVolumeTextureFromFileA = (decltype(D3DXCreateVolumeTextureFromFileA)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromFileA"));
        pD3DXCreateVolumeTextureFromFileExA = (decltype(D3DXCreateVolumeTextureFromFileExA)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromFileExA"));
        pD3DXCreateVolumeTextureFromFileExW = (decltype(D3DXCreateVolumeTextureFromFileExW)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromFileExW"));
        pD3DXCreateVolumeTextureFromFileInMemory = (decltype(D3DXCreateVolumeTextureFromFileInMemory)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromFileInMemory"));
        pD3DXCreateVolumeTextureFromFileInMemoryEx = (decltype(D3DXCreateVolumeTextureFromFileInMemoryEx)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromFileInMemoryEx"));
        pD3DXCreateVolumeTextureFromFileW = (decltype(D3DXCreateVolumeTextureFromFileW)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromFileW"));
        pD3DXCreateVolumeTextureFromResourceA = (decltype(D3DXCreateVolumeTextureFromResourceA)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromResourceA"));
        pD3DXCreateVolumeTextureFromResourceExA = (decltype(D3DXCreateVolumeTextureFromResourceExA)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromResourceExA"));
        pD3DXCreateVolumeTextureFromResourceExW = (decltype(D3DXCreateVolumeTextureFromResourceExW)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromResourceExW"));
        pD3DXCreateVolumeTextureFromResourceW = (decltype(D3DXCreateVolumeTextureFromResourceW)*)(GetProcAddress(dll, "D3DXCreateVolumeTextureFromResourceW"));
        pD3DXDebugMute = (decltype(D3DXDebugMute)*)(GetProcAddress(dll, "D3DXDebugMute"));
        pD3DXDeclaratorFromFVF = (decltype(D3DXDeclaratorFromFVF)*)(GetProcAddress(dll, "D3DXDeclaratorFromFVF"));
        pD3DXDisassembleEffect = (decltype(D3DXDisassembleEffect)*)(GetProcAddress(dll, "D3DXDisassembleEffect"));
        pD3DXDisassembleShader = (decltype(D3DXDisassembleShader)*)(GetProcAddress(dll, "D3DXDisassembleShader"));
        pD3DXFVFFromDeclarator = (decltype(D3DXFVFFromDeclarator)*)(GetProcAddress(dll, "D3DXFVFFromDeclarator"));
        pD3DXFillCubeTexture = (decltype(D3DXFillCubeTexture)*)(GetProcAddress(dll, "D3DXFillCubeTexture"));
        pD3DXFillCubeTextureTX = (decltype(D3DXFillCubeTextureTX)*)(GetProcAddress(dll, "D3DXFillCubeTextureTX"));
        pD3DXFillTexture = (decltype(D3DXFillTexture)*)(GetProcAddress(dll, "D3DXFillTexture"));
        pD3DXFillTextureTX = (decltype(D3DXFillTextureTX)*)(GetProcAddress(dll, "D3DXFillTextureTX"));
        pD3DXFillVolumeTexture = (decltype(D3DXFillVolumeTexture)*)(GetProcAddress(dll, "D3DXFillVolumeTexture"));
        pD3DXFillVolumeTextureTX = (decltype(D3DXFillVolumeTextureTX)*)(GetProcAddress(dll, "D3DXFillVolumeTextureTX"));
        pD3DXFilterTexture = (decltype(D3DXFilterTexture)*)(GetProcAddress(dll, "D3DXFilterTexture"));
        pD3DXFindShaderComment = (decltype(D3DXFindShaderComment)*)(GetProcAddress(dll, "D3DXFindShaderComment"));
        pD3DXFloat16To32Array = (decltype(D3DXFloat16To32Array)*)(GetProcAddress(dll, "D3DXFloat16To32Array"));
        pD3DXFloat32To16Array = (decltype(D3DXFloat32To16Array)*)(GetProcAddress(dll, "D3DXFloat32To16Array"));
        pD3DXFrameAppendChild = (decltype(D3DXFrameAppendChild)*)(GetProcAddress(dll, "D3DXFrameAppendChild"));
        pD3DXFrameCalculateBoundingSphere = (decltype(D3DXFrameCalculateBoundingSphere)*)(GetProcAddress(dll, "D3DXFrameCalculateBoundingSphere"));
        pD3DXFrameDestroy = (decltype(D3DXFrameDestroy)*)(GetProcAddress(dll, "D3DXFrameDestroy"));
        pD3DXFrameFind = (decltype(D3DXFrameFind)*)(GetProcAddress(dll, "D3DXFrameFind"));
        pD3DXFrameNumNamedMatrices = (decltype(D3DXFrameNumNamedMatrices)*)(GetProcAddress(dll, "D3DXFrameNumNamedMatrices"));
        pD3DXFrameRegisterNamedMatrices = (decltype(D3DXFrameRegisterNamedMatrices)*)(GetProcAddress(dll, "D3DXFrameRegisterNamedMatrices"));
        pD3DXFresnelTerm = (decltype(D3DXFresnelTerm)*)(GetProcAddress(dll, "D3DXFresnelTerm"));
        pD3DXGenerateOutputDecl = (decltype(D3DXGenerateOutputDecl)*)(GetProcAddress(dll, "D3DXGenerateOutputDecl"));
        pD3DXGeneratePMesh = (decltype(D3DXGeneratePMesh)*)(GetProcAddress(dll, "D3DXGeneratePMesh"));
        pD3DXGetDeclLength = (decltype(D3DXGetDeclLength)*)(GetProcAddress(dll, "D3DXGetDeclLength"));
        pD3DXGetDeclVertexSize = (decltype(D3DXGetDeclVertexSize)*)(GetProcAddress(dll, "D3DXGetDeclVertexSize"));
        pD3DXGetDriverLevel = (decltype(D3DXGetDriverLevel)*)(GetProcAddress(dll, "D3DXGetDriverLevel"));
        pD3DXGetFVFVertexSize = (decltype(D3DXGetFVFVertexSize)*)(GetProcAddress(dll, "D3DXGetFVFVertexSize"));
        pD3DXGetImageInfoFromFileA = (decltype(D3DXGetImageInfoFromFileA)*)(GetProcAddress(dll, "D3DXGetImageInfoFromFileA"));
        pD3DXGetImageInfoFromFileInMemory = (decltype(D3DXGetImageInfoFromFileInMemory)*)(GetProcAddress(dll, "D3DXGetImageInfoFromFileInMemory"));
        pD3DXGetImageInfoFromFileW = (decltype(D3DXGetImageInfoFromFileW)*)(GetProcAddress(dll, "D3DXGetImageInfoFromFileW"));
        pD3DXGetImageInfoFromResourceA = (decltype(D3DXGetImageInfoFromResourceA)*)(GetProcAddress(dll, "D3DXGetImageInfoFromResourceA"));
        pD3DXGetImageInfoFromResourceW = (decltype(D3DXGetImageInfoFromResourceW)*)(GetProcAddress(dll, "D3DXGetImageInfoFromResourceW"));
        pD3DXGetPixelShaderProfile = (decltype(D3DXGetPixelShaderProfile)*)(GetProcAddress(dll, "D3DXGetPixelShaderProfile"));
        pD3DXGetShaderConstantTable = (decltype(D3DXGetShaderConstantTable)*)(GetProcAddress(dll, "D3DXGetShaderConstantTable"));
        pD3DXGetShaderInputSemantics = (decltype(D3DXGetShaderInputSemantics)*)(GetProcAddress(dll, "D3DXGetShaderInputSemantics"));
        pD3DXGetShaderOutputSemantics = (decltype(D3DXGetShaderOutputSemantics)*)(GetProcAddress(dll, "D3DXGetShaderOutputSemantics"));
        pD3DXGetShaderSamplers = (decltype(D3DXGetShaderSamplers)*)(GetProcAddress(dll, "D3DXGetShaderSamplers"));
        pD3DXGetShaderSize = (decltype(D3DXGetShaderSize)*)(GetProcAddress(dll, "D3DXGetShaderSize"));
        pD3DXGetShaderVersion = (decltype(D3DXGetShaderVersion)*)(GetProcAddress(dll, "D3DXGetShaderVersion"));
        pD3DXGetVertexShaderProfile = (decltype(D3DXGetVertexShaderProfile)*)(GetProcAddress(dll, "D3DXGetVertexShaderProfile"));
        pD3DXIntersect = (decltype(D3DXIntersect)*)(GetProcAddress(dll, "D3DXIntersect"));
        pD3DXIntersectSubset = (decltype(D3DXIntersectSubset)*)(GetProcAddress(dll, "D3DXIntersectSubset"));
        pD3DXIntersectTri = (decltype(D3DXIntersectTri)*)(GetProcAddress(dll, "D3DXIntersectTri"));
        pD3DXLoadMeshFromXA = (decltype(D3DXLoadMeshFromXA)*)(GetProcAddress(dll, "D3DXLoadMeshFromXA"));
        pD3DXLoadMeshFromXInMemory = (decltype(D3DXLoadMeshFromXInMemory)*)(GetProcAddress(dll, "D3DXLoadMeshFromXInMemory"));
        pD3DXLoadMeshFromXResource = (decltype(D3DXLoadMeshFromXResource)*)(GetProcAddress(dll, "D3DXLoadMeshFromXResource"));
        pD3DXLoadMeshFromXW = (decltype(D3DXLoadMeshFromXW)*)(GetProcAddress(dll, "D3DXLoadMeshFromXW"));
        pD3DXLoadMeshFromXof = (decltype(D3DXLoadMeshFromXof)*)(GetProcAddress(dll, "D3DXLoadMeshFromXof"));
        pD3DXLoadMeshHierarchyFromXA = (decltype(D3DXLoadMeshHierarchyFromXA)*)(GetProcAddress(dll, "D3DXLoadMeshHierarchyFromXA"));
        pD3DXLoadMeshHierarchyFromXInMemory = (decltype(D3DXLoadMeshHierarchyFromXInMemory)*)(GetProcAddress(dll, "D3DXLoadMeshHierarchyFromXInMemory"));
        pD3DXLoadMeshHierarchyFromXW = (decltype(D3DXLoadMeshHierarchyFromXW)*)(GetProcAddress(dll, "D3DXLoadMeshHierarchyFromXW"));
        pD3DXLoadPRTBufferFromFileA = (decltype(D3DXLoadPRTBufferFromFileA)*)(GetProcAddress(dll, "D3DXLoadPRTBufferFromFileA"));
        pD3DXLoadPRTBufferFromFileW = (decltype(D3DXLoadPRTBufferFromFileW)*)(GetProcAddress(dll, "D3DXLoadPRTBufferFromFileW"));
        pD3DXLoadPRTCompBufferFromFileA = (decltype(D3DXLoadPRTCompBufferFromFileA)*)(GetProcAddress(dll, "D3DXLoadPRTCompBufferFromFileA"));
        pD3DXLoadPRTCompBufferFromFileW = (decltype(D3DXLoadPRTCompBufferFromFileW)*)(GetProcAddress(dll, "D3DXLoadPRTCompBufferFromFileW"));
        pD3DXLoadPatchMeshFromXof = (decltype(D3DXLoadPatchMeshFromXof)*)(GetProcAddress(dll, "D3DXLoadPatchMeshFromXof"));
        pD3DXLoadSkinMeshFromXof = (decltype(D3DXLoadSkinMeshFromXof)*)(GetProcAddress(dll, "D3DXLoadSkinMeshFromXof"));
        pD3DXLoadSurfaceFromFileA = (decltype(D3DXLoadSurfaceFromFileA)*)(GetProcAddress(dll, "D3DXLoadSurfaceFromFileA"));
        pD3DXLoadSurfaceFromFileInMemory = (decltype(D3DXLoadSurfaceFromFileInMemory)*)(GetProcAddress(dll, "D3DXLoadSurfaceFromFileInMemory"));
        pD3DXLoadSurfaceFromFileW = (decltype(D3DXLoadSurfaceFromFileW)*)(GetProcAddress(dll, "D3DXLoadSurfaceFromFileW"));
        pD3DXLoadSurfaceFromMemory = (decltype(D3DXLoadSurfaceFromMemory)*)(GetProcAddress(dll, "D3DXLoadSurfaceFromMemory"));
        pD3DXLoadSurfaceFromResourceA = (decltype(D3DXLoadSurfaceFromResourceA)*)(GetProcAddress(dll, "D3DXLoadSurfaceFromResourceA"));
        pD3DXLoadSurfaceFromResourceW = (decltype(D3DXLoadSurfaceFromResourceW)*)(GetProcAddress(dll, "D3DXLoadSurfaceFromResourceW"));
        pD3DXLoadSurfaceFromSurface = (decltype(D3DXLoadSurfaceFromSurface)*)(GetProcAddress(dll, "D3DXLoadSurfaceFromSurface"));
        pD3DXLoadVolumeFromFileA = (decltype(D3DXLoadVolumeFromFileA)*)(GetProcAddress(dll, "D3DXLoadVolumeFromFileA"));
        pD3DXLoadVolumeFromFileInMemory = (decltype(D3DXLoadVolumeFromFileInMemory)*)(GetProcAddress(dll, "D3DXLoadVolumeFromFileInMemory"));
        pD3DXLoadVolumeFromFileW = (decltype(D3DXLoadVolumeFromFileW)*)(GetProcAddress(dll, "D3DXLoadVolumeFromFileW"));
        pD3DXLoadVolumeFromMemory = (decltype(D3DXLoadVolumeFromMemory)*)(GetProcAddress(dll, "D3DXLoadVolumeFromMemory"));
        pD3DXLoadVolumeFromResourceA = (decltype(D3DXLoadVolumeFromResourceA)*)(GetProcAddress(dll, "D3DXLoadVolumeFromResourceA"));
        pD3DXLoadVolumeFromResourceW = (decltype(D3DXLoadVolumeFromResourceW)*)(GetProcAddress(dll, "D3DXLoadVolumeFromResourceW"));
        pD3DXLoadVolumeFromVolume = (decltype(D3DXLoadVolumeFromVolume)*)(GetProcAddress(dll, "D3DXLoadVolumeFromVolume"));
        pD3DXMatrixAffineTransformation = (decltype(D3DXMatrixAffineTransformation)*)(GetProcAddress(dll, "D3DXMatrixAffineTransformation"));
        pD3DXMatrixAffineTransformation2D = (decltype(D3DXMatrixAffineTransformation2D)*)(GetProcAddress(dll, "D3DXMatrixAffineTransformation2D"));
        pD3DXMatrixDecompose = (decltype(D3DXMatrixDecompose)*)(GetProcAddress(dll, "D3DXMatrixDecompose"));
        pD3DXMatrixDeterminant = (decltype(D3DXMatrixDeterminant)*)(GetProcAddress(dll, "D3DXMatrixDeterminant"));
        pD3DXMatrixInverse = (decltype(D3DXMatrixInverse)*)(GetProcAddress(dll, "D3DXMatrixInverse"));
        pD3DXMatrixLookAtLH = (decltype(D3DXMatrixLookAtLH)*)(GetProcAddress(dll, "D3DXMatrixLookAtLH"));
        pD3DXMatrixLookAtRH = (decltype(D3DXMatrixLookAtRH)*)(GetProcAddress(dll, "D3DXMatrixLookAtRH"));
        pD3DXMatrixMultiply = (decltype(D3DXMatrixMultiply)*)(GetProcAddress(dll, "D3DXMatrixMultiply"));
        pD3DXMatrixMultiplyTranspose = (decltype(D3DXMatrixMultiplyTranspose)*)(GetProcAddress(dll, "D3DXMatrixMultiplyTranspose"));
        pD3DXMatrixOrthoLH = (decltype(D3DXMatrixOrthoLH)*)(GetProcAddress(dll, "D3DXMatrixOrthoLH"));
        pD3DXMatrixOrthoOffCenterLH = (decltype(D3DXMatrixOrthoOffCenterLH)*)(GetProcAddress(dll, "D3DXMatrixOrthoOffCenterLH"));
        pD3DXMatrixOrthoOffCenterRH = (decltype(D3DXMatrixOrthoOffCenterRH)*)(GetProcAddress(dll, "D3DXMatrixOrthoOffCenterRH"));
        pD3DXMatrixOrthoRH = (decltype(D3DXMatrixOrthoRH)*)(GetProcAddress(dll, "D3DXMatrixOrthoRH"));
        pD3DXMatrixPerspectiveFovLH = (decltype(D3DXMatrixPerspectiveFovLH)*)(GetProcAddress(dll, "D3DXMatrixPerspectiveFovLH"));
        pD3DXMatrixPerspectiveFovRH = (decltype(D3DXMatrixPerspectiveFovRH)*)(GetProcAddress(dll, "D3DXMatrixPerspectiveFovRH"));
        pD3DXMatrixPerspectiveLH = (decltype(D3DXMatrixPerspectiveLH)*)(GetProcAddress(dll, "D3DXMatrixPerspectiveLH"));
        pD3DXMatrixPerspectiveOffCenterLH = (decltype(D3DXMatrixPerspectiveOffCenterLH)*)(GetProcAddress(dll, "D3DXMatrixPerspectiveOffCenterLH"));
        pD3DXMatrixPerspectiveOffCenterRH = (decltype(D3DXMatrixPerspectiveOffCenterRH)*)(GetProcAddress(dll, "D3DXMatrixPerspectiveOffCenterRH"));
        pD3DXMatrixPerspectiveRH = (decltype(D3DXMatrixPerspectiveRH)*)(GetProcAddress(dll, "D3DXMatrixPerspectiveRH"));
        pD3DXMatrixReflect = (decltype(D3DXMatrixReflect)*)(GetProcAddress(dll, "D3DXMatrixReflect"));
        pD3DXMatrixRotationAxis = (decltype(D3DXMatrixRotationAxis)*)(GetProcAddress(dll, "D3DXMatrixRotationAxis"));
        pD3DXMatrixRotationQuaternion = (decltype(D3DXMatrixRotationQuaternion)*)(GetProcAddress(dll, "D3DXMatrixRotationQuaternion"));
        pD3DXMatrixRotationX = (decltype(D3DXMatrixRotationX)*)(GetProcAddress(dll, "D3DXMatrixRotationX"));
        pD3DXMatrixRotationY = (decltype(D3DXMatrixRotationY)*)(GetProcAddress(dll, "D3DXMatrixRotationY"));
        pD3DXMatrixRotationYawPitchRoll = (decltype(D3DXMatrixRotationYawPitchRoll)*)(GetProcAddress(dll, "D3DXMatrixRotationYawPitchRoll"));
        pD3DXMatrixRotationZ = (decltype(D3DXMatrixRotationZ)*)(GetProcAddress(dll, "D3DXMatrixRotationZ"));
        pD3DXMatrixScaling = (decltype(D3DXMatrixScaling)*)(GetProcAddress(dll, "D3DXMatrixScaling"));
        pD3DXMatrixShadow = (decltype(D3DXMatrixShadow)*)(GetProcAddress(dll, "D3DXMatrixShadow"));
        pD3DXMatrixTransformation = (decltype(D3DXMatrixTransformation)*)(GetProcAddress(dll, "D3DXMatrixTransformation"));
        pD3DXMatrixTransformation2D = (decltype(D3DXMatrixTransformation2D)*)(GetProcAddress(dll, "D3DXMatrixTransformation2D"));
        pD3DXMatrixTranslation = (decltype(D3DXMatrixTranslation)*)(GetProcAddress(dll, "D3DXMatrixTranslation"));
        pD3DXMatrixTranspose = (decltype(D3DXMatrixTranspose)*)(GetProcAddress(dll, "D3DXMatrixTranspose"));
        pD3DXOptimizeFaces = (decltype(D3DXOptimizeFaces)*)(GetProcAddress(dll, "D3DXOptimizeFaces"));
        pD3DXOptimizeVertices = (decltype(D3DXOptimizeVertices)*)(GetProcAddress(dll, "D3DXOptimizeVertices"));
        pD3DXPlaneFromPointNormal = (decltype(D3DXPlaneFromPointNormal)*)(GetProcAddress(dll, "D3DXPlaneFromPointNormal"));
        pD3DXPlaneFromPoints = (decltype(D3DXPlaneFromPoints)*)(GetProcAddress(dll, "D3DXPlaneFromPoints"));
        pD3DXPlaneIntersectLine = (decltype(D3DXPlaneIntersectLine)*)(GetProcAddress(dll, "D3DXPlaneIntersectLine"));
        pD3DXPlaneNormalize = (decltype(D3DXPlaneNormalize)*)(GetProcAddress(dll, "D3DXPlaneNormalize"));
        pD3DXPlaneTransform = (decltype(D3DXPlaneTransform)*)(GetProcAddress(dll, "D3DXPlaneTransform"));
        pD3DXPlaneTransformArray = (decltype(D3DXPlaneTransformArray)*)(GetProcAddress(dll, "D3DXPlaneTransformArray"));
        pD3DXPreprocessShader = (decltype(D3DXPreprocessShader)*)(GetProcAddress(dll, "D3DXPreprocessShader"));
        pD3DXPreprocessShaderFromFileA = (decltype(D3DXPreprocessShaderFromFileA)*)(GetProcAddress(dll, "D3DXPreprocessShaderFromFileA"));
        pD3DXPreprocessShaderFromFileW = (decltype(D3DXPreprocessShaderFromFileW)*)(GetProcAddress(dll, "D3DXPreprocessShaderFromFileW"));
        pD3DXPreprocessShaderFromResourceA = (decltype(D3DXPreprocessShaderFromResourceA)*)(GetProcAddress(dll, "D3DXPreprocessShaderFromResourceA"));
        pD3DXPreprocessShaderFromResourceW = (decltype(D3DXPreprocessShaderFromResourceW)*)(GetProcAddress(dll, "D3DXPreprocessShaderFromResourceW"));
        pD3DXQuaternionBaryCentric = (decltype(D3DXQuaternionBaryCentric)*)(GetProcAddress(dll, "D3DXQuaternionBaryCentric"));
        pD3DXQuaternionExp = (decltype(D3DXQuaternionExp)*)(GetProcAddress(dll, "D3DXQuaternionExp"));
        pD3DXQuaternionInverse = (decltype(D3DXQuaternionInverse)*)(GetProcAddress(dll, "D3DXQuaternionInverse"));
        pD3DXQuaternionLn = (decltype(D3DXQuaternionLn)*)(GetProcAddress(dll, "D3DXQuaternionLn"));
        pD3DXQuaternionMultiply = (decltype(D3DXQuaternionMultiply)*)(GetProcAddress(dll, "D3DXQuaternionMultiply"));
        pD3DXQuaternionNormalize = (decltype(D3DXQuaternionNormalize)*)(GetProcAddress(dll, "D3DXQuaternionNormalize"));
        pD3DXQuaternionRotationAxis = (decltype(D3DXQuaternionRotationAxis)*)(GetProcAddress(dll, "D3DXQuaternionRotationAxis"));
        pD3DXQuaternionRotationMatrix = (decltype(D3DXQuaternionRotationMatrix)*)(GetProcAddress(dll, "D3DXQuaternionRotationMatrix"));
        pD3DXQuaternionRotationYawPitchRoll = (decltype(D3DXQuaternionRotationYawPitchRoll)*)(GetProcAddress(dll, "D3DXQuaternionRotationYawPitchRoll"));
        pD3DXQuaternionSlerp = (decltype(D3DXQuaternionSlerp)*)(GetProcAddress(dll, "D3DXQuaternionSlerp"));
        pD3DXQuaternionSquad = (decltype(D3DXQuaternionSquad)*)(GetProcAddress(dll, "D3DXQuaternionSquad"));
        pD3DXQuaternionSquadSetup = (decltype(D3DXQuaternionSquadSetup)*)(GetProcAddress(dll, "D3DXQuaternionSquadSetup"));
        pD3DXQuaternionToAxisAngle = (decltype(D3DXQuaternionToAxisAngle)*)(GetProcAddress(dll, "D3DXQuaternionToAxisAngle"));
        pD3DXRectPatchSize = (decltype(D3DXRectPatchSize)*)(GetProcAddress(dll, "D3DXRectPatchSize"));
        pD3DXSHAdd = (decltype(D3DXSHAdd)*)(GetProcAddress(dll, "D3DXSHAdd"));
        pD3DXSHDot = (decltype(D3DXSHDot)*)(GetProcAddress(dll, "D3DXSHDot"));
        pD3DXSHEvalConeLight = (decltype(D3DXSHEvalConeLight)*)(GetProcAddress(dll, "D3DXSHEvalConeLight"));
        pD3DXSHEvalDirection = (decltype(D3DXSHEvalDirection)*)(GetProcAddress(dll, "D3DXSHEvalDirection"));
        pD3DXSHEvalDirectionalLight = (decltype(D3DXSHEvalDirectionalLight)*)(GetProcAddress(dll, "D3DXSHEvalDirectionalLight"));
        pD3DXSHEvalHemisphereLight = (decltype(D3DXSHEvalHemisphereLight)*)(GetProcAddress(dll, "D3DXSHEvalHemisphereLight"));
        pD3DXSHEvalSphericalLight = (decltype(D3DXSHEvalSphericalLight)*)(GetProcAddress(dll, "D3DXSHEvalSphericalLight"));
        pD3DXSHMultiply2 = (decltype(D3DXSHMultiply2)*)(GetProcAddress(dll, "D3DXSHMultiply2"));
        pD3DXSHMultiply3 = (decltype(D3DXSHMultiply3)*)(GetProcAddress(dll, "D3DXSHMultiply3"));
        pD3DXSHMultiply4 = (decltype(D3DXSHMultiply4)*)(GetProcAddress(dll, "D3DXSHMultiply4"));
        pD3DXSHMultiply5 = (decltype(D3DXSHMultiply5)*)(GetProcAddress(dll, "D3DXSHMultiply5("));
        pD3DXSHMultiply6 = (decltype(D3DXSHMultiply6)*)(GetProcAddress(dll, "D3DXSHMultiply6("));
        pD3DXSHPRTCompSplitMeshSC = (decltype(D3DXSHPRTCompSplitMeshSC)*)(GetProcAddress(dll, "D3DXSHPRTCompSplitMeshSC"));
        pD3DXSHPRTCompSuperCluster = (decltype(D3DXSHPRTCompSuperCluster)*)(GetProcAddress(dll, "D3DXSHPRTCompSuperCluster"));
        pD3DXSHProjectCubeMap = (decltype(D3DXSHProjectCubeMap)*)(GetProcAddress(dll, "D3DXSHProjectCubeMap"));
        pD3DXSHRotate = (decltype(D3DXSHRotate)*)(GetProcAddress(dll, "D3DXSHRotate"));
        pD3DXSHRotateZ = (decltype(D3DXSHRotateZ)*)(GetProcAddress(dll, "D3DXSHRotateZ"));
        pD3DXSHScale = (decltype(D3DXSHScale)*)(GetProcAddress(dll, "D3DXSHScale"));
        pD3DXSaveMeshHierarchyToFileA = (decltype(D3DXSaveMeshHierarchyToFileA)*)(GetProcAddress(dll, "D3DXSaveMeshHierarchyToFileA"));
        pD3DXSaveMeshHierarchyToFileW = (decltype(D3DXSaveMeshHierarchyToFileW)*)(GetProcAddress(dll, "D3DXSaveMeshHierarchyToFileW"));
        pD3DXSaveMeshToXA = (decltype(D3DXSaveMeshToXA)*)(GetProcAddress(dll, "D3DXSaveMeshToXA"));
        pD3DXSaveMeshToXW = (decltype(D3DXSaveMeshToXW)*)(GetProcAddress(dll, "D3DXSaveMeshToXW"));
        pD3DXSavePRTBufferToFileA = (decltype(D3DXSavePRTBufferToFileA)*)(GetProcAddress(dll, "D3DXSavePRTBufferToFileA"));
        pD3DXSavePRTBufferToFileW = (decltype(D3DXSavePRTBufferToFileW)*)(GetProcAddress(dll, "D3DXSavePRTBufferToFileW"));
        pD3DXSavePRTCompBufferToFileA = (decltype(D3DXSavePRTCompBufferToFileA)*)(GetProcAddress(dll, "D3DXSavePRTCompBufferToFileA"));
        pD3DXSavePRTCompBufferToFileW = (decltype(D3DXSavePRTCompBufferToFileW)*)(GetProcAddress(dll, "D3DXSavePRTCompBufferToFileW"));
        pD3DXSaveSurfaceToFileA = (decltype(D3DXSaveSurfaceToFileA)*)(GetProcAddress(dll, "D3DXSaveSurfaceToFileA"));
        pD3DXSaveSurfaceToFileInMemory = (decltype(D3DXSaveSurfaceToFileInMemory)*)(GetProcAddress(dll, "D3DXSaveSurfaceToFileInMemory"));
        pD3DXSaveSurfaceToFileW = (decltype(D3DXSaveSurfaceToFileW)*)(GetProcAddress(dll, "D3DXSaveSurfaceToFileW"));
        pD3DXSaveTextureToFileA = (decltype(D3DXSaveTextureToFileA)*)(GetProcAddress(dll, "D3DXSaveTextureToFileA"));
        pD3DXSaveTextureToFileInMemory = (decltype(D3DXSaveTextureToFileInMemory)*)(GetProcAddress(dll, "D3DXSaveTextureToFileInMemory"));
        pD3DXSaveTextureToFileW = (decltype(D3DXSaveTextureToFileW)*)(GetProcAddress(dll, "D3DXSaveTextureToFileW"));
        pD3DXSaveVolumeToFileA = (decltype(D3DXSaveVolumeToFileA)*)(GetProcAddress(dll, "D3DXSaveVolumeToFileA"));
        pD3DXSaveVolumeToFileInMemory = (decltype(D3DXSaveVolumeToFileInMemory)*)(GetProcAddress(dll, "D3DXSaveVolumeToFileInMemory"));
        pD3DXSaveVolumeToFileW = (decltype(D3DXSaveVolumeToFileW)*)(GetProcAddress(dll, "D3DXSaveVolumeToFileW"));
        pD3DXSimplifyMesh = (decltype(D3DXSimplifyMesh)*)(GetProcAddress(dll, "D3DXSimplifyMesh"));
        pD3DXSphereBoundProbe = (decltype(D3DXSphereBoundProbe)*)(GetProcAddress(dll, "D3DXSphereBoundProbe"));
        pD3DXSplitMesh = (decltype(D3DXSplitMesh)*)(GetProcAddress(dll, "D3DXSplitMesh"));
        pD3DXTessellateNPatches = (decltype(D3DXTessellateNPatches)*)(GetProcAddress(dll, "D3DXTessellateNPatches"));
        pD3DXTessellateRectPatch = (decltype(D3DXTessellateRectPatch)*)(GetProcAddress(dll, "D3DXTessellateRectPatch"));
        pD3DXTessellateTriPatch = (decltype(D3DXTessellateTriPatch)*)(GetProcAddress(dll, "D3DXTessellateTriPatch"));
        pD3DXTriPatchSize = (decltype(D3DXTriPatchSize)*)(GetProcAddress(dll, "D3DXTriPatchSize"));
        pD3DXUVAtlasCreate = (decltype(D3DXUVAtlasCreate)*)(GetProcAddress(dll, "D3DXUVAtlasCreate"));
        pD3DXUVAtlasPack = (decltype(D3DXUVAtlasPack)*)(GetProcAddress(dll, "D3DXUVAtlasPack"));
        pD3DXUVAtlasPartition = (decltype(D3DXUVAtlasPartition)*)(GetProcAddress(dll, "D3DXUVAtlasPartition"));
        pD3DXValidMesh = (decltype(D3DXValidMesh)*)(GetProcAddress(dll, "D3DXValidMesh"));
        pD3DXValidPatchMesh = (decltype(D3DXValidPatchMesh)*)(GetProcAddress(dll, "D3DXValidPatchMesh("));
        pD3DXVec2BaryCentric = (decltype(D3DXVec2BaryCentric)*)(GetProcAddress(dll, "D3DXVec2BaryCentric"));
        pD3DXVec2CatmullRom = (decltype(D3DXVec2CatmullRom)*)(GetProcAddress(dll, "D3DXVec2CatmullRom"));
        pD3DXVec2Hermite = (decltype(D3DXVec2Hermite)*)(GetProcAddress(dll, "D3DXVec2Hermite"));
        pD3DXVec2Normalize = (decltype(D3DXVec2Normalize)*)(GetProcAddress(dll, "D3DXVec2Normalize"));
        pD3DXVec2Transform = (decltype(D3DXVec2Transform)*)(GetProcAddress(dll, "D3DXVec2Transform"));
        pD3DXVec2TransformArray = (decltype(D3DXVec2TransformArray)*)(GetProcAddress(dll, "D3DXVec2TransformArray"));
        pD3DXVec2TransformCoord = (decltype(D3DXVec2TransformCoord)*)(GetProcAddress(dll, "D3DXVec2TransformCoord"));
        pD3DXVec2TransformCoordArray = (decltype(D3DXVec2TransformCoordArray)*)(GetProcAddress(dll, "D3DXVec2TransformCoordArray"));
        pD3DXVec2TransformNormal = (decltype(D3DXVec2TransformNormal)*)(GetProcAddress(dll, "D3DXVec2TransformNormal"));
        pD3DXVec2TransformNormalArray = (decltype(D3DXVec2TransformNormalArray)*)(GetProcAddress(dll, "D3DXVec2TransformNormalArray"));
        pD3DXVec3BaryCentric = (decltype(D3DXVec3BaryCentric)*)(GetProcAddress(dll, "D3DXVec3BaryCentric"));
        pD3DXVec3CatmullRom = (decltype(D3DXVec3CatmullRom)*)(GetProcAddress(dll, "D3DXVec3CatmullRom"));
        pD3DXVec3Hermite = (decltype(D3DXVec3Hermite)*)(GetProcAddress(dll, "D3DXVec3Hermite"));
        pD3DXVec3Normalize = (decltype(D3DXVec3Normalize)*)(GetProcAddress(dll, "D3DXVec3Normalize"));
        pD3DXVec3Project = (decltype(D3DXVec3Project)*)(GetProcAddress(dll, "D3DXVec3Project"));
        pD3DXVec3ProjectArray = (decltype(D3DXVec3ProjectArray)*)(GetProcAddress(dll, "D3DXVec3ProjectArray"));
        pD3DXVec3Transform = (decltype(D3DXVec3Transform)*)(GetProcAddress(dll, "D3DXVec3Transform"));
        pD3DXVec3TransformArray = (decltype(D3DXVec3TransformArray)*)(GetProcAddress(dll, "D3DXVec3TransformArray"));
        pD3DXVec3TransformCoord = (decltype(D3DXVec3TransformCoord)*)(GetProcAddress(dll, "D3DXVec3TransformCoord"));
        pD3DXVec3TransformCoordArray = (decltype(D3DXVec3TransformCoordArray)*)(GetProcAddress(dll, "D3DXVec3TransformCoordArray"));
        pD3DXVec3TransformNormal = (decltype(D3DXVec3TransformNormal)*)(GetProcAddress(dll, "D3DXVec3TransformNormal"));
        pD3DXVec3TransformNormalArray = (decltype(D3DXVec3TransformNormalArray)*)(GetProcAddress(dll, "D3DXVec3TransformNormalArray"));
        pD3DXVec3Unproject = (decltype(D3DXVec3Unproject)*)(GetProcAddress(dll, "D3DXVec3Unproject"));
        pD3DXVec3UnprojectArray = (decltype(D3DXVec3UnprojectArray)*)(GetProcAddress(dll, "D3DXVec3UnprojectArray"));
        pD3DXVec4BaryCentric = (decltype(D3DXVec4BaryCentric)*)(GetProcAddress(dll, "D3DXVec4BaryCentric"));
        pD3DXVec4CatmullRom = (decltype(D3DXVec4CatmullRom)*)(GetProcAddress(dll, "D3DXVec4CatmullRom"));
        pD3DXVec4Cross = (decltype(D3DXVec4Cross)*)(GetProcAddress(dll, "D3DXVec4Cross"));
        pD3DXVec4Hermite = (decltype(D3DXVec4Hermite)*)(GetProcAddress(dll, "D3DXVec4Hermite"));
        pD3DXVec4Normalize = (decltype(D3DXVec4Normalize)*)(GetProcAddress(dll, "D3DXVec4Normalize"));
        pD3DXVec4Transform = (decltype(D3DXVec4Transform)*)(GetProcAddress(dll, "D3DXVec4Transform"));
        pD3DXVec4TransformArray = (decltype(D3DXVec4TransformArray)*)(GetProcAddress(dll, "D3DXVec4TransformArray"));
        pD3DXWeldVertices = (decltype(D3DXWeldVertices)*)(GetProcAddress(dll, "D3DXWeldVertices"));
    }
} d3dx9_43;