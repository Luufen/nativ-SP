/*
 * gpu_family.h: which GPU family a Vulkan device belongs to, from VkPhysicalDeviceProperties' vendorID and deviceName.
 * The driver picker (DriverStore.java) takes an imported driver only for the system GPU's family, and a loaded custom
 * driver has to report that family again (driver/vulkan.c). The numbers are DriverPackage.java's.
 * Pure: tests/gpu_family_test.c.
 */
#ifndef NATIV_GPU_FAMILY_H
#define NATIV_GPU_FAMILY_H
#include <stdint.h>
#include <string.h>

enum { GPU_FAMILY_UNKNOWN = 0, GPU_FAMILY_ADRENO = 1, GPU_FAMILY_MALI = 2, GPU_FAMILY_XCLIPSE = 3 };

static inline int gpu_family(uint32_t vendor_id, const char *device_name) {
	switch (vendor_id) {
	case 0x5143: return GPU_FAMILY_ADRENO;    /* Qualcomm */
	case 0x13B5: return GPU_FAMILY_MALI;      /* Arm: the vendor's driver and PanVK */
	case 0x144D: return GPU_FAMILY_XCLIPSE;   /* Samsung: the vendor's driver, and RADV ports that report it */
	case 0x1002: return GPU_FAMILY_XCLIPSE;   /* AMD: RADV otherwise; the AMD GPU of an ARM64 Android device is an Xclipse */
	}
	/* other vendor IDs (Mesa's own on some Turnip versions): the device name */
	if (!device_name) return GPU_FAMILY_UNKNOWN;
	if (strstr(device_name, "Adreno")) return GPU_FAMILY_ADRENO;
	if (strstr(device_name, "Mali") || strstr(device_name, "Immortalis") || strstr(device_name, "PanVK")) return GPU_FAMILY_MALI;
	if (strstr(device_name, "Xclipse")) return GPU_FAMILY_XCLIPSE;
	return GPU_FAMILY_UNKNOWN;
}
#endif
