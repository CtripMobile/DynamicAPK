package ctrip.android.bundle.framework.storage;

import android.content.res.AssetManager;
import android.os.Build;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import ctrip.android.bundle.hack.SysHacks;
import ctrip.android.bundle.loader.BundlePathLoader;
import ctrip.android.bundle.log.Logger;
import ctrip.android.bundle.log.LoggerFactory;
import ctrip.android.bundle.runtime.RuntimeArgs;
import ctrip.android.bundle.util.APKUtil;
import ctrip.android.bundle.util.StringUtil;

/**
 * Created by yb.wang on 14/12/31.
 * <p/>
 * Bundle 存储文件：bundle.zip，bundle.dex
 * 采用DexFile 加载 dex文件，并opt释放优化后的dex
 * findClass会被BundleClassLoader调用
 */
public class BundleArchiveRevision {
    static final Logger log;
    static final String BUNDLE_FILE_NAME = "bundle.zip";
    static final String BUNDEL_DEX_FILE = "bundle.dex";
    static final String FILE_PROTOCOL = "file:";
    static final String REFERENCE_PROTOCOL = "reference:";
    private final long revisionNum;
    private File revisionDir;
    private File bundleFile;
    private String revisionLocation;

    static {
        log = LoggerFactory.getLogcatLogger("BundleArchiveRevision");
    }


    BundleArchiveRevision(long revisionNumber, File file, InputStream inputStream) throws IOException {
        this.revisionNum = revisionNumber;
        this.revisionDir = file;
        if (!this.revisionDir.exists()) {
            this.revisionDir.mkdirs();
        }
        this.revisionLocation = FILE_PROTOCOL;
        this.bundleFile = new File(file, BUNDLE_FILE_NAME);
        APKUtil.copyInputStreamToFile(inputStream, this.bundleFile);
        updateMetaData();

    }

    BundleArchiveRevision(long revisionNumber, File file, File file2) throws IOException {
        this.revisionNum = revisionNumber;
        this.revisionDir = file;
        if (!this.revisionDir.exists()) {
            this.revisionDir.mkdirs();
        }
        if (file2.canWrite()) {
            if (isSameDir(file, file2)) {
                this.revisionLocation = FILE_PROTOCOL;
                this.bundleFile = new File(file, BUNDLE_FILE_NAME);
                file2.renameTo(this.bundleFile);
            } else {
                this.revisionLocation = FILE_PROTOCOL;
                this.bundleFile = new File(file, BUNDLE_FILE_NAME);
                APKUtil.copyInputStreamToFile(new FileInputStream(file2), this.bundleFile);
            }
        } else if (Build.HARDWARE.toLowerCase().contains("mt6592") && file2.getName().endsWith(".apk")) {
            this.revisionLocation = FILE_PROTOCOL;
            this.bundleFile = new File(file, BUNDLE_FILE_NAME);
            Runtime.getRuntime().exec(String.format("ln -s %s %s", new Object[]{file2.getAbsolutePath(), this.bundleFile.getAbsolutePath()}));
        } else if (SysHacks.LexFile == null || SysHacks.LexFile.getmClass() == null) {
            this.revisionLocation = REFERENCE_PROTOCOL + file2.getAbsolutePath();
            this.bundleFile = file2;
        } else {
            this.revisionLocation = FILE_PROTOCOL;
            this.bundleFile = new File(file, BUNDLE_FILE_NAME);
            APKUtil.copyInputStreamToFile(new FileInputStream(file2), this.bundleFile);
        }
        updateMetaData();
    }

    BundleArchiveRevision(long revisionNumber, File file) throws IOException {
        File fileMeta = new File(file, "meta");
        if (fileMeta.exists()) {
            DataInputStream dataInputStream = new DataInputStream(new FileInputStream(fileMeta));
            this.revisionLocation = dataInputStream.readUTF();
            dataInputStream.close();
            this.revisionNum = revisionNumber;
            this.revisionDir = file;
            if (!this.revisionDir.exists()) {
                this.revisionDir.mkdirs();
            }
            if (this.revisionLocation.startsWith(REFERENCE_PROTOCOL)) {
                this.bundleFile = new File(StringUtil.subStringAfter(this.revisionLocation, REFERENCE_PROTOCOL));
                return;
            } else {
                this.bundleFile = new File(file, BUNDLE_FILE_NAME);
                return;
            }
        }
        throw new IOException("Can not find meta file in " + file.getAbsolutePath());
    }

    void updateMetaData() throws IOException {

        File file = new File(this.revisionDir, "meta");
        DataOutputStream dataOutputStream = null;
        try {
            if (!file.getParentFile().exists()) {
                file.getParentFile().mkdirs();
            }
            dataOutputStream = new DataOutputStream(new FileOutputStream(file));
            dataOutputStream.writeUTF(this.revisionLocation);
            dataOutputStream.flush();

        } catch (IOException ex) {
            throw new IOException("Can not save meta data " + file.getAbsolutePath());
        } finally {
            if (dataOutputStream != null) dataOutputStream.close();
        }
    }

    private boolean isSameDir(File file, File file2) {
        return StringUtil.equals(StringUtil.subStringBetween(file.getAbsolutePath(), File.separator, File.separator),
                StringUtil.subStringBetween(file2.getAbsolutePath(), File.separator, File.separator));
    }

    public long getRevisionNum() {
        return this.revisionNum;
    }

    public File getRevisionDir() {
        return this.revisionDir;
    }

    public File getRevisionFile() {
        return this.bundleFile;
    }

    public boolean isDexOpted() {
        return new File(this.revisionDir, BUNDEL_DEX_FILE).exists();
    }

    public boolean isBundleInstalled(){
        if(bundleFile.exists()){
          return verifyZipFile(bundleFile);
        }
        return false;
    }
    private boolean verifyZipFile(File file) {
        try {
            ZipFile zipFile = new ZipFile(file);
            try {
                zipFile.close();
                return true;
            } catch (IOException e) {
                log.log("Failed to close zip file: " + file.getAbsolutePath(), Logger.LogLevel.ERROR,e);
            }
        } catch (ZipException ex) {
            log.log("File " + file.getAbsolutePath() + " is not a valid zip file.", Logger.LogLevel.ERROR,ex);
        } catch (IOException ex) {
            log.log("Got an IOException trying to open zip file: " + file.getAbsolutePath(), Logger.LogLevel.ERROR,ex);
        }
        return false;
    }

    public void optDexFile() throws Exception{
            List<File> files = new ArrayList<File>();
            files.add(this.bundleFile);
            BundlePathLoader.installBundleDexs(RuntimeArgs.androidApplication.getClassLoader(), revisionDir, files,false);
    }

    public InputStream openAssetInputStream(String fileName) throws IOException {
        try {
            AssetManager assetManager = AssetManager.class.newInstance();
            if (((Integer) SysHacks.AssetManager_addAssetPath.invoke(assetManager, this.bundleFile.getAbsoluteFile())).intValue() != 0) {
                return assetManager.open(fileName);
            }
        } catch (InstantiationException e) {
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
        return null;
    }

    public InputStream openNonAssetInputStream(String fileName) throws IOException {
        try {
            AssetManager assetManager = AssetManager.class.newInstance();
            int intValue = ((Integer) SysHacks.AssetManager_addAssetPath.invoke(assetManager, this.bundleFile.getAbsoluteFile())).intValue();
            if (intValue != 0) {
                return assetManager.openNonAssetFd(intValue, fileName).createInputStream();
            }
        } catch (InstantiationException e) {
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
        return null;
    }


}
