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
    if (!lastSlash) { LOGW("extractPackageNameFromDataDir: 在 dataDir '%s' 中未找到 '/'", dataDir); return nullptr; }
    size_t packageNameLen = strlen(lastSlash + 1);
    if (packageNameLen == 0) { LOGW("extractPackageNameFromDataDir: dataDir '%s' 以 '/' 结尾", dataDir); return nullptr; }
    char* packageName = (char*)malloc(packageNameLen + 1);
    if (!packageName) { LOGE("extractPackageNameFromDataDir: 分配内存失败"); return nullptr; }
    strcpy(packageName, lastSlash + 1);
    packageName[packageNameLen] = '\0';
    return packageName;
}

class SimulateTabletModule : public zygisk::ModuleBase {
private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;

    void simulateTabletDevice(const char *brand_c, const char *model_c, const char *manufacturer_c) {
        LOGI("模拟设备: Brand=%s, Model=%s, Manufacturer=%s", brand_c, model_c, manufacturer_c);
        jclass buildClass = env->FindClass("android/os/Build");
        if (!buildClass) { LOGE("simulateTabletDevice: 未找到 Build 类"); if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        jfieldID manuField = env->GetStaticFieldID(buildClass, "MANUFACTURER", "Ljava/lang/String;");
        jfieldID brandField = env->GetStaticFieldID(buildClass, "BRAND", "Ljava/lang/String;");
        jfieldID modelField = env->GetStaticFieldID(buildClass, "MODEL", "Ljava/lang/String;");
        if (!manuField || !brandField || !modelField) { LOGE("simulateTabletDevice: 获取 Build 类字段失败"); env->DeleteLocalRef(buildClass); if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        jstring manuJ = env->NewStringUTF(manufacturer_c);
        jstring brandJ = env->NewStringUTF(brand_c);
        jstring modelJ = env->NewStringUTF(model_c);
        if (!manuJ || !brandJ || !modelJ) {
            LOGE("simulateTabletDevice: 创建 JNI 字符串失败");
            if (manuJ) env->DeleteLocalRef(manuJ); if (brandJ) env->DeleteLocalRef(brandJ); if (modelJ) env->DeleteLocalRef(modelJ);
        } else {
            env->SetStaticObjectField(buildClass, manuField, manuJ);
            env->SetStaticObjectField(buildClass, brandField, brandJ);
            env->SetStaticObjectField(buildClass, modelField, modelJ);
            LOGI("simulateTabletDevice: Build 字段设置成功。");
            env->DeleteLocalRef(manuJ); env->DeleteLocalRef(brandJ); env->DeleteLocalRef(modelJ);
        }
        env->DeleteLocalRef(buildClass);
        if (env->ExceptionCheck()) { LOGE("simulateTabletDevice: 设置字段时发生异常"); env->ExceptionClear(); }
    }

public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        LOGI("SimulateTablet 模块已加载 (仅微信模式)。");
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
                        LOGI("Pre-specialize: 匹配到目标包 WeChat (%s)。设置 FORCE_DENYLIST_UNMOUNT。", packageName);
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
            LOGW("Pre-specialize: app_data_dir JNI 字符串为空。");
        }

        if (!isWeChat) {
            const char* niceName = args->nice_name ? args->nice_name : "(null)";
            LOGW("Pre-specialize: 未通过 app_data_dir 匹配，检查 nice_name (%s)...", niceName);
            if (args->nice_name && strstr(niceName, WECHAT_PACKAGE_NAME) != nullptr) {
                LOGW("Pre-specialize: 通过后备 nice_name 匹配到 WeChat (%s)。设置 FORCE_DENYLIST_UNMOUNT。", niceName);
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
                env->ReleaseStringUTFChars(appDataDirJ, appDataDirC); // 尽早释放
                if (packageName != nullptr) {
                    if (strcmp(packageName, WECHAT_PACKAGE_NAME) == 0) isWeChat = true;
                }
            } else {
                LOGE("Post-specialize: GetStringUTFChars for app_data_dir failed!");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        } else {
            LOGW("Post-specialize: app_data_dir JNI 字符串为空。");
        }

        if (isWeChat) {
            LOGI("Post-specialize: 处理 WeChat (%s)", packageName ? packageName : "unknown (解析失败)");
            simulateTabletDevice(WECHAT_TARGET_BRAND, WECHAT_TARGET_MODEL, WECHAT_TARGET_BRAND);
            LOGI("Post-specialize: WeChat 处理完成。");
        } else if (packageName != nullptr) {
            LOGI("Post-specialize: 解析到包名 %s，但不是目标应用 WeChat。", packageName);
        } else {
            const char* niceName = args->nice_name ? args->nice_name : "(null)";
            LOGW("Post-specialize: 无法从 app_data_dir 解析包名，尝试使用 nice_name (%s)...", niceName);
            if (args->nice_name && strstr(niceName, WECHAT_PACKAGE_NAME) != nullptr) {
                LOGW("Post-specialize: 使用后备 nice_name 处理 WeChat (%s)", niceName);
                simulateTabletDevice(WECHAT_TARGET_BRAND, WECHAT_TARGET_MODEL, WECHAT_TARGET_BRAND);
                LOGI("Post-specialize: WeChat 处理完成。");
                isWeChat = true;
            } else {
                LOGI("Post-specialize: nice_name (%s) 也未匹配 WeChat。", niceName);
            }
        }

        if (packageName) {
            free(packageName);
            packageName = nullptr;
        }

    }
};

REGISTER_ZYGISK_MODULE(SimulateTabletModule)