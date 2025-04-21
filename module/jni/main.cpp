#include <jni.h>
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include "zygisk.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SimulateTablet", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SimulateTablet", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "SimulateTablet", __VA_ARGS__)

#define WECHAT_PACKAGE_NAME "com.tencent.mm"
#define WECHAT_TARGET_MODEL "SM-F9560"
#define WECHAT_TARGET_BRAND "samsung"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;
using zygisk::Option;

static char* extractPackageNameFromDataDir(const char* dataDir);

static char* extractPackageNameFromDataDir(const char* dataDir) {
    if (!dataDir) return nullptr;
    const char* lastSlash = strrchr(dataDir, '/');
    if (!lastSlash) { LOGW("extractPackageNameFromDataDir: '/' not found in dataDir '%s'", dataDir); return nullptr; }
    size_t packageNameLen = strlen(lastSlash + 1);
    if (packageNameLen == 0) { LOGW("extractPackageNameFromDataDir: dataDir '%s' ends with '/'", dataDir); return nullptr; }
    char* packageName = (char*)malloc(packageNameLen + 1);
    if (!packageName) { LOGE("extractPackageNameFromDataDir: Memory allocation failed"); return nullptr; }
    strcpy(packageName, lastSlash + 1);
    packageName[packageNameLen] = '\0';
    return packageName;
}

class SimulateTabletModule : public zygisk::ModuleBase {
private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;

    void simulateTabletDevice(const char *brand_c, const char *model_c, const char *manufacturer_c) {
        LOGI("Simulating device: Brand=%s, Model=%s, Manufacturer=%s", brand_c, model_c, manufacturer_c);
        jclass buildClass = env->FindClass("android/os/Build");
        if (!buildClass) { LOGE("simulateTabletDevice: Build class not found"); if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        jfieldID manuField = env->GetStaticFieldID(buildClass, "MANUFACTURER", "Ljava/lang/String;");
        jfieldID brandField = env->GetStaticFieldID(buildClass, "BRAND", "Ljava/lang/String;");
        jfieldID modelField = env->GetStaticFieldID(buildClass, "MODEL", "Ljava/lang/String;");
        if (!manuField || !brandField || !modelField) { LOGE("simulateTabletDevice: Failed to get Build class fields"); env->DeleteLocalRef(buildClass); if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        jstring manuJ = env->NewStringUTF(manufacturer_c);
        jstring brandJ = env->NewStringUTF(brand_c);
        jstring modelJ = env->NewStringUTF(model_c);
        if (!manuJ || !brandJ || !modelJ) {
            LOGE("simulateTabletDevice: Failed to create JNI strings");
            if (manuJ) env->DeleteLocalRef(manuJ); if (brandJ) env->DeleteLocalRef(brandJ); if (modelJ) env->DeleteLocalRef(modelJ);
        } else {
            env->SetStaticObjectField(buildClass, manuField, manuJ);
            env->SetStaticObjectField(buildClass, brandField, brandJ);
            env->SetStaticObjectField(buildClass, modelField, modelJ);
            LOGI("simulateTabletDevice: Build fields set successfully.");
            env->DeleteLocalRef(manuJ); env->DeleteLocalRef(brandJ); env->DeleteLocalRef(modelJ);
        }
        env->DeleteLocalRef(buildClass);
        if (env->ExceptionCheck()) { LOGE("simulateTabletDevice: Exception occurred while setting fields"); env->ExceptionClear(); }
    }

public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        LOGI("SimulateTablet module loaded.");
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        jstring appDataDirJ = args->app_data_dir;
        const char *appDataDirC = nullptr;
        char *packageName = nullptr;
        bool isWeChat = false;

        if (appDataDirJ != nullptr) {
            appDataDirC = env->GetStringUTFChars(appDataDirJ, nullptr);
            if (appDataDirC != nullptr) {
                packageName = extractPackageNameFromDataDir(appDataDirC);
                if (packageName != nullptr) {
                    if (strcmp(packageName, WECHAT_PACKAGE_NAME) == 0) {
                        LOGI("Pre-specialize: Matched target package WeChat (%s). Setting FORCE_DENYLIST_UNMOUNT.", packageName);
                        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
                        isWeChat = true;
                    }
                    free(packageName);
                    packageName = nullptr;
                } else {
                    // extractPackageNameFromDataDir
                }
                env->ReleaseStringUTFChars(appDataDirJ, appDataDirC);
            } else {
                LOGE("Pre-specialize: GetStringUTFChars for app_data_dir failed!");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        } else {
            LOGW("Pre-specialize: app_data_dir JNI string is null.");
        }

        if (!isWeChat) {
            const char* niceName = args->nice_name ? args->nice_name : "(null)";
            LOGW("Pre-specialize: Not matched via app_data_dir, checking nice_name (%s)...", niceName);
            if (args->nice_name && strstr(niceName, WECHAT_PACKAGE_NAME) != nullptr) {
                LOGW("Pre-specialize: Matched WeChat (%s) via fallback nice_name. Setting FORCE_DENYLIST_UNMOUNT.", niceName);
                api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
                isWeChat = true;
            }
        }

        if (!isWeChat) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        jstring appDataDirJ = args->app_data_dir;
        const char *appDataDirC = nullptr;
        char *packageName = nullptr;
        bool isWeChat = false;

        if (appDataDirJ != nullptr) {
            appDataDirC = env->GetStringUTFChars(appDataDirJ, nullptr);
            if (appDataDirC != nullptr) {
                packageName = extractPackageNameFromDataDir(appDataDirC);
                env->ReleaseStringUTFChars(appDataDirJ, appDataDirC); // Release early
                if (packageName != nullptr) {
                    if (strcmp(packageName, WECHAT_PACKAGE_NAME) == 0) isWeChat = true;
                }
            } else {
                LOGE("Post-specialize: GetStringUTFChars for app_data_dir failed!");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        } else {
            LOGW("Post-specialize: app_data_dir JNI string is null.");
        }

        if (isWeChat) {
            LOGI("Post-specialize: Handling WeChat (%s)", packageName ? packageName : "unknown (parsing failed)");
            simulateTabletDevice(WECHAT_TARGET_BRAND, WECHAT_TARGET_MODEL, WECHAT_TARGET_BRAND);
            LOGI("Post-specialize: WeChat handling completed.");
        } else if (packageName != nullptr) {
            LOGI("Post-specialize: Parsed package name %s, but it is not the target app WeChat.", packageName);
        } else {
            const char* niceName = args->nice_name ? args->nice_name : "(null)";
            LOGW("Post-specialize: Unable to parse package name from app_data_dir, trying nice_name (%s)...", niceName);
            if (args->nice_name && strstr(niceName, WECHAT_PACKAGE_NAME) != nullptr) {
                LOGW("Post-specialize: Handling WeChat (%s) via fallback nice_name", niceName);
                simulateTabletDevice(WECHAT_TARGET_BRAND, WECHAT_TARGET_MODEL, WECHAT_TARGET_BRAND);
                LOGI("Post-specialize: WeChat handling completed.");
                isWeChat = true;
            } else {
                LOGI("Post-specialize: nice_name (%s) did not match WeChat.", niceName);
            }
        }

        if (packageName) {
            free(packageName);
            packageName = nullptr;
        }
    }
};

REGISTER_ZYGISK_MODULE(SimulateTabletModule)
