/* dev/test_android_driver.py's checks of runtime/platform/android/DriverPackage.java, on synthetic ZIPs: made-up
 * entry names and a 64-byte ELF header, no driver. argv[0]: an empty folder to work in. */
package org.projectnativ.app;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public final class DriverPackageTest {
    private static int checks, failed, next;
    private static File root;

    private static void check(String what, boolean ok) {
        checks++;
        if (!ok) failed++;
        System.out.println(what + (ok ? "  ok" : "  FAILED"));
    }

    private static byte[] elf(int machine) {
        byte[] h = new byte[64];
        h[0] = 0x7f; h[1] = 'E'; h[2] = 'L'; h[3] = 'F'; h[4] = 2; h[5] = 1; h[16] = 3; h[18] = (byte) machine;
        return h;
    }

    private static File folder() throws IOException {
        File f = new File(root, "d" + next++);
        if (!f.mkdirs()) throw new IOException("mkdirs " + f);
        return f;
    }

    /* pairs of entry name and content */
    private static File zip(Object... entries) throws IOException {
        File f = new File(root, "z" + next++ + ".zip");
        try (ZipOutputStream out = new ZipOutputStream(new FileOutputStream(f))) {
            for (int i = 0; i < entries.length; i += 2) {
                out.putNextEntry(new ZipEntry((String) entries[i]));
                out.write((byte[]) entries[i + 1]);
                out.closeEntry();
            }
        }
        return f;
    }

    /* what the import does with a package, short of reading meta.json (Android's JSON) */
    private static String take(File zip, String library, String name, int minApi, int device) {
        try {
            File stage = folder();
            DriverPackage.unpack(zip, stage);
            if (!new File(stage, DriverPackage.META).isFile()) return "Missing or invalid meta.json.";
            DriverPackage.checkMeta(stage, library, minApi, 33);
            DriverPackage.checkArm64(new File(stage, library));
            return DriverPackage.refusal(library, name, device);
        } catch (IOException e) {
            return e.getMessage();
        }
    }

    private static boolean has(String message, String part) {
        return message != null && message.contains(part);
    }

    public static void main(String[] args) throws Exception {
        root = new File(args[0]);
        byte[] meta = "{}".getBytes(StandardCharsets.UTF_8);
        byte[] arm = elf(183);
        final int adreno = DriverPackage.ADRENO, mali = DriverPackage.MALI, xclipse = DriverPackage.XCLIPSE;

        /* the layouts of the surveyed packages (docs: meta.json + one library, a notice beside them) */
        String panvk = "libvulkan_panfrost.so", radv = "vulkan.radeon.so", turnip = "vulkan.ad07xx.so";
        File p = zip("meta.json", meta, panvk, arm, "NOTICE.txt", meta);
        File r = zip("meta.json", meta, radv, arm, "NOTICE.txt", meta);
        File t = zip("meta.json", meta, turnip, arm);
        check("PanVK layout accepted on Mali", take(p, panvk, "PanVK Mali (kbase)", 31, mali) == null);
        check("RADV layout accepted on Xclipse", take(r, radv, "RADV Xclipse", 33, xclipse) == null);
        check("Turnip layout accepted on Adreno", take(t, turnip, "Turnip", 0, adreno) == null);
        check("PanVK refused on Adreno, both families named",
              has(take(p, panvk, "PanVK Mali (kbase)", 31, adreno), "for Arm Mali GPUs. This device has a Qualcomm Adreno"));
        check("PanVK refused on Xclipse", has(take(p, panvk, "PanVK", 31, xclipse), "This device has a Samsung Xclipse"));
        check("RADV refused on Mali", has(take(r, radv, "RADV Xclipse", 33, mali), "for Samsung Xclipse GPUs"));
        check("Turnip refused on Mali", has(take(t, turnip, "Turnip", 0, mali), "for Qualcomm Adreno GPUs"));
        check("Turnip refused on Xclipse", has(take(t, turnip, "Turnip", 0, xclipse), "This device has a Samsung Xclipse"));
        check("no family on an unrecognised GPU", has(take(t, turnip, "Turnip", 0, DriverPackage.UNKNOWN), "can't be used"));

        File plain = zip("meta.json", meta, "vulkan.custom.so", arm);
        check("a package naming no GPU: taken on Adreno, as before",
              take(plain, "vulkan.custom.so", "My driver v2", 0, adreno) == null);
        check("a package naming no GPU: refused on Mali, saying why",
              has(take(plain, "vulkan.custom.so", "My driver v2", 0, mali), "doesn't name a GPU"));

        check("the library name decides over the package name",
              DriverPackage.family("libvulkan_panfrost.so", "Better than Turnip") == mali);
        check("a plain library name: the package name decides",
              DriverPackage.family("libvulkan.so", "Mesa PanVK 26.3.1") == mali
              && DriverPackage.family("vulkan.samsung.so", "Vortek Xclipse Native") == xclipse);
        check("libvulkan_freedreno_radv.so is RADV",
              DriverPackage.family("libvulkan_freedreno_radv.so", "x") == xclipse);
        check("words only: 'Normalized' is not Mali, 'Custom_RADV_Xclipse540' is Xclipse",
              DriverPackage.declaredFamily("vulkan.x.so", "Normalized build") == DriverPackage.UNKNOWN
              && DriverPackage.family("vulkan.x.so", "Custom_RADV_Xclipse540") == xclipse);

        check("no meta.json refused", has(take(zip(panvk, arm), panvk, "PanVK", 0, mali), "meta.json"));
        check("meta.json's library missing refused",
              has(take(zip("meta.json", meta), panvk, "PanVK", 0, mali), "missing or invalid"));
        check("a library only in a subfolder refused",
              has(take(zip("meta.json", meta, "lib/" + panvk, arm), panvk, "PanVK", 0, mali), "missing or invalid"));
        check("a library name with a path refused",
              has(take(p, "../" + panvk, "PanVK", 0, mali), "missing or invalid"));
        check("a path leaving the folder refused",
              has(take(zip("meta.json", meta, "../" + panvk, arm), panvk, "PanVK", 0, mali), "Invalid driver archive path"));
        check("a backslash entry refused",
              has(take(zip("meta.json", meta, "a\\b.so", arm), panvk, "PanVK", 0, mali), "Invalid driver archive entries"));
        check("an x86-64 library refused",
              has(take(zip("meta.json", meta, panvk, elf(62)), panvk, "PanVK", 0, mali), "ARM64"));
        check("a file that is no ELF refused",
              has(take(zip("meta.json", meta, panvk, meta), panvk, "PanVK", 0, mali), "ARM64"));
        check("minApi above this Android refused, the level named", has(take(r, radv, "RADV", 34, xclipse), "API level 34"));
        check("an oversized meta.json refused",
              has(take(zip("meta.json", new byte[(int) DriverPackage.MAX_META + 1], panvk, arm), panvk, "PanVK", 0, mali),
                  "too large"));

        System.out.println(checks + " checks, " + (failed == 0 ? "all passed" : failed + " failed"));
        System.exit(failed == 0 ? 0 : 1);
    }
}
