//
// Created by Reveny on 2023/1/13.
//
#define targetLibName OBFUSCATE("libyoyo.so")

#include <jni.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include "Includes/Utils.h"
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"

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
    LOGI(OBFUSCATE("hack_thread running, pid=%d"), getpid());
    sleep(5);
    LOGI(OBFUSCATE("hack_thread done"));
    return nullptr;
}