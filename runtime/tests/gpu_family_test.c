/* gpu_family_test: platform/android/driver/gpu_family.h with the vendor IDs and device names Android Vulkan drivers
 * report: Qualcomm's and Turnip, Arm's and PanVK, Samsung's and RADV; anything else is unknown. No Vulkan or GPU. */
#include <stdio.h>
#include "gpu_family.h"

static int checks, failed;
#define CHECK(x) do { checks++; if (!(x)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

int main(void) {
	CHECK(gpu_family(0x5143, "Adreno (TM) 650") == GPU_FAMILY_ADRENO);
	CHECK(gpu_family(0x10005, "Turnip Adreno (TM) 650") == GPU_FAMILY_ADRENO);   /* Mesa's vendor ID: by name */
	CHECK(gpu_family(0x13B5, "Mali-G610") == GPU_FAMILY_MALI);
	CHECK(gpu_family(0x13B5, "PanVK-G720 MC12") == GPU_FAMILY_MALI);
	CHECK(gpu_family(0x10005, "Immortalis-G925") == GPU_FAMILY_MALI);
	CHECK(gpu_family(0x144D, "Samsung Xclipse 940") == GPU_FAMILY_XCLIPSE);
	CHECK(gpu_family(0x144D, "AMD Unknown (RADV)") == GPU_FAMILY_XCLIPSE);
	CHECK(gpu_family(0x1002, "AMD Unknown (RADV)") == GPU_FAMILY_XCLIPSE);
	CHECK(gpu_family(0x10005, "Xclipse 920 (RADV)") == GPU_FAMILY_XCLIPSE);
	/* the vendor ID wins over the name */
	CHECK(gpu_family(0x13B5, "Adreno") == GPU_FAMILY_MALI);
	CHECK(gpu_family(0x1010, "PowerVR B-Series BXM-8-256") == GPU_FAMILY_UNKNOWN);
	CHECK(gpu_family(0x10005, "llvmpipe (LLVM 21.1.8, 256 bits)") == GPU_FAMILY_UNKNOWN);
	CHECK(gpu_family(0x10DE, "NVIDIA Tegra X1") == GPU_FAMILY_UNKNOWN);
	CHECK(gpu_family(0, "") == GPU_FAMILY_UNKNOWN);
	CHECK(gpu_family(0, 0) == GPU_FAMILY_UNKNOWN);
	printf("gpu_family_test: %d checks, %d failed\n", checks, failed);
	return failed != 0;
}
