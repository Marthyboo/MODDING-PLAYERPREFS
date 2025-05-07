# Polar ImGui
This is an implementation of ImGui for Android Unity-based games using il2cpp.

**[Visit the Polarmods forum](https://polarmods.com/)**

**[Join our Discord](https://discord.gg/swFjrMZuYr)**

## Android Studio
1. First of all, download Android Studio [here](https://developer.android.com/studio). Then, complete the installation process.
2. Download the PolarImGui repository, and extract it with any archiver (such as 7-Zip, WinRAR, etc).
 
![image](https://user-images.githubusercontent.com/64957743/173016188-401c7a8f-7fce-4f7a-b560-453098e8b0e0.png)

![image](https://user-images.githubusercontent.com/64957743/173016572-d18c5ba9-00ff-43d1-9f0c-9c163d3e255d.png)

3. Now, open the project in Android Studio. You will need to wait for the gradle to sync once it is open. As for the NDK, I recommend using the latest lts version, which can be downloaded [here](https://dl.google.com/android/repository/android-ndk-r23c-windows.zip).

![image](https://user-images.githubusercontent.com/64957743/173019991-0d7db98d-6b67-41e0-a0a0-c34f5a68fe78.png)

4. Build it! (Build -> Build Bundle(s)/APK(s) -> Build APK(s), or Ctrl + F9).

## Implementation
Implementing the menu into an application is very easy, just follow the steps below!

1. Once you build the apk, open it using an archiver (such as 7-Zip, WinRAR, etc), and find the 'lib' directory.

![image](https://user-images.githubusercontent.com/64957743/173011691-9563f44f-840a-400e-9931-7a80eb816ea7.png)

From there, you will be presented with two additional folders. Choose the folder that resembles the cpu architecture you want to use (whether it be armeabi-v7a, or arm64-v8a). I would also like to mention that you can implement both libraries into a game if it supports ARMv7a and ARMv8a, and it will work for most games (Guns of Boom did crash when I tested this on that game though)! So, there is no need to remove the folders as you 'normally' would.

![image](https://user-images.githubusercontent.com/64957743/173011171-7a89375d-8477-4a90-93ce-6998100aca04.png)

2. Move the 'libnative-lib.so' library to the target applications lib/chosen-cpu-architecture/ folder.

![image](https://user-images.githubusercontent.com/64957743/173011227-a94a7889-4d23-4803-a796-652a34f6f3dc.png)

3. Then, find your applications launch activity, and place the following code under the 'onCreate' method in that launch activity.

```
const-string v0, "native-lib"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```

![image](https://user-images.githubusercontent.com/64957743/173011463-7721f889-76e6-4df2-8bcd-b9fc04445262.png)

4. Compile it!

## Notes
* This was the first ImGui Android implementation for Unity-based games using il2cpp, before people (whom we won't mention) leeched from it to create their own version. Yes, we will be updating the menu with new features when we can.

<img width="1276" alt="POLARMODS-BANNER-HQ-2" src="https://user-images.githubusercontent.com/64957743/173014961-edc5c55f-8dca-4c39-a1aa-c3a22f8beb22.png">

//
,arthj
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#include <fstream>
#include <sstream>
#include "nlohmann/json.hpp"
#include "http/cpr/cpr.h"
#include "Misc/Logging.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_android.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "Obfuscation/Obfuscate.h"
#include <stdio.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include "Misc/JNIStuff.h"
#include "Misc/FileWrapper.h"
#include "Misc/Utils.h"
#include "ByNameModding/BNM.hpp"
#include "Obfuscation/Custom_Obfuscate.h"
#include "Unity/Unity.h"
#include "Misc/FunctionPointers.h"
#include "Hooking/Hooks.h"
#include "Misc/ImGuiStuff.h"
#include "Unity/Screen.h"
#include "Unity/Input.h"
#include "Memory/MemoryPatch.h"
#include "Hooking/JNIHooks.h"

namespace options {
static bool
EnableHealing = false,
AntiRadius = false,
HideName = false,
InstantDestroyCar = false,
KillLogs = false,
HardlyAmmo = false,
Fastspawnitem = false,
playernamechange = false,
InfiniteNick = true,
ShopPack = true,
instantSpawn = false,
FakeImpulse = false; // Changed from float to bool

    static float
            aimfov = 0.0f,
            speed = 0.0f,
            walkSpeedSliderValue = 6.4f,
            jumpForceSliderValue = 60.0f,
            PG26LaunchForce = 20.0f;


    static int firerate = 0;
}

bool emulator = true;

namespace Menu {
ImVec4 color = ImVec4(1, 1, 1, 1);

    struct Vector2 {
        float x, y;
        Vector2(float x_, float y_) : x(x_), y(y_) {}
    };

    struct Vector3 {
        float x, y, z;
        Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
        Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    };

    // Invoke Hook (for InstantDestroyCar)
    typedef void (*Invoke_t)(void* instance, monoString* methodName, float time);
    static Invoke_t oldInvoke = nullptr;

    void Invoke(void* instance, monoString* methodName, float time) {
        if (instance == nullptr || methodName == nullptr || oldInvoke == nullptr) {
            if (oldInvoke) {
                oldInvoke(instance, methodName, time);
            }
            return;
        }

        if (options::InstantDestroyCar) {
            monoString* explodableMethod = CreateMonoString("Explodable");
            if (explodableMethod == nullptr) {
                oldInvoke(instance, methodName, time);
                return;
            }
            oldInvoke(instance, explodableMethod, 0.0f);
        } else {
            oldInvoke(instance, methodName, time);
        }
    }

    // UpdateDesiredTargetSpeed Hook (for Speed and Jump)
    typedef void (*UpdateDesiredTargetSpeed_t)(void* instance, Vector2 input);
    static UpdateDesiredTargetSpeed_t oldUpdateDesiredTargetSpeed = nullptr;

    void UpdateDesiredTargetSpeed(void* instance, Vector2 input) {
        if (instance) {
            if (options::walkSpeedSliderValue >= 6.4f) {
                *(float*)((uint64_t)instance + 0x10) = options::walkSpeedSliderValue;
                *(float*)((uint64_t)instance + 0x14) = options::walkSpeedSliderValue * (4.0f / 6.4f);
                if (options::walkSpeedSliderValue == 6.4f && options::jumpForceSliderValue == 60.0f) {
                    if (oldUpdateDesiredTargetSpeed) {
                        oldUpdateDesiredTargetSpeed(instance, input);
                    }
                    return;
                }
            }

            if (options::jumpForceSliderValue != 60.0f) {
                *(float*)((uint64_t)instance + 0x24) = options::jumpForceSliderValue;
            }

            if (oldUpdateDesiredTargetSpeed) {
                oldUpdateDesiredTargetSpeed(instance, input);
            }
        } else {
            if (oldUpdateDesiredTargetSpeed) {
                oldUpdateDesiredTargetSpeed(instance, input);
            }
        }
    }

    // playername Hook (for HideName)
    typedef monoString* (*playername_t)(void* instance);
    static playername_t old_playername = nullptr;

    monoString* playername(void* instance) {
        if (instance != nullptr && options::HideName) {
            monoString* newPlayerName = CreateMonoString("JS453");
            if (newPlayerName) {
                *(monoString**)((uint64_t)instance + 0x158) = newPlayerName;
                return newPlayerName;
            }
        }
        if (old_playername) {
            return old_playername(instance);
        }
        return nullptr;
    }

    // AwakeInputField Hook (for InfiniteNick)
    typedef void (*AwakeInputField_t)(void* instance);
    static AwakeInputField_t old_AwakeInputField = nullptr;

    void AwakeInputField(void* instance) {
        if (instance != nullptr) {
            *(int*)((uint64_t)instance + 0x158) = 24000000; // Max nickname length
        }
        if (old_AwakeInputField) {
            old_AwakeInputField(instance);
        }
    }

    // TakeDamage Hook (for EnableHealing)
    typedef void (*TakeDamage_t)(void* instance, float amount, Vector3 blood_point, monoString* NamePlayer);
    static TakeDamage_t old_takedick = nullptr;

    typedef bool (*GetIsMine_t)(void* photonView);
    GetIsMine_t GetIsMine = nullptr;

    typedef void* (*PhotonViewGet_t)(void* component);
    PhotonViewGet_t PhotonViewGet = nullptr;

    void Hook_takedick(void* instance, float amount, Vector3 blood_point, monoString* NamePlayer) {
        if (!instance) {
            if (old_takedick) {
                old_takedick(instance, amount, blood_point, NamePlayer);
            }
            return;
        }

        bool isLocalPlayer = false;
        void* photonView = nullptr;

        if (!PhotonViewGet) {
            PhotonViewGet = (PhotonViewGet_t)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x12661A0")));
        }
        if (PhotonViewGet) {
            photonView = PhotonViewGet(instance);
        }
        if (photonView) {
            if (!GetIsMine) {
                GetIsMine = (GetIsMine_t)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x1264894")));
            }
            if (GetIsMine) {
                isLocalPlayer = GetIsMine(photonView);
            }
        }

        float finalAmount = amount;
        if (isLocalPlayer && options::EnableHealing) {
            finalAmount = -1000.0f; // Heal instead of damage
        }

        if (old_takedick) {
            old_takedick(instance, finalAmount, blood_point, NamePlayer);
        }
    }

    // FakeImpulse Hook
    typedef float (*fakeim_t)(void* instance);
    static fakeim_t old_fakeim = nullptr;

    float hooked_fakeim(void* instance) {
        if (!instance) {
            if (old_fakeim) {
                return old_fakeim(instance);
            }
            return 1.0f;
        }
        if (options::FakeImpulse) {
            return 10.0f; // High impulse when enabled
        }
        if (old_fakeim) {
            return old_fakeim(instance);
        }
        return 1.0f;
    }


    // CalculateExplosionRadius Hook (for AntiRadius)
    typedef float (*CalculateExplosionRadius_t)(void* instance);
    static CalculateExplosionRadius_t old_CalculateExplosionRadius = nullptr;

    float Hook_CalculateExplosionRadius(void* instance) {
        if (instance && options::AntiRadius) {
            return 0.0f; // Nullify explosion radius
        }
        if (old_CalculateExplosionRadius) {
            return old_CalculateExplosionRadius(instance);
        }
        return 1.0f;
    }

    // KillLogs Hook
    typedef bool (*KillLogs_t)(void* instance);
    static KillLogs_t old_KillLogs = nullptr;

    bool hooked_KillLogs(void* instance) {
        if (!instance) {
            if (old_KillLogs) {
                return old_KillLogs(instance);
            }
            return true;
        }
        if (options::KillLogs) {
            return false; // Suppress kill logging
        }
        if (old_KillLogs) {
            return old_KillLogs(instance);
        }
        return true;
    }

    // HardlyAmmo Hook
    typedef void (*fuckingdickoad_t)(void* instance);
    static fuckingdickoad_t old_fuckingdickoad = nullptr;

    void fuckingdickoad(void* instance) {
        if (instance != nullptr && options::HardlyAmmo) {
            void* weaponSpec = *(void**)((uint64_t)instance + string2Offset(OBFUSCATE("0x78")));
            if (weaponSpec != nullptr) {
                *(int*)((uint64_t)weaponSpec + string2Offset(OBFUSCATE("0x14"))) = 9;
                *(int*)((uint64_t)weaponSpec + string2Offset(OBFUSCATE("0x18"))) = 9;
            }
        }
        if (old_fuckingdickoad) {
            old_fuckingdickoad(instance);
        }
    }

    // PG26LaunchForce Hook
    typedef void (*pg2god_t)(void* instance);
    static pg2god_t old_pg2god = nullptr;

    void pg2god(void* instance) {
        if (instance != nullptr && options::PG26LaunchForce > 0.0f) {
            *(float*)((uint64_t)instance + string2Offset(OBFUSCATE("0x80"))) = options::PG26LaunchForce; // Min launch force
            *(float*)((uint64_t)instance + string2Offset(OBFUSCATE("0x84"))) = options::PG26LaunchForce; // Max launch force
        }
        if (old_pg2god) {
            old_pg2god(instance);
        }
    }

    // ShopPack Hook
    typedef void (*holyfuck_t)(void* instance);
    holyfuck_t old_holyfuck = nullptr;

    void Hook_holyfuck(void* instance) {
        if (instance != nullptr && options::ShopPack) {
            *(int*)((uint64_t)instance + string2Offset(OBFUSCATE("0x30"))) = 1;
        }
        if (old_holyfuck) {
            old_holyfuck(instance);
        }
    }

    // Weapon-related Hooks
    static int (*old_unknownwep)() = nullptr;
    int unknownwep() { return 1; }

    static int (*old_unknownwep2)() = nullptr;
    int unknownwep2() { return 1; }

    static int (*old_unknownwep3)() = nullptr;
    int unknownwep3() { return 1; }

    static int (*old_unknownwep4)() = nullptr;
    int unknownwep4() { return 1; }

    // CheckAcceptPlayerChange Hook
    static void (*original_CheckAcceptPlayerChange)(void*) = nullptr;

    void my_CheckAcceptPlayerChange(void* self) {
        if (!self) return;
        void* butonBuyPlayer = *(void**)((uint64_t)self + string2Offset(OBFUSCATE("0x150")));
        void* butonAcceptPlayer = *(void**)((uint64_t)self + string2Offset(OBFUSCATE("0x170")));
        void (*SetActive)(void*, int) = (void (*)(void*, int))getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0xC23AC0")));
        if (butonBuyPlayer && SetActive) {
            SetActive(butonBuyPlayer, 0); // false
        }
        if (butonAcceptPlayer && SetActive) {
            SetActive(butonAcceptPlayer, 1); // true
        }
    }

    // ReadyToSpawnObject_MoveNext Hook (for Fastspawnitem)
    typedef bool (*ReadyToSpawnObject_MoveNext_t)(void* instance);
    static ReadyToSpawnObject_MoveNext_t old_ReadyToSpawnObject_MoveNext = nullptr;

    bool ReadyToSpawnObject_MoveNext(void* instance) {
        if (instance && options::Fastspawnitem) {
            int* state = (int*)((uint64_t)instance + 0x10);
            void** current = (void**)((uint64_t)instance + 0x18);
            void* playerMain = *(void**)((uint64_t)instance + 0x20);
            if (state && current && playerMain && *state == 0) {
                *state = 1;
                *current = nullptr;
                bool* b_ReadyToSpawn = (bool*)((uint64_t)playerMain + 0x111);
                if (b_ReadyToSpawn) {
                    *b_ReadyToSpawn = true;
                }
                return true;
            }
        }
        if (old_ReadyToSpawnObject_MoveNext) {
            return old_ReadyToSpawnObject_MoveNext(instance);
        }
        return false;
    }

    void *hack_thread(void *) {
        using namespace BNM;
        // logd("hack_thread started");

        // Wait for IL2CPP to be fully loaded
        int retries = 0;
        while (!Il2cppLoaded() && retries < 30) {
            sleep(2);
            retries++;
        }
        if (!Il2cppLoaded()) {
            // logd("IL2CPP not loaded after 60 seconds");
            return nullptr;
        }
        // logd("IL2CPP loaded");

        AttachIl2Cpp();
        // logd("Attached to IL2CPP");

        // Hook for Invoke (InstantDestroyCar)
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0xC2FBE4"))),
                (void*)Menu::Invoke,
                (void**)&Menu::oldInvoke)) {
            // logd("Hooked Invoke");
        } else {
            // logd("Failed to hook Invoke"); // not culprit
        }

        // Hook for UpdateDesiredTargetSpeed (Speed and Jump)
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x14C0AA0"))),
                (void*)Menu::UpdateDesiredTargetSpeed,
                (void**)&Menu::oldUpdateDesiredTargetSpeed)) {
            // logd("Hooked UpdateDesiredTargetSpeed");
        } else {
            // logd("Failed to hook UpdateDesiredTargetSpeed");
        }

        // Hook for playername (HideName)
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x13BF4D8"))),
                (void*)Menu::playername,
                (void**)&Menu::old_playername)) {
            // logd("Hooked playername");
        } else {
            // logd("Failed to hook playername"); // not da cuplit
        }

        // Hook for AwakeInputField (InfiniteNick)
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x16B053C"))),
                (void*)Menu::AwakeInputField,
                (void**)&Menu::old_AwakeInputField)) {
            // logd("Hooked AwakeInputField");
        } else {
            // logd("Failed to hook AwakeInputField");
        }

        // Hook for TakeDamage (EnableHealing)
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x1582430"))),
                (void*)Hook_takedick,
                (void**)&old_takedick)) {
            // logd("Hooked TakeDamage");
        } else {
            // logd("Failed to hook TakeDamage");
        }

        // Hook for FakeImpulse
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x8FCA78"))),
                (void*)hooked_fakeim,
                (void**)&old_fakeim)) {
            // logd("Hooked FakeImpulse");
        } else {
            // logd("Failed to hook FakeImpulse");
        }

        // Hook for CalculateExplosionRadius (AntiRadius)
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x11D169C"))),
                (void*)Hook_CalculateExplosionRadius,
                (void**)&old_CalculateExplosionRadius)) {
            // logd("Hooked CalculateExplosionRadius");
        } else {
            // logd("Failed to hook CalculateExplosionRadius");
        }

        // Hook for KillLogs
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x157CCE4"))),
                (void*)hooked_KillLogs,
                (void**)&old_KillLogs)) {
            // logd("Hooked KillLogs");
        } else {
            // logd("Failed to hook KillLogs");
        }

        // Hook for HardlyAmmo
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x15B1C58"))),
                (void*)fuckingdickoad,
                (void**)&old_fuckingdickoad)) {
            // logd("Hooked HardlyAmmo");
        } else {
            // logd("Failed to hook HardlyAmmo");
        }

        // Hook for PG26LaunchForce
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x1580AC4"))),
                (void*)pg2god,
                (void**)&old_pg2god)) {
            // logd("Hooked PG26LaunchForce");
        } else {
            // logd("Failed to hook PG26LaunchForce");
        }

        // Hook for ShopPack
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x13C3DE4"))),
                (void*)Hook_holyfuck,
                (void**)&old_holyfuck)) {
            // logd("Hooked ShopPack");
        } else {
            // logd("Failed to hook ShopPack");
        }

        // Hook for ReadyToSpawnObject_MoveNext (Fastspawnitem)
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x12CC148"))),
                (void*)Menu::ReadyToSpawnObject_MoveNext,
                (void**)&Menu::old_ReadyToSpawnObject_MoveNext)) {
            // logd("Hooked ReadyToSpawnObject_MoveNext");
        } else {
            // logd("Failed to hook ReadyToSpawnObject_MoveNext");
        }

        // Weapon-related Hooks
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x14B1D7C"))),
                (void*)unknownwep,
                (void**)&old_unknownwep)) {
            // logd("Hooked unknownwep");
        } else {
            // logd("Failed to hook unknownwep");
        }

        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x14B1C98"))),
                (void*)unknownwep2,
                (void**)&old_unknownwep2)) {
            // logd("Hooked unknownwep2");
        } else {
            // logd("Failed to hook unknownwep2");
        }

        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x14B1B0C"))),
                (void*)unknownwep3,
                (void**)&old_unknownwep3)) {
            // logd("Hooked unknownwep3");
        } else {
            // logd("Failed to hook unknownwep3");
        }

        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x14B15FC"))),
                (void*)unknownwep4,
                (void**)&old_unknownwep4)) {
            // logd("Hooked unknownwep4");
        } else {
            // logd("Failed to hook unknownwep4");
        }

        // CheckAcceptPlayerChange Hook
        if (DobbyHook(
                (void*)getAbsoluteAddress("libil2cpp.so", string2Offset(OBFUSCATE("0x137AED4"))),
                (void*)my_CheckAcceptPlayerChange,
                (void**)&original_CheckAcceptPlayerChange)) {
            // logd("Hooked CheckAcceptPlayerChange");
        } else {
            // logd("Failed to hook CheckAcceptPlayerChange");
        }

        Unity::Screen::Setup();
        if (emulator) {
            Unity::Input::Setup();
        }

        Pointers::LoadPointers();
        // logd("Loaded pointers");

        DetachIl2Cpp();
        // logd("Detached from IL2CPP");

        return nullptr;
    }

    void DrawMenu() {
        ImGui::Begin(OBFUSCATE("Mod Menu"));
        ImGui::Checkbox(OBFUSCATE("Enable Healing"), &options::EnableHealing);
        ImGui::Checkbox(OBFUSCATE("FakeImpulse"), &options::FakeImpulse); // Changed to Checkbox
        ImGui::Checkbox(OBFUSCATE("AntiRadius"), &options::AntiRadius);            // 2
        ImGui::Checkbox(OBFUSCATE("HideName"), &options::HideName);                // 3
        ImGui::Checkbox(OBFUSCATE("InstantDestroyCar"), &options::InstantDestroyCar); // 4
        ImGui::Checkbox(OBFUSCATE("KillLogs"), &options::KillLogs);                // 5
        ImGui::Checkbox(OBFUSCATE("HardlyAmmo"), &options::HardlyAmmo);            // 6
        ImGui::Checkbox(OBFUSCATE("Fastspawnitem"), &options::Fastspawnitem);      // 7
        ImGui::SliderFloat(OBFUSCATE("Jump"), &options::jumpForceSliderValue, 60.0f, 160.0f); // 8
        ImGui::SliderFloat(OBFUSCATE("Speed"), &options::walkSpeedSliderValue, 6.4f, 64.0f);  // 9
        ImGui::SliderFloat(OBFUSCATE("PG26LaunchForce"), &options::PG26LaunchForce, 15.0f, 300.0f); // 10
        ImGui::End();
    }

    void DrawImGui() {
        if (init && Unity::Screen::is_done) {
            ImGuiIO &io = ImGui::GetIO();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplAndroid_NewFrame(Unity::Screen::Width.get(), Unity::Screen::Height.get());
            ImGui::NewFrame();
            DrawMenu();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            ImGui::EndFrame();
        }
    }
}

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface);
EGLBoolean new_eglSwapBuffers(EGLDisplay _display, EGLSurface _surface) {
SetupImGui();
Menu::DrawImGui();
return old_eglSwapBuffers(_display, _surface);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
if (!emulator) {
jvm = vm;
JNIEnv *env;
vm->GetEnv((void **) &env, JNI_VERSION_1_6);
UnityPlayer_cls = env->FindClass(OBFUSCATE("com/unity3d/player/UnityPlayer"));
UnityPlayer_CurrentActivity_fid = env->GetStaticFieldID(UnityPlayer_cls,
OBFUSCATE("currentActivity"),
OBFUSCATE("Landroid/app/Activity;"));
hook((void *) env->functions->RegisterNatives, (void *) hook_RegisterNatives,
(void **) &old_RegisterNatives);
}
return JNI_VERSION_1_6;
}

__attribute__((constructor))
void lib_main() {
auto eglhandle = dlopen(OBFUSCATE("libEGL.so"), RTLD_LAZY);
if (!eglhandle) return;

    auto eglSwapBuffers = dlsym(eglhandle, OBFUSCATE("eglSwapBuffers"));
    if (!eglSwapBuffers) {
        dlclose(eglhandle);
        return;
    }

    hook(eglSwapBuffers, (void *) new_eglSwapBuffers, (void **) &old_eglSwapBuffers);
    if (!old_eglSwapBuffers) {
        dlclose(eglhandle);
        return;
    }

    pthread_t ptid;
    pthread_create(&ptid, NULL, Menu::hack_thread, NULL);
}