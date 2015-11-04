package ctrip.android.bundle.framework;


import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import ctrip.android.bundle.framework.storage.Archive;
import ctrip.android.bundle.framework.storage.BundleAchive;
import ctrip.android.bundle.log.Logger;
import ctrip.android.bundle.log.LoggerFactory;

/**
 * Created by yb.wang on 14/12/31.
 * Bundle接口实现类，管理Bundle的生命周期。
 * meta文件存储BundleId，Location等
 */
public final class BundleImpl implements Bundle {
    static final Logger log;
    Archive archive;
    final File bundleDir;
    final String location;
    final long bundleID;
    int state;
    //是否dex优化
    volatile boolean isOpt;

    static {
        log = LoggerFactory.getLogcatLogger("BundleImpl");
    }

    BundleImpl(File bundleDir) throws Exception {
        this.isOpt = false;


        DataInputStream dataInputStream = new DataInputStream(new FileInputStream(new File(bundleDir, "meta")));
        this.bundleID = dataInputStream.readLong();
        this.location = dataInputStream.readUTF();

        dataInputStream.close();


        this.bundleDir = bundleDir;
        try {
            this.archive = new BundleAchive(bundleDir);
            Framework.bundles.put(this.location, this);


        } catch (Exception e) {
            new BundleException("Could not load bundle " + this.location, e.getCause());
        }
    }

    BundleImpl(File bundleDir, String location, long bundleID, InputStream inputStream) throws BundleException {
        this.isOpt = false;
        this.bundleID = bundleID;
        this.location = location;
        this.bundleDir = bundleDir;
        if (inputStream == null) {
            throw new BundleException("Arg InputStream is null.Bundle:" + location);

        } else {
            try {
                this.archive = new BundleAchive(bundleDir, inputStream);
            } catch (Exception e) {
                Framework.deleteDirectory(bundleDir);
                throw new BundleException("Can not install bundle " + location, e);
            }
        }
        this.updateMetadata();
        Framework.bundles.put(location, this);

    }


    public boolean getIsOpt() {
        return this.isOpt;
    }

    public Archive getArchive() {
        return this.archive;
    }


    @Override
    public long getBundleId() {
        return this.bundleID;
    }

    @Override
    public String getLocation() {
        return this.location;
    }


    @Override
    public int getState() {
        return this.state;
    }


    @Override
    public synchronized void update(InputStream inputStream) throws BundleException {
        try {
            this.archive.newRevision(this.bundleDir, inputStream);
        } catch (Throwable e) {
            throw new BundleException("Could not update bundle " + toString(), e);
        }
    }


    public synchronized void optDexFile() throws Exception {
        if (!isOpt) {
            long startTime = System.currentTimeMillis();
            getArchive().optDexFile();
            isOpt = true;
            log.log("执行：" + getLocation() + ",时间-----" + String.valueOf(System.currentTimeMillis() - startTime), Logger.LogLevel.ERROR);
        }
    }

    public synchronized void purge() throws BundleException {
        try {
            getArchive().purge();
        } catch (Throwable e) {
            throw new BundleException("Could not purge bundle " + toString(), e);
        }
    }

    void updateMetadata() {
        File file = new File(this.bundleDir, "meta");
        DataOutputStream dataOutputStream;
        try {
            if (!file.getParentFile().exists()) {
                file.getParentFile().mkdirs();
            }
            FileOutputStream fileOutputStream = new FileOutputStream(file);
            dataOutputStream = new DataOutputStream(fileOutputStream);
            dataOutputStream.writeLong(this.bundleID);
            dataOutputStream.writeUTF(this.location);

            dataOutputStream.flush();
            fileOutputStream.getFD().sync();
            if (dataOutputStream != null)
                try {
                    dataOutputStream.close();
                } catch (IOException ex) {
                    ex.printStackTrace();
                }
        } catch (Throwable e) {
            log.log("Could not save meta data " + file.getAbsolutePath(), Logger.LogLevel.ERROR, e);
        }

    }

    public String toString() {
        return "Bundle [" + this.bundleID + "]: " + this.location;
    }

}
