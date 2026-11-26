/*
 * Synthetic USSE register-split fixture (ANDROID-PERF-02): the GPU translator's per-word register variables.
 *
 * usse_spirv.c declared TEMP, PA and O as `Private` uint[160] arrays and FPINTERNAL as uint[16] in every module, all
 * zero-initialised -- 1,984 bytes of addressable state per invocation that proprietary mobile compilers may keep in
 * memory even though every index but an INDEXED1/2 operand's is constant. Now a bank no INDEXED operand addresses is
 * split into one zero-initialised `Function` uint per word the program touches; an indexed bank keeps its array, and
 * RECOMP_GPU_REGSPLIT=0 keeps them all. The tap variants (here: blend-lerp, fetch mode 4) restore split words one by one.
 *
 * The programs are hand-encoded VBW words (bitwise ops, field layout as usse_vbw_test.c / Vita3K's matcher) that copy
 * and mix SA words through TEMP, O, FPINTERNAL and the INDEX registers into the colour (PA8..11, F32). Each is run by
 * the CPU interpreter (the oracle) and translated with the split on and off. Checks:
 *  - which banks became Function variables and which stayed Private arrays (INDEXED TEMP read, INDEXED O write);
 *  - with SPIRV_VAL=<spirv-val>, the validator accepts every module (SPIRV_TMP=<dir> for the files, default /tmp);
 *  - built with -DUSSE_REGSPLIT_VK and Vulkan headers, each module renders a 4x4 RGBA32F target on the first Vulkan
 *    device (lavapipe: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json) and the pixel's four words equal the
 *    interpreter's colour words bit for bit (the lerp variant: rgb bit-exact, alpha within 1e-6).
 * fail-before (the arrays-only translator): the "split" bank checks fail; the value checks pass both ways.
 *
 *     cc -O1 -g -Wall -D_GNU_SOURCE -I ../gfx -I ../core -I ../../recompiler -I ../.. usse_regsplit_test.c ../gfx/usse.c ../gfx/usse_spirv.c -o /tmp/usse_regsplit_test -lm && /tmp/usse_regsplit_test
 *     (GPU execution: add -DUSSE_REGSPLIT_VK -I <Vulkan-Headers>/include -ldl)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include "usse.h"
#include "usse_internal.h"
#include "usse_spirv.h"

uint8_t *recomp_mem;
uint32_t recomp_watch_lo, recomp_watch_hi;   /* rt.h's write-watch: disabled */
void recomp_watch_write(uint32_t a, uint32_t v, int width) { (void)a; (void)v; (void)width; }

static int checks, failed;
static void ck(int cond, const char *what) { checks++; printf("%-96s %s\n", what, cond ? "ok" : "FAIL"); if (!cond) failed++; }

/* ---- synthetic GXP (usse.c's H_* layout, as usse_cf_test.c) ---- */
#define GXP_ADDR 0x10000000u
#define CODE_OFF 128
static void put32(uint32_t a, uint32_t v) { memcpy(recomp_mem + a, &v, 4); }
static void put64(uint32_t a, uint64_t v) { memcpy(recomp_mem + a, &v, 8); }
static UsseProgram *load(const uint64_t *w, int n) {
	static uint32_t seq;
	uint32_t g = GXP_ADDR + (++seq) * 0x10000;   /* a fresh address per program (the decoder caches by address) */
	memset(recomp_mem + g, 0, CODE_OFF + 8 * (size_t)n + 64);
	put32(g + 8, CODE_OFF + 8 * (uint32_t)n);
	put32(g + 20, 1);                 /* fragment program */
	put32(g + 48, 16);
	put32(g + 60, (uint32_t)n);       /* primary instruction count */
	put32(g + 64, CODE_OFF - 64);     /* primary offset */
	for (int i = 0; i < n; i++) put64(g + CODE_OFF + 8 * (uint32_t)i, w[i]);
	return usse_decode_uncached(g);
}

/* VBW (usse.c dec_vbw): op1 61:59, end 50, dst bank 33:32 + ext 51, src1 31:30 + ext 49, src2 29:28 + ext 48,
 * repeat 47:44 (+1), op2 35, dst 27:21, src2_sel 20:14 (an immediate's high bits), src1 13:7, src2 6:0.
 * dst banks (2-bit, ext): TEMP 0/0, SA 0/1, O 1/0, PA 2/0, INDEX 2/1, IDXD1 3/0, IDXD2 3/1.
 * src banks: TEMP 0/0, IDXD1 0/1, O 1/0, PA 2/0, IMM 2/1, SA 3/0, IDXD2 3/1. A TEMP number >= 124 is FPINTERNAL. */
enum { OR, AND, XOR };
typedef struct { int b, e, n; } Opd;
#define TEMP(n) ((Opd){0, 0, n})
#define FPI(k)  ((Opd){0, 0, 124 + (k)})
#define OUT(n)  ((Opd){1, 0, n})
#define PA(n)   ((Opd){2, 0, n})
#define SA(n)   ((Opd){3, 0, n})
#define IMM(v)  ((Opd){2, 1, v})
#define IDXREG(k) ((Opd){2, 1, k})                         /* destination only: the index register k (1 or 2) */
#define IDX1_SRC(base, k) ((Opd){0, 1, (base) << 5 | (k)})  /* INDEXED1 source: base 0 TEMP, 1 O, 2 PA, 3 SA */
#define IDX2_DST(base, k) ((Opd){3, 1, (base) << 5 | (k)})  /* INDEXED2 destination */
static uint64_t vbw(int f, int reps, Opd d, Opd s1, Opd s2) {
	int op1 = f == XOR ? 3 : 2, op2 = f == OR;
	uint64_t w = 1ull << 62;
	w |= (uint64_t)op1 << 59;
	w |= (uint64_t)(d.e & 1) << 51 | (uint64_t)(s1.e & 1) << 49 | (uint64_t)(s2.e & 1) << 48;
	w |= (uint64_t)((reps - 1) & 15) << 44;
	w |= (uint64_t)op2 << 35;
	w |= (uint64_t)(d.b & 3) << 32 | (uint64_t)(s1.b & 3) << 30 | (uint64_t)(s2.b & 3) << 28;
	w |= (uint64_t)(d.n & 0x7F) << 21;
	w |= (uint64_t)(s2.b == 2 && s2.e ? (s2.n >> 7) & 0x7F : 0) << 14;
	w |= (uint64_t)(s1.n & 0x7F) << 7;
	w |= (uint64_t)(s2.n & 0x7F);
	return w;
}

/* ---- module inspection: OpVariable (59) by storage class ---- */
static void count_vars(const SpirvModule *m, int *function, int *private_) {
	*function = *private_ = 0;
	for (int i = 5; i < m->nwords; ) {
		uint32_t wc = m->words[i] >> 16, op = m->words[i] & 0xFFFF;
		if (!wc) break;
		if (op == 59 && wc >= 4) { if (m->words[i + 3] == 7) (*function)++; else if (m->words[i + 3] == 6) (*private_)++; }
		i += (int)wc;
	}
}
static int spirv_val(const SpirvModule *m, const char *name) {
	const char *val = getenv("SPIRV_VAL");
	if (!val || !*val) return -1;
	const char *dir = getenv("SPIRV_TMP"); char path[512], cmd[1200];
	snprintf(path, sizeof path, "%s/regsplit_%s.spv", dir && *dir ? dir : "/tmp", name);
	FILE *f = fopen(path, "wb"); if (!f) return 0;
	fwrite(m->words, 4, (size_t)m->nwords, f); fclose(f);
	snprintf(cmd, sizeof cmd, "\"%s\" --target-env vulkan1.1 \"%s\"", val, path);
	int rc = system(cmd);
	return rc == 0;
}

#ifdef USSE_REGSPLIT_VK
/* ---- the GPU half: one fragment module at a time into a 4x4 RGBA32F target (a fullscreen triangle) ---- */
#include <dlfcn.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
static PFN_vkGetInstanceProcAddr gip; static PFN_vkGetDeviceProcAddr gdp;
static VkInstance vki; static VkPhysicalDevice vkpd; static VkDevice vkd; static VkQueue vkq; static uint32_t vkqf;
static char vk_name[256];
#define IP(name) PFN_##name name = (PFN_##name)gip(vki, #name)
#define DP(name) PFN_##name name = (PFN_##name)gdp(vkd, #name)
static int vk_init(void) {
	void *lib = dlopen("libvulkan.so.1", RTLD_NOW); if (!lib) return 0;
	gip = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr"); if (!gip) return 0;
	PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)gip(0, "vkCreateInstance");
	VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "usse_regsplit_test", 1, 0, 0, VK_API_VERSION_1_1};
	VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0, &ai, 0, 0, 0, 0};
	if (vkCreateInstance(&ici, 0, &vki) != VK_SUCCESS) return 0;
	IP(vkEnumeratePhysicalDevices); IP(vkGetPhysicalDeviceProperties); IP(vkGetPhysicalDeviceQueueFamilyProperties); IP(vkCreateDevice);
	gdp = (PFN_vkGetDeviceProcAddr)gip(vki, "vkGetDeviceProcAddr");
	uint32_t n = 1; if (vkEnumeratePhysicalDevices(vki, &n, &vkpd) < 0 || !n) return 0;
	VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(vkpd, &pp); snprintf(vk_name, sizeof vk_name, "%s", pp.deviceName);
	VkQueueFamilyProperties qf[16]; uint32_t nq = 16; vkGetPhysicalDeviceQueueFamilyProperties(vkpd, &nq, qf);
	for (vkqf = 0; vkqf < nq && !(qf[vkqf].queueFlags & VK_QUEUE_GRAPHICS_BIT); vkqf++) {}
	if (vkqf == nq) return 0;
	float prio = 1; VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0, vkqf, 1, &prio};
	VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, 0, 0, 1, &qci, 0, 0, 0, 0, 0};
	if (vkCreateDevice(vkpd, &dci, 0, &vkd) != VK_SUCCESS) return 0;
	DP(vkGetDeviceQueue); vkGetDeviceQueue(vkd, vkqf, 0, &vkq);
	return 1;
}
static uint32_t mem_type(uint32_t bits, VkMemoryPropertyFlags want) {
	IP(vkGetPhysicalDeviceMemoryProperties); VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(vkpd, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++) if ((bits >> i & 1) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
	return 0;
}
static VkDeviceMemory bind_mem(VkMemoryRequirements r, VkMemoryPropertyFlags want) {
	DP(vkAllocateMemory); VkDeviceMemory mem;
	VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, r.size, mem_type(r.memoryTypeBits, want)};
	return vkAllocateMemory(vkd, &mai, 0, &mem) == VK_SUCCESS ? mem : VK_NULL_HANDLE;
}
/* a fullscreen triangle from gl_VertexIndex: (-1,-1) (3,-1) (-1,3) */
static const uint32_t vert_spv[] = {
	0x07230203, 0x00010000, 0, 25, 0,
	2u << 16 | 17, 1,                                   /* OpCapability Shader */
	3u << 16 | 14, 0, 1,                                /* OpMemoryModel Logical GLSL450 */
	7u << 16 | 15, 0, 17, 0x6E69616D, 0, 8, 9,          /* OpEntryPoint Vertex %17 "main" %8 %9 */
	4u << 16 | 71, 8, 11, 0,                            /* OpDecorate %8 BuiltIn Position */
	4u << 16 | 71, 9, 11, 42,                           /* OpDecorate %9 BuiltIn VertexIndex */
	2u << 16 | 19, 1, 3u << 16 | 33, 2, 1,              /* void, fn() */
	3u << 16 | 22, 3, 32, 4u << 16 | 23, 4, 3, 4,       /* float, vec4 */
	4u << 16 | 21, 5, 32, 1,                            /* int */
	4u << 16 | 32, 6, 3, 4, 4u << 16 | 32, 7, 1, 5,     /* Output vec4*, Input int* */
	4u << 16 | 59, 6, 8, 3, 4u << 16 | 59, 7, 9, 1,     /* %8 Position, %9 VertexIndex */
	4u << 16 | 43, 5, 10, 1, 4u << 16 | 43, 5, 11, 2,   /* int 1, 2 */
	4u << 16 | 43, 3, 12, 0xBF800000, 4u << 16 | 43, 3, 13, 0x40400000,   /* -1.0, 3.0 */
	4u << 16 | 43, 3, 14, 0, 4u << 16 | 43, 3, 15, 0x3F800000,            /* 0.0, 1.0 */
	2u << 16 | 20, 16,                                  /* bool */
	5u << 16 | 54, 1, 17, 0, 2, 2u << 16 | 248, 18,     /* main: */
	4u << 16 | 61, 5, 19, 9,                            /* vid */
	5u << 16 | 170, 16, 20, 19, 10, 6u << 16 | 169, 3, 21, 20, 13, 12,   /* x = vid == 1 ? 3 : -1 */
	5u << 16 | 170, 16, 22, 19, 11, 6u << 16 | 169, 3, 23, 22, 13, 12,   /* y = vid == 2 ? 3 : -1 */
	7u << 16 | 80, 4, 24, 21, 23, 14, 15, 3u << 16 | 62, 8, 24,
	1u << 16 | 253, 1u << 16 | 56,
};
/* render one pixel's worth (all 16 are the same: no varyings) with `sa` in the SA block; out: pixel (1,1)'s words */
static int vk_run(const SpirvModule *m, const uint32_t sa[256], uint32_t out[4]) {
	DP(vkCreateImage); DP(vkGetImageMemoryRequirements); DP(vkBindImageMemory); DP(vkCreateImageView); DP(vkCreateRenderPass);
	DP(vkCreateFramebuffer); DP(vkCreateBuffer); DP(vkGetBufferMemoryRequirements); DP(vkBindBufferMemory); DP(vkMapMemory);
	DP(vkCreateDescriptorSetLayout); DP(vkCreatePipelineLayout); DP(vkCreateDescriptorPool); DP(vkAllocateDescriptorSets);
	DP(vkUpdateDescriptorSets); DP(vkCreateShaderModule); DP(vkCreateGraphicsPipelines); DP(vkCreateCommandPool);
	DP(vkAllocateCommandBuffers); DP(vkBeginCommandBuffer); DP(vkCmdBeginRenderPass); DP(vkCmdBindPipeline);
	DP(vkCmdBindDescriptorSets); DP(vkCmdDraw); DP(vkCmdEndRenderPass); DP(vkCmdPipelineBarrier); DP(vkCmdCopyImageToBuffer);
	DP(vkEndCommandBuffer); DP(vkCreateFence); DP(vkQueueSubmit); DP(vkWaitForFences); DP(vkDeviceWaitIdle); DP(vkDestroyDevice);
	(void)vkDestroyDevice;
	const VkFormat fmt = VK_FORMAT_R32G32B32A32_SFLOAT; const uint32_t W = 4;
	VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 0, 0, VK_IMAGE_TYPE_2D, fmt, {W, W, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT,
	                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED};
	VkImage img; if (vkCreateImage(vkd, &ii, 0, &img)) return 0;
	VkMemoryRequirements r; vkGetImageMemoryRequirements(vkd, img, &r); vkBindImageMemory(vkd, img, bind_mem(r, 0), 0);
	VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0, 0, img, VK_IMAGE_VIEW_TYPE_2D, fmt, {0}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
	VkImageView view; vkCreateImageView(vkd, &vi, 0, &view);
	VkAttachmentDescription ad = {0, fmt, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	                              VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
	VkAttachmentReference ar = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkSubpassDescription sd = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, 1, &ar, 0, 0, 0, 0};
	VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, 0, 0, 1, &ad, 1, &sd, 0, 0};
	VkRenderPass rp; vkCreateRenderPass(vkd, &rpi, 0, &rp);
	VkFramebufferCreateInfo fi = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, 0, 0, rp, 1, &view, W, W, 1};
	VkFramebuffer fb; vkCreateFramebuffer(vkd, &fi, 0, &fb);
	VkBuffer ubo, rb; void *map;
	VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, 0, 0, 2048, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, 0};
	vkCreateBuffer(vkd, &bi, 0, &ubo); vkGetBufferMemoryRequirements(vkd, ubo, &r);
	VkDeviceMemory um = bind_mem(r, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); vkBindBufferMemory(vkd, ubo, um, 0);
	vkMapMemory(vkd, um, 0, 2048, 0, &map); memset(map, 0, 2048); memcpy(map, sa, 1024);
	bi.size = W * W * 16; bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer(vkd, &bi, 0, &rb); vkGetBufferMemoryRequirements(vkd, rb, &r);
	VkDeviceMemory rm = bind_mem(r, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); vkBindBufferMemory(vkd, rb, rm, 0);
	VkDescriptorSetLayoutBinding ub = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0};
	VkDescriptorSetLayoutCreateInfo dli = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 0, 0, 0, 0};
	VkDescriptorSetLayout dsl[4];
	for (int k = 0; k < 3; k++) vkCreateDescriptorSetLayout(vkd, &dli, 0, &dsl[k]);   /* sets 0-2: the vertex stage's and the samplers', empty here */
	dli.bindingCount = 1; dli.pBindings = &ub; vkCreateDescriptorSetLayout(vkd, &dli, 0, &dsl[3]);   /* set 3: the fragment SA block */
	VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, 0, 0, 4, dsl, 0, 0};
	VkPipelineLayout pl; vkCreatePipelineLayout(vkd, &pli, 0, &pl);
	VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
	VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, 0, 0, 1, 1, &ps};
	VkDescriptorPool dp; vkCreateDescriptorPool(vkd, &dpi, 0, &dp);
	VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0, dp, 1, &dsl[3]};
	VkDescriptorSet ds; vkAllocateDescriptorSets(vkd, &dai, &ds);
	VkDescriptorBufferInfo dbi = {ubo, 0, 2048};
	VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, ds, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &dbi, 0};
	vkUpdateDescriptorSets(vkd, 1, &wds, 0, 0);
	VkShaderModule vs, fs;
	VkShaderModuleCreateInfo smi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, 0, 0, sizeof vert_spv, vert_spv};
	if (vkCreateShaderModule(vkd, &smi, 0, &vs)) return 0;
	smi.codeSize = (size_t)m->nwords * 4; smi.pCode = m->words;
	if (vkCreateShaderModule(vkd, &smi, 0, &fs)) return 0;
	VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", 0},
	                                         {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", 0}};
	VkPipelineVertexInputStateCreateInfo vin = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, 0, 0, 0, 0, 0, 0};
	VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, 0, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0};
	VkViewport vp = {0, 0, (float)W, (float)W, 0, 1}; VkRect2D sc = {{0, 0}, {W, W}};
	VkPipelineViewportStateCreateInfo vps = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, 0, 0, 1, &vp, 1, &sc};
	VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, 0, 0, 0, 0, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
	                                             VK_FRONT_FACE_COUNTER_CLOCKWISE, 0, 0, 0, 0, 1};
	VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, 0, 0, VK_SAMPLE_COUNT_1_BIT, 0, 0, 0, 0, 0};
	VkPipelineColorBlendAttachmentState cba = {0}; cba.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, 0, 0, 0, 0, 1, &cba, {0}};
	VkGraphicsPipelineCreateInfo gpi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, 0, 0, 2, st, &vin, &ia, 0, &vps, &rs, &ms, 0, &cb, 0, pl, rp, 0, 0, 0};
	VkPipeline pipe; if (vkCreateGraphicsPipelines(vkd, 0, 1, &gpi, 0, &pipe)) return 0;
	VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 0, 0, vkqf};
	VkCommandPool cp; vkCreateCommandPool(vkd, &cpi, 0, &cp);
	VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0, cp, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
	VkCommandBuffer cmd; vkAllocateCommandBuffers(vkd, &cai, &cmd);
	VkCommandBufferBeginInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, 0};
	vkBeginCommandBuffer(cmd, &cbi);
	VkClearValue clear; memset(&clear, 0, sizeof clear); clear.color.uint32[0] = 0x7FC00001u;   /* a NaN no program writes: an unshaded pixel fails */
	VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, 0, rp, fb, {{0, 0}, {W, W}}, 1, &clear};
	vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 3, 1, &ds, 0, 0);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRenderPass(cmd);
	VkBufferImageCopy bic = {0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {W, W, 1}};
	vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb, 1, &bic);
	VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, 0, 0, 0);
	vkEndCommandBuffer(cmd);
	VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 0, 0}; VkFence fence; vkCreateFence(vkd, &fci, 0, &fence);
	VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, 0, 0, 0, 0, 1, &cmd, 0, 0};
	if (vkQueueSubmit(vkq, 1, &si, fence)) return 0;
	vkWaitForFences(vkd, 1, &fence, 1, ~0ull);
	void *px; vkMapMemory(vkd, rm, 0, W * W * 16, 0, &px);
	memcpy(out, (uint8_t *)px + (1 * W + 1) * 16, 16);
	vkDeviceWaitIdle(vkd);
	return 1;   /* (the objects stay: a test process, a handful of programs) */
}
#endif

/* ---- the programs ---- */
typedef struct {
	const char *name; uint64_t w[8]; int n;
	int want_split[4];   /* TEMP, PA, O, FPINTERNAL with the split on: 1 Function variables, 0 an array */
	int lerp;            /* translate as the blend-lerp variant (fetch mode 4: the program twice, colour 0 then 1) */
} Prog;

static void interp(UsseProgram *p, const uint32_t sa[256], uint32_t pre, uint32_t out[4]) {
	UsseInst inst; memset(&inst, 0, sizeof inst);
	UsseDraw draw; memset(&draw, 0, sizeof draw);
	for (int k = 0; k < 256; k++) draw.sa[k].u = sa[k];
	for (int k = 0; k < 4; k++) inst.pa[8 + k].u = pre;
	usse_run_primary(p, &inst, &draw, 0, 0);
	for (int k = 0; k < 4; k++) out[k] = inst.pa[8 + k].u;
}
static float f_of(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

int main(void) {
	recomp_mem = mmap(0, 1ull << 32, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (recomp_mem == MAP_FAILED) { perror("mmap"); return 2; }
	uint32_t sa[256] = {0};
	for (int k = 0; k < 16; k++) sa[k] = 0x3F800000u + 0x00012345u * (uint32_t)(k + 1);   /* finite floats 1.0x .. */
	for (int k = 4; k < 8; k++) sa[k] = 0x00001111u * (uint32_t)(k - 3);                   /* small mantissa masks to XOR */
	sa[8] = 0x00000F0Fu; sa[9] = 0x000F0F00u; sa[10] = 0xFFFFF0FFu; sa[11] = 0xFFFF0FFFu;
	sa[20] = 3; sa[21] = 1;   /* the index registers' runtime values */
	const Prog progs[] = {
		{"no INDEXED (TEMP, O, FPINTERNAL, PA)", {
			vbw(OR, 4, TEMP(0), SA(0), IMM(0)),          /* r0..3 = sa0..3 */
			vbw(XOR, 4, OUT(4), TEMP(0), SA(4)),         /* o4..7 = r0..3 ^ sa4..7 */
			vbw(OR, 2, FPI(0), OUT(4), IMM(0)),          /* i0, i1 (words 0 and 4) = o4, o5 */
			vbw(OR, 2, PA(8), FPI(0), IMM(0x10)),        /* pa8, pa9 = i0, i1 | 0x10 */
			vbw(AND, 2, PA(10), OUT(6), SA(10)),         /* pa10, pa11 = o6, o7 & sa10, sa11 */
		}, 5, {1, 1, 1, 1}, 0},
		{"INDEXED1 TEMP read (idx1 from SA)", {
			vbw(OR, 16, TEMP(0), SA(0), IMM(0)),         /* r0..15 = sa0..15 */
			vbw(OR, 1, IDXREG(1), SA(20), IMM(0)),       /* idx1 = sa20 (3) */
			vbw(OR, 4, PA(8), IDX1_SRC(0, 2), IMM(0)),   /* pa8..11 = r[idx1*2 + 2 ..] = r8..11 */
		}, 3, {0, 1, 1, 1}, 0},
		{"INDEXED2 O write (idx2 from SA)", {
			vbw(OR, 8, TEMP(0), SA(0), IMM(0)),          /* r0..7 = sa0..7 */
			vbw(OR, 1, IDXREG(2), SA(21), IMM(0)),       /* idx2 = sa21 (1) */
			vbw(XOR, 4, IDX2_DST(1, 4), TEMP(0), SA(4)), /* o[idx2*2 + 4 ..] = o6..9 = r0..3 ^ sa4..7 */
			vbw(OR, 4, PA(8), OUT(6), IMM(0)),           /* pa8..11 = o6..9 */
		}, 4, {1, 1, 0, 1}, 0},
		{"blend-lerp variant (two runs, split words restored)", {
			vbw(OR, 4, TEMP(0), PA(8), TEMP(5)),         /* r0..3 = the colour register (preloaded 0, then 1.0) | r5..8 (still 0) */
			vbw(XOR, 4, OUT(0), TEMP(0), SA(4)),         /* o0..3 = r ^ sa4..7 */
			vbw(OR, 4, PA(8), OUT(0), IMM(0)),           /* the colour = o0..3 */
			vbw(OR, 4, TEMP(5), SA(0), IMM(0)),          /* r5..8 = sa0..3, after they were read: a second run that did not
			                                              * restore them would OR these bits into its colour */
		}, 4, {1, 1, 1, 1}, 1},
	};
#ifdef USSE_REGSPLIT_VK
	int vk = vk_init();
	printf("Vulkan device: %s\n", vk ? vk_name : "(none: GPU execution skipped)");
#endif
	for (unsigned pi = 0; pi < sizeof progs / sizeof progs[0]; pi++) {
		const Prog *pr = &progs[pi]; char what[200];
		uint32_t want[4];
		{	UsseProgram *p = load(pr->w, pr->n);
			int n = 0; usse_program_code(p, 0, &n);
			snprintf(what, sizeof what, "[%s] decodes %d instructions", pr->name, pr->n); ck(n == pr->n, what);
			if (pr->lerp) {   /* gfx_gpu.c's ONE / ONE_MINUS_SRC_ALPHA: (X.rgb, 1 - mean(out(1) - out(0))) */
				uint32_t x[4], y[4]; interp(p, sa, 0, x); interp(p, sa, 0x3F800000u, y);
				float k = 0; for (int c = 0; c < 3; c++) k += f_of(y[c]) - f_of(x[c]);
				float a = 1.0f - k * (1.0f / 3.0f);
				want[0] = x[0]; want[1] = x[1]; want[2] = x[2]; memcpy(&want[3], &a, 4);
			} else interp(p, sa, 0, want);
			usse_program_free(p);
		}
		uint32_t got[2][4]; int ran[2] = {0, 0};
		for (int split = 1; split >= 0; split--) {
			setenv("RECOMP_GPU_REGSPLIT", split ? "1" : "0", 1);
			UsseProgram *p = load(pr->w, pr->n);
			UsseFragLayout fl; memset(&fl, 0, sizeof fl);
			fl.color_pa_start = 8; fl.color_type = TY_F32; fl.color_comps = 4; fl.frag_color_used = pr->lerp;
			uint32_t tex[256] = {0}; int units[16] = {0}, nsmp = 0;
			usse_spirv_set_canonical_varyings(1);   /* (as gfx_gpu.c: no vertex layout needed for a fragment module) */
			usse_spirv_set_fetch_zero(pr->lerp ? 2 : 0);
			SpirvModule m = usse_spirv_translate(p, 0, &fl, tex, 16, units, &nsmp, 2 /*RGBA32F*/, 0, 0, 0);
			usse_spirv_set_fetch_zero(0);
			snprintf(what, sizeof what, "[%s] split %s: translates", pr->name, split ? "on " : "off");
			ck(m.ok && m.nwords > 0, what);
			if (!m.ok) { printf("    err: %s\n", m.err); usse_program_free(p); continue; }
			int fv, pv; count_vars(&m, &fv, &pv);
			int want_arrays = 2;   /* idx1/idx2 and the predicates are always Private arrays */
			for (int b = 0; b < 4; b++) want_arrays += split ? !pr->want_split[b] : 1;
			snprintf(what, sizeof what, "[%s] split %s: %d Private arrays (want %d), %d Function words%s", pr->name, split ? "on " : "off", pv, want_arrays, fv, split ? "" : " (want 0)");
			ck(pv == want_arrays && (split ? fv > 0 : fv == 0), what);
			char nm[64]; snprintf(nm, sizeof nm, "p%u_%s", pi, split ? "split" : "arrays");
			int v = spirv_val(&m, nm);
			if (v >= 0) { snprintf(what, sizeof what, "[%s] split %s: spirv-val accepts the module (%d words)", pr->name, split ? "on " : "off", m.nwords); ck(v == 1, what); }
#ifdef USSE_REGSPLIT_VK
			if (vk) {
				ran[split] = vk_run(&m, sa, got[split]);
				snprintf(what, sizeof what, "[%s] split %s: GPU pixel %08x %08x %08x %08x, interpreter %08x %08x %08x %08x", pr->name, split ? "on " : "off",
				         got[split][0], got[split][1], got[split][2], got[split][3], want[0], want[1], want[2], want[3]);
				int same = ran[split] && got[split][0] == want[0] && got[split][1] == want[1] && got[split][2] == want[2] &&
				           (pr->lerp ? fabsf(f_of(got[split][3]) - f_of(want[3])) <= 1e-6f : got[split][3] == want[3]);
				ck(same, what);
			}
#endif
			usse_spirv_free(&m);
			usse_program_free(p);
		}
		if (ran[0] && ran[1]) { snprintf(what, sizeof what, "[%s] split on and off give the same pixel bits", pr->name); ck(!memcmp(got[0], got[1], 16), what); }
	}
	unsetenv("RECOMP_GPU_REGSPLIT");
	printf("%d checks, %d failed\n", checks, failed);
	return failed ? 1 : 0;
}
