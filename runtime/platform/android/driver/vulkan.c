/* SDL's Android Vulkan backend opens this library via SDL_HINT_VULKAN_LIBRARY.
 * The driver and its loader stay alive for the lifetime of the game process. */
#include <adrenotools/driver.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "gpu_family.h"

static PFN_vkGetInstanceProcAddr custom_proc;

static bool library_mapped(const char *library_path);

/* The GPU family the driver behind proc reports: its first device of a known family, or of `wanted` when that is set.
 * GPU_FAMILY_UNKNOWN when there is none, or when library_path is given and that file is not the one loaded. */
static int probe_family(PFN_vkGetInstanceProcAddr proc, const char *library_path, int wanted)
{
    if (!proc) return GPU_FAMILY_UNKNOWN;
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)proc(NULL, "vkCreateInstance");
    if (!create) return GPU_FAMILY_UNKNOWN;
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Project Nativ",
        .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    VkInstance instance;
    if (create(&info, NULL, &instance) != VK_SUCCESS) return GPU_FAMILY_UNKNOWN;

    PFN_vkDestroyInstance destroy = (PFN_vkDestroyInstance)proc(instance, "vkDestroyInstance");
    PFN_vkEnumeratePhysicalDevices enumerate =
        (PFN_vkEnumeratePhysicalDevices)proc(instance, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties properties =
        (PFN_vkGetPhysicalDeviceProperties)proc(instance, "vkGetPhysicalDeviceProperties");
    int family = GPU_FAMILY_UNKNOWN;
    uint32_t count = 0;
    if (enumerate && properties && enumerate(instance, &count, NULL) == VK_SUCCESS && count) {
        VkPhysicalDevice *devices = calloc(count, sizeof(*devices));
        if (devices && enumerate(instance, &count, devices) == VK_SUCCESS) {
            for (uint32_t i = 0; i < count; ++i) {
                VkPhysicalDeviceProperties props;
                properties(devices[i], &props);
                int found = gpu_family(props.vendorID, props.deviceName);
                __android_log_print(ANDROID_LOG_INFO, "Nativ", "Vulkan probe: %s (vendor 0x%x, driver 0x%x, family %d)",
                                    props.deviceName, props.vendorID, props.driverVersion, found);
                if (found != GPU_FAMILY_UNKNOWN && (wanted == GPU_FAMILY_UNKNOWN || found == wanted)) {
                    family = found;
                    break;
                }
            }
        }
        free(devices);
    }
    // Android may unload its selected HAL when the last instance is destroyed.
    if (family != GPU_FAMILY_UNKNOWN && library_path && !library_mapped(library_path)) family = GPU_FAMILY_UNKNOWN;
    if (destroy) destroy(instance, NULL);
    return family;
}

JNIEXPORT jint JNICALL Java_org_projectnativ_app_DriverStore_systemFamily(
    JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    void *loader = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!loader) return GPU_FAMILY_UNKNOWN;
    int family = probe_family((PFN_vkGetInstanceProcAddr)dlsym(loader, "vkGetInstanceProcAddr"), NULL,
                              GPU_FAMILY_UNKNOWN);
    dlclose(loader);
    return family;
}

static bool library_mapped(const char *library_path)
{
    // Android's loader can silently fall back to the vendor library. Check the imported file's
    // actual mapping after vkCreateInstance has made the loader resolve its HAL.
    char *resolved = realpath(library_path, NULL);
    if (!resolved) return false;
    FILE *maps = fopen("/proc/self/maps", "r");
    char *line = NULL;
    size_t capacity = 0;
    size_t length = strlen(resolved);
    bool found = false;
    while (maps && getline(&line, &capacity, maps) >= 0) {
        const char *path = strchr(line, '/');
        if (path && !strncmp(path, resolved, length) && path[length] == '\n') {
            found = true;
            break;
        }
    }
    free(line);
    if (maps) fclose(maps);
    free(resolved);
    return found;
}

JNIEXPORT jboolean JNICALL Java_org_projectnativ_app_DriverStore_activate(
    JNIEnv *env, jclass type, jstring hooks_path, jstring driver_path, jstring filename, jint family)
{
    (void)type;
    const char *hooks = (*env)->GetStringUTFChars(env, hooks_path, NULL);
    const char *directory = (*env)->GetStringUTFChars(env, driver_path, NULL);
    const char *library = (*env)->GetStringUTFChars(env, filename, NULL);
    PFN_vkGetInstanceProcAddr proc = NULL;
    char *library_path = NULL;
    if (hooks && directory && library && asprintf(&library_path, "%s%s", directory, library) >= 0) {
        void *loader = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM,
                                                  NULL, hooks, directory, library, NULL, NULL);
        if (loader) proc = (PFN_vkGetInstanceProcAddr)dlsym(loader, "vkGetInstanceProcAddr");
        if (family == GPU_FAMILY_UNKNOWN || probe_family(proc, library_path, family) != family) proc = NULL;
        if (proc)
            __android_log_print(ANDROID_LOG_INFO, "Nativ", "Custom Vulkan driver loaded: %s", library);
    }
    free(library_path);
    if (hooks) (*env)->ReleaseStringUTFChars(env, hooks_path, hooks);
    if (directory) (*env)->ReleaseStringUTFChars(env, driver_path, directory);
    if (library) (*env)->ReleaseStringUTFChars(env, filename, library);
    custom_proc = proc;
    return proc != NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL __attribute__((visibility("default")))
vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
    return custom_proc ? custom_proc(instance, name) : NULL;
}
