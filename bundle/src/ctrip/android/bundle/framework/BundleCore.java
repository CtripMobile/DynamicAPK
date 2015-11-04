package ctrip.android.bundle.framework;

import android.app.Application;
import android.content.res.Resources;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import ctrip.android.bundle.hack.AndroidHack;
import ctrip.android.bundle.hack.SysHacks;
import ctrip.android.bundle.log.Logger;
import ctrip.android.bundle.log.LoggerFactory;
import ctrip.android.bundle.runtime.BundleInstalledListener;
import ctrip.android.bundle.runtime.DelegateResources;
import ctrip.android.bundle.runtime.InstrumentationHook;
import ctrip.android.bundle.runtime.RuntimeArgs;

/**
 * Created by yb.wang on 15/1/5.
 * Bundle机制外部核心类
 * 采用单例模式封装了外部调用方法
 */
public class BundleCore {

    public static final String LIB_PATH = "assets/baseres/";
    protected static BundleCore instance;
    static final Logger log;
    private List<BundleInstalledListener> bundleDelayListeners;
    private List<BundleInstalledListener> bundleSyncListeners;

    static {
        log = LoggerFactory.getLogcatLogger("BundleCore");

    }

    private BundleCore() {
        bundleDelayListeners = new ArrayList<BundleInstalledListener>();
        bundleSyncListeners = new ArrayList<BundleInstalledListener>();
    }

    public static synchronized BundleCore getInstance() {
        if (instance == null)
            instance = new BundleCore();
        return instance;
    }


    public void ConfigLogger(boolean isOpenLog, int level) {
        LoggerFactory.isNeedLog = isOpenLog;
        LoggerFactory.minLevel = Logger.LogLevel.getValue(level);
    }

    public void init(Application application) throws Exception {
        SysHacks.defineAndVerify();
        RuntimeArgs.androidApplication = application;
        RuntimeArgs.delegateResources = application.getResources();
        AndroidHack.injectInstrumentationHook(new InstrumentationHook(AndroidHack.getInstrumentation(), application.getBaseContext()));
    }

    public void startup(Properties properties) {
        try {
            Framework.startup(properties);
        } catch (Exception e) {
            log.log("Bundle Dex installation failure", Logger.LogLevel.ERROR, e);
            throw new RuntimeException("Bundle dex installation failed (" + e.getMessage() + ").");
        }
    }

    public void run() {
        try {

            log.log("run", Logger.LogLevel.ERROR);
            for (Bundle bundle : BundleCore.getInstance().getBundles()) {

                BundleImpl bundleImpl = (BundleImpl) bundle;
                try {
                    bundleImpl.optDexFile();
                } catch (Exception e) {
                    e.printStackTrace();
                    log.log("Error while dexopt >>>", Logger.LogLevel.ERROR, e);
                }
            }
            notifySyncBundleListers();
            DelegateResources.newDelegateResources(RuntimeArgs.androidApplication, RuntimeArgs.delegateResources);

        } catch (Exception e) {
            Log.e("Bundleinstall", "Bundle Dex installation failure", e);
            throw new RuntimeException("Bundle dex installation failed (" + e.getMessage() + ").");
        }
        System.setProperty("BUNDLES_INSTALLED", "true");
    }


    private void notifyDelayBundleListers() {
        if (!bundleDelayListeners.isEmpty()) {
            for (BundleInstalledListener bundleInstalledListener : bundleDelayListeners) {
                bundleInstalledListener.onBundleInstalled();
            }
        }

    }

    private void notifySyncBundleListers() {
        if (!bundleSyncListeners.isEmpty()) {
            for (BundleInstalledListener bundleInstalledListener : bundleSyncListeners) {
                bundleInstalledListener.onBundleInstalled();
            }
        }
    }

    public Bundle getBundle(String bundleName) {
        return Framework.getBundle(bundleName);
    }

    public Bundle installBundle(String location, InputStream inputStream) throws BundleException {
        return Framework.installNewBundle(location, inputStream);
    }


    public void updateBundle(String location, InputStream inputStream) throws BundleException {
        Bundle bundle = Framework.getBundle(location);
        if (bundle != null) {
            bundle.update(inputStream);
            return;
        }
        throw new BundleException("Could not update bundle " + location + ", because could not find it");
    }


    public void uninstallBundle(String location) throws BundleException {
        Bundle bundle = Framework.getBundle(location);
        if (bundle != null) {
            BundleImpl bundleImpl = (BundleImpl) bundle;
            try {
                bundleImpl.getArchive().purge();

            } catch (Exception e) {
                log.log("uninstall bundle error: " + location + e.getMessage(), Logger.LogLevel.ERROR);
            }
        }
    }

    public List<Bundle> getBundles() {
        return Framework.getBundles();
    }

    public Resources getDelegateResources() {
        return RuntimeArgs.delegateResources;
    }



    public File getBundleFile(String location) {
        Bundle bundle = Framework.getBundle(location);
        return bundle != null ? ((BundleImpl) bundle).archive.getArchiveFile() : null;
    }

    public InputStream openAssetInputStream(String location, String fileName) throws IOException {
        Bundle bundle = Framework.getBundle(location);
        return bundle != null ? ((BundleImpl) bundle).archive.openAssetInputStream(fileName) : null;
    }

    public InputStream openNonAssetInputStream(String location, String str2) throws IOException {
        Bundle bundle = Framework.getBundle(location);
        return bundle != null ? ((BundleImpl) bundle).archive.openNonAssetInputStream(str2) : null;
    }

    public void addBundleDelayListener(BundleInstalledListener bundleListener) {
        bundleDelayListeners.add(bundleListener);
    }

    public void removeBundleDelayListener(BundleInstalledListener bundleListener) {
        bundleDelayListeners.remove(bundleListener);
    }

    public void addBundleSyncListener(BundleInstalledListener bundleListener) {
        bundleSyncListeners.add(bundleListener);
    }

    public void removeBundleSyncListener(BundleInstalledListener bundleListener) {
        bundleSyncListeners.remove(bundleListener);
    }


}
