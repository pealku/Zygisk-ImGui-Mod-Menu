//
// Created by Reveny on 2023/1/13.
//
#define targetLibName OBFUSCATE("libyoyo.so")

#include <jni.h>
#include <sys/system_properties.h>

#include "KittyMemory/MemoryPatch.h"
#include "Includes/ESP.h"
#include "Includes/Dobby/dobby.h"
#include "Includes/Utils.h"
#include "Includes/ImGui.h"
#include "sara_whitespike.h"

static int enable_hack;
static char *game_data_dir = nullptr;
static char *game_package_name = "com.daerisoft.thespikerm";

static bool sara_ws_enabled = false;
static bool sara_ws_ready   = false;

int isGame(JNIEnv *env, jstring appDataDir) {
    if (!appDataDir) return 0;
    const char *app_data_dir = env->GetStringUTFChars(appDataDir, nullptr);
    static char package_name[256];
    if (sscanf(app_data_dir, OBFUSCATE("/data/%*[^/]/%d/%s"), 0, package_name) != 2) {
        if (sscanf(app_data_dir, OBFUSCATE("/data/%*[^/]/%s"), package_name) != 1) {
            package_name[0] = '\0';
            LOGW(OBFUSCATE("can't parse %s"), app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, game_package_name) == 0) {
        LOGI(OBFUSCATE("detect game: %s"), package_name);
        game_data_dir = new char[strlen(app_data_dir) + 1];
        strcpy(game_data_dir, app_data_dir);
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    } else {
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 0;
    }
}

void drawMenu() {
    static const char* status_text = "";
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Once);
    ImGui::Begin(OBFUSCATE("The Spike RM v1.0"));

    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f),
        sara_ws_ready ? (g_saraCtx.enabled ? "Sara: WhiteSpike ACTIVE" : "Sara: Ready")
                      : "Sara: Scanning...");

    bool prev = sara_ws_enabled;
    ImGui::Checkbox(OBFUSCATE("Sara + WhiteSpike"), &sara_ws_enabled);

    if (sara_ws_enabled != prev && sara_ws_ready) {
        if (sara_ws_enabled) sara_apply();
        else                 sara_restore();
    } else if (sara_ws_enabled != prev && !sara_ws_ready) {
        sara_ws_enabled = prev; // revert, not ready yet
    }

    ImGui::TextDisabled(OBFUSCATE("Gives Wingspiker Sara the WhiteSpike skill"));
    ImGui::End();
}

void *hack_thread(void *) {
    LOGI(OBFUSCATE("hack thread: %d"), gettid());
    initModMenu((void *)drawMenu);

    do {
        LOGI(OBFUSCATE("Waiting for libyoyo.so... PID: %d"), getpid());
        sleep(1);
    } while (!isLibraryLoaded(OBFUSCATE("libyoyo.so")));

    uintptr_t base = getBaseAddress(OBFUSCATE("libyoyo.so"));
    LOGI(OBFUSCATE("libyoyo.so base: 0x%lx"), base);

    if (base) {
        lib_info info = find_library("libyoyo.so");
        LOGI(OBFUSCATE("libyoyo.so size: 0x%lx"), info.size);

        bool ok = sara_find_and_patch(base, info.size);
        if (ok) {
            sara_ws_ready = true;
            LOGI(OBFUSCATE("SpikerSara found at 0x%lx, blocks ready"), g_saraCtx.funcAddr);
        } else {
            LOGE(OBFUSCATE("SpikerSara NOT found - game version mismatch"));
        }
    }

    LOGI(OBFUSCATE("Init Done"));
    return nullptr;
}