/* The Activity of a game exported by vitarecomp export-apk (pipeline/apk.py): SDL's own (SDLActivity: the window, input,
 * sound, the libraries SDL3 and main), plus the game's start as `vitarecomp run` gives it -- read from the APK's
 * assets/nativ/launch.txt:
 *   arg <value>          one argument of the game's main (its image, entry, import budget)
 *   env <NAME>=<value>   one setting of its environment
 * "$FILES" in a value is where the game's images are (the app's own files folder: assets/nativ/ copied there once per
 * APK), "$DATA" its data folder (the app's external files folder, Android/data/<package>/files: fs/app0 pushed there,
 * its saves, settings and logs). NATIV_DATA names the data folder for the game (runtime/core/main.c changes into it). */
package org.projectnativ.app;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.system.Os;
import android.util.Log;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

import org.libsdl.app.SDLActivity;

public class NativActivity extends SDLActivity {
    private static final String TAG = "Nativ";
    private final ArrayList<String> args = new ArrayList<>();

    @Override
    protected void onCreate(Bundle state) {
        try { prepare(); } catch (Exception e) { Log.e(TAG, "the game's start could not be prepared", e); }
        prepareDriver();
        super.onCreate(state);
    }

    private void prepareDriver() {
        String id = getIntent().getStringExtra(DriverStore.EXTRA_DRIVER);
        if (id == null) {
            Log.i(TAG, "Using the system Vulkan driver");
            return;
        }
        DriverStore.Driver driver = new DriverStore(this).find(id);
        try {
            if (driver != null) {
                String hooks = getApplicationInfo().nativeLibraryDir + File.separator;
                System.loadLibrary("nativ_vulkan");
                if (DriverStore.activate(hooks, driver.directory.getAbsolutePath() + File.separator,
                                         driver.library, driver.family)) {
                    Os.setenv("SDL_VULKAN_LIBRARY", hooks + DriverStore.LOADER, true);
                    Os.setenv("NATIV_CUSTOM_VULKAN", "1", true);
                    return;
                }
            }
        } catch (Exception | LinkageError e) {
            Log.w(TAG, "Could not start the selected Vulkan driver", e);
        }
        Log.w(TAG, "Custom Vulkan driver unavailable. Using the system Vulkan driver.");
        Toast.makeText(this, "Custom GPU driver unavailable. Using the system driver.", Toast.LENGTH_LONG).show();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // Vulkan loader and driver globals are scoped to :game. Restart that process for the next Play.
        if (isFinishing()) android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    protected String[] getArguments() { return args.toArray(new String[0]); }

    private static String read(InputStream in) throws Exception {
        StringBuilder s = new StringBuilder();
        try (BufferedReader r = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
            for (String l; (l = r.readLine()) != null; ) s.append(l).append('\n');
        }
        return s.toString();
    }

    private void prepare() throws Exception {
        AssetManager am = getAssets();
        File files = new File(getFilesDir(), "nativ");
        File ext = getExternalFilesDir(null);
        File data = ext != null ? ext : getFilesDir();
        new File(data, "fs").mkdirs();
        // the images once per APK: assets/nativ/stamp.txt names its build
        String stamp = read(am.open("nativ/stamp.txt"));
        File have = new File(files, "stamp.txt");
        if (!have.isFile() || !read(new java.io.FileInputStream(have)).equals(stamp)) {
            files.mkdirs();
            byte[] buf = new byte[1 << 20];
            for (String n : am.list("nativ")) {
                if (n.equals("stamp.txt")) continue;
                try (InputStream in = am.open("nativ/" + n); OutputStream out = new FileOutputStream(new File(files, n + ".part"))) {
                    for (int k; (k = in.read(buf)) > 0; ) out.write(buf, 0, k);
                }
                if (!new File(files, n + ".part").renameTo(new File(files, n))) throw new Exception("can't place " + n);
            }
            try (OutputStream out = new FileOutputStream(have)) { out.write(stamp.getBytes(StandardCharsets.UTF_8)); }
            Log.i(TAG, "the game's images copied to " + files);
        }
        Os.setenv("NATIV_DATA", data.getAbsolutePath(), true);
        for (String l : read(am.open("nativ/launch.txt")).split("\n")) {
            String v = l.length() > 4 ? l.substring(4).replace("$FILES", files.getAbsolutePath()).replace("$DATA", data.getAbsolutePath()) : "";
            if (l.startsWith("arg ")) args.add(v);
            else if (l.startsWith("env ") && v.indexOf('=') > 0) Os.setenv(v.substring(0, v.indexOf('=')), v.substring(v.indexOf('=') + 1), true);
        }
        Log.i(TAG, "data folder " + data + ", " + args.size() + " arguments");
    }
}
