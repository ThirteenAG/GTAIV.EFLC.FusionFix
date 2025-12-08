module;

#include <common.hxx>
#include <D3D9Types.h>

export module comvars;

import common;

#define VALIDATE_SIZE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)

enum GameLanguage
{
    eLANG_AMERICAN,
    eLANG_FRENCH,
    eLANG_GERMAN,
    eLANG_ITALIAN,
    eLANG_SPANISH,
    eLANG_RUSSIAN,
    eLANG_JAPANESE,
};

enum eCamMode
{
    CAM_SKELETON = 0,
    CAM_FOLLOW_PED = 1,
    CAM_FOLLOW_VEHICLE = 2,
    CAM_INTERP = 3,
    CAM_SHAKE = 4,
    CAM_FINAL = 5,
    CAM_SCRIPT = 6,
    CAM_GAME = 7,
    CAM_TRANS = 8,
    CAM_AIM_WEAPON = 9,
    CAM_BUSTED = 10,
    CAM_PHOTO = 11,
    CAM_IDLE = 12,
    CAM_2_PLAYER = 13,
    CAM_SCRIPTED = 14,
    CAM_CUTSCENE = 15,
    CAM_WASTED = 16,
    CAM_1ST_PERSON = 17,
    CAM_2_PLAYER_VEH = 18,
    CAM_AIM_WEAPON_VEH = 19,
    CAM_VIEWPORTS = 20,
    CAM_HISTORY = 21,
    CAM_CINEMATIC = 22,
    CAM_CINEMATIC_HELI_CHASE = 23,
    CAM_CINEMATIC_CAM_MAN = 24,
    CAM_SPLINE = 25,
    CAM_CINEMATOGRAPHY = 26,
    CAM_FPS_WEAPON = 27,
    CAM_FIRE_TRUCK = 28,
    CAM_RADAR = 29,
    CAM_WEAPON_AIMING = 30,
    CAM_ANIMATED = 31,
    CAM_INTERMEZZO = 32,
    CAM_VIEW_SEQ = 33,
    CAM_VIEWFIND = 34,
    CAM_PLAYER_SETTINGS = 35,
    CAM_CINEMATIC_VEH_OFFSET = 36,
    CAM_REPLAY = 37,
    CAM_FREE = 38,
    CAM_DEBUG = 39,
    CAM_MARKET = 40,
    CAM_SECTOR = 41,
    NUM_CAM_MODES,
};

export enum eVehicleType
{
    VEHICLETYPE_AUTOMOBILE = 0x0,
    VEHICLETYPE_BIKE = 0x1,
    VEHICLETYPE_BOAT = 0x2,
    VEHICLETYPE_TRAIN = 0x3,
    VEHICLETYPE_HELI = 0x4,
    VEHICLETYPE_PLANE = 0x5,
};

export enum class TaskID : int32_t
{
    TaskSimpleMovePlayer = 2,
    TaskComplexPlayerOnFoot = 4,
    TaskComplexPlayerGun = 6,
    TaskComplexPlayerPlaceCarBomb = 7,
    TaskComplexPlayerIdles = 8,
    TaskComplexMedicTreatInjuredPed = 100,
    TaskComplexTreatAccident = 101,
    TaskComplexDriveFireTruck = 107,
    TaskComplexUseWaterCannon = 109,
    TaskSimpleAssessInjuredPed = 111,
    TaskComplexMedicDriver = 112,
    TaskComplexMedicPassenger = 113,
    TaskComplexMedicWandering = 113,
    TaskComplexPlayerSettingsTask = 115,
    TaskSimpleNone = 200,
    TaskSimpleSetCharCombatDecisionMaker = 200,
    TaskSimpleLeaveGroup = 200,
    TaskSimplePause = 202,
    TaskSimplePauseSystemTimer = 202,
    TaskSimpleStandStill = 203,
    TaskSimpleGetUp = 205,
    TaskComplexGetUpAndStandStill = 206,
    TaskSimpleFall = 207,
    TaskComplexFallAndGetUp = 208,
    TaskSimpleJumpLaunch = 210,
    TaskComplexJump = 211,
    TaskSimpleDie = 212,
    TaskComplexDie = 217,
    TaskSimpleDead = 218,
    TaskSimpleTired = 219,
    TaskSimpleSitDown = 220,
    TaskComplexSitIdle = 221,
    TaskSimpleStandUp = 222,
    TaskComplexSitDownThenIdleThenStandUp = 223,
    TaskComplexHitResponse = 230,
    TaskComplexUseEffect = 233,
    TaskComplexWaitAtAttractor = 234,
    TaskComplexUseAttractor = 235,
    TaskComplexWaitForDryWeather = 236,
    TaskComplexWaitForBus = 237,
    TaskSimpleWaitForBus = 238,
    TaskSimpleJumpInAir = 241,
    TaskSimpleJumpLand = 242,
    TaskComplexSequence = 244,
    TaskComplexOnFire = 250,
    TaskSimpleClimb = 254,
    TaskComplexInWater = 268,
    TaskSimpleTriggerLookAt = 269,
    TaskSimpleClearLookAt = 270,
    TaskSimpleSetCharDecisionMaker = 271,
    TaskComplexUseSequence = 273,
    TaskComplexInjuredOnGround = 278,
    TaskSimplePathfindProblem = 280,
    TaskSimpleDoNothing = 281,
    TaskSimpleMoveStandStill = 282,
    TaskSimpleMoveDoNothing = 283,
    TaskSimpleMoveNone = 284,
    TaskComplexControlMovement = 285,
    TaskComplexMoveSequence = 286,
    TaskComplexClimbLadder = 287,
    TaskSimpleClimbLadder = 288,
    TaskComplexClimbLadderFully = 289,
    TaskComplexMoveAroundCoverPoints = 290,
    TaskSimplePlayRandomAmbients = 291,
    TaskSimpleMovePathfindProblem = 292,
    TaskSimpleMoveInAir = 293,
    TaskSimpleNetworkClone = 294,
    TaskSimpleInjuredOnGroundTransition = 295,
    TaskComplexUseClimbOnRoute = 296,
    TaskComplexUseDropDownOnRoute = 297,
    TaskComplexUseLadderOnRoute = 298,
    TaskSimpleSay = 301,
    TaskSimpleShakeFist = 302,
    TaskSimpleAffectSecondaryBehaviour = 306,
    TaskSimplePickUpObject = 312,
    TaskSimplePutDownObject = 313,
    TaskComplexPickUpObject = 314,
    TaskComplexPickUpAndCarryObject = 315,
    TaskSimpleOpenDoor = 317,
    TaskSimpleShovePed = 318,
    TaskSimpleSwapWeapon = 319,
    TaskComplexShockingEventWatch = 321,
    TaskComplexShockingEventFlee = 322,
    TaskComplexShockingEventGoto = 323,
    TaskComplexShockingEventHurryAway = 324,
    TaskSimplePutOnHelmet = 325,
    TaskSimpleTakeOffHelmet = 326,
    TaskComplexCarReactToVehicleCollision = 327,
    TaskComplexReactToRanPedOver = 328,
    TaskComplexCarReactToVehicleCollisionGetOut = 329,
    TaskComplexStationaryScenario = 350,
    TaskComplexSeatedScenario = 351,
    TaskComplexWanderingScenario = 352,
    TaskComplexDrivingScenario = 353,
    TaskComplexMoveBetweenPointsScenario = 354,
    TaskComplexChatScenario = 355,
    TaskComplexWaitForSeatToBeFree = 356,
    TaskComplexWaitForMyCarToStop = 357,
    TaskComplexWaitForDoorToBeOpen = 358,
    TaskComplexDriveWanderForTime = 359,
    TaskComplexWaitForTime = 360,
    TaskComplexWaitTillItsOkToStop = 360,
    TaskComplexGetInCarScenario = 361,
    TaskComplexWalkWithPedScenario = 362,
    TaskComplexMobileChatScenario = 363,
    TaskSimpleSayAudio = 364,
    TaskComplexPoliceSniperScenario = 365,
    TaskComplexMobileMakeCall = 366,
    TaskComplexWaitForSteppingOut = 367,
    TaskSimpleRunDictAnim = 400,
    TaskSimpleRunNamedAnim = 401,
    TaskSimpleHitFromBack = 403,
    TaskSimpleHitFromFront = 404,
    TaskSimpleHitFromLeft = 405,
    TaskSimpleHitFromRight = 406,
    TaskSimpleHitWall = 411,
    TaskSimpleCower = 412,
    TaskSimpleHandsUp = 413,
    TaskSimpleDuck = 415,
    TaskComplexMelee = 431,
    TaskSimpleMoveMeleeMovement = 432,
    TaskSimpleMeleeActionResult = 433,
    TaskSimpleHitHead = 500,
    TaskComplexEvasiveStep = 502,
    TaskComplexWalkRoundCarWhileWandering = 508,
    TaskComplexWalkRoundFire = 514,
    TaskComplexStuckInAir = 515,
    TaskComplexMove_StepAwayFromCollisionObjects = 516,
    TaskComplexWalkRoundEntity = 517,
    TaskSimpleSidewaysDive = 519,
    TaskComplexInvestigateDeadPed = 600,
    TaskComplexReactToGunAimedAt = 601,
    TaskComplexExtinguishFires = 604,
    TaskComplexAvoidPlayerTargetting = 605,
    TaskComplexStealCar = 702,
    TaskComplexLeaveCarAndFlee = 706,
    TaskComplexLeaveCarAndWander = 707,
    TaskComplexScreamInCarThenLeave = 708,
    TaskComplexCarDriveBasic = 709,
    TaskComplexDriveToPoint = 710,
    TaskComplexCarDriveWander = 711,
    TaskComplexLeaveAnyCar = 714,
    TaskComplexGetOffBoat = 716,
    TaskComplexEnterAnyCarAsDriver = 717,
    TaskComplexCarDriveTimed = 719,
    TaskComplexDrivePointRoute = 721,
    TaskComplexCarSetTempAction = 723,
    TaskComplexCarDriveMission = 724,
    TaskComplexCarDrive = 725,
    TaskComplexCarDriveMissionFleeScene = 726,
    TaskComplexCarDriveMissionKillPed = 729,
    TaskComplexPlayerDrive = 732,
    TaskComplexNewGetInVehicle = 734,
    TaskComplexOpenVehicleDoor = 735,
    TaskComplexClimbIntoVehicle = 736,
    TaskComplexClearVehicleSeat = 737,
    TaskComplexNewExitVehicle = 738,
    TaskComplexShuffleBetweenSeats = 739,
    TaskComplexGangDriveby = 740,
    TaskComplexCloseVehicleDoor = 741,
    TaskComplexBackOff = 742,
    TaskComplexBeArrestedAndDrivenAway = 743,
    TaskComplexArrestedAIPedAndDriveAway = 744,
    TaskComplexGoToCarDoorAndStandStill = 800,
    TaskSimpleCarAlign = 801,
    TaskSimpleCarOpenDoorFromOutside = 802,
    TaskSimpleCarOpenLockedDoorFromOutside = 803,
    TaskSimpleCarCloseDoorFromInside = 805,
    TaskSimpleCarCloseDoorFromOutside = 806,
    TaskSimpleCarGetIn = 807,
    TaskSimpleCarShuffle = 808,
    TaskSimpleCarSetPedInVehicle = 811,
    TaskSimpleCarGetOut = 813,
    TaskSimpleCarJumpOut = 814,
    TaskSimpleCarSetPedOut = 816,
    TaskSimpleCarSlowDragPedOut = 820,
    TaskSimpleCarSlowBeDraggedOut = 821,
    TaskSimpleSetPedAsAutoDriver = 827,
    TaskSimpleWaitUntilPedIsOutCar = 829,
    TaskSimpleCreateCarAndGetIn = 832,
    TaskSimpleStartCar = 835,
    TaskSimpleShunt = 836,
    TaskSimpleSmashCarWindow = 838,
    TaskSimpleThrowGrenadeFromVehicle = 839,
    TaskSimpleMoveGoToPoint = 900,
    TaskComplexGoToPointShooting = 901,
    TaskSimpleMoveAchieveHeading = 902,
    TaskComplexMoveGoToPointAndStandStill = 903,
    TaskComplexGoToPointAndStandStillTimed = 903,
    TaskComplexMoveFollowPointRoute = 905,
    TaskComplexMoveSeekEntity = 907,
    TaskComplexSmartFleePoint = 910,
    TaskComplexSmartFleeEntity = 911,
    TaskComplexWanderStandard = 912,
    TaskComplexWander = 912,
    TaskComplexWanderFlee = 912,
    TaskComplexWanderMedic = 912,
    TaskComplexWanderCriminal = 912,
    TaskComplexWanderCop = 912,
    TaskComplexFollowLeaderInFormation = 913,
    TaskComplexGoToAttractor = 915,
    TaskComplexMoveAvoidOtherPedWhileWandering = 917,
    TaskComplexGoToPointAnyMeans = 918,
    TaskComplexTurnToFaceEntityOrCoord = 920,
    TaskComplexSeekEntityAnyMeans = 922,
    TaskComplexFollowLeaderAnyMeans = 923,
    TaskComplexGoToPointAiming = 924,
    TaskComplexTrackEntity = 925,
    TaskComplexFleeAnyMeans = 927,
    TaskComplexFleeShooting = 928,
    TaskComplexFollowPatrolRoute = 931,
    TaskComplexSeekEntityAiming = 933,
    TaskSimpleSlideToCoord = 934,
    TaskComplexFollowPedFootsteps = 936,
    TaskSimpleMoveTrackingEntity = 941,
    TaskComplexMoveFollowNavMeshRoute = 942,
    TaskSimpleMoveGoToPointOnRoute = 943,
    TaskComplexEscapeBlast = 944,
    TaskComplexMoveGetToPointContinuous = 945,
    TaskComplexCop = 946,
    TaskComplexSearchForPedOnFoot = 947,
    TaskComplexSearchForPedInCar = 948,
    TaskComplexMoveWander = 949,
    TaskComplexMoveBeInFormation = 950,
    TaskComplexMoveCrowdAroundLocation = 951,
    TaskComplexMoveCrossRoadAtTrafficLights = 952,
    TaskComplexMoveWaitForTraffic = 953,
    TaskComplexMoveGoToPointStandStillAchieveHeading = 955,
    TaskSimpleMoveWaitForNavMeshSpecialActionEvent = 956,
    TaskComplexMoveReturnToRoute = 958,
    TaskComplexMoveGoToShelterAndWait = 959,
    TaskComplexMoveGetOntoMainNavMesh = 960,
    TaskSimpleMoveSlideToCoord = 961,
    TaskComplexMoveGoToPointRelativeToEntityAndStandStill = 962,
    TaskComplexCopHelicopter = 963,
    TaskComplexHelicopterStrafe = 964,
    TaskComplexUseMobilePhoneAndMovement = 965,
    TaskComplexFleeAndDive = 966,
    TaskComplexGetOutOfWater = 967,
    TaskComplexSearchWander1 = 968,
    TaskComplexSearchWander2 = 969,
    TaskComplexDestroyCar = 1003,
    TaskComplexDestroyCarArmed = 1005,
    TaskSimpleBeHit = 1008,
    TaskSimpleThrowProjectile = 1018,
    TaskSimpleSetCharIgnoreWeaponRangeFlag = 1033,
    TaskComplexSeekCoverShooting = 1036,
    TaskComplexSeekCover = 1036,
    TaskComplexAimAndThrowProjectile = 1038,
    TaskSimplePlayerAimProjectile = 1039,
    TaskComplexGun = 1040,
    TaskSimpleAimGun = 1041,
    TaskSimpleFireGun = 1042,
    TaskSimpleReloadGun = 1043,
    TaskComplexSlideIntoCover = 1044,
    TaskComplexPlayerInCover = 1046,
    TaskComplexGoIntoCover = 1047,
    TaskComplexCombatClosestTargetInArea = 1048,
    TaskSimpleNewGangDriveBy = 1049,
    TaskComplexCombatAdditionalTask = 1050,
    TaskSimplePlayUpperCombatAnim = 1051,
    TaskSimplePlaySwatSignalAnim = 1052,
    TaskSimpleCombatRoll = 1053,
    TaskComplexNewUseCover = 1054,
    TaskSimplePlayAnimAndSlideIntoCover = 1055,
    TaskSimpleLoadAnimPlayAnim = 1056,
    TaskSimplePlayAnimAndSlideOutOfCover = 1057,
    TaskComplexThrowProjectile = 1058,
    TaskSimpleArrestPed = 1100,
    TaskComplexArrestPed = 1101,
    TaskSimplePlayerBeArrested = 1108,
    TaskComplexGangHasslePed = 1212,
    TaskSimpleTogglePedThreatScanner = 1301,
    TaskSimpleMoveSwim = 1304,
    TaskSimpleDuckToggle = 1306,
    TaskSimpleWaitUntilAreaCodesMatch = 1307,
    TaskComplexMoveAboutInjured = 1310,
    TaskComplexRevive = 1311,
    TaskComplexReact = 1312,
    TaskComplexUseMobilePhone = 1600,
    TaskComplexCombat = 1900,
    TaskComplexCombatFireSubtask = 1901,
    TaskComplexCombatAdvanceSubtask = 1902,
    TaskComplexCombatSeekCoverSubtask = 1903,
    TaskComplexCombatRetreatSubtask = 1904,
    TaskComplexCombatChargeSubtask = 1905,
    TaskComplexCombatInvestigateSubtask = 1906,
    TaskComplexCombatPullFromCarSubtask = 1907,
    TaskComplexCombatPersueInCarSubtask = 1908,
    TaskComplexCombatBustPed = 1910,
    TaskComplexCombatExecutePedSubtask = 1911,
    TaskComplexCombatFlankSubtask = 1913,
    TaskComplexSetAndGuardArea = 1932,
    TaskComplexStandGuard = 1933,
    TaskComplexSeperate = 1934,
    TaskSimpleNMRelax = 2104,
    TaskSimpleNMRollUpAndRelax = 2104,
    TaskSimpleNMPose = 2105,
    TaskSimpleNMBrace = 2106,
    TaskSimpleNMShot = 2107,
    TaskSimpleNMHighFall = 2108,
    TaskSimpleNMBalance = 2109,
    TaskSimpleNMExplosion = 2110,
    TaskSimpleNMOnFire = 2111,
    TaskSimpleNMScriptControl = 2112,
    TaskSimpleNMJumpRollFromRoadVehicle = 2113,
    TaskSimpleNMFlinch = 2114,
    TaskSimpleNMSit = 2115,
    TaskSimpleNMFallDown = 2116,
    TaskSimpleBlendFromNM = 2117,
    TaskComplexNM = 2118
};

export constexpr TaskID operator-(TaskID id) noexcept
{
    return static_cast<TaskID>(-std::to_underlying(id));
}

export unsigned int hashStringLowercaseFromSeed(const char* str, unsigned int seed)
{
    auto hash = seed;
    auto currentChar = str;

    if (*str == '"')
        currentChar = str + 1;

    while (*currentChar)
    {
        char character = *currentChar;

        if (*str == '"' && character == '"')
            break;

        ++currentChar;

        if ((uint8_t)(character - 'A') <= 25)
        {
            character += 32; // Convert uppercase to lowercase
        }
        else if (character == '\\')
        {
            character = '/';
        }

        hash = (1025 * (hash + character) >> 6) ^ 1025 * (hash + character);
    }

    return 32769 * (9 * hash ^ (9 * hash >> 11));
}

export class CBaseDC
{
public:
    uint32_t field_1;

public:
    static inline void* AppendAddr;
    void Append()
    {
        reinterpret_cast<void(__thiscall*)(CBaseDC*)>(AppendAddr)(this);
    }

    static inline void* operator_newAddr;
    void* operator new(std::size_t size)
    {
        return reinterpret_cast<void* (__cdecl*)(std::size_t, int32_t)>(operator_newAddr)(size, 0);
    }

    virtual ~CBaseDC() {}
    virtual void DrawCommand() {}
    virtual int32_t GetSize() { return sizeof(CBaseDC); }
};

export class T_CB_Generic_NoArgs : public CBaseDC
{
public:
    void (*cb)();

public:
    T_CB_Generic_NoArgs(void (*c)()) : CBaseDC()
    {
        cb = c;
    }

    void DrawCommand() override
    {
        cb();
    }

    int32_t GetSize() override
    {
        return sizeof(T_CB_Generic_NoArgs);
    }
};

export namespace rage
{
#define POOL_FLAG_ISFREE 0x80
#define POOL_FLAG_REFERENCEMASK 0x7f

    class fwBasePool
    {
    public:
        uint8_t* m_aStorage;
        uint8_t* m_aFlags;
        int32_t m_nSize;
        int32_t m_nStorageSize;
        int32_t m_nFirstFreeIndex;
        int32_t m_numSlotsUsed;

    public:
        int32_t GetSize() const
        {
            return m_nSize;
        }

        int32_t GetNoOfUsedSpaces() const
        {
            return m_numSlotsUsed;
        }

        int32_t GetNoOfFreeSpaces() const
        {
            return m_nSize - m_numSlotsUsed;
        }

        int32_t GetPoolSize() const
        {
            return m_nSize;
        }

        bool GetIsFree(int32_t index) const
        {
            return (m_aFlags[index] & POOL_FLAG_ISFREE) != 0;
        }

        int GetId(int i) const
        {
            return m_aFlags[i] & POOL_FLAG_REFERENCEMASK;
        }

        uint8_t GetReference(int32_t index) const
        {
            return (m_aFlags[index] & POOL_FLAG_REFERENCEMASK);
        }

        uint8_t GetFlags(int32_t index) const
        {
            return (m_aFlags[index]);
        }

    public:
        void SetIsFree(int32_t index, bool bIsFree)
        {
            bIsFree ? (m_aFlags[index] |= POOL_FLAG_ISFREE) : (m_aFlags[index] &= ~POOL_FLAG_ISFREE);
        }

        void SetId(int i, int id)
        {
            m_aFlags[i] = (m_aFlags[i] & POOL_FLAG_ISFREE) | (id & POOL_FLAG_REFERENCEMASK);
        }

        void SetReference(int32_t index, uint8_t nReference)
        {
            m_aFlags[index] = (m_aFlags[index] & ~POOL_FLAG_REFERENCEMASK) | (((nReference & POOL_FLAG_REFERENCEMASK) > 1 ? (nReference & POOL_FLAG_REFERENCEMASK) : 1));
        }

        void MakeEmpty()
        {
            for (int32_t i = 0; i < m_nSize; i++)
            {
                SetIsFree(i, true);
                SetReference(i, 0);
            }
        }

        void Reset()
        {
            m_nFirstFreeIndex = 0;
            MakeEmpty();
        }

        void* GetAt(int32_t index)
        {
            return m_aFlags[index >> 8] == (index & 0xFF) ? &m_aStorage[(index >> 8) * m_nStorageSize] : nullptr;
        }

        int32_t GetJustIndex_NoFreeAssert(const void* entry)
        {
            int index = (((uint8_t*)entry - m_aStorage) / m_nStorageSize);
            return index;
        }

        int32_t GetIndex(const void* entry)
        {
            int i = GetJustIndex_NoFreeAssert(entry);
            return m_aFlags[i] + (i << 8);
        }

        int32_t GetJustIndex(const void* entry)
        {
            int index = GetJustIndex_NoFreeAssert(entry);
            return index;
        }

        void* GetSlot(int32_t index)
        {
            return GetIsFree(index) ? nullptr : &m_aStorage[index * m_nStorageSize];
        }

        void* New()
        {
            int32_t freeIndex = m_nFirstFreeIndex;
            int32_t* nextFree = (int32_t*)(m_aStorage + freeIndex * m_nStorageSize);
            m_nFirstFreeIndex = *nextFree;

            void* result = (void*)nextFree;
            SetIsFree(freeIndex, false);
            SetReference(freeIndex, GetReference(freeIndex) + 1);
            return result;
        }

        void* New(int32_t index)
        {
            const uint32_t i = (index >> 8);

            SetIsFree(i, false);
            SetReference(i, index & 255);

            uint8_t* pT = &m_aStorage[i * m_nStorageSize];
            int32_t* prev = &m_nFirstFreeIndex;
            while (*prev != -1 && *prev != i)
                prev = (int32_t*)(m_aStorage + *prev * m_nStorageSize);

            *prev = *(int32_t*)(m_aStorage + i * m_nStorageSize);

            m_numSlotsUsed++;
            return pT;
        }

        void Delete(void* pT)
        {
            int32_t index = GetJustIndex(pT);
            int32_t* node = (int32_t*)(m_aStorage + index * m_nStorageSize);
            *node = -1;

            if (m_nFirstFreeIndex == -1)
                m_nFirstFreeIndex = index;

            SetIsFree(index, true);

            m_numSlotsUsed--;
        }
    };

    template<class T>
    class fwPool : public fwBasePool
    {
    public:
        T* New()
        {
            return static_cast<T*>(fwBasePool::New());
        }

        T* New(int32_t index)
        {
            return static_cast<T*>(fwBasePool::New(index));
        }

        T* GetAt(int32_t index)
        {
            return static_cast<T*>(fwBasePool::GetAt(index));
        }

        T* GetSlot(int32_t index)
        {
            return static_cast<T*>(fwBasePool::GetSlot(index));
        }

        const T* GetSlot(int32_t index) const
        {
            return static_cast<const T*>(fwBasePool::GetSlot(index));
        }
    };
}

export template <typename T>
using CPool = rage::fwPool<T>;

CPool<void*>** pObjectsPool;
CPool<void*>** pTaskPool;
CPool<void*>** pPedPool;
CPool<void*>** pCamPool;
CPool<void*>** pVehiclePool;

namespace CObject
{
    export CPool<void*>* GetObjectsPool()
    {
        return *pObjectsPool;
    }
}

namespace CTask
{
    export CPool<void*>* GetTaskPool()
    {
        return *pTaskPool;
    }
}

namespace CPed
{
    export CPool<void*>* GetPedPool()
    {
        return *pPedPool;
    }

    export template <typename... Args, typename = std::enable_if_t<(std::is_convertible_v<Args, TaskID> && ...)>>
    bool ComparePedTasks(uintptr_t pPed, Args... args)
    {
        auto condition = [](TaskID val, TaskID param) -> bool {
            if (std::to_underlying(param) >= 0)
                return std::to_underlying(val) == std::to_underlying(param);
            else
                return std::to_underlying(val) != std::abs(std::to_underlying(param));
        };

        for (uintptr_t* node = *(uintptr_t**)(*(uintptr_t*)(pPed + 0x224) + 0x2E0); node != nullptr; node = reinterpret_cast<uintptr_t*>(node[3]))
        {
            TaskID value = static_cast<TaskID>(node[1]);

            if constexpr (sizeof...(args) == 0)
            {
                return false;
            }
            else
            {
                bool satisfies = true;
                ((satisfies = satisfies && condition(value, args)), ...);
                if (satisfies)
                {
                    return true;
                }
            }
        }
        return false;
    }
}

namespace CCam
{
    export CPool<void*>* GetCamPool()
    {
        return *pCamPool;
    }
}

namespace CVehicle
{
    export CPool<void*>* GetVehiclePool()
    {
        return *pVehiclePool;
    }
}

export namespace rage
{
    class Vector3
    {
    public:
        float x, y, z;

    public:
        Vector3() = default;
        Vector3(float x, float y, float z) : x(x), y(y), z(z)
        {
        }

        Vector3 operator+(const Vector3& other) const
        {
            return Vector3(x + other.x, y + other.y, z + other.z);
        }

        Vector3 operator-(const Vector3& other) const
        {
            return Vector3(x - other.x, y - other.y, z - other.z);
        }

        Vector3 operator*(float scalar) const
        {
            return Vector3(x * scalar, y * scalar, z * scalar);
        }

        Vector3 operator/(float scalar) const
        {
            if (scalar != 0.0f)
                return Vector3(x / scalar, y / scalar, z / scalar);
            else
                throw std::runtime_error("Division by zero");
        }

        Vector3& operator=(const Vector3& other)
        {
            if (this != &other)
            {
                x = other.x;
                y = other.y;
                z = other.z;
            }
            return *this;
        }

        Vector3& operator+=(const Vector3& other)
        {
            x += other.x;
            y += other.y;
            z += other.z;
            return *this;
        }

        Vector3& operator+=(float scalar)
        {
            x += scalar;
            y += scalar;
            z += scalar;
            return *this;
        }

        Vector3& operator-=(const Vector3& other)
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            return *this;
        }

        Vector3& operator*=(const Vector3& other)
        {
            x *= other.x;
            y *= other.y;
            z *= other.z;
            return *this;
        }

        bool operator==(const Vector3& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator!=(const Vector3& other) const
        {
            return !(*this == other);
        }

        float Heading() const
        {
            return atan2f(-x, y);
        }

        float Magnitude()
        {
            return sqrt(x * x + y * y + z * z);
        }

        void Translate(float x, float y, float z)
        {
            this->x += x;
            this->y += y;
            this->z += z;
        }

        void Normalize()
        {
            float mag = Magnitude();
            if (mag > 0.0f)
            {
                x /= mag;
                y /= mag;
                z /= mag;
            }
            else
            {
                x = 0.0f;
                y = 0.0f;
                z = 0.0f;
            }
        }
    };

    class Vector4
    {
    public:
        float x, y, z, w;

    public:
        Vector4() = default;
        Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w)
        {
        }
        Vector4(const Vector3& other) : x(other.x), y(other.y), z(other.z), w(0.0f)
        {
        }

        operator Vector3() const
        {
            return Vector3(x, y, z);
        }

        Vector4 operator+(const Vector4& other) const
        {
            return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        Vector4 operator-(const Vector4& other) const
        {
            return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        Vector4 operator*(float scalar) const
        {
            return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        Vector4 operator+(float scalar) const
        {
            return Vector4(x + scalar, y + scalar, z + scalar, w + scalar);
        }

        Vector4 operator-(float scalar) const
        {
            return Vector4(x - scalar, y - scalar, z - scalar, w - scalar);
        }

        friend Vector4 operator*(float scalar, const Vector4& vec)
        {
            Vector4 result;

            result.x = vec.x * scalar;
            result.y = vec.y * scalar;
            result.z = vec.z * scalar;
            result.w = vec.w * scalar;

            return result;
        }

        Vector4 operator/(float scalar) const
        {
            if (scalar != 0.0f)
                return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
            else
                throw std::runtime_error("Division by zero");
        }

        Vector4& operator=(const Vector4& other)
        {
            if (this != &other)
            {
                x = other.x;
                y = other.y;
                z = other.z;
                w = other.w;
            }
            return *this;
        }

        Vector4& operator+=(const Vector4& other)
        {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }

        Vector4& operator-=(const Vector4& other)
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }

        bool operator==(const Vector4& other) const
        {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }

        bool operator!=(const Vector4& other) const
        {
            return !(*this == other);
        }

        float Heading() const
        {
            return atan2f(-x, y);
        }

        float Magnitude()
        {
            return sqrt(x * x + y * y + z * z + w * w);
        }

        void Translate(float x, float y, float z, float w)
        {
            this->x += x;
            this->y += y;
            this->z += z;
            this->w += w;
        }

        void Translate(float x, float y)
        {
            this->x += x;
            this->y += y;
            this->z += x;
            this->w += y;
        }
    };

    Vector3 operator+(const Vector3& v3, const Vector4& v4)
    {
        return Vector3(v3.x + v4.x, v3.y + v4.y, v3.z + v4.z);
    }

    Vector3 operator+(const Vector4& v4, const Vector3& v3)
    {
        return Vector3(v4.x + v3.x, v4.y + v3.y, v4.z + v3.z);
    }

    class Matrix44
    {
    public:
        Vector4 right;
        Vector4 up;
        Vector4 at;
        Vector4 pos;
    };

    enum grcTextureFormat : uint8_t
    {
        GRCFMT_UNKNOWN,
        GRCFMT_R5G6B5,
        GRCFMT_A8R8G8B8,
        GRCFMT_R16F,
        GRCFMT_R32F,
        GRCFMT_A2R10G10B10,
        GRCFMT_A16B16G16R16F,
        GRCFMT_G16R16,
        GRCFMT_G16R16F,
        GRCFMT_A32B32G32R32F,
        GRCFMT_A16B16G16R16F2,
        GRCFMT_A16B16G16R16,
        GRCFMT_L8,
        GRCFMT_D24S8 = 0xE,
        GRCFMT_X8R8G8B8 = 0x10,
    };

    inline std::vector<std::pair<grcTextureFormat, D3DFORMAT>> m_Formats
    {
        { GRCFMT_UNKNOWN,        D3DFMT_UNKNOWN },
        { GRCFMT_R5G6B5,         D3DFMT_R5G6B5 },
        { GRCFMT_A8R8G8B8,       D3DFMT_A8R8G8B8 },
        { GRCFMT_R16F,           D3DFMT_R16F },
        { GRCFMT_R32F,           D3DFMT_R32F },
        { GRCFMT_A2R10G10B10,    D3DFMT_A2R10G10B10 },
        { GRCFMT_A16B16G16R16F,  D3DFMT_A16B16G16R16F },
        { GRCFMT_G16R16,         D3DFMT_G16R16 },
        { GRCFMT_G16R16F,        D3DFMT_G16R16F },
        { GRCFMT_A32B32G32R32F,  D3DFMT_A32B32G32R32F },
        { GRCFMT_A16B16G16R16F2, D3DFMT_A16B16G16R16F },
        { GRCFMT_A16B16G16R16,   D3DFMT_A16B16G16R16 },
        { GRCFMT_L8,             D3DFMT_L8 },
        { GRCFMT_D24S8,          D3DFMT_D24S8 },
        { GRCFMT_X8R8G8B8,       D3DFMT_X8R8G8B8 },
    };

    inline grcTextureFormat getEngineTextureFormat(D3DFORMAT format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.second == format)
                return pair.first;
        }
        return GRCFMT_UNKNOWN;
    }

    inline D3DFORMAT getD3DTextureFormat(grcTextureFormat format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.first == format)
                return pair.second;
        }
        return D3DFMT_UNKNOWN;
    }

    struct grcViewportWindow
    {
        float X;
        float Y;
        float Width;
        float Height;
        float MinZ;
        float MaxZ;
    };

    class grcViewport
    {
    public:
        float mWorldMatrix44[4][4];
        float mCameraMatrix[4][4];
        float mWorldMatrix[4][4];
        float mWorldViewMatrix[4][4];
        float mWorldViewProjMatrix[4][4];
        float mViewInverseMatrix[4][4];
        float mViewMatrix[4][4];
        float mProjectionMatrix[4][4];
        float mMatrix_200[4][4];//FrustumLRTB?
        float mMatrix_240[4][4];//FrustumNFNF?
        grcViewportWindow mGrcViewportWindow1;
        grcViewportWindow mGrcViewportWindow2;//UnclippedWindow?
        int mWidth;
        int mHeight;
        float mFov;
        float mAspect;
        float mNearClip;
        float mFarClip;
        float field_2C8;
        float field_2CC;
        float mScaleX;
        float mScaleY;
        float field_2D8;
        float field_2DC;
        Vector4 field_2E0;
        bool mIsPerspective;
        char gap_2f1[3];
        int field_2F4;
        int field_2F8;
        int field_2FC;
        Vector4 mFrustumClipPlanes[6];
        int field_360;
        int field_364;
        int field_368;
        int field_36C;
        int field_370;
        int field_374;
        int field_378;
        int field_37C;
        int field_380;
        int field_384;
        int field_388;
        int field_38C;
        int field_390;
        int field_394;
        int field_398;
        int field_39C;
        int field_3A0;
        int field_3A4;
        int field_3A8;
        int field_3AC;
        int field_3B0;
        int field_3B4;
        int field_3B8;
        int field_3BC;
        int field_3C0;
        int field_3C4;
        int field_3C8;
        int field_3CC;
        int field_3D0;
        int field_3D4;
        int field_3D8;
        int field_3DC;
        bool mInvertZInProjectionMatrix;
        char field_3E1[3];
        int field_3E4;
        int field_3E8;
        int field_3EC;
    };

    grcViewport** pCurrentViewport = nullptr;
    grcViewport* GetCurrentViewport()
    {
        return *pCurrentViewport;
    }

    VALIDATE_SIZE(grcViewport, 0x3F0);

    struct grcImage
    {
        uint16_t mWidth;
        uint16_t mHeight;
        int32_t mFormat;
        int32_t mType;
        uint16_t mStride;
        uint16_t mDepth;
        void* mPixelData;
        int32_t* field_14;
        grcImage* mNextLevel;
        grcImage* mNextSlice;
        uint32_t mRefCount;
        uint8_t gap24[8];
        int field_2C;
        float field_30[3];
        int32_t field_3C;
        float field_40[3];
        int field_4C;
    };

    VALIDATE_SIZE(grcImage, 0x50);

    class datBase
    {
    protected:
        uint32_t* _vft;
    };

    class pgBase : public datBase
    {
        uint32_t mBlockMap;
    };

    VALIDATE_SIZE(pgBase, 0x8);

    class grcRenderTargetPC;
    struct grcRenderTargetDesc
    {
        grcRenderTargetDesc()
            : field_0(0)
            , mMultisampleCount(0)
            , field_8(1)
            , mLevels(1)
            , field_10(1)
            , field_11(1)
            , field_1C(1)
            , field_24(1)
            , field_26(1)
            , field_28(1)
            , field_12(0)
            , mDepthRT(0)
            , field_18(0)
            , field_20(0)
            , field_25(0)
            , field_27(0)
            , field_29(0)
            , field_2A(0)
            , mFormat(GRCFMT_UNKNOWN)
        {}

        char field_0;
        int mMultisampleCount;
        char field_8;
        int mLevels;
        char field_10;
        char field_11;
        char field_12;
        grcRenderTargetPC* mDepthRT;
        char field_18;
        int field_1C;
        int field_20;
        bool field_24;
        char field_25;
        char field_26;
        char field_27;
        char field_28;
        char field_29;
        char field_2A;
        alignas(4) grcTextureFormat mFormat;
    };

    class grcRenderTarget : public pgBase
    {
    public:
        uint8_t field_8;
        uint8_t field_9;
        int16_t field_A;
        int32_t field_C;
        int32_t field_10;
        uint32_t mType;
        int32_t* field_18;
        int16_t field_1C;
        int16_t field_1E;
        int32_t field_20;
        int16_t field_24;
        int16_t field_26;
        int32_t field_28;
        uint16_t field_2C;
        uint16_t field_2E;
    };

    VALIDATE_SIZE(grcRenderTargetDesc, 0x30);

    class grcRenderTargetPC : public  grcRenderTarget
    {
    public:
        char* mName;
        IDirect3DTexture9* mD3DTexture;
        IDirect3DSurface9* mD3DSurface;
        uint16_t mWidth;
        uint16_t mHeight;
        grcTextureFormat mFormat;
        uint8_t mIndex;
        uint8_t mBitsPerPixel;
        uint8_t mMultisampleCount;
        bool field_44;
        uint8_t gap45[2];
        uint8_t field_47;
        uint32_t mLevels;
        bool field_4C;
        bool field_4D;
        uint8_t field_4E;

        void Destroy(uint8_t a2 = 1)
        {
            auto func = (void(__thiscall*)(grcRenderTargetPC*, uint8_t))(_vft[0]);
            func(this, a2);
        }
    };

    VALIDATE_SIZE(grcRenderTargetPC, 0x50);

    class grcTexturePC : pgBase
    {
    public:
        uint8_t field_8;
        uint8_t mDepth;
        uint16_t mRefCount;
        int32_t field_C;
        int32_t field_10;
        char* mName;
        IDirect3DTexture9* mD3DTexture;
        uint16_t mWidth;
        uint16_t mHeight;
        int32_t mFormat;
        uint16_t mStride;
        uint8_t mTextureType;
        uint8_t mMipCount;
        D3DVECTOR field_28;
        D3DVECTOR field_34;
        grcTexturePC* mPrevious;
        grcTexturePC* mNext;
        void* mPixelData;
        uint8_t field_4C;
        uint8_t field_4D;
        uint8_t field_4E;
        uint8_t field_4F;

        bool Init()
        {
            auto func = (grcTexturePC * (__thiscall*)(grcTexturePC*))(_vft[4]);
            return func(this);
        }
    };

    VALIDATE_SIZE(grcTexturePC, 0x50);

    struct grcTextureReferenceBase : public pgBase
    {
        char field_8;
        char field_9;
        int16_t m_wUsageCount;
        int field_C;
    };

    struct grcTextureReference : public grcTextureReferenceBase
    {
        int field_10;
        const char* m_pszName;
        grcTexturePC* m_pTexture;
    };

    namespace grcDevice
    {
        struct grcResolveFlags
        {
            grcResolveFlags()
                : Depth(1.0f)
                , BlurKernelSize(1.0f)
                , Color(0)
                , Stencil(0)
                , ColorExpBias(0)
                , ClearColor(false)
                , ClearDepthStencil(false)
                , BlurResult(false)
                , NeedResolve(true)
                , MipMap(true)
            {}

            float Depth;
            float BlurKernelSize;
            uint32_t Color;
            uint32_t Stencil;
            int ColorExpBias;
            bool ClearColor;
            bool ClearDepthStencil;
            bool BlurResult;
            bool NeedResolve;
            bool MipMap;
        };

        IDirect3DDevice9** ms_pD3DDevice = nullptr;
        int32_t* ms_nActiveWidth = nullptr;
        int32_t* ms_nActiveHeight = nullptr;
        bool* ms_bNoBlockOnLostFocus = nullptr;

        IDirect3DDevice9* GetD3DDevice()
        {
            return *ms_pD3DDevice;
        }

        void* SetCallbackAddr;
        class FunctorBase
        {
        public:
            FunctorBase()
            {
                memset(mMemFunc, 0xAA, 8);
                mCallee = 0;
            }

            FunctorBase(void* callee, void(__fastcall* function)(), void* mf, uint32_t size)
            {
                auto SetCallback = (void(__thiscall*)(FunctorBase*, void*, void(__fastcall*)(), void*, uint32_t))SetCallbackAddr;
                SetCallback(this, callee, function, mf, size);
            }

            union
            {
                void(__fastcall* mFunction)();
                uint8_t mMemFunc[8];
            };

            uint32_t mCallee;
        };

        class Functor0 : public FunctorBase
        {
        public:
            Functor0(void* callee, void(__fastcall* function)(), void* mf, uint32_t size)
                : FunctorBase(callee, function, mf, size)
            {
                mThunk = Translator;
            }

        private:
            void(__cdecl* mThunk)(FunctorBase*);

            static void Translator(FunctorBase* functor)
            {
                functor->mFunction();
            }
        };

        VALIDATE_SIZE(Functor0, 0x10);

        void(__cdecl* RegisterDeviceCallbacks)(Functor0 onLost, Functor0 onReset);
    }

    class grcTextureFactoryPC;
    class grcTextureFactory
    {
    protected:
        uint32_t* _vft;

    public:
        bool field_4;
        char _gap5[3];

        static inline grcTextureFactoryPC** g_pTextureFactory = nullptr;
        static grcTextureFactoryPC* GetInstance()
        {
            return *g_pTextureFactory;
        }
    };

    class grcTextureFactoryPC : public grcTextureFactory
    {
    public:
        IDirect3DSurface9* mPrevRenderTargets[3];
        int32_t field_14;
        int32_t field_18;
        int32_t field_1C;
        int32_t field_20;
        int32_t field_24;
        IDirect3DSurface9* mD3D9Surfaces[4];
        uint8_t gap38[4];
        int32_t field_3C;
        int32_t field_40;
        int32_t field_44;
        IDirect3DSurface9* field_48;
        IDirect3DSurface9* mDepthStencilSurface;
        IDirect3DSurface9* field_50;
        grcRenderTargetPC* field_54;
        int32_t field_58;
        int32_t field_5C;
        int32_t field_60;
        int32_t field_64;
        int32_t field_68;
        int32_t field_6C;
        int32_t field_70;

        //virtuals
        grcTexturePC* Create(grcImage* image, void* arg2)
        {
            auto func = (grcTexturePC * (__stdcall*)(grcImage*, void*))(_vft[2]);
            return func(image, arg2);
        }

        grcRenderTargetPC* __stdcall CreateRenderTarget(const char* name, int32_t a2, uint32_t width, uint32_t height, uint32_t bitsPerPixel, grcRenderTargetDesc* desc)
        {
            auto func = (grcRenderTargetPC * (__stdcall*)(const char*, int32_t, uint32_t, uint32_t, uint32_t, grcRenderTargetDesc*))(_vft[14]);
            return func(name, a2, width, height, bitsPerPixel, desc);
        }

        void LockRenderTarget(uint32_t index, grcRenderTargetPC* color, grcRenderTargetPC* depth, uint32_t a5 = 0, bool a6 = 1, uint32_t mip = 0)
        {
            auto func = (void(__thiscall*)(grcTextureFactory*, uint32_t, grcRenderTargetPC*, grcRenderTargetPC*, uint32_t, bool, uint32_t))(_vft[15]);
            func(this, index, color, depth, a5, a6, mip);
        }

        void UnlockRenderTarget(uint32_t index, grcDevice::grcResolveFlags* resolveFlags, int32_t unused = -1)
        {
            auto func = (void(__thiscall*)(grcTextureFactory*, uint32_t, grcDevice::grcResolveFlags*, int32_t))(_vft[16]);
            func(this, index, resolveFlags, unused);
        }

        static inline std::unordered_map<std::string, grcTexturePC*> TextureCache;
        static inline std::unordered_map<std::string, grcRenderTargetPC*> RTCache;
        static inline SafetyHookInline shCreateTexture{};
        static grcTexturePC* __fastcall CreateTexture(grcTextureFactoryPC* _this, void* edx, const char* name, void* a3)
        {
            auto ret = shCreateTexture.fastcall<grcTexturePC*>(_this, edx, name, a3);
            TextureCache[name] = ret;
            return ret;
        }

        static inline SafetyHookInline shCreateRT{};
        static grcRenderTargetPC* __stdcall CreateRT(const char* name, int32_t a2, uint32_t width, uint32_t height, uint32_t bitsPerPixel, grcRenderTargetDesc* desc)
        {
            // Scale phone screen/phone camera rendertarget resolution with game resolution.
            if (std::string_view(name) == "PHONE_SCREEN" || std::string_view(name) == "PHOTO" || std::string_view(name) == "FullScreenCopy2")
            {
                auto res = (int32_t)(std::ceil((float)*rage::grcDevice::ms_nActiveHeight / 720.0f) * 256.0f);
                width  = res;
                height = res;
            }
            else if (std::string_view(name) == "WATER_SURFACE0_COLOUR" || std::string_view(name) == "WATER_SURFACE1_COLOUR")
            {
                // Force water surface rendertarget resolution to always be 256x256. This matches the water tiling on the console version.
                width  = 256;
                height = 256;
            }
            else if (std::string_view(name) == "MIRROR_RT" || std::string_view(name) == "MIRROR_DT")
            {
                // Adjust mirror rendertarget dimensions (PS3 dimensions)
                height /= 2;
            }
            else if (std::string_view(name) == "WATER_REFLECTION_COLOUR" || std::string_view(name) == "WATER_REFLECTION_DEPTH")
            {
                // Adjust water reflection rendertarget dimensions (PS3 dimensions)
                width = (width * 5) / 4;
                height /= 2;
            }

            auto ret = shCreateRT.stdcall<grcRenderTargetPC*>(name, a2, width, height, bitsPerPixel, desc);

            RTCache[name] = ret;
            return ret;
        }

        static grcTexturePC* GetTextureByName(const char* name)
        {
            if (TextureCache.contains(name))
                return TextureCache[name];
            return nullptr;
        }

        static grcRenderTargetPC* GetRTByName(const char* name)
        {
            if (RTCache.contains(name))
                return RTCache[name];
            return nullptr;
        }

        static grcTexturePC* FindTextureByRawPointer(IDirect3DTexture9* ptr)
        {
            for (auto& [name, tex] : TextureCache)
            {
                if (tex->mD3DTexture == ptr)
                    return tex;
            }
            return nullptr;
        }

        static grcRenderTargetPC* FindRTByRawPointer(IDirect3DTexture9* ptr)
        {
            for (auto& [name, rt] : RTCache)
            {
                if (rt->mD3DTexture == ptr)
                    return rt;
            }
            return nullptr;
        }
    };

    VALIDATE_SIZE(grcTextureFactoryPC, 0x74);

    enum eLightType
    {
        LT_POINT   = 0x0,
        LT_DIR     = 0x1,
        LT_SPOT    = 0x2,
        LT_AMBOCC  = 0x3,
        LT_CLAMPED = 0x4,
    };

    class CLightSource
    {
    public:
        Vector3 mDirection;
        float field_C;
        Vector3 mTangent;
        float field_1C;
        Vector3 mPosition;
        float field_2C;
        Vector4 mColor;
        float mIntensity;
        eLightType mType;
        uint32_t mFlags;
        int32_t mCoronaTexHash;
        int32_t mProjTexHash;
        float mRadius;
        float mInnerConeAngle;
        float mOuterConeAngle;
        int32_t mCastShadows;
        int32_t mShadowCacheIndex;
        int32_t mInteriorIndex;
        int32_t mRoomIndex;
        float mVolumeIntensity;
        float mVolumeScale;
        int8_t field_78[7];
        char field_7F;
    };

    VALIDATE_SIZE(CLightSource, 0x80);

    namespace scrEngine
    {
        uint32_t* ms_dwNativeTableSize;
        uint32_t** ms_pNatives;
    }

    namespace scrProgram
    {
        uintptr_t** ms_pGlobals;
        uint32_t* ms_pGlobalsSize;
    }

    namespace grcWindow
    {
        int32_t* ms_nActiveWidth;
        int32_t* ms_nActiveHeight;
        int32_t* ms_nActiveRefreshrate;

        bool* ms_bWindowed;
        bool* ms_bOnTop;
        bool* ms_bFocusLost;
    }

    struct grmShaderInfo_Parameter
    {
        char nbType;
        char nbCount;
        char nbValueLength;
        char nbAnnotationsCount;
        const char* pszName;
        const char* pszDescription;
        int dwNameHash;
        int dwDescriptionHash;
        int pAnnotations;
        void* pValue;
        int16_t m_wVertexFragmentRegister;
        int16_t m_wPixelFragmentRegister;
        int pdwParameterHashes;
        int field_24;
        int field_28;
        int field_2C;
    };

    VALIDATE_SIZE(grmShaderInfo_Parameter, 0x30);

    #pragma pack(push, 1)
    struct sysArray
    {
        int pData;
        int16_t wCount;
        int16_t wSize;
    };
    #pragma pack(pop)

    struct grmShaderInfo_Values
    {
        int ppValues;
        int pOwner;
        int dwSize;
        int dwTotalDataSize;
        int pnbArraySizes;
        int dwEffectHash;
        int field_18;
        int field_1C;
        int field_20;
    };

    class grmShaderInfo
    {
        sysArray m_techniques;
        sysArray m_parameters;
        sysArray m_vsFragments;
        sysArray m_psFragments;
        const char* m_pszShaderPath;
        int m_dwEffectHash;
        int m_dwFileTimeLow;
        int m_dwFileTimeHigh;
        grmShaderInfo_Values m_values;
        int m_pNext;

    public:
        static inline std::vector<std::pair<std::string, std::pair<std::string, int>>> ShaderParamNames;
        static inline std::vector<std::array<float, 4>> ShaderParamData;

        static inline size_t registerShaderParam(const char* shader, const char* param)
        {
            ShaderParamNames.emplace_back(shader, std::make_pair(param, -1));
            ShaderParamData.emplace_back(); // zero-initialized
            return ShaderParamNames.size() - 1;
        }

        static inline void setShaderParamData(size_t idx, const void* src, size_t size)
        {
            std::memcpy(ShaderParamData[idx].data(), src, std::min(size, sizeof(decltype(ShaderParamData)::value_type)));
        }

        static inline decltype(ShaderParamData)::value_type& getShaderParamData(size_t idx)
        {
            return ShaderParamData[idx];
        }

        static inline int(__cdecl* getGlobalParameterIndexByName)(const char* a1) = nullptr;
        static inline int(__fastcall* getParamIndex)(grmShaderInfo* instance, void* edx, const char* name, int a3) = nullptr;

        static inline SafetyHookInline shsub_436D70{};
        static void __fastcall setShaderParam(grmShaderInfo* _this, void* edx, void* a2, int index, void* pDataArr, int nArrSize, int a6, int a7)
        {
            if (_this->m_parameters.wCount)
            {
                auto sv = std::string_view(_this->m_pszShaderPath);
                auto last_slash = sv.find_last_of('/');
                auto shader_name = last_slash != std::string_view::npos ? sv.substr(last_slash + 1) : sv;

                auto it = std::find_if(ShaderParamNames.begin(), ShaderParamNames.end(), [&](auto& pair)
                {
                    int& cachedIdx = pair.second.second;
                    if (cachedIdx <= 0) cachedIdx = getParamIndex(_this, edx, pair.second.first.c_str(), 1);
                    return index == cachedIdx && shader_name == pair.first;
                });

                if (it != ShaderParamNames.end())
                {
                    size_t idx = std::distance(ShaderParamNames.begin(), it);
                    setShaderParamData(idx, pDataArr, nArrSize);
                }
            }
            shsub_436D70.unsafe_fastcall(_this, edx, a2, index, pDataArr, nArrSize, a6, a7);
        }

        static void __fastcall setShaderParamOriginal(grmShaderInfo* _this, void* edx, void* a2, int index, void* pDataArr, int nArrSize, int a6, int a7)
        {
            shsub_436D70.unsafe_fastcall(_this, edx, a2, index, pDataArr, nArrSize, a6, a7);
        }
    };
}

export namespace rage
{
    enum scrThreadId : int32_t
    {
        THREAD_INVALID = 0x0,
    };

    enum scrProgramId : uint32_t
    {
        srcpidNONE = 0x0,
    };

    union scrValue
    {
        int Int;
        float Float;
        bool Bool;
        void* Reference;
        const char* String;
    };

    class scrThread
    {
    public:
        enum State
        {
            RUNNING,
            BLOCKED,
            ABORTED,
            HALTED
        };

        struct Serialized
        {
            scrThreadId m_ThreadId;
            scrProgramId m_Prog;
            State m_State;
            int m_PC;
            int m_FP;
            int m_SP;
            float m_TimerA;
            float m_TimerB;
            float m_Wait;
            int m_MinPC;
            int m_MaxPC;
            uint8_t pad[42];
        };

        struct Info
        {
            scrValue* ResultPtr;
            int32_t ParamCount;
            scrValue* Params;
            int32_t BufferCount;
            Vector3* Orig[4];
            Vector4 Buffer[4];

            Info() = default;
            Info(scrValue* resultPtr, int32_t parameterCount, scrValue* params) :
                ResultPtr(resultPtr), ParamCount(parameterCount), Params(params),
                BufferCount(0)
            {
            }
        };

        struct InfoWithBuf : Info
        {
            uint8_t Buf[64] = {};

            inline InfoWithBuf()
            {
                Params = (rage::scrValue*)&Buf;
                ResultPtr = (rage::scrValue*)&Buf;
                ParamCount = 0;
                BufferCount = 0;
            }

            template <typename T>
            void Fill(T value)
            {
                *reinterpret_cast<T*>(Buf + 4 * ParamCount) = value;
                ParamCount++;
            }
        };

        int32_t vt;
        Serialized m_Serialized;
        int32_t* m_Stack;
        int32_t* m_iInstructionCount;
        int32_t* m_argStructSize;
        int32_t* m_argStructOffset;
        const char* m_AbortReason;
        char m_szProgramName[24];
        int m_dwSavedDeathArrestIP;
        int m_dwSavedDeatharrestFrameSP;
        int m_dwSavedDeatharrestStackOff;
        char m_bOnMission;
        bool m_bSafeForNetworkGame;
        bool m_bThisScriptShouldBeSaved;
        bool m_bPlayerControlOnInMissionCleanup;
        bool m_bClearHelpInMissionCleanup;
        bool m_bMiniGameScript;
        bool m_bAllowNonMinigameTextMessages;
        bool m_bAllowOneTimeOnlyCommandToRun;
        bool m_bPaused;
        bool m_bCanBePaused;
        bool m_eHandleType;                   //handle type 0: ped, 1: object, 2: world point
        bool field_9F;
        int m_handle;
        bool m_bCanRemoveBlipsCreatedByAnyScript;
        int field_A8;
        int m_dwFlags;                        //0x1 = player may enter only specified vehicle

    public:
        static inline sysArray* sm_Threads;
        static inline scrThread** s_CurrentThread;

    public:
        static inline scrThread* (*GetActiveThread)();
        static inline scrThread* (*GetThread)(scrThreadId id);
        static inline void (*RegisterCommand)(uint32_t hashCode, void (*handler)());

        static inline SafetyHookInline shGetActiveThread{};
        static inline void* GetActiveThreadHook()
        {
            auto ret = shGetActiveThread.unsafe_call<void*>();

            if (!ret)
            {
                static rage::scrThread dummyThread;
                memset(&dummyThread, 0, sizeof(rage::scrThread));
                return static_cast<void*>(&dummyThread);
            }

            return ret;
        }
    };
}

export namespace CViewport3DScene
{
    rage::grcRenderTargetPC** pGBufferRTs = nullptr;
    rage::grcRenderTargetPC** pStencilRT = nullptr;

    rage::grcRenderTargetPC* GetDiffuseRT()
    {
        return pGBufferRTs[0];
    }

    rage::grcRenderTargetPC* GetNormalRT()
    {
        return pGBufferRTs[1];
    }

    rage::grcRenderTargetPC* GetSpecularAoRT()
    {
        return pGBufferRTs[2];
    }

    rage::grcRenderTargetPC* GetDepthRT()
    {
        return pGBufferRTs[3];
    }

    rage::grcRenderTargetPC* GetStencilRT()
    {
        return *pStencilRT;
    }
}

export namespace CMenuManager
{
    bool* m_MenuActive = nullptr;
    uint8_t* bLoadscreenShown = nullptr;
}

export namespace CMenu
{
    int* m_pMenuID = nullptr;
    int(__cdecl* m_pGetSelectedItem)(int menuID) = nullptr;

    int getSelectedItem()
    {
        return CMenu::m_pGetSelectedItem(*m_pMenuID);
    }
}

export namespace CCutscenes
{
    uint32_t* m_dwCutsceneState;

    bool(*hasCutsceneFinished)();
}

export namespace CCamera
{
    bool(*isWidescreenBordersActive)();
}

export namespace CTimer
{
    float* fTimeStep;
    float* fTimeScale1;
    float* fTimeScale2;
    uint8_t* m_UserPause = nullptr;
    uint8_t* m_CodePause = nullptr;
    int32_t* m_snTimeInMilliseconds = nullptr;
}

export namespace CTimeCycle
{
    void(__cdecl* Initialise)() = nullptr;
    void(__cdecl* InitialiseModifiers)() = nullptr;
}

export namespace CWeather
{
    enum eWeatherType : uint32_t
    {
        EXTRASUNNY,
        SUNNY,
        SUNNY_WINDY,
        CLOUDY,
        RAIN,
        DRIZZLE,
        FOGGY,
        LIGHTNING
    };

    float* Rain = nullptr;
    eWeatherType* OldWeatherType = nullptr;
    eWeatherType* NewWeatherType = nullptr;
    float* InterpolationValue = nullptr;

    eWeatherType GetOldWeatherType()
    {
        return *OldWeatherType;
    }

    eWeatherType GetNewWeatherType()
    {
        return *NewWeatherType;
    }

    float GetWeatherInterpolationValue()
    {
        return *InterpolationValue;
    }
}

export namespace CClock
{
    uint8_t* ms_nGameClockHours;
    uint8_t* ms_nGameClockMinutes;
    uint16_t* ms_nGameClockSeconds;

    uint8_t GetHours() { return *ms_nGameClockHours; }
    uint8_t GetMinutes() { return *ms_nGameClockMinutes; }
    int16_t GetSeconds() { return *ms_nGameClockSeconds; }
}

export namespace CPlayer
{
    uintptr_t (*getLocalPlayerPed)() = nullptr;
    uintptr_t (*findPlayerCar)() = nullptr;
}

export namespace CWeaponData
{
    uintptr_t (__fastcall* getWeaponData)(uintptr_t weaponData, int edx) = nullptr;
}

export namespace CWeapon
{
    uintptr_t (__cdecl* getWeaponByType)(int weaponID) = nullptr;
}

export namespace RageDirect3DDevice9
{
    enum eTexture : uint32_t
    {
        unk,
        unk2,
        HDRTex,
    };

    IDirect3DDevice9** m_pRealDevice = nullptr;

    IDirect3DTexture9** g_TexturesBySampler = nullptr;

    IDirect3DTexture9* GetTexture(uint32_t index)
    {
        if (index < 272)
            return g_TexturesBySampler[index];
        return nullptr;
    }
}

export class CRenderPhaseDeferredLighting_SceneToGBuffer
{
public:
    static FusionFix::Event<>& OnBuildRenderList() {
        static FusionFix::Event<> BuildRenderListEvent;
        return BuildRenderListEvent;
    }

    static inline SafetyHookInline shBuildRenderList{};
    static void __fastcall BuildRenderList(CBaseDC* _this, void* edx)
    {
        OnBuildRenderList().executeAll();
        shBuildRenderList.fastcall<void*>(_this, edx);
    }
};

export class CRenderPhaseDeferredLighting_LightsToScreen
{
public:
    static FusionFix::Event<>& OnBuildRenderList() {
        static FusionFix::Event<> BuildRenderListEvent;
        return BuildRenderListEvent;
    }
    static FusionFix::Event<rage::CLightSource*>& OnAfterCopyLight() {
        static FusionFix::Event<rage::CLightSource*> AfterCopyLightEvent;
        return AfterCopyLightEvent;
    }

    static inline SafetyHookInline shBuildRenderList{};
    static inline SafetyHookInline shCopyLight{};

    static void __fastcall BuildRenderList(CBaseDC* _this, void* edx)
    {
        OnBuildRenderList().executeAll();
        return shBuildRenderList.unsafe_fastcall(_this, edx);
    }

    static rage::CLightSource* __fastcall CopyLight(void* _this, void* edx, void* a2)
    {
        auto ret = shCopyLight.fastcall<rage::CLightSource*>(_this, edx, a2);
        OnAfterCopyLight().executeAll(ret);
        return ret;
    }
};

export class CRenderPhaseDrawScene
{
public:
    static FusionFix::Event<>& OnBuildRenderList() {
        static FusionFix::Event<> BuildRenderListEvent;
        return BuildRenderListEvent;
    }
    //static FusionFix::Event<>& onBeforePostFX() {
    //    static FusionFix::Event<> BeforePostFX;
    //    return BeforePostFX;
    //}
    //static FusionFix::Event<>& onAfterPostFX() {
    //    static FusionFix::Event<> AfterPostFX;
    //    return AfterPostFX;
    //}

    static inline SafetyHookInline shBuildRenderList{};
    static void __fastcall BuildRenderList(CBaseDC* _this, void* edx)
    {
        OnBuildRenderList().executeAll();
        shBuildRenderList.fastcall<void*>(_this, edx);
    }
};

export namespace CTxdStore
{
    rage::grcTexturePC* (__fastcall* getEntryByKey)(int* _this, void* edx, unsigned int a2);
    int* (__cdecl* at)(int);
}

export namespace Matrix34
{
    void (__fastcall* fromEulersXYZ)(float* _this, void* edx, float* a2);
}

export uint8_t(*GTAIV_ENCRYPTION_KEY)[32] = nullptr;

export void* (*FindPlayerPed)(int32_t id);
export void* (*FindPlayerVehicle)(int32_t id);

export int32_t* pMenuTab;
export int32_t* _dwCurrentEpisode;
export void* (__stdcall* getNativeAddress)(uint32_t);
export const char* pszCurrentCutsceneName = nullptr;
export HWND gWnd;
export RECT gRect;
export bool bDynamicShadowsForTrees = true;
export bool bLoadingShown = false;
export int bMenuNeedsUpdate = 0;
export int bMenuNeedsUpdate2 = 0;
export bool bExtraNightShadows = false;
export bool bHeadlightShadows = false;
export bool bVehicleNightShadows = true;
export bool bHighResolutionShadows = false;
export bool bIsQUB3D = false;
export float fMenuBlur = 0.0f;
export bool bInSniperScope = false;
export eCamMode nCurrentCamera = NUM_CAM_MODES;

export auto currentEpisodePath() -> std::filesystem::path
{
    static constexpr auto episodicPaths = std::array{ "", "TLAD", "TBoGT" };
    static const auto moduleParentPath = GetModulePath(GetModuleHandleW(NULL)).parent_path();
    return moduleParentPath / episodicPaths[*_dwCurrentEpisode];
}

export namespace phMaterialGta
{
    enum FxGroup
    {
        _VOID,
        CONCRETE,
        STONE,
        PAVING_SLABS,
        BRICK_COBBLE,
        GRAVEL,
        MUD_SOFT,
        DIRT_DRY,
        SAND,
        SNOW,
        WOOD,
        METAL,
        CERAMICS,
        MARBLE,
        LAMINATE,
        CARPET_FABRIC,
        LINOLEUM,
        RUBBER,
        PLASTIC,
        CARDBOARD,
        PAPER,
        MATTRESS_FOAM,
        PILLOW_FEATHERS,
        GRASS,
        BUSHES,
        TREE_BARK_DARK,
        TREE_BARK_MEDIUM,
        TREE_BARK_LIGHT,
        FLOWERS,
        LEAVES_PILE,
        GLASS,
        WINDSCREEN,
        CAR_METAL,
        CAR_PLASTIC,
        WATER,
        GENERIC,
        PED_HEAD,
        PED_TORSO,
        PED_LIMB,
        PED_FOOT,
        TVSCREEN,
        VIDEOWALL,
        DEFAULT,
    };

    enum HeliFx
    {
        HELIFX_DEFAULT,
        HELIFX_SAND,
        HELIFX_DIRT,
        HELIFX_WATER,
    };

#pragma pack(push, 8)
    struct phMaterial : rage::datBase
    {
        int field_4;
        const char* m_pszName;
    };

    struct phMaterialGta : phMaterial
    {
        float m_fFriction;
        float m_fElasticity;
        float m_fDensity;
        float m_fTyreGrip;
        float m_fWetGrip;
        int16_t m_wFxGroup;
        int16_t m_wHeliFx;
        float m_fFlammability;
        float m_fBurnTime;
        float m_fBurnStrenght;
        int m_nRoughness;
        float m_fPedDensity;
        int m_dwFlags;                        ///< 1 = SeeThru, 2 = ShootThru, 4 = IsWet
    };
#pragma pack(pop)
}

export struct gtaRainEmitter
{
    float posX;
    float posY;
    float posZ;
    int unk1;
    float posRangeX;
    float posRangeY;
    float posRangeZ;
    int unk2;
    float LifeRangeOffsetX;
    float LifeRangeOffsetY;
    int unk3;
    int unk4;
    float velX;
    float velY;
    float velZ;
    int unk5;
    float velRangeX;
    float velRangeY;
    float velRangeZ;
    int posRelative;
    int AddCameraOffset;
    float collisionVelocityModifierX;
    float collisionVelocityModifierY;
    float collisionVelocityModifierZ;
    int unk6;
    int unk7;
    int unk8;
    int unk9;
    float collisionLifeModifierX;
    float collisionLifeModifierY;
    int unk10;
    int unk11;
    float collisionPositionModifierX;
    float collisionPositionModifierY;
    float collisionPositionModifierZ;
    int unk12;
    float gustSpacingX;
    float gustSpacingY;
    int unk13[6];
    float disperseRange;
    float disperseAmt;
    float inertia;
    float probablityPhase2;
};

export struct gtaRainRender
{
    float motionBlurAmt;
    float radius;
    float radiusVariance;
    float radius2;
    float radius2Variance;
    float lifeFadeX;
    float lifeFadeY;
    int useDirectional;
    int unk1;
    int unk2;
    int unk3;
    float tintColorX;
    float tintColorY;
    float tintColorZ;
    float tintColorW;
    float tintColorPhase2X;
    float tintColorPhase2Y;
    float tintColorPhase2Z;
    float tintColorPhase2W;
    float AnimStart;
    float AnimEnd;
    float NumFrames;
};

export namespace ShockingEvents
{
    enum eShockingEvents
    {
        SexyCar,
        RunningPed,
        VisibleWeapon,
        VisibleWeaponMELEE,
        VisibleWeaponTHROWN,
        VisibleWeaponHANDGUN,
        VisibleWeaponSHOTGUN,
        VisibleWeaponSMG,
        VisibleWeaponSNIPER,
        VisibleWeaponRIFLE,
        VisibleWeaponHEAVY,
        HornSounded,
        PlaneFlyby,
        SeenCarStolen,
        HelicopterOverhead,
        SeenMeleeAction,
        SeenGangFight,
        PedRunOver,
        PanickedPed,
        InjuredPed,
        DeadBody,
        DrivingOnPavement,
        MadDriver,
        CarCrash,
        CarPileUp,
        Fire,
        GunshotFired,
        PedShot,
        GunFight,
        Explosion,
    };

    constexpr auto ShockingEventIndexSize = 4;
    constexpr auto ShockingEventDataFullSize = 52;
    constexpr auto ShockingEventSize = ShockingEventDataFullSize - ShockingEventIndexSize;
}

export class ISeasonal
{
public:
   virtual auto Init() -> void = 0;
   virtual auto Enable() -> void = 0;
   virtual auto Disable() -> void = 0;
};

export inline LONG getWindowWidth()
{
    return gRect.right - gRect.left;
}

export inline LONG getWindowHeight()
{
    return gRect.bottom - gRect.top;
}

#pragma pack(push, 1)
struct CImgFile
{
    int m_nTimeLow;
    int m_nTimeHigh;
    int field_8;
    int field_C;
    char pszFilename[128];
    int m_pDevice;
    int field_94;
    int m_hFile;
    char field_9C;
    char field_9D;
    char m_bEpisode;
    char m_bEpisodicContent;
}; VALIDATE_SIZE(CImgFile, 160);
#pragma pack(pop)

export namespace CGameConfigReader
{
    CImgFile(*ms_imgFiles)[255];
}

export int* dwSniperInverted = nullptr;

export namespace UAL
{
    bool (WINAPI* GetOverloadPathW)(wchar_t* out, size_t out_size) = nullptr;
    bool (WINAPI* AddVirtualFileForOverloadW)(const wchar_t* virtualPath, const uint8_t* data, size_t size, int priority) = nullptr;
}

class Common
{
public:
    Common()
    {
        ModuleList dlls;
        dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
        for (auto& e : dlls.m_moduleList)
        {
            auto m = std::get<HMODULE>(e);
            if (IsModuleUAL(m))
            {
                UAL::GetOverloadPathW = (decltype(UAL::GetOverloadPathW))GetProcAddress(m, "GetOverloadPathW");
                UAL::AddVirtualFileForOverloadW = (decltype(UAL::AddVirtualFileForOverloadW))GetProcAddress(m, "AddVirtualFileForOverloadW");
                break;
            }
        }

        auto pattern = find_pattern("56 57 8B F9 8B 07 FF 50 08 25", "56 8B F1 8B 06 8B 50 08 57 FF D2 25");
        CBaseDC::AppendAddr = pattern.get_first(0);

        pattern = find_pattern("53 56 57 8B 7C 24 10 FF 74 24 14", "8B 44 24 08 56 57 8B 7C 24 0C 8B F7");
        CBaseDC::operator_newAddr = pattern.get_first(0);

        _dwCurrentEpisode = *find_pattern("83 3D ? ? ? ? ? 75 0F 6A 02", "89 35 ? ? ? ? 89 35 ? ? ? ? 6A 00 6A 01").get_first<int32_t*>(2);

        pattern = find_pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 75 38", "0A 05 ? ? ? ? 0A 05");
        CTimer::m_UserPause = *pattern.get_first<uint8_t*>(2);
        CTimer::m_CodePause = *pattern.get_first<uint8_t*>(8);

        pattern = find_pattern("A1 ? ? ? ? A3 ? ? ? ? EB 3A", "A1 ? ? ? ? 39 05 ? ? ? ? 76 1F");
        CTimer::m_snTimeInMilliseconds = *pattern.get_first<int32_t*>(1);

        pattern = find_pattern("83 3D ? ? ? ? ? 74 17 8B 4D 14", "83 3D ? ? ? ? ? 74 15 8B 44 24 1C", "83 3D ? ? ? ? ? 74 EF");
        rage::grcDevice::ms_pD3DDevice = *pattern.get_first<IDirect3DDevice9**>(2);

        pattern = find_pattern("89 15 ? ? ? ? 89 0D ? ? ? ? 89 15 ? ? ? ? A3 ? ? ? ? E8", "8B 0D ? ? ? ? 89 4C 24 04 DB 44 24 04 D9 7C 24 04 0F B7 44 24 ? D8 0E");
        rage::grcDevice::ms_nActiveWidth = *pattern.get_first<int32_t*>(2);

        pattern = find_pattern("A3 ? ? ? ? A3 ? ? ? ? A1 ? ? ? ? 85 C9", "A1 ? ? ? ? 85 C0 74 1F F3 0F 2A 05");
        rage::grcDevice::ms_nActiveHeight = *pattern.get_first<int32_t*>(1);

        pattern = hook::pattern("80 3D ? ? ? ? ? 74 4B E8 ? ? ? ? 84 C0");
        CMenuManager::m_MenuActive = *pattern.get_first<bool*>(2);

        rage::scrEngine::ms_dwNativeTableSize = *find_pattern<2>("8B 35 ? ? ? ? 85 F6 75 06 33 C0 5E C2 04 00 53 57 8B 7C 24 10", "8B 3D ? ? ? ? 85 FF 75 04 33 C0 5F C3").count(2).get(0).get<uint32_t*>(2);
        rage::scrEngine::ms_pNatives = *find_pattern<2>("8B 1D ? ? ? ? 8B CF 8B 04 D3 3B C7 74 19 8D 64 24 00 85 C0", "8B 1D ? ? ? ? 8B CE 8B 04 D3 3B C6 74 17 85 C0").count(2).get(0).get<uint32_t**>(2);

        pattern = find_pattern("A3 ? ? ? ? A1 ? ? ? ? 33 DB F7 E7 0F 90 C3 6A 00", "A3 ? ? ? ? A1 ? ? ? ? 33 C9 BA ? ? ? ? F7 E2 0F 90 C1 6A 00 6A 10");
        rage::scrProgram::ms_pGlobals = *pattern.get_first<uintptr_t**>(1);
        rage::scrProgram::ms_pGlobalsSize = *pattern.get_first<uint32_t*>(6);

        pattern = find_pattern("80 3D ? ? ? ? ? 53 56 8A FA", "80 3D ? ? ? ? ? 53 8A 5C 24 1C");
        CMenuManager::bLoadscreenShown = *pattern.get_first<uint8_t*>(2);

        pattern = hook::pattern("56 8B 35 ? ? ? ? 85 F6 75 06");
        if (!pattern.empty())
            getNativeAddress = pattern.count(2).get(1).get<void* (__stdcall)(uint32_t)>(0);

        pattern = find_pattern("8B 0D ? ? ? ? 53 56 33 F6", "53 56 8B 35 ? ? ? ? 33 D2");
        rage::grcDevice::RegisterDeviceCallbacks = pattern.get_first<void(__cdecl)(rage::grcDevice::Functor0, rage::grcDevice::Functor0)>(0);

        pattern = find_pattern("8B 44 24 04 56 8B F1 85 C0 74 38", "8B 44 24 04 85 C0 56 8B F1 74 38");
        rage::grcDevice::SetCallbackAddr = pattern.get_first<void*>(0);

        pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 8B 43 20 53", "F3 0F 10 05 ? ? ? ? F3 0F 59 44 24 ? 83 C4 04 83 7C 24");
        CTimer::fTimeStep = *pattern.get_first<float*>(4);

        pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2F C8 F3 0F 11 44 24", "F3 0F 10 05 ? ? ? ? 0F 2F C8 77 ? F3 0F 10 05");
        CTimer::fTimeScale1 = *pattern.get_first<float*>(4);

        pattern = find_pattern("F3 0F 11 05 ? ? ? ? EB ? F3 0F 10 05 ? ? ? ? 0F 2F C8 F3 0F 11 44 24", "F3 0F 11 05 ? ? ? ? F3 0F 10 05 ? ? ? ? 56 F3 0F 11 44 24 ? E8 ? ? ? ? 8B 0D");
        CTimer::fTimeScale2 = *pattern.get_first<float*>(4);

        pattern = find_pattern("BE ? ? ? ? 8D 44 24 0C 50 8D 46 10 50", "BE ? ? ? ? 8D 44 24 0C 50 8D 4E 10 51");
        CGameConfigReader::ms_imgFiles = *pattern.get_first<decltype(CGameConfigReader::ms_imgFiles)>(1);

        pattern = hook::pattern("A1 ? ? ? ? 83 F8 08 74 05");
        CCutscenes::m_dwCutsceneState = *pattern.get_first<uint32_t*>(1);

        pattern = find_pattern("89 3D ? ? ? ? A3 ? ? ? ? C7 05", "89 1D ? ? ? ? A3 ? ? ? ? 89 35");
        rage::grcWindow::ms_nActiveWidth = *pattern.get_first<int32_t*>(2);
        rage::grcWindow::ms_nActiveHeight = *pattern.get_first<int32_t*>(7);

        pattern = find_pattern("C7 05 ? ? ? ? ? ? ? ? A1 ? ? ? ? 85 C0 74 ? 80 38 ? 74 ? 6A ? 6A ? 50 E8 ? ? ? ? 83 C4 ? A3", "89 35 ? ? ? ? A1 ? ? ? ? 3B C6 74 ? 80 38");
        rage::grcWindow::ms_nActiveRefreshrate = *pattern.get_first<int32_t*>(2);

        pattern = find_pattern("80 3D ? ? ? ? ? 74 0E 68 ? ? ? ? 53", "80 3D ? ? ? ? ? 74 13 68");
        rage::grcWindow::ms_bWindowed = *pattern.get_first<bool*>(2);

        pattern = find_pattern("80 3D ? ? ? ? ? 74 07 BF", "80 3D ? ? ? ? ? 74 07 B8 ? ? ? ? EB 02 33 C0 8B 55 F4");
        rage::grcWindow::ms_bOnTop = *pattern.get_first<bool*>(2);

        pattern = find_pattern("C6 05 ? ? ? ? ? 85 C0 74 02 FF D0 E8", "C6 05 ? ? ? ? ? 74 02 FF D0 C6 05");
        rage::grcWindow::ms_bFocusLost = *pattern.get_first<bool*>(2);

        pattern = find_pattern("80 3D ? ? ? ? ? 74 29 80 3D ? ? ? ? ? 75 1A", "80 3D ? ? ? ? ? 74 2F 80 3D ? ? ? ? ? 75 20");
        rage::grcDevice::ms_bNoBlockOnLostFocus = *pattern.get_first<bool*>(2);

        pattern = find_pattern("8B 35 ? ? ? ? 75 14", "8B 3D ? ? ? ? 75 14 6A 00");
        rage::pCurrentViewport = *pattern.get_first<rage::grcViewport**>(2);

        pattern = find_pattern("A3 ? ? ? ? E8 ? ? ? ? 83 EC 0C", "A3 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 5E");
        rage::grcTextureFactory::g_pTextureFactory = *pattern.get_first<rage::grcTextureFactoryPC**>(1);

        pattern = find_pattern("A3 ? ? ? ? FF 35 ? ? ? ? 8B 01 FF 35 ? ? ? ? 6A 03 68 ? ? ? ? FF 50 38 8B 0D", "A3 ? ? ? ? 8B 01 8B 40 38 6A 03 68 ? ? ? ? FF D0 8B 0D ? ? ? ? A3 ? ? ? ? 8B 11 8B 52 38");
        CViewport3DScene::pGBufferRTs = *pattern.get_first<rage::grcRenderTargetPC**>(1);

        pattern = find_pattern("A3 ? ? ? ? FF 35 ? ? ? ? C7 44 24 ? ? ? ? ? 8B 01", "A3 ? ? ? ? C7 44 24 ? ? ? ? ? 8B 01 8B 40 38 6A 03 68 ? ? ? ? FF D0 80 7C 24");
        CViewport3DScene::pStencilRT = *pattern.get_first<rage::grcRenderTargetPC**>(1);

        pattern = find_pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 8B 0D ? ? ? ? 53 0F B7 41 04", "55 8B EC 83 E4 F0 81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 8B 0D ? ? ? ? 0F B7 41 04");
        CTimeCycle::Initialise = pattern.get_first<void(__cdecl)()>(0);

        pattern = find_pattern("68 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 68");
        CTimeCycle::InitialiseModifiers = pattern.get_first<void(__cdecl)()>(0);

        pattern = find_pattern("56 57 6A 00 FF 74 24 10 8B F9 E8 ? ? ? ? 0F B7 77 0C", "8B 44 24 04 56 57 6A 00 50 8B F9");
        rage::grmShaderInfo::getParamIndex = (decltype(rage::grmShaderInfo::getParamIndex))pattern.get_first(0);

        pattern = find_pattern("56 6A 00 FF 74 24 0C E8 ? ? ? ? 8B 35", "8B 44 24 04 56 6A 00 50 E8 ? ? ? ? 8B 35");
        rage::grmShaderInfo::getGlobalParameterIndexByName = pattern.get_first<int(__cdecl)(const char*)>(0);

        pattern = find_pattern("8B 54 24 08 85 D2 74 62", "56 8B 74 24 0C 85 F6 74 62");
        rage::grmShaderInfo::shsub_436D70 = safetyhook::create_inline(pattern.get_first(0), rage::grmShaderInfo::setShaderParam);

        pattern = find_pattern("F3 0F 11 44 24 ? E8 ? ? ? ? 83 C6 ? 3B F3");
        if (!pattern.empty())
        {
            // workaround for car reflections
            injector::MakeCALL(pattern.get_first(6), rage::grmShaderInfo::setShaderParamOriginal, true);
        }

        pattern = find_pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 53 8B D9 56 F7 83", "55 8B EC 83 E4 F0 81 EC ? ? ? ? 53 56 57 8B F9 33 DB");
        CRenderPhaseDeferredLighting_LightsToScreen::shBuildRenderList = safetyhook::create_inline(pattern.get_first(), CRenderPhaseDeferredLighting_LightsToScreen::BuildRenderList);

        pattern = find_pattern("E8 ? ? ? ? F3 0F 10 44 24 ? 51 F3 0F 11 04 24 56 E8 ? ? ? ? 83 C4 08 FF 05", "E8 ? ? ? ? D9 44 24 0C 51 D9 1C 24 56 E8 ? ? ? ? 83 C4 08");
        CRenderPhaseDeferredLighting_LightsToScreen::shCopyLight = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first()).get<void*>(), CRenderPhaseDeferredLighting_LightsToScreen::CopyLight);

        pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B F9 83 BF", "55 8B EC 83 E4 F0 83 EC 24 53 56 8B D9 83 BB");
        CRenderPhaseDrawScene::shBuildRenderList = safetyhook::create_inline(pattern.get_first(0), CRenderPhaseDrawScene::BuildRenderList);

        pattern = hook::pattern("F3 0F 11 05 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? F3 0F 11 0D");
        if (pattern.empty())
            pattern = hook::pattern("F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 84 C0 74 15 E8 ? ? ? ? 84 C0");
        CWeather::Rain = *pattern.get_first<float*>(4);

        pattern = find_pattern("A1 ? ? ? ? 83 C4 08 8B CF", "A1 ? ? ? ? 80 3F 04");
        CWeather::OldWeatherType = *pattern.get_first<CWeather::eWeatherType*>(1);

        pattern = find_pattern("A1 ? ? ? ? 89 46 4C A1", "A1 ? ? ? ? 77 05 A1 ? ? ? ? 80 3F 04");
        CWeather::NewWeatherType = *pattern.get_first<CWeather::eWeatherType*>(1);

        pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 8B 44 24 0C 8B 4C 24 04");
        CWeather::InterpolationValue = *pattern.get_first<float*>(4);

        pattern = find_pattern("BF ? ? ? ? F3 AB B8", "BF ? ? ? ? F3 AB B8 ? ? ? ? B9");
        RageDirect3DDevice9::g_TexturesBySampler = *pattern.get_first<IDirect3DTexture9**>(1);

        pattern = find_pattern("81 EC ? ? ? ? 8D 04 24 68 ? ? ? ? FF B4 24", "8B 44 24 04 81 EC ? ? ? ? 68");
        rage::grcTextureFactoryPC::shCreateTexture = safetyhook::create_inline(pattern.get_first(0), rage::grcTextureFactoryPC::CreateTexture);

        pattern = find_pattern("53 8B 5C 24 08 56 33 F6 57 39 35 ? ? ? ? 7E 25 8B 3C B5", "53 8B 5C 24 08 56 57 33 FF 39 3D", "53 8B 5C 24 08 56 33 F6 39 35 ? ? ? ? 57 7E 29 8B 3C B5 ? ? ? ? 8B 07 8B 50 14 53 8B CF FF D2 50 E8 ? ? ? ? 83 C4 08 85 C0");
        rage::grcTextureFactoryPC::shCreateRT = safetyhook::create_inline(pattern.get_first(0), rage::grcTextureFactoryPC::CreateRT);

        pattern = find_pattern("53 55 56 57 8B F9 85 FF 74 3F", "53 55 8B 6C 24 0C 56 57 EB 06 8D 9B 00 00 00 00 0F B7 51 14 33 FF 83 EA 01 78 26 8B 59 10", "85 C9 53 55 56 57 74 40 8B 6C 24 14 8D 64 24 00");
        CTxdStore::getEntryByKey = pattern.get_first<rage::grcTexturePC*(__fastcall)(int*, void*, unsigned int)>(0);

        pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? F3 0F 10 05 ? ? ? ? 56 8B 75 ? 0F 57 DB F3 0F 10 0E 0F 2E CB 57 9F 8B F9 F3 0F 11 44 24 ? F6 C4 ? 7A ? 0F 28 CB F3 0F 11 44 24", "55 8B EC 83 E4 ? 0F 57 D2 83 EC ? 56 57");
        Matrix34::fromEulersXYZ = pattern.get_first<void(__fastcall)(float*, void*, float*)>(0);

        pattern = hook::pattern("68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 8B C8 E8 ? ? ? ? A3 ? ? ? ? 5E");
        CTxdStore::at = (int* (__cdecl*)(int))injector::ReadMemory<uint32_t>(pattern.get_first(1), true);

        pattern = find_pattern("A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? A1", "A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 8B 0D");
        RageDirect3DDevice9::m_pRealDevice = *pattern.get_first<IDirect3DDevice9**>(1);

        pattern = find_pattern("A1 ? ? ? ? 83 F8 08 74 17", "A1 ? ? ? ? 83 F8 08 74 0C");
        pMenuTab = *pattern.get_first<int32_t*>(1);

        pattern = find_pattern("E8 ? ? ? ? 84 C0 75 89", "E8 ? ? ? ? 84 C0 75 15 38 05");
        CCutscenes::hasCutsceneFinished = (bool(*)())injector::GetBranchDestination(pattern.get_first(0)).get();
        pattern = find_pattern("E8 ? ? ? ? 84 C0 75 44 38 05 ? ? ? ? 74 26", "E8 ? ? ? ? 84 C0 75 42 38 05");
        CCamera::isWidescreenBordersActive = (bool(*)())injector::GetBranchDestination(pattern.get_first(0)).get();

        pattern = hook::pattern("A3 ? ? ? ? 8B 44 24 ? A3 ? ? ? ? 8B 44 24 ? A3 ? ? ? ? A1");
        if (!pattern.empty())
        {
            CClock::ms_nGameClockHours = *pattern.get_first<uint8_t*>(1);
            CClock::ms_nGameClockMinutes = *pattern.get_first<uint8_t*>(10);
            CClock::ms_nGameClockSeconds = *pattern.get_first<uint16_t*>(19);
        }
        else
        {
            pattern = hook::pattern("A3 ? ? ? ? 8B 44 24 ? 89 0D ? ? ? ? 8B 0D");
            CClock::ms_nGameClockHours = *pattern.get_first<uint8_t*>(11);
            CClock::ms_nGameClockMinutes = *pattern.get_first<uint8_t*>(23);
            CClock::ms_nGameClockSeconds = *pattern.get_first<uint16_t*>(28);
        }

        pattern = hook::pattern("51 56 8B F1 83 BE ? ? ? ? ? 0F 84 ? ? ? ? 68");
        CRenderPhaseDeferredLighting_SceneToGBuffer::shBuildRenderList = safetyhook::create_inline(pattern.get_first(0), CRenderPhaseDeferredLighting_SceneToGBuffer::BuildRenderList);

        pattern = find_pattern("83 3D ? ? ? ? ? 8D 81 ? ? ? ? 75 ? 8D 81 ? ? ? ? 50", "83 3D ? ? ? ? ? 74 ? 8D 8B");
        dwSniperInverted = *pattern.get_first<int*>(2);

        pattern = find_pattern("8B 44 24 04 85 C0 75 18 A1", "8B 44 24 ? 85 C0 75 ? A1 ? ? ? ? 83 F8 ? 74");
        FindPlayerPed = (decltype(FindPlayerPed))pattern.get_first();

        pattern = hook::pattern("8B 44 24 04 85 C0 75 15 A1 ? ? ? ? 83 F8 FF 75 04 33 C0 EB 07");
        FindPlayerVehicle = (decltype(FindPlayerVehicle))pattern.get_first();

        pattern = find_pattern("8B 0D ? ? ? ? 83 C4 ? 50 E8 ? ? ? ? 85 C0 74", "8B 3D ? ? ? ? 8B CE FF D2 6A ? 6A ? 6A ? 2B 07");
        pObjectsPool = *pattern.get_first<decltype(pObjectsPool)>(2);

        pattern = find_pattern("8B 0D ? ? ? ? C6 46");
        pTaskPool = *pattern.get_first<decltype(pTaskPool)>(2);

        pattern = find_pattern("8B 3D ? ? ? ? 8B F1 8B 47", "8B 15 ? ? ? ? 81 EC ? ? ? ? 8B C1");
        pPedPool = *pattern.get_first<decltype(pPedPool)>(2);

        pattern = find_pattern("8B 0D ? ? ? ? E8 ? ? ? ? 85 C0 74 ? 80 7C 24 ? ? 75", "8B 0D ? ? ? ? E8 ? ? ? ? 85 C0 74 ? 8B C8 E8 ? ? ? ? E9 ? ? ? ? 33 F6");
        pCamPool = *pattern.get_first<decltype(pCamPool)>(2);

        pattern = find_pattern("8B 15 ? ? ? ? 46 3B 72 ? 7C ? 5E", "8B 3D ? ? ? ? 8B CE FF D2 6A ? 6A ? 6A ? EB");
        pVehiclePool = *pattern.get_first<decltype(pVehiclePool)>(2);

        pattern = find_pattern("68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 85 C0 0F 85 ? ? ? ? F3 0F 10 3D");
        pszCurrentCutsceneName = *pattern.get_first<const char*>(1);

        pattern = find_pattern("FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 ? 85 C0 79", "8B 0D ? ? ? ? 53 51 E8 ? ? ? ? 83 C4 ? 84 C0");
        CMenu::m_pMenuID = *pattern.get_first<int*>(2);

        pattern = find_pattern("8B 44 24 ? 83 F8 ? 74 ? 8B 0C 85 ? ? ? ? 85 C9 74 ? 80 B8 ? ? ? ? ? 74 ? 8B 81 ? ? ? ? C3 B8", "8B 4C 24 ? 83 F9 ? 74 ? 8B 04 8D ? ? ? ? 85 C0 74 ? 80 B9 ? ? ? ? ? 74 ? 8B 80 ? ? ? ? C3 B8");
        CMenu::m_pGetSelectedItem = (decltype(CMenu::m_pGetSelectedItem))pattern.get_first();

        pattern = find_pattern("B9 ? ? ? ? E8 ? ? ? ? 83 C4 ? 83 BC 24", "25 ? ? ? ? 53 55 56 0B C1");
        GTAIV_ENCRYPTION_KEY = reinterpret_cast<uint8_t(*)[32]>(*pattern.get_first<void*>(1));

        pattern = hook::pattern("E8 ? ? ? ? 85 C0 74 29 6A 00");
        if (!pattern.empty())
        {
            CPlayer::getLocalPlayerPed = (uintptr_t(*)())injector::GetBranchDestination(pattern.get_first(0)).as_int();
            CPlayer::findPlayerCar = (uintptr_t(*)())injector::GetBranchDestination(pattern.get_first(11)).as_int();
        }
        else
        {
            pattern = hook::pattern("E8 ? ? ? ? 85 C0 74 2A 53 E8");
            CPlayer::getLocalPlayerPed = (uintptr_t(*)())injector::GetBranchDestination(pattern.get_first(0)).as_int();
            CPlayer::findPlayerCar = (uintptr_t(*)())injector::GetBranchDestination(pattern.get_first(10)).as_int();
        }

        pattern = find_pattern("8B C1 56 8B 70 ? 85 F6", "8B 41 ? 85 C0 74 ? 8B 80 ? ? ? ? 85 C0 74 ? 8B 51");
        CWeaponData::getWeaponData = (decltype(CWeaponData::getWeaponData))pattern.get_first(0);

        pattern = find_pattern("8B 44 24 ? 83 F8 ? 7D ? 69 C0");
        CWeapon::getWeaponByType = (decltype(CWeapon::getWeaponByType))pattern.get_first(0);

        pattern = find_pattern("8B 44 24 ? 83 F8 ? 0F 87 ? ? ? ? FF 24 85 ? ? ? ? 8B 0D ? ? ? ? E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 8B C8 E8", "8B 44 24 ? 83 F8 ? 0F 87 ? ? ? ? FF 24 85 ? ? ? ? 8B 0D ? ? ? ? E8 ? ? ? ? 85 C0 74");
        static auto CreateCamHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
        {
            nCurrentCamera = eCamMode(*(int32_t*)(regs.esp + 0xC));
            if (nCurrentCamera == CAM_FPS_WEAPON)
            {
                bInSniperScope = true;
            }
            else
            {
                bInSniperScope = false;
            }
        });

        pattern = find_pattern("8B 15 ? ? ? ? 5E 85 D2 74 ? 8B 03", "8B 15 ? ? ? ? 53 66 8B 1D ? ? ? ? 33 FF 66 3B DD");
        rage::scrThread::sm_Threads = *pattern.get_first<decltype(rage::scrThread::sm_Threads)>(2);

        pattern = find_pattern("8B 35 ? ? ? ? 8B 47 ? FF 77", "89 3D ? ? ? ? E8 ? ? ? ? 83 7D");
        rage::scrThread::s_CurrentThread = *pattern.get_first<decltype(rage::scrThread::s_CurrentThread)>(2);

        pattern = find_pattern("E8 ? ? ? ? 8B 10 8B C8 FF 62", "E8 ? ? ? ? 8B 10 8B C8 8B 42 ? FF E0");
        rage::scrThread::GetActiveThread = (decltype(rage::scrThread::GetActiveThread))injector::GetBranchDestination(pattern.get_first(0)).as_int();

        pattern = find_pattern("8B 54 24 ? 85 D2 75 ? 33 C0", "56 8B 74 24 ? 33 C0 85 F6");
        rage::scrThread::GetThread = (decltype(rage::scrThread::GetThread))pattern.get_first(0);

        pattern = find_pattern("FF 74 24 ? FF 74 24 ? E8 ? ? ? ? 84 C0 75 ? 6A ? FF 74 24 ? 68", "8B 44 24 ? 56 8B 74 24 ? 50 56 E8 ? ? ? ? 84 C0");
        rage::scrThread::RegisterCommand = (decltype(rage::scrThread::RegisterCommand))pattern.get_first(0);

        pattern = find_pattern("E8 ? ? ? ? E9 ? ? ? ? 80 BC 3B", "E8 ? ? ? ? D9 45 ? F3 0F 10 05 ? ? ? ? F3 0F 2A 4D");
        rage::scrThread::shGetActiveThread = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first()).as_int(), rage::scrThread::GetActiveThreadHook);
    }
} Common;