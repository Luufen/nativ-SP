/* The rules for an imported Vulkan driver package: an AdrenoTools ZIP (meta.json naming one ARM64 library). Community
 * Android builds of Mesa's PanVK (Arm Mali) and RADV (Samsung Xclipse) use the same layout. The AdrenoTools fields of
 * meta.json don't name the GPU a driver is made for, so the family is read from the library's name, then the package's
 * name; a package that names none is taken to be for Adreno, the format's origin. No Android classes
 * (dev/test_android_driver.py runs this on a desktop JDK). */
package org.projectnativ.app;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

final class DriverPackage {
    /* driver/gpu_family.h's numbers */
    static final int UNKNOWN = 0;
    static final int ADRENO = 1;
    static final int MALI = 2;
    static final int XCLIPSE = 3;

    static final long MAX_PACKAGE = 256L << 20;
    static final long MAX_META = 64L << 10;
    static final String META = "meta.json";

    private DriverPackage() { }

    static boolean libraryName(String name) {
        return name.matches("[A-Za-z0-9_][A-Za-z0-9_.+-]{0,126}\\.so(\\.[0-9]+)*");
    }

    /* the family one name speaks of, UNKNOWN for none. Whole words only ("normal" holds "mali"). RADV's before
     * freedreno's: a RADV build for another launcher is named libvulkan_freedreno_radv.so. */
    private static int named(String text) {
        List<String> words = Arrays.asList(text.toLowerCase(Locale.ROOT).split("[^a-z]+"));
        if (has(words, "panfrost", "panvk", "mali", "immortalis")) return MALI;
        if (has(words, "radv", "radeon", "xclipse")) return XCLIPSE;
        if (has(words, "freedreno", "turnip", "adreno", "qualcomm")) return ADRENO;
        return UNKNOWN;
    }

    private static boolean has(List<String> words, String... any) {
        for (String word : any) if (words.contains(word)) return true;
        return false;
    }

    /* the family the package says it is for, UNKNOWN when it says none */
    static int declaredFamily(String library, String name) {
        int family = named(library);
        return family != UNKNOWN ? family : named(name);
    }

    /* the family the package is treated as */
    static int family(String library, String name) {
        int family = declaredFamily(library, name);
        return family != UNKNOWN ? family : ADRENO;
    }

    static String familyName(int family) {
        switch (family) {
            case ADRENO: return "Qualcomm Adreno";
            case MALI: return "Arm Mali";
            case XCLIPSE: return "Samsung Xclipse";
            default: return "unrecognised";
        }
    }

    /* null when a driver with these names may be used on a device of this GPU family, else why not */
    static String refusal(String library, String name, int device) {
        if (device == UNKNOWN) return "Custom Vulkan drivers can't be used on this device's GPU.";
        if (family(library, name) == device) return null;
        String here = " This device has a " + familyName(device) + " GPU.";
        int declared = declaredFamily(library, name);
        if (declared != UNKNOWN) return "This driver is for " + familyName(declared) + " GPUs." + here;
        return "This package doesn't name a GPU, so it counts as a Qualcomm Adreno driver." + here;
    }

    /* the unpacked package: its library is there, and this Android is new enough */
    static void checkMeta(File directory, String library, int minApi, int sdk) throws IOException {
        if (!libraryName(library) || !new File(directory, library).isFile())
            throw new IOException("The Vulkan library listed in meta.json is missing or invalid.");
        if (minApi > sdk)
            throw new IOException("This driver requires a newer Android version (API level " + minApi + ").");
    }

    /* meta.json and the libraries at the ZIP's top level into stage, as plain files */
    static void unpack(File archive, File stage) throws IOException {
        long remaining = MAX_PACKAGE;
        int count = 0;
        HashSet<String> names = new HashSet<>();
        try (ZipFile zip = new ZipFile(archive)) {
            Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String name = entry.getName();
                if (++count > 256 || name.isEmpty() || name.startsWith("/") || name.indexOf('\\') >= 0 ||
                    name.indexOf(':') >= 0 || !names.add(name))
                    throw new IOException("Invalid driver archive entries.");
                for (String part : name.split("/"))
                    if (part.equals("..") || part.equals(".")) throw new IOException("Invalid driver archive path.");
                if (entry.isDirectory() || (!name.equals(META) && !libraryName(name))) continue;
                long limit = name.equals(META) ? Math.min(MAX_META, remaining) : remaining;
                try (InputStream input = zip.getInputStream(entry)) {
                    remaining -= copy(input, new File(stage, name), limit);
                }
            }
        }
    }

    static long copy(InputStream input, File dest, long limit) throws IOException {
        long size = 0;
        byte[] buffer = new byte[64 << 10];
        try (FileOutputStream output = new FileOutputStream(dest)) {
            for (int count; (count = input.read(buffer)) != -1;) {
                size += count;
                if (size > limit) throw new IOException("The driver package is too large.");
                output.write(buffer, 0, count);
            }
        }
        return size;
    }

    static void checkArm64(File library) throws IOException {
        try (InputStream input = new FileInputStream(library)) {
            byte[] header = new byte[20];
            int offset = 0;
            while (offset < header.length) {
                int read = input.read(header, offset, header.length - offset);
                if (read < 0) break;
                offset += read;
            }
            if (offset != header.length || header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' ||
                header[3] != 'F' || header[4] != 2 || header[5] != 1 || header[16] != 3 ||
                header[17] != 0 || (header[18] & 0xff) != 183 || header[19] != 0)
                throw new IOException("The driver does not contain an ARM64 Vulkan shared library.");
        }
    }
}
