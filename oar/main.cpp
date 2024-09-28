
#include  "main.h"
#include "memory.h"
#include "overlay.h"
#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <cmath>
#include "d3dx9.h"
#include <d2d1.h>
#include <vector>
// Inheritance: UObject
#define M_PI 3.14159265358979323846264338327950288419716939937510

Memory memory{ "OAR-Win64-Shipping.exe" };
const auto base = memory.GetModuleAddress("OAR-Win64-Shipping.exe");

struct TArray
{
    uintptr_t Array;
    uint32_t Count;
    uint32_t MaxCount;

    uintptr_t operator[](uint32_t index) const
    {
        if (Array && index < Count) {
            return memory.Read<uintptr_t>(Array + (index * 8));
        }
        return 0;
    }

    uint32_t Size() const {
        return Count;
    }

    bool IsValid() const
    {
        constexpr uint32_t MAX_ARRAY_COUNT = 1000000;
        if (!Array)
            return false;

        if (Count > MaxCount)
            return false;

        if (MaxCount > MAX_ARRAY_COUNT)
            return false;

        return true;
    }

    uintptr_t GetAddress() const
    {
        return Array;
    }
};
class Vector3
{
public:
    Vector3() : x(0.f), y(0.f), z(0.f)
    {

    }

    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z)
    {

    }
    ~Vector3()
    {

    }

    float x;
    float y;
    float z;

    inline float Dot(Vector3 v)
    {
        return x * v.x + y * v.y + z * v.z;
    }

    inline float Distance(Vector3 v)
    {
        float x = this->x - v.x;
        float y = this->y - v.y;
        float z = this->z - v.z;

        return sqrtf((x * x) + (y * y) + (z * z)) * 0.01048f;
    }

    Vector3 operator+(Vector3 v)
    {
        return Vector3(x + v.x, y + v.y, z + v.z);
    }

    Vector3 operator-(Vector3 v)
    {
        return Vector3(x - v.x, y - v.y, z - v.z);
    }

    Vector3 operator*(float number) const {
        return Vector3(x * number, y * number, z * number);
    }
};
struct FQuat
{
    double x;
    double y;
    double z;
    double w;
};

struct FMinimalViewInfo
{
    Vector3 Location;
    Vector3 Rotation;
    float FOV;
};
struct FCameraCacheEntry
{
    char pad_0x0[0x10];
    FMinimalViewInfo POV;
};

namespace offsets {

    constexpr auto OFFSET_GWORLD = 0x4c233f0;
    constexpr auto OFFSET_GOBJECTS = 0x4adba50;
    constexpr auto OFFSET_GNAMES = 0x4a9f700;
    constexpr auto OFFSET_OwningGameInstance = 0x180;
    constexpr auto LocalPlayers = 0x38; // TArray<ULocalPlayer>
    constexpr auto Player = 0x298; // UPlayer
    constexpr auto AcknowledgedPawn = 0x2a0; // APawn*
    constexpr auto ViewportClient = 0x78;
}

struct FName
{
    int ComparisonIndex;
    int number;
};


D3DXMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
    float radPitch = (rot.x * float(M_PI) / 180.f);
    float radYaw = (rot.y * float(M_PI) / 180.f);
    float radRoll = (rot.z * float(M_PI) / 180.f);

    float SP = sinf(radPitch);
    float CP = cosf(radPitch);
    float SY = sinf(radYaw);
    float CY = cosf(radYaw);
    float SR = sinf(radRoll);
    float CR = cosf(radRoll);

    D3DMATRIX matrix;
    matrix.m[0][0] = CP * CY;
    matrix.m[0][1] = CP * SY;
    matrix.m[0][2] = SP;
    matrix.m[0][3] = 0.f;

    matrix.m[1][0] = SR * SP * CY - CR * SY;
    matrix.m[1][1] = SR * SP * SY + CR * CY;
    matrix.m[1][2] = -SR * CP;
    matrix.m[1][3] = 0.f;

    matrix.m[2][0] = -(CR * SP * CY + SR * SY);
    matrix.m[2][1] = CY * SR - CR * SP * SY;
    matrix.m[2][2] = CR * CP;
    matrix.m[2][3] = 0.f;

    matrix.m[3][0] = origin.x;
    matrix.m[3][1] = origin.y;
    matrix.m[3][2] = origin.z;
    matrix.m[3][3] = 1.f;

    return matrix;

}

float ScreenCenterX = GetSystemMetrics(SM_CXSCREEN) / 2;
float ScreenCenterY = GetSystemMetrics(SM_CYSCREEN) / 2;
bool WorldToScreenX(Vector3 WorldLocation, FMinimalViewInfo CameraCacheL, Vector3& Screenlocation)
{

    auto POV = CameraCacheL;
    Vector3 Rotation = POV.Rotation;
    D3DMATRIX tempMatrix = Matrix(Rotation);

    Vector3 vAxisX, vAxisY, vAxisZ;

    vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
    vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
    vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

    Vector3 vDelta = WorldLocation - POV.Location;
    Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

    if (vTransformed.z < 1.f)
        return false;

    float FovAngle = POV.FOV;

    Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
    Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;

    return true;
}


struct PrefixFilter {
    std::vector<std::string> prefixes;

    // Function to check if a given name starts with any of the prefixes
    bool Matches(const std::string& name) const {
        for (const auto& prefix : prefixes) {
            if (name.rfind(prefix, 0) == 0) { // Check if the name starts with the prefix
                return true;
            }
        }
        return false;
    }
};

std::string FNameToString(FName& fname)
{
    enum { NAME_SIZE = 1024 };
    char name[NAME_SIZE] = { 0 };

    const unsigned int chunkOffset = fname.ComparisonIndex >> 16;
    const unsigned short nameOffset = fname.ComparisonIndex;

    int64_t namePoolChunk = memory.Read<uint64_t>(base + offsets::OFFSET_GNAMES + 8 * (chunkOffset + 2)) + 2 * nameOffset;
    const auto nameLength = memory.Read<uint16_t>(namePoolChunk) >> 6;

    ::ReadProcessMemory(memory.processHandle, reinterpret_cast<LPCVOID>(namePoolChunk + 2), name, nameLength <= NAME_SIZE ? nameLength : NAME_SIZE, nullptr);
    

    std::string finalName = std::string(name);
    return finalName.empty() ? "null" : finalName;
}



    void esp::thread() {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();


       
        auto gworld = memory.Read<uint64_t>(base + offsets::OFFSET_GWORLD);
        auto PersistentLevel = memory.Read<uint64_t>(gworld + 0x30); //0xd8
      




        auto owninggameinstance = memory.Read<uint64_t>(gworld + offsets::OFFSET_OwningGameInstance);
        auto LocalPlayers = memory.Read<TArray>(owninggameinstance + 0x38);
        auto localplayer = memory.Read<uint64_t>(LocalPlayers.GetAddress());
        auto PlayerController = memory.Read<uint64_t>(localplayer + 0x30);
        auto player_camera_manager = memory.Read<uint64_t>(PlayerController + 0x2B8);
        auto  CameraCache = memory.Read<FCameraCacheEntry>(player_camera_manager + 0x1AE0);
        auto Actors = memory.Read<TArray>(PersistentLevel + 0x98);

        PrefixFilter filter;
        filter.prefixes = { "NPC_Guard",  "RatCharacter" , "NPC_Police"};
        for (int i = 0; i < Actors.Size(); i++)
        {
            auto CurrentActor = Actors[i];
            auto trust = memory.Read<FName>(CurrentActor + 0x18); //0x18 is fname btw
            std::string brain = FNameToString(trust);

            if (!filterEnabled || filter.Matches(brain))
            {
                auto RootComponent = memory.Read<uint64_t>(CurrentActor + 0x130);
                Vector3 WorldPos = memory.Read<Vector3>(RootComponent + 0x11c);
               
                Vector3 Screen;
                if (WorldToScreenX(WorldPos, CameraCache.POV, Screen))
                {
                    ImU32 color = IM_COL32(0, 255, 0, 255); // Green color

                    ImGui::GetForegroundDrawList()->AddText(ImVec2(Screen.x - 15, Screen.y), color, brain.c_str());
                }

            }
        

            

        }
        ImGui::Render();
    }
 
int main()
{

    
  


    auto gworld = memory.Read<uint64_t>(base + offsets::OFFSET_GWORLD);
    auto PersistentLevel = memory.Read<uint64_t>(gworld + 0x30); //0xd8
   auto Actors = memory.Read<TArray>(PersistentLevel + 0x98);




    auto owninggameinstance = memory.Read<uint64_t>(gworld + offsets::OFFSET_OwningGameInstance);
    auto LocalPlayers = memory.Read<TArray>(owninggameinstance + 0x38);
    auto localplayer = memory.Read<uint64_t>(LocalPlayers.GetAddress());
    auto PlayerController = memory.Read<uint64_t>(localplayer + 0x30);
    auto player_camera_manager = memory.Read<uint64_t>(PlayerController + 0x2B8);
  auto  CameraCache = memory.Read<FCameraCacheEntry>(player_camera_manager + 0x1AE0);
    //auto Camerafov = memory.Readv<FCameraCacheEntry>(CameraCache + 0x1AF0);
    auto localplayer_pawn = memory.Read<uint64_t>(PlayerController + 0x2A0);
    auto localplayer_root = memory.Read<uint64_t>(localplayer_pawn + 0x130);
    auto localplayer_state = memory.Read<uint64_t>(localplayer_pawn + 0x240);
    

    auto ahud = memory.Read<uint64_t>(PlayerController + 0x2b0);
    bool debuginfo = memory.Read<bool>(ahud + 0x228);
    
    memory.Write<int32_t>(ahud + 0x228, 4);

    auto spawnlocation = memory.Read<Vector3>(PlayerController + 0x558);
 
    printf("base address %p\n", base);
    printf("uworld %p\n", gworld);
    printf("OwningGameInstance %p\n", owninggameinstance);
    printf("PersistentLevel %p\n", PersistentLevel);
    printf("localplayers %p\n", LocalPlayers.GetAddress());
    printf("localplayer size % i\n", LocalPlayers.Size());
    printf("actors size %p\n", Actors);
    printf("actors size % i\n", Actors.Size());
    printf("localplayer %p\n", localplayer);
    printf("playercontroller %p\n", PlayerController);
    printf("ahud %p\n", ahud);
    printf("debuginfo %d\n", debuginfo);
    printf("camera cache %.3f\n", CameraCache.POV.FOV);

    printf("spawn location: %.3f, %.3f, %.3f\n", spawnlocation.x, spawnlocation.y, spawnlocation.z);
 
    overlay::render();
    return 0;
}