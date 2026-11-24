package org.projectnativ.app;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

final class DiagnosticsExport {
    private static final String[] LOGS = {"session.log", "session.prev.log"};
    private static final Pattern IDENTITY = Pattern.compile("^([ \t]*(?:open_psid|online_id|np_[a-z_]*)[ \t]*=[ \t]*).*$");

    static boolean hasLogs(File data) {
        for (String name : LOGS) {
            if (new File(data, name).isFile()) return true;
        }
        return false;
    }

    static void save(Context context, File data, Uri destination, boolean crashReport) throws IOException {
        ContentResolver resolver = context.getContentResolver();
        try {
            String report = crashReport ? crashDetails(context) : null;
            File internalData = context.getDataDir();
            OutputStream output = resolver.openOutputStream(destination, "w");
            if (output == null) throw new IOException("No output stream from the file picker");
            writeArchive(output, data, internalData, report);
        } catch (IOException | RuntimeException e) {
            // Save As created this document. Remove it if the provider permits cleanup.
            try {
                DocumentsContract.deleteDocument(resolver, destination);
            } catch (IOException | RuntimeException cleanup) {
                e.addSuppressed(cleanup);
            }
            throw e;
        }
    }

    private static void writeArchive(OutputStream output, File data, File internalData, String report) throws IOException {
        try (ZipOutputStream zip = new ZipOutputStream(output)) {
            if (report != null) {
                zip.putNextEntry(new ZipEntry("report.txt"));
                zip.write(report.getBytes(StandardCharsets.UTF_8));
                zip.closeEntry();
            }
            boolean wroteLog = false;
            for (String name : LOGS) {
                File log = new File(data, name);
                if (!log.isFile()) continue;
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                        new FileInputStream(log), StandardCharsets.UTF_8))) {
                    ZipEntry entry = new ZipEntry(name);
                    entry.setTime(log.lastModified());
                    zip.putNextEntry(entry);
                    for (String line; (line = reader.readLine()) != null; ) {
                        line = line.replace(data.getAbsolutePath(), "~")
                                   .replace(internalData.getAbsolutePath(), "~");
                        line = IDENTITY.matcher(line).replaceFirst("$1<removed>");
                        zip.write(line.getBytes(StandardCharsets.UTF_8));
                        zip.write('\n');
                    }
                    zip.closeEntry();
                    wroteLog = true;
                }
            }
            if (!wroteLog && report == null) throw new IOException("No session logs found");
        }
    }

    private static String crashDetails(Context context) {
        return "Project Nativ Android crash report\n"
                + "Created: " + Instant.now() + "\n"
                + "Package: " + context.getPackageName() + "\n"
                + "Device: " + Build.MANUFACTURER + " " + Build.MODEL + "\n"
                + "Android: " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")\n\n"
                + lastAbnormalExit(context);
    }

    private static String lastAbnormalExit(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return "Android exit history requires Android 11 or newer.\n";
        ActivityManager manager = context.getSystemService(ActivityManager.class);
        if (manager == null) return "Android exit history is unavailable.\n";
        String gameProcess = context.getPackageName() + ":game";
        try {
            for (ApplicationExitInfo exit : manager.getHistoricalProcessExitReasons(context.getPackageName(), 0, 0)) {
                if (!gameProcess.equals(exit.getProcessName())) continue;
                String reason;
                switch (exit.getReason()) {
                    case ApplicationExitInfo.REASON_CRASH: reason = "Java crash"; break;
                    case ApplicationExitInfo.REASON_CRASH_NATIVE: reason = "Native crash"; break;
                    case ApplicationExitInfo.REASON_ANR: reason = "App not responding"; break;
                    case ApplicationExitInfo.REASON_LOW_MEMORY: reason = "Low memory"; break;
                    case ApplicationExitInfo.REASON_SIGNALED: reason = "Terminated by signal"; break;
                    case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE: reason = "Initialization failure"; break;
                    default: continue;
                }
                return "Latest abnormal game process exit\n"
                        + "Time: " + Instant.ofEpochMilli(exit.getTimestamp()) + "\n"
                        + "Reason: " + reason + "\n"
                        + "Status: " + exit.getStatus() + "\n";
            }
        } catch (RuntimeException e) {
            Log.w("Nativ", "Could not read Android exit history", e);
            return "Android exit history is unavailable.\n";
        }
        return "Android has no recorded abnormal exit for the game process.\n";
    }
}
