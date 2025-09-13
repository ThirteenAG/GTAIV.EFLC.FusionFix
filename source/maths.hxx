#ifndef __MATHS__H
#define __MATHS__H

#define _USE_MATH_DEFINES
#include <math.h>

#define RAD_TO_DEG								(180.0/M_PI)
#define DEG_TO_RAD								(M_PI/180.0)
#define RADTODEG(x)                             ((x) * 180.0f / M_PI)

#define RWALIGN(type, x)   type
#define rwMATRIXALIGNMENT sizeof(RwUInt32)
#define rwFRAMEALIGNMENT sizeof(RwUInt32)
#define rwV4DALIGNMENT sizeof(RwUInt32)

#if (!defined(RWFORCEENUMSIZEINT))
#define RWFORCEENUMSIZEINT ((RwInt32)((~((RwUInt32)0))>>1))
#endif /* (!defined(RWFORCEENUMSIZEINT)) */

typedef long RwFixed;
typedef int  RwInt32;
typedef unsigned int RwUInt32;
typedef short RwInt16;
typedef unsigned short RwUInt16;
typedef unsigned char RwUInt8;
typedef signed char RwInt8;

typedef struct RwV2d RwV2d;

struct RwV2d
{
    float x;
    float y;
};

typedef struct RwV3d RwV3d;

struct RwV3d
{
    float x;
    float y;
    float z;
};

typedef RwV3d RwV3D;

struct RwMatrixTag
{
    RwV3d               right;
    RwUInt32            flags;
    RwV3d               up;
    RwUInt32            pad1;
    RwV3d               at;
    RwUInt32            pad2;
    RwV3d               pos;
    RwUInt32            pad3;
};

typedef struct RwMatrixTag RWALIGN(RwMatrix, rwMATRIXALIGNMENT);

enum RwMatrixType
{
    rwMATRIXTYPENORMAL = 0x00000001,
    rwMATRIXTYPEORTHOGONAL = 0x00000002,
    rwMATRIXTYPEORTHONORMAL = 0x00000003,
    rwMATRIXTYPEMASK = 0x00000003,
    rwMATRIXTYPEFORCEENUMSIZEINT = RWFORCEENUMSIZEINT
};

typedef enum RwMatrixType RwMatrixType;

enum RwMatrixFlag
{
    rwMATRIXINTERNALIDENTITY = 0x00020000,
    rwMATRIXFLAGFORCEENUMSIZEINT = RWFORCEENUMSIZEINT
};

typedef enum RwMatrixFlag RwMatrixFlag;

enum RwMatrixOptimizations
{
    rwMATRIXOPTIMIZE_IDENTITY = 0x00020000,
    rwMATRIXOPTIMIZATIONSFORCEENUMSIZEINT = RWFORCEENUMSIZEINT
};

typedef enum RwMatrixOptimizations RwMatrixOptimizations;

class CVector
{
public:
    float	x, y, z;

    CVector()
    {}

    CVector(float fX, float fY, float fZ=0.0f)
        : x(fX), y(fY), z(fZ)
    {}

    CVector(const RwV3d& rwVec)
        : x(rwVec.x), y(rwVec.y), z(rwVec.z)
    {}

    CVector&		operator+=(const CVector& vec)
            { x += vec.x; y += vec.y; z += vec.z;
            return *this; }
    CVector&		operator+=(const RwV3d& vec)
            { x += vec.x; y += vec.y; z += vec.z;
            return *this; }
    CVector&		operator-=(const CVector& vec)
            { x -= vec.x; y -= vec.y; z -= vec.z;
            return *this; }
    CVector&		operator-=(const RwV3d& vec)
            { x -= vec.x; y -= vec.y; z -= vec.z;
            return *this; }

    inline float	Magnitude()
        { return sqrt(x * x + y * y + z * z); }
    inline float	MagnitudeSqr()
        { return x * x + y * y + z * z; }
    inline CVector&	Normalise()
        { float	fInvLen = 1.0f / Magnitude(); x *= fInvLen; y *= fInvLen; z *= fInvLen; return *this; }
    inline float	NormaliseAndMag()
        { float fLen = Magnitude(); float fInvLen = 1.0f / fLen; x *= fInvLen; y *= fInvLen; z *= fInvLen; return fLen; }

    friend inline float DotProduct(const CVector& vec1, const CVector& vec2)
        { return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z; }
    friend inline CVector CrossProduct(const CVector& vec1, const CVector& vec2)
        { return CVector(	vec1.y * vec2.z - vec1.z * vec2.y,
                            vec1.z * vec2.x - vec1.x * vec2.z,
                            vec1.x * vec2.y - vec1.y * vec2.x); }

    friend inline CVector operator*(const CVector& in, float fMul)
        { return CVector(in.x * fMul, in.y * fMul, in.z * fMul); }
    friend inline CVector operator+(const CVector& vec1, const CVector& vec2)
        { return CVector(vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z); }
    friend inline CVector operator+(const CVector& vec1, const RwV3d& vec2)
        { return CVector(vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z); }
    friend inline CVector operator-(const CVector& vec1, const CVector& vec2)
        { return CVector(vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z); }
    friend inline CVector operator-(const CVector& vec1, const RwV3d& vec2)
        { return CVector(vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z); }
    friend inline CVector operator-(const CVector& vec)
        { return CVector(-vec.x, -vec.y, -vec.z); }

    inline CVector&		FromMultiply(const class CMatrix& mat, const CVector& vec);
    inline CVector&		FromMultiply3X3(const class CMatrix& mat, const CVector& vec);
};

class CVector2D
{
public:
    float	x, y;

    CVector2D()
    {}

    CVector2D(float fX, float fY)
        : x(fX), y(fY)
    {}

    CVector2D(const RwV2d& rwVec)
        : x(rwVec.x), y(rwVec.y)
    {}

    CVector2D&		operator+=(const CVector2D& vec)
            { x += vec.x; y += vec.y;
            return *this; }
    CVector2D&		operator-=(const CVector2D& vec)
            { x -= vec.x; y -= vec.y;
            return *this; }

    inline float	Magnitude()
        { return sqrt(x * x + y * y); }
    inline float	MagnitudeSqr()
        { return x * x + y * y; }
    inline CVector2D&	Normalise()
        { float	fInvLen = 1.0f / Magnitude(); x *= fInvLen; y *= fInvLen; return *this; }
    inline float	NormaliseAndMag()
        { float fLen = Magnitude(); float fInvLen = 1.0f / fLen; x *= fInvLen; y *= fInvLen; return fLen; }

    friend inline float DotProduct(const CVector2D& vec1, const CVector2D& vec2)
        { return vec1.x * vec2.x + vec1.y * vec2.y; }
    friend inline float CrossProduct(const CVector2D& vec1, const CVector2D& vec2)
        { return vec1.x * vec2.y - vec1.y * vec2.x; }

    friend inline CVector2D operator*(const CVector2D& in, float fMul)
        { return CVector2D(in.x * fMul, in.y * fMul); }
    friend inline CVector2D operator+(const CVector2D& vec1, const CVector2D& vec2)
        { return CVector2D(vec1.x + vec2.x, vec1.y + vec2.y); }
    friend inline CVector2D operator-(const CVector2D& vec1, const CVector2D& vec2)
        { return CVector2D(vec1.x - vec2.x, vec1.y - vec2.y); }
    friend inline CVector2D operator-(const CVector2D& vec)
        { return CVector2D(-vec.x, -vec.y); }
};

class CMatrix
{
public:
    RwMatrix	matrix;
    RwMatrix*	pMatrix;
    int		    haveRwMatrix;

public:
    inline CMatrix()
        : pMatrix(nullptr), haveRwMatrix(FALSE)
    {}

    inline CMatrix(RwMatrix* pMatrix, bool bHasMatrix=false)
        : pMatrix(nullptr)
    { Attach(pMatrix, bHasMatrix); }

    inline CMatrix(const CMatrix& theMatrix)
        : pMatrix(nullptr), haveRwMatrix(FALSE), matrix(theMatrix.matrix)
    {}

    inline CMatrix(const CVector& vecRight, const CVector& vecUp, const CVector& vecAt, const CVector& vecPos)
    {
        matrix.right.x = vecRight.x;
        matrix.right.y = vecRight.y;
        matrix.right.z = vecRight.z;

        matrix.up.x = vecUp.x;
        matrix.up.y = vecUp.y;
        matrix.up.z = vecUp.z;

        matrix.at.x = vecAt.x;
        matrix.at.y = vecAt.y;
        matrix.at.z = vecAt.z;

        matrix.pos.x = vecPos.x;
        matrix.pos.y = vecPos.y;
        matrix.pos.z = vecPos.z;
    }

    int(__cdecl *RwMatrixDestroy)(RwMatrix* mpMat) = (int(__cdecl *)(RwMatrix* mpMat))0x7F2A20;

    inline ~CMatrix()
        {	if ( haveRwMatrix && pMatrix ) 
                RwMatrixDestroy(pMatrix); }

    friend inline CMatrix operator*(const CMatrix& Rot1, const CMatrix& Rot2)
        { return CMatrix(	CVector(Rot1.matrix.right.x * Rot2.matrix.right.x + Rot1.matrix.right.y * Rot2.matrix.up.x + Rot1.matrix.right.z * Rot2.matrix.at.x + Rot2.matrix.pos.x,
                                Rot1.matrix.right.x * Rot2.matrix.right.y + Rot1.matrix.right.y * Rot2.matrix.up.y + Rot1.matrix.right.z * Rot2.matrix.at.y + Rot2.matrix.pos.y,
                                Rot1.matrix.right.x * Rot2.matrix.right.z + Rot1.matrix.right.y * Rot2.matrix.up.z + Rot1.matrix.right.z * Rot2.matrix.at.z + Rot2.matrix.pos.z),
                        CVector(Rot1.matrix.up.x * Rot2.matrix.right.x + Rot1.matrix.up.y * Rot2.matrix.up.x + Rot1.matrix.up.z * Rot2.matrix.at.x + Rot2.matrix.pos.x,
                                Rot1.matrix.up.x * Rot2.matrix.right.y + Rot1.matrix.up.y * Rot2.matrix.up.y + Rot1.matrix.up.z * Rot2.matrix.at.y + Rot2.matrix.pos.y,
                                Rot1.matrix.up.x * Rot2.matrix.right.z + Rot1.matrix.up.y * Rot2.matrix.up.z + Rot1.matrix.up.z * Rot2.matrix.at.z + Rot2.matrix.pos.z),
                        CVector(Rot1.matrix.at.x * Rot2.matrix.right.x + Rot1.matrix.at.y * Rot2.matrix.up.x + Rot1.matrix.at.z * Rot2.matrix.at.x + Rot2.matrix.pos.x,
                                Rot1.matrix.at.x * Rot2.matrix.right.y + Rot1.matrix.at.y * Rot2.matrix.up.y + Rot1.matrix.at.z * Rot2.matrix.at.y + Rot2.matrix.pos.y,
                                Rot1.matrix.at.x * Rot2.matrix.right.z + Rot1.matrix.at.y * Rot2.matrix.up.z + Rot1.matrix.at.z * Rot2.matrix.at.z + Rot2.matrix.pos.z),
                        CVector(Rot1.matrix.pos.x * Rot2.matrix.right.x + Rot1.matrix.pos.y * Rot2.matrix.up.x + Rot1.matrix.pos.z * Rot2.matrix.at.x + Rot2.matrix.pos.x,
                                Rot1.matrix.pos.x * Rot2.matrix.right.y + Rot1.matrix.pos.y * Rot2.matrix.up.y + Rot1.matrix.pos.z * Rot2.matrix.at.y + Rot2.matrix.pos.y,
                                Rot1.matrix.pos.x * Rot2.matrix.right.z + Rot1.matrix.pos.y * Rot2.matrix.up.z + Rot1.matrix.pos.z * Rot2.matrix.at.z + Rot2.matrix.pos.z)); };

    friend inline CVector operator*(const CMatrix& matrix, const CVector& vec)
            { return CVector(matrix.matrix.up.x * vec.y + matrix.matrix.right.x * vec.x + matrix.matrix.at.x * vec.z + matrix.matrix.pos.x,
                                matrix.matrix.up.y * vec.y + matrix.matrix.right.y * vec.x + matrix.matrix.at.y * vec.z + matrix.matrix.pos.y,
                                matrix.matrix.up.z * vec.y + matrix.matrix.right.z * vec.x + matrix.matrix.at.z * vec.z + matrix.matrix.pos.z); };

    friend inline CMatrix operator+(const CMatrix& Rot1, const CMatrix& Rot2)
    { return CMatrix(	CVector(Rot1.matrix.right.x + Rot2.matrix.right.x, Rot1.matrix.right.y + Rot2.matrix.right.y, Rot1.matrix.right.z + Rot2.matrix.right.z),
                        CVector(Rot1.matrix.up.x + Rot2.matrix.up.x, Rot1.matrix.up.y + Rot2.matrix.up.y, Rot1.matrix.up.z + Rot2.matrix.up.z),
                        CVector(Rot1.matrix.at.x + Rot2.matrix.at.x, Rot1.matrix.at.y + Rot2.matrix.at.y, Rot1.matrix.at.z + Rot2.matrix.at.z),
                        CVector(Rot1.matrix.pos.x + Rot2.matrix.pos.x, Rot1.matrix.pos.y + Rot2.matrix.pos.y, Rot1.matrix.pos.z + Rot2.matrix.pos.z)); }

    inline CMatrix& operator=(const CMatrix& mat)
    {
        matrix = mat.matrix;
        if ( this->pMatrix )
            UpdateRwMatrix(pMatrix);
        return *this;
    }

    inline CMatrix& operator+=(const CMatrix& mat)
    {
        matrix.right.x += mat.matrix.right.x;
        matrix.right.y += mat.matrix.right.y;
        matrix.right.z += mat.matrix.right.z;

        matrix.up.x += mat.matrix.up.x;
        matrix.up.y += mat.matrix.up.y;
        matrix.up.z += mat.matrix.up.z;

        matrix.at.x += mat.matrix.at.x;
        matrix.at.y += mat.matrix.at.y;
        matrix.at.z += mat.matrix.at.z;

        matrix.pos.x += mat.matrix.pos.x;
        matrix.pos.y += mat.matrix.pos.y;
        matrix.pos.z += mat.matrix.pos.z;

        return *this;
    }

    friend inline CMatrix& Invert(const CMatrix& src, CMatrix& dst)
    {
        dst.matrix.right.x = src.matrix.right.x;
        dst.matrix.right.y = src.matrix.up.x;
        dst.matrix.right.z = src.matrix.at.x;

        dst.matrix.up.x = src.matrix.right.y;
        dst.matrix.up.y = src.matrix.up.y;
        dst.matrix.up.z = src.matrix.at.y;

        dst.matrix.at.x = src.matrix.right.z;
        dst.matrix.at.y = src.matrix.up.z;
        dst.matrix.at.z = src.matrix.at.z;

        dst.matrix.pos.x = dst.matrix.right.x * src.matrix.pos.x;
        dst.matrix.pos.y = dst.matrix.right.y * src.matrix.pos.x;
        dst.matrix.pos.z = dst.matrix.right.z * src.matrix.pos.x;

        dst.matrix.pos.x += dst.matrix.up.x * src.matrix.pos.y;
        dst.matrix.pos.y += dst.matrix.up.y * src.matrix.pos.y;
        dst.matrix.pos.z += dst.matrix.up.z * src.matrix.pos.y;

        dst.matrix.pos.x += dst.matrix.at.x * src.matrix.pos.z;
        dst.matrix.pos.y += dst.matrix.at.y * src.matrix.pos.z;
        dst.matrix.pos.z += dst.matrix.at.z * src.matrix.pos.z;

        dst.matrix.pos.x = -dst.matrix.pos.x;
        dst.matrix.pos.y = -dst.matrix.pos.y;
        dst.matrix.pos.z = -dst.matrix.pos.z;

        return dst;
    }

    friend inline CMatrix Invert(const CMatrix& src)
    {
        CMatrix		NewMatrix;
        return CMatrix(Invert(src, NewMatrix));
    }

    friend inline CVector Multiply3x3(const CMatrix& matrix, const CVector& vec)
            { return CVector(matrix.matrix.up.x * vec.y + matrix.matrix.right.x * vec.x + matrix.matrix.at.x * vec.z,
                                matrix.matrix.up.y * vec.y + matrix.matrix.right.y * vec.x + matrix.matrix.at.y * vec.z,
                                matrix.matrix.up.z * vec.y + matrix.matrix.right.z * vec.x + matrix.matrix.at.z * vec.z); };

    friend inline CVector Multiply3x3(const CVector& vec, const CMatrix& matrix)
            { return CVector(matrix.matrix.right.x * vec.x + matrix.matrix.right.y * vec.y + matrix.matrix.right.z * vec.z,
                                matrix.matrix.up.x * vec.x + matrix.matrix.up.y * vec.y + matrix.matrix.up.z * vec.z,
                                matrix.matrix.at.x * vec.x + matrix.matrix.at.y * vec.y + matrix.matrix.at.z * vec.z); };

    inline CVector*	GetRight()
        { return reinterpret_cast<CVector*>(&matrix.right); }
    inline CVector*	GetUp()
        { return reinterpret_cast<CVector*>(&matrix.up); }
    inline CVector*	GetAt()
        { return reinterpret_cast<CVector*>(&matrix.at); }
    inline CVector* GetPos()
        { return reinterpret_cast<CVector*>(&matrix.pos); }

    inline void		SetTranslateOnly(float fX, float fY, float fZ)
        { matrix.pos.x = fX; matrix.pos.y = fY; matrix.pos.z = fZ; }
    
    inline void		SetRotateX(float fAngle)
        { SetRotateXOnly(fAngle); matrix.pos.x = 0.0f; matrix.pos.y = 0.0f; matrix.pos.z = 0.0f; }
    inline void		SetRotateY(float fAngle)
        { SetRotateYOnly(fAngle); matrix.pos.x = 0.0f; matrix.pos.y = 0.0f; matrix.pos.z = 0.0f; }
    inline void		SetRotateZ(float fAngle)
        { SetRotateZOnly(fAngle); matrix.pos.x = 0.0f; matrix.pos.y = 0.0f; matrix.pos.z = 0.0f; }
    inline void		SetRotate(float fAngleX, float fAngleY, float fAngleZ)
        { SetRotateOnly(fAngleX, fAngleY, fAngleZ); matrix.pos.x = 0.0f; matrix.pos.y = 0.0f; matrix.pos.z = 0.0f; }
    inline void		SetTranslate(float fX, float fY, float fZ)	
        {	matrix.right.x = 1.0f; matrix.right.y = 0.0f; matrix.right.z = 0.0f;
            matrix.up.x = 0.0f; matrix.up.y = 1.0f; matrix.up.z = 0.0f;
            matrix.at.x = 0.0f; matrix.at.y = 0.0f; matrix.at.z = 1.0f;
            SetTranslateOnly(fX, fY, fZ); }

    inline void		ResetOrientation()
    {	
        matrix.right.x = 1.0f; matrix.right.y = 0.0f; matrix.right.z = 0.0f;
        matrix.up.x = 0.0f; matrix.up.y = 1.0f; matrix.up.z = 0.0f;
        matrix.at.x = 0.0f; matrix.at.y = 0.0f; matrix.at.z = 1.0f;
    }

    inline void		SetUnity()
    {	
        matrix.right.x = 1.0f; matrix.right.y = 0.0f; matrix.right.z = 0.0f;
        matrix.up.x = 0.0f; matrix.up.y = 1.0f; matrix.up.z = 0.0f;
        matrix.at.x = 0.0f; matrix.at.y = 0.0f; matrix.at.z = 1.0f;
        matrix.pos.x = 0.0f; matrix.pos.y = 0.0f; matrix.pos.z = 0.0f;
    }

    inline void		SetScale(float fScale)
    {	
        matrix.right.x = fScale; matrix.right.y = 0.0f; matrix.right.z = 0.0f;
        matrix.up.x = 0.0f; matrix.up.y = fScale; matrix.up.z = 0.0f;
        matrix.at.x = 0.0f; matrix.at.y = 0.0f; matrix.at.z = fScale;
        matrix.pos.x = 0.0f; matrix.pos.y = 0.0f; matrix.pos.z = 0.0f;
    }

    inline void		RotateX(float fAngle)
    {
        CMatrix		RotationMatrix;
        RotationMatrix.SetRotateX(fAngle);
        *this = *this * RotationMatrix;
    }

    inline void		RotateY(float fAngle)
    {
        CMatrix		RotationMatrix;
        RotationMatrix.SetRotateY(fAngle);
        *this = *this * RotationMatrix;
    }

    inline void		RotateZ(float fAngle)
    {
        CMatrix		RotationMatrix;
        RotationMatrix.SetRotateZ(fAngle);
        *this = *this * RotationMatrix;
    }

    inline void		Rotate(float fAngleX, float fAngleY, float fAngleZ)
    {
        CMatrix		RotationMatrix;
        RotationMatrix.SetRotate(fAngleX, fAngleY, fAngleZ);
        *this = *this * RotationMatrix;
    }

    inline void		SetRotateXOnly(float fAngle)
    {
        matrix.right.x = 1.0f;
        matrix.right.y = 0.0f;
        matrix.right.z = 0.0f;

        matrix.up.x = 0.0f;
        matrix.up.y = cos(fAngle);
        matrix.up.z = sin(fAngle);

        matrix.at.x = 0.0f;
        matrix.at.y = -sin(fAngle);
        matrix.at.z = cos(fAngle);
    }

    inline void		SetRotateYOnly(float fAngle)
    {
        matrix.right.x = cos(fAngle);
        matrix.right.y = 0.0f;
        matrix.right.z = sin(fAngle);

        matrix.up.x = 0.0f;
        matrix.up.y = 1.0f;
        matrix.up.z = 0.0f;

        matrix.at.x = -sin(fAngle);
        matrix.at.y = 0.0f;
        matrix.at.z = cos(fAngle);
    }

    inline void		SetRotateZOnly(float fAngle)
    {
        matrix.at.x = 0.0f;
        matrix.at.y = 0.0f;
        matrix.at.z = 1.0f;

        matrix.up.x = -sin(fAngle);
        matrix.up.y = cos(fAngle);
        matrix.up.z = 0.0f;

        matrix.right.x = cos(fAngle);
        matrix.right.y = sin(fAngle);
        matrix.right.z = 0.0f;
    }

    inline void		SetRotateOnly(float fAngleX, float fAngleY, float fAngleZ)
    {
        matrix.right.x = cos(fAngleZ) * cos(fAngleY) - sin(fAngleZ) * sin(fAngleX) * sin(fAngleY);
        matrix.right.y = cos(fAngleZ) * sin(fAngleX) * sin(fAngleY) + sin(fAngleZ) * cos(fAngleY);
        matrix.right.z = -cos(fAngleX) * sin(fAngleY);

        matrix.up.x = -sin(fAngleZ) * cos(fAngleX);
        matrix.up.y = cos(fAngleZ) * cos(fAngleX);
        matrix.up.z = sin(fAngleX);

        matrix.at.x = sin(fAngleZ) * sin(fAngleX) * cos(fAngleY) + cos(fAngleZ) * sin(fAngleY);
        matrix.at.y = sin(fAngleZ) * sin(fAngleY) - cos(fAngleZ) * sin(fAngleX) * cos(fAngleY);
        matrix.at.z = cos(fAngleX) * cos(fAngleY);
    }

    inline void		Attach(RwMatrix* pMatrix, bool bHasMatrix)
    {
        if ( this->pMatrix && haveRwMatrix )
            RwMatrixDestroy(this->pMatrix);

        this->pMatrix = pMatrix;
        haveRwMatrix = bHasMatrix;

        Update();
    }

    inline void		AttachRw(RwMatrix* pMatrix, bool bHasMatrix)
    {
        if ( this->pMatrix && haveRwMatrix )
            RwMatrixDestroy(this->pMatrix);

        this->pMatrix = pMatrix;
        haveRwMatrix = bHasMatrix;

        UpdateRW();
    }

    inline void		Detach()
    {
        if ( pMatrix )
        {
            if ( haveRwMatrix )
                RwMatrixDestroy(pMatrix);
            pMatrix = nullptr;
        }
    }

    inline void		UpdateRW() const
    {
        if ( pMatrix )
            UpdateRwMatrix(pMatrix);
    }

    inline void		Update()
    {
        UpdateMatrix(pMatrix);
    }

    inline void		UpdateMatrix(RwMatrix* pMatrix)
    {
        matrix.right = pMatrix->right;
        matrix.up = pMatrix->up;
        matrix.at = pMatrix->at;
        matrix.pos = pMatrix->pos;
    }

    inline void		UpdateRwMatrix(RwMatrix* pMatrix) const
    {
        pMatrix->right = matrix.right;
        pMatrix->up = matrix.up;
        pMatrix->at = matrix.at;
        pMatrix->pos = matrix.pos;
        pMatrix->flags &= ~(rwMATRIXTYPEMASK | rwMATRIXINTERNALIDENTITY);
    }

    inline void		CopyToRwMatrix(RwMatrix* pMatrix) const
    {
        pMatrix->right = this->pMatrix->right;
        pMatrix->up = this->pMatrix->up;
        pMatrix->at = this->pMatrix->at;
        pMatrix->pos = this->pMatrix->pos;
        pMatrix->flags &= ~(rwMATRIXTYPEMASK | rwMATRIXINTERNALIDENTITY);
    }

    inline void		CopyOnlyMatrix(const CMatrix& from)
    {
        matrix = from.matrix;
    }
};

// These need to land here
inline CVector& CVector::FromMultiply(const CMatrix& mat, const CVector& vec)
{
    return *this = mat * vec;
}

inline CVector& CVector::FromMultiply3X3(const CMatrix& mat, const CVector& vec)
{
    return *this = Multiply3x3(mat, vec);
}

static CMatrix identMat = {
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f },
};

inline RwV3d makeV3d(float x, float y, float z) { RwV3d v = { x, y, z }; return v; }
inline float dot(const RwV3d& a, const RwV3d& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline RwV3d scale(const RwV3d& a, float r) { return makeV3d(a.x * r, a.y * r, a.z * r); }
inline void makeRotation(CMatrix* dst, const RwV3d* axis, float angle, const RwV3d* translation)
{
    float len = dot(*axis, *axis);
    if (len != 0.0f) len = 1.0f / sqrtf(len);
    RwV3d v = scale(*axis, len);
    angle = angle * (float)M_PI / 180.0f;
    float s = sinf(angle);
    float c = cosf(angle);
    float t = 1.0f - c;

    *dst = identMat;
    dst->matrix.pos = *translation;
    dst->matrix.right.x = c + v.x * v.x * t;
    dst->matrix.right.y = v.x * v.y * t + v.z * s;
    dst->matrix.right.z = v.z * v.x * t - v.y * s;
    dst->matrix.up.x = v.x * v.y * t - v.z * s;
    dst->matrix.up.y = c + v.y * v.y * t;
    dst->matrix.up.z = v.y * v.z * t + v.x * s;
    dst->matrix.at.x = v.z * v.x * t + v.y * s;
    dst->matrix.at.y = v.y * v.z * t - v.x * s;
    dst->matrix.at.z = c + v.z * v.z * t;
    //dst->matrix.pos.x = 0.0;
    //dst->matrix.pos.y = 0.0;
    //dst->matrix.pos.z = 0.0;
}

#endif