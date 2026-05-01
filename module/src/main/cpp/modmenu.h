//
// Created by Reveny on 2023/1/13.
//
#define targetLibName OBFUSCATE("libyoyo.so")

#include <jni.h>
#include <sys/system_properties.h>

#include "KittyMemory/MemoryPatch.h"
#include "Includes/Dobby/dobby.h"
#include "Includes/Utils.h"
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "sara_whitespike.h"

static int enable_hack;
static char *game_data_dir = nullptr;
static char *game_package_name = "com.daerisoft.thespikerm";

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

void *hack_thread(void *) {
    LOGI(OBFUSCATE("hack thread: %d"), gettid());

    do {
        LOGI(OBFUSCATE("Waiting for libyoyo.so..."));
        sleep(2);
    } while (!isLibraryLoaded(OBFUSCATE("libyoyo.so")));

    uintptr_t base = getBaseAddress(OBFUSCATE("libyoyo.so"));
    LOGI(OBFUSCATE("libyoyo.so base: 0x%lx"), base);

    if (!base) {
        LOGE(OBFUSCATE("Failed to get libyoyo.so base"));
        return nullptr;
    }

    lib_info info = find_library("libyoyo.so");
    LOGI(OBFUSCATE("libyoyo.so size: 0x%lx"), (unsigned long)info.size);

    bool ok = sara_find_and_patch(base, info.size);
    if (ok) {
        LOGI(OBFUSCATE("SpikerSara found at 0x%lx"), g_saraCtx.funcAddr);
        sara_apply();
        LOGI(OBFUSCATE("WhiteSpike AUTO-APPLIED to Sara!"));
    } else {
        LOGE(OBFUSCATE("SpikerSara NOT found - wrong game version?"));
    }

    return nullptr;
}