package org.projectnativ.app;

import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.UUID;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

final class DriverStore {
    static final String SYSTEM = "System driver";
    static final String EXTRA_DRIVER = "org.projectnativ.app.DRIVER";
    static final String LOADER = "libnativ_vulkan.so";
    private static final String TAG = "Nativ";
    private static final long MAX_PACKAGE = 256L << 20;
    private static final long MAX_META = 64L << 10;
    private static final String META = "meta.json";
    private final Context context;
    private final File root;

    DriverStore(Context context) {
        this.context = context.getApplicationContext();
        root = new File(context.getFilesDir(), "gpu-drivers");
    }

    static native boolean supportsAdreno();
    static native boolean activate(String hooks, String directory, String filename);

    boolean supported() {
        if (!new File(context.getApplicationInfo().nativeLibraryDir, LOADER).isFile()) return false;
        try {
            System.loadLibrary("nativ_vulkan");
            return supportsAdreno();
        } catch (LinkageError e) {
            Log.w(TAG, "Custom Vulkan support unavailable", e);
            return false;
        }
    }

    static final class Driver {
        final String id;
        final File directory;
        final String library;
        final String label;

        Driver(File directory, String library, String label) {
            this.id = directory.getName();
            this.directory = directory;
            this.library = library;
            this.label = label;
        }
    }

    private static boolean libraryName(String name) {
        return name.matches("[A-Za-z0-9_][A-Za-z0-9_.+-]{0,126}\\.so(\\.[0-9]+)*");
    }

    private static Driver read(File directory) throws Exception {
        File meta = new File(directory, META);
        if (!meta.isFile() || meta.length() > MAX_META) throw new IOException("Missing or invalid meta.json.");
        JSONObject json = new JSONObject(new String(Files.readAllBytes(meta.toPath()), StandardCharsets.UTF_8));
        String library = json.getString("libraryName");
        if (!libraryName(library) || !new File(directory, library).isFile())
            throw new IOException("The Vulkan library listed in meta.json is missing or invalid.");
        if (json.optInt("minApi", 0) > Build.VERSION.SDK_INT)
            throw new IOException("This driver requires a newer Android version.");
        String name = json.getString("name").replaceAll("[\\p{Cntrl}\\p{Cf}]", " ").trim();
        String version = json.optString("driverVersion").replaceAll("[\\p{Cntrl}\\p{Cf}]", " ").trim();
        String label = version.isEmpty() ? name : name + "  (" + version + ")";
        if (name.isEmpty() || label.length() > 180) throw new IOException("Invalid driver name.");
        return new Driver(directory, library, label);
    }

    Driver find(String id) {
        if (id == null || !id.matches("[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}")) return null;
        try {
            return read(new File(root, id));
        } catch (Exception e) {
            Log.w(TAG, "Installed driver is unavailable", e);
            return null;
        }
    }

    List<Driver> list() {
        List<Driver> drivers = new ArrayList<>();
        String[] ids = root.list();
        if (ids != null) for (String id : ids) {
            Driver driver = find(id);
            if (driver != null) drivers.add(driver);
        }
        drivers.sort(Comparator.comparing((Driver driver) -> driver.label, String.CASE_INSENSITIVE_ORDER)
                               .thenComparing(driver -> driver.id));
        return drivers;
    }

    Driver selected() {
        return find(context.getSharedPreferences("gpu-driver", Context.MODE_PRIVATE).getString("driver", null));
    }

    boolean select(Driver driver) {
        return context.getSharedPreferences("gpu-driver", Context.MODE_PRIVATE).edit()
                .putString("driver", driver == null ? null : driver.id).commit();
    }

    Driver install(Uri uri) throws Exception {
        if (!root.isDirectory() && !root.mkdirs()) throw new IOException("Could not create driver storage.");
        File stage = Files.createTempDirectory(root.toPath(), ".import-").toFile();
        try {
            File archive = new File(stage, "package.zip");
            try (InputStream input = context.getContentResolver().openInputStream(uri)) {
                if (input == null) throw new IOException("Could not open the selected ZIP.");
                copy(input, archive, MAX_PACKAGE);
            }
            unpack(archive, stage);
            Files.delete(archive.toPath());
            Driver driver = read(stage);
            checkArm64(new File(stage, driver.library));
            File installed = new File(root, UUID.randomUUID().toString());
            if (!stage.renameTo(installed)) throw new IOException("Could not install the driver.");
            return new Driver(installed, driver.library, driver.label);
        } finally {
            delete(stage);
        }
    }

    private static void unpack(File archive, File stage) throws IOException {
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

    private static long copy(InputStream input, File dest, long limit) throws IOException {
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

    private static void checkArm64(File library) throws IOException {
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

    boolean remove(Driver driver) {
        Driver active = selected();
        if (active != null && active.id.equals(driver.id) && !select(null)) return false;
        File trash = new File(root, ".removed-" + driver.id);
        if (!driver.directory.renameTo(trash)) return false;
        delete(trash);
        return true;
    }

    // Only used on private directories created by this class. ZIP entries are written as plain files.
    private static void delete(File directory) {
        File[] files = directory.listFiles();
        if (files != null) for (File file : files) file.delete();
        directory.delete();
    }
}
