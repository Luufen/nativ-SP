package org.projectnativ.app;

import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.UUID;

final class DriverStore {
    static final String SYSTEM = "System driver";
    static final String EXTRA_DRIVER = "org.projectnativ.app.DRIVER";
    static final String LOADER = "libnativ_vulkan.so";
    private static final String TAG = "Nativ";
    private final Context context;
    private final File root;

    DriverStore(Context context) {
        this.context = context.getApplicationContext();
        root = new File(context.getFilesDir(), "gpu-drivers");
    }

    // DriverPackage's family numbers. activate() succeeds only when the loaded driver reports that family.
    static native int systemFamily();
    static native boolean activate(String hooks, String directory, String filename, int family);

    /* The system GPU's family, UNKNOWN when custom drivers can't be used here. Not for the :game process: it loads the
     * system driver, and libadrenotools notes its hooks aren't applied once a process already has a driver loaded. */
    int deviceFamily() {
        if (!new File(context.getApplicationInfo().nativeLibraryDir, LOADER).isFile()) return DriverPackage.UNKNOWN;
        try {
            System.loadLibrary("nativ_vulkan");
            return systemFamily();
        } catch (LinkageError e) {
            Log.w(TAG, "Custom Vulkan support unavailable", e);
            return DriverPackage.UNKNOWN;
        }
    }

    static final class Driver {
        final String id;
        final File directory;
        final String library;
        final String label;
        final String name;
        final int family;

        Driver(File directory, String library, String label, String name) {
            this.id = directory.getName();
            this.directory = directory;
            this.library = library;
            this.label = label;
            this.name = name;
            this.family = DriverPackage.family(library, name);
        }
    }

    private static Driver read(File directory) throws Exception {
        File meta = new File(directory, DriverPackage.META);
        if (!meta.isFile() || meta.length() > DriverPackage.MAX_META)
            throw new IOException("Missing or invalid meta.json.");
        JSONObject json = new JSONObject(new String(Files.readAllBytes(meta.toPath()), StandardCharsets.UTF_8));
        String library = json.getString("libraryName");
        DriverPackage.checkMeta(directory, library, json.optInt("minApi", 0), Build.VERSION.SDK_INT);
        String name = json.getString("name").replaceAll("[\\p{Cntrl}\\p{Cf}]", " ").trim();
        String version = json.optString("driverVersion").replaceAll("[\\p{Cntrl}\\p{Cf}]", " ").trim();
        String label = version.isEmpty() ? name : name + "  (" + version + ")";
        if (name.isEmpty() || label.length() > 180) throw new IOException("Invalid driver name.");
        return new Driver(directory, library, label, name);
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
                DriverPackage.copy(input, archive, DriverPackage.MAX_PACKAGE);
            }
            DriverPackage.unpack(archive, stage);
            Files.delete(archive.toPath());
            Driver driver = read(stage);
            DriverPackage.checkArm64(new File(stage, driver.library));
            String refusal = DriverPackage.refusal(driver.library, driver.name, deviceFamily());
            if (refusal != null) throw new IOException(refusal);
            File installed = new File(root, UUID.randomUUID().toString());
            if (!stage.renameTo(installed)) throw new IOException("Could not install the driver.");
            return new Driver(installed, driver.library, driver.label, driver.name);
        } finally {
            delete(stage);
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
