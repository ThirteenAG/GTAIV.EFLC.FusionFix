#pragma once
#include <stdint.h>
#include <random>

class CSnow
{
public:
    struct RwV3d
    {
        float x;
        float y;
        float z;
    };

    struct RwMatrix
    {
        RwV3d    right;
        uint32_t flags;
        RwV3d    up;
        uint32_t pad1;
        RwV3d    at;
        uint32_t pad2;
        RwV3d    pos;
        uint32_t pad3;
    };

    struct CVector
    {
        float x, y, z;
    };

    struct snowFlake {
        CVector pos;
        float xChange;
        float yChange;
    };

    class CBox {
    public:
        CVector min;
        CVector max;
        void Set(CVector a, CVector b) { min = a; max = b; }
    };

    struct Im3DVertex
    {
        RwV3d position;
        RwV3d normal;
        uint32_t color;
        float u, v;
    };

public:
    static inline int32_t ms_initialised;
    static inline IDirect3DVertexDeclaration9* im3ddecl;
    static inline IDirect3DVertexBuffer9* im3dvertbuf;
    static inline IDirect3DIndexBuffer9* im3dindbuf;
    static inline IDirect3DTexture9* snowTex;
    static inline int32_t num3DVertices;
    static inline int32_t ms_fbWidth;
    static inline int32_t ms_fbHeight;
    static inline float zn = 0.0f;
    static inline float zf = 500.0f;

public:
    static inline float targetSnow = 0.0f;
    static inline int snowFlakes = 400;
    static inline std::vector<snowFlake> snowArray;
    static inline bool snowArrayInitialized = false;
    static inline float Snow;
    static inline CBox snowBox;

public:
    static inline void openIm3D(LPDIRECT3DDEVICE9 pDev)
    {
        D3DVERTEXELEMENT9 elements[5] = {
            { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, offsetof(Im3DVertex, normal), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
            { 0, offsetof(Im3DVertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
            { 0, offsetof(Im3DVertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        static constexpr auto NUMINDICES = 10000;
        static constexpr auto NUMVERTICES = 10000;

        pDev->CreateVertexDeclaration(elements, &im3ddecl);
        assert(im3ddecl);

        pDev->CreateVertexBuffer(NUMVERTICES * sizeof(Im3DVertex), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &im3dvertbuf, nullptr);
        assert(im3dvertbuf);

        pDev->CreateIndexBuffer(NUMINDICES * sizeof(uint16_t), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &im3dindbuf, nullptr);
        assert(im3dindbuf);
    }

    static inline void Reset()
    {
        auto SafeRelease = [](auto ppT) {
            if (*ppT)
            {
                (*ppT)->Release();
                *ppT = NULL;
            }
        };
        SafeRelease(&im3ddecl);
        SafeRelease(&im3dvertbuf);
        SafeRelease(&im3dindbuf);
        SafeRelease(&snowTex);
        ms_initialised = 0;
    }

    static inline void im3DRenderIndexedPrimitive(LPDIRECT3DDEVICE9 pDev, D3DPRIMITIVETYPE primType, void* indices, int32_t numIndices)
    {
        auto lockIndices = [](auto indexBuffer, uint32_t offset, uint32_t size, uint32_t flags) -> uint16_t*
        {
            if (indexBuffer == 0)
                return 0;
            uint16_t* indices;
            auto ibuf = indexBuffer;
            ibuf->Lock(offset, size, (void**)&indices, flags);
            return indices;
        };

        auto unlockIndices = [](auto indexBuffer)
        {
            if (indexBuffer == 0)
                return;
            auto ibuf = indexBuffer;
            ibuf->Unlock();
        };

        auto lockedindices = lockIndices(im3dindbuf, 0, numIndices * sizeof(uint16_t), D3DLOCK_DISCARD);
        memcpy(lockedindices, indices, numIndices * sizeof(uint16_t));
        unlockIndices(im3dindbuf);

        pDev->SetIndices(im3dindbuf);

        uint32_t primCount = 0;
        switch (primType) {
        case D3DPT_LINELIST:
            primCount = numIndices / 2;
            break;
        case D3DPT_TRIANGLELIST:
            primCount = numIndices / 3;
            break;
        case D3DPT_TRIANGLESTRIP:
            primCount = numIndices - 2;
            break;
        case D3DPT_TRIANGLEFAN:
            primCount = numIndices - 2;
            break;
        case D3DPT_POINTLIST:
            primCount = numIndices;
            break;
        }
        pDev->DrawIndexedPrimitive(primType, 0, 0, num3DVertices, 0, primCount);
    }

    static inline void im3DTransform(LPDIRECT3DDEVICE9 pDev, RwMatrix* view, void* vertices, int32_t numVertices, RwMatrix* world, uint32_t flags, uint8_t rainDropsOnScreen)
    {
        auto lockVertices = [](auto vertexBuffer, uint32_t offset, uint32_t size, uint32_t flags) -> uint8_t*
        {
            if (vertexBuffer == 0)
                return 0;
            uint8_t* verts;
            auto vertbuf = vertexBuffer;
            vertbuf->Lock(offset, size, (void**)&verts, flags);
            return verts;
        };

        auto unlockVertices = [](auto vertexBuffer)
        {
            if (vertexBuffer == 0)
                return;
            auto vertbuf = vertexBuffer;
            vertbuf->Unlock();
        };

        D3DXMATRIXA16 ProjectionMatrix;

        if (rainDropsOnScreen)
        {
            if (zn < 10.0f)
                zn += 0.0001f;
            else
                zn = 10.0f;
        }
        else
        {
            zn = 0.0f;
        }
        D3DXMatrixPerspectiveFovLH(&ProjectionMatrix, 3.14f / 2.0f, (float)ms_fbWidth / (float)ms_fbHeight, zn, zf);
        pDev->SetTransform(D3DTS_PROJECTION, &ProjectionMatrix);

        D3DMATRIX ViewMatrix{
            view->right.x,    view->right.y, view->right.z, 0.0f,
            view->up.x,       view->up.y,    view->up.z, 0.0f,
            view->at.x,       view->at.y,    view->at.z, 0.0f,
            view->pos.x,      view->pos.y,   view->pos.z, 1.0f,
        };
        pDev->SetTransform(D3DTS_VIEW, &ViewMatrix);

        D3DMATRIX WorldMatrix{
            world->right.x, world->right.z,   world->right.y, 0.0f,
            world->up.x,       world->up.z,      world->up.y, 0.0f,
            world->at.x,       world->at.z,      world->at.y, 0.0f,
            world->pos.x,     world->pos.z,     world->pos.y, 1.0f,
        };
        pDev->SetTransform(D3DTS_WORLD, &WorldMatrix);

        pDev->SetVertexDeclaration(im3ddecl);

        uint8_t* lockedvertices = lockVertices(im3dvertbuf, 0, numVertices * sizeof(Im3DVertex), D3DLOCK_DISCARD);
        memcpy(lockedvertices, vertices, numVertices * sizeof(Im3DVertex));
        unlockVertices(im3dvertbuf);

        pDev->SetStreamSource(0, im3dvertbuf, 0, sizeof(Im3DVertex));

        num3DVertices = numVertices;
    }

    static inline void AddSnow(LPDIRECT3DDEVICE9 pDev, int32_t Width, int32_t Height, RwMatrix* camMatrix, RwMatrix* viewMatrix, float* fTimeStep, uint8_t rainDropsOnScreen)
    {
        auto clamp = [](auto v, auto low, auto high) -> float
        {
            return ((v) < (low) ? (low) : (v) > (high) ? (high) : (v));
        };

        if (!ms_initialised)
        {
            ms_fbWidth = Width;
            ms_fbHeight = Height;

            snowFlakes = 2000;
            snowArray.resize(snowFlakes);
            openIm3D(pDev);

            if (!snowTex)
            {
                static constexpr auto MaskSize = 32;
                D3DXCreateTexture(pDev, MaskSize, MaskSize, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &snowTex);
                D3DLOCKED_RECT LockedRect;
                snowTex->LockRect(0, &LockedRect, NULL, 0);
                uint8_t* pixels = (uint8_t*)LockedRect.pBits;
                int32_t stride = LockedRect.Pitch;
                for (int y = 0; y < MaskSize; y++)
                {
                    float yf = ((y + 0.5f) / MaskSize - 0.5f) * 2.0f;
                    for (int x = 0; x < MaskSize; x++)
                    {
                        float xf = ((x + 0.5f) / MaskSize - 0.5f) * 2.0f;
                        memset(&pixels[y * stride + x * 4], xf * xf + yf * yf < 1.0f ? 0xFF : 0x00, 4);
                    }
                }
                snowTex->UnlockRect(0);
            }

            ms_initialised = 1;
        }

        // InterpolationValue is a value between 0 and 1 corresponds to current minute 0-60
        auto InterpolationValue = 0.5f;
        //targetSnow = 1.0f;

        if (targetSnow != 0.0f || Snow != 0.0f) { // Weather == SNOW

            if (targetSnow == 0.0f) {
                Snow -= Snow / 100.0f;

                Snow = clamp(Snow, 0.0f, 1.0f);
            }
            else {
                //Snow = InterpolationValue * 4.0f;
                if (Snow < targetSnow)
                    Snow += targetSnow / 100.0f;

                Snow = clamp(Snow, 0.0f, 1.0f);

                //Snow = targetSnow;
                //Snow = clamp(Snow, 0.0f, targetSnow);
            }
        }
        else {
            return;
        }

        {
            auto snowAmount = (int)min(snowFlakes, Snow * snowFlakes);
            snowBox.Set(CVector(camMatrix->pos.x, camMatrix->pos.y, camMatrix->pos.z), CVector(camMatrix->pos.x, camMatrix->pos.y, camMatrix->pos.z));
            snowBox.min.x -= 40.0f;
            snowBox.min.y -= 40.0f;
            snowBox.max.x += 40.0f;
            snowBox.min.z -= 15.0f; // -= 10.0f; in PSP
            snowBox.max.z += 15.0f; // += 10.0f; in PSP
            snowBox.max.y += 40.0f;

            if (!snowArrayInitialized)
            {
                static auto GetRandomFloat = [](float range = RAND_MAX) -> float
                {
                    static std::random_device rd;
                    static std::mt19937 gen(rd());
                    std::uniform_real_distribution<> dis(0.0f, range);
                    return static_cast<float>(dis(gen));
                };
                
                snowArrayInitialized = true;
                for (int i = 0; i < snowFlakes; i++)
                {
                    snowArray[i].pos.x = snowBox.min.x + ((snowBox.max.x - snowBox.min.x) * (GetRandomFloat() / (float)RAND_MAX));
                    snowArray[i].pos.y = snowBox.min.y + ((GetRandomFloat() / (float)RAND_MAX) * (snowBox.max.y - snowBox.min.y));
                    snowArray[i].pos.z = snowBox.min.z + ((GetRandomFloat() / (float)RAND_MAX) * (snowBox.max.z - snowBox.min.z));
                    snowArray[i].xChange = 0.0f;
                    snowArray[i].yChange = 0.0f;
                }
            }

            LPDIRECT3DSTATEBLOCK9 pStateBlock = NULL;
            pDev->CreateStateBlock(D3DSBT_ALL, &pStateBlock);
            pStateBlock->Capture();

            pDev->SetRenderState(D3DRS_FOGENABLE, 0);
            pDev->SetTexture(0, snowTex);
            pDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_CONSTANT);
            pDev->SetTextureStageState(0, D3DTSS_CONSTANT, D3DCOLOR_ARGB(200, 255, 255, 255));
            pDev->SetRenderState(D3DRS_ZENABLE, 1);
            pDev->SetRenderState(D3DRS_ZWRITEENABLE, 0);
            pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); // 1 in psp, 5 in mobile
            pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE); // 1 in psp, 6 in mobile

            pDev->SetVertexShader(NULL);
            pDev->SetPixelShader(NULL);

            auto mat = *camMatrix;

            int i = 0;
            for (; i < snowAmount; i++)
            {
                float& xPos = snowArray[i].pos.x; // s3
                float& yPos = snowArray[i].pos.y;
                float& zPos = snowArray[i].pos.z;
                float& xChangeRate = snowArray[i].xChange; // s4
                float& yChangeRate = snowArray[i].yChange; // s5

                float minChange = -*fTimeStep / 10.0f;
                float maxChange = -minChange;

                zPos -= maxChange;

                xChangeRate += minChange + (2 * maxChange * (rand() / (float)RAND_MAX));

                yChangeRate += minChange + (2 * maxChange * (rand() / (float)RAND_MAX));

                xChangeRate = clamp(xChangeRate, minChange, maxChange);
                yChangeRate = clamp(yChangeRate, minChange, maxChange);

                yPos += yChangeRate;
                xPos += xChangeRate;

                while (zPos < snowBox.min.z) {
                    zPos += 30.0f; // += 20.0f; in PSP
                }

                while (zPos > snowBox.max.z) {
                    zPos -= 30.0f; // -= 20.0f; in PSP
                }

                while (xPos < snowBox.min.x) {
                    xPos += 80.0f;
                }

                while (xPos > snowBox.max.x) {
                    xPos -= 80.0f;
                }

                while (yPos < snowBox.min.y) {
                    yPos += 80.0f;
                }

                while (yPos > snowBox.max.y) {
                    yPos -= 80.0f;
                }

                mat.pos.x = snowArray[i].pos.x;
                mat.pos.y = snowArray[i].pos.y;
                mat.pos.z = snowArray[i].pos.z;

                static constexpr Im3DVertex snowVertexBuffer[] =
                {
                    {RwV3d(0.1f  / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 1.0f},
                    {RwV3d(-0.1f / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 1.0f},
                    {RwV3d(-0.1f / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 0.0f},
                    {RwV3d(0.1f  / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 1.0f},
                    {RwV3d(0.1f  / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 0.0f},
                    {RwV3d(-0.1f / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 0.0f},
                };

                static constexpr uint16_t snowRenderOrder[] =
                {
                    0, 1, 2, 3, 4, 5
                };

                #define ARRAY_SIZE(array)	(sizeof(array) / sizeof(array[0]))
                im3DTransform(pDev, viewMatrix, (void*)snowVertexBuffer, ARRAY_SIZE(snowVertexBuffer), (RwMatrix*)&mat, 1, rainDropsOnScreen);
                im3DRenderIndexedPrimitive(pDev, D3DPT_TRIANGLELIST, (void*)snowRenderOrder, ARRAY_SIZE(snowRenderOrder));
            }

            pDev->SetRenderState(D3DRS_ZENABLE, 1);
            pDev->SetRenderState(D3DRS_ZWRITEENABLE, 1);
            pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            pDev->SetRenderState(D3DRS_FOGENABLE, 0);

            pStateBlock->Apply();
            pStateBlock->Release();
        }
    }
};