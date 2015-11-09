package ctrip.android.bundle.hack;

import android.app.Application;
import android.app.Instrumentation;
import android.app.Service;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Build;
import android.view.ContextThemeWrapper;

import java.util.ArrayList;
import java.util.Map;

import ctrip.android.bundle.hack.Hack.HackedClass;
import ctrip.android.bundle.hack.Hack.HackedField;
import ctrip.android.bundle.hack.Hack.HackedMethod;
import ctrip.android.bundle.log.Logger;
import ctrip.android.bundle.log.LoggerFactory;

/**
 * Created by yb.wang on 14/12/31.
 * Hack 系统的功能：包括类加载机制，资源加载，Context等
 */
public class SysHacks extends Hack.HackDeclaration implements Hack.AssertionFailureHandler {
    public static HackedClass<Object> ActivityThread;
    public static HackedMethod ActivityThread_currentActivityThread;
    public static HackedField<Object, ArrayList<android.app.Application>> ActivityThread_mAllApplications;
    public static HackedField<Object, Instrumentation> ActivityThread_mInstrumentation;
    public static HackedField<Object, Map<String, Object>> ActivityThread_mPackages;
    public static HackedField<Object, Object> ActivityThread_sPackageManager;
    public static HackedClass<Application> Application;
    public static HackedMethod Application_attach;
    public static HackedClass<android.content.res.AssetManager> AssetManager;
    public static HackedMethod AssetManager_addAssetPath;
    public static HackedClass<Object> ContextImpl;
    public static HackedField<Object, android.content.res.Resources> ContextImpl_mResources;
    public static HackedField<Object, android.content.res.Resources.Theme> ContextImpl_mTheme;
    public static HackedClass<android.view.ContextThemeWrapper> ContextThemeWrapper;
    public static HackedField<ContextThemeWrapper, Context> ContextThemeWrapper_mBase;
    public static HackedField<ContextThemeWrapper, Resources> ContextThemeWrapper_mResources;
    public static HackedField<ContextThemeWrapper, android.content.res.Resources.Theme> ContextThemeWrapper_mTheme;
    public static HackedClass<android.content.ContextWrapper> ContextWrapper;
    public static HackedField<ContextWrapper, Context> ContextWrapper_mBase;
    public static ArrayList<HackedMethod> GeneratePackageInfoList;
    public static ArrayList<HackedMethod> GetPackageInfoList;
    public static HackedClass<Object> IPackageManager;
    public static HackedClass<Object> LoadedApk;
    public static HackedField<Object, String> LoadedApk_mAppDir;
    public static HackedField<Object, Application> LoadedApk_mApplication;
    public static HackedField<Object, String> LoadedApk_mResDir;
    public static HackedField<Object, Resources> LoadedApk_mResources;
    public static HackedClass<Resources> Resources;
    public static HackedField<Resources, Object> Resources_mAssets;
    public static HackedClass<android.app.Service> Service;
    public static HackedClass<Instrumentation> Instrumentation;

    public static boolean sIsIgnoreFailure;
    public static boolean sIsReflectAvailable;
    public static boolean sIsReflectChecked;
    private AssertionArrayException mExceptionArray;
    static final Logger log;

    public SysHacks() {
        this.mExceptionArray = null;
    }

    static {
        log = LoggerFactory.getLogcatLogger("SysHacks");
        sIsReflectAvailable = false;
        sIsReflectChecked = false;
        sIsIgnoreFailure = false;
        GeneratePackageInfoList = new ArrayList();
        GetPackageInfoList = new ArrayList();
    }

    public static boolean defineAndVerify() throws AssertionArrayException {
        if (sIsReflectChecked) {
            return sIsReflectAvailable;
        }
        SysHacks atlasHacks = new SysHacks();
        try {
            Hack.setAssertionFailureHandler(atlasHacks);
            if (Build.VERSION.SDK_INT == 11) {
                atlasHacks.onAssertionFailure(new HackAssertionException("Hack Assertion Failed: Android OS Version 11"));
            }
            allClasses();
            allConstructors();
            allFields();
            allMethods();
            if (atlasHacks.mExceptionArray != null) {
                sIsReflectAvailable = false;
                throw atlasHacks.mExceptionArray;
            }
            sIsReflectAvailable = true;
            return sIsReflectAvailable;
        } catch (Throwable e) {
            sIsReflectAvailable = false;
            log.log("HackAssertionException", Logger.LogLevel.ERROR, e);
            throw new AssertionArrayException("defineAndVerify HackAssertionException");
        } finally {
            Hack.setAssertionFailureHandler(null);
            sIsReflectChecked = true;
        }
    }

    public static void allClasses() throws HackAssertionException {
        if (Build.VERSION.SDK_INT <= 8) {
            LoadedApk = Hack.into("android.app.ActivityThread$PackageInfo");
        } else {
            LoadedApk = Hack.into("android.app.LoadedApk");
        }
        ActivityThread = Hack.into("android.app.ActivityThread");
        Resources = Hack.into(Resources.class);
        Application = Hack.into(Application.class);
        AssetManager = Hack.into(AssetManager.class);
        IPackageManager = Hack.into("android.content.pm.IPackageManager");
        Service = Hack.into(Service.class);
        ContextImpl = Hack.into("android.app.ContextImpl");
        ContextThemeWrapper = Hack.into(ContextThemeWrapper.class);
        ContextWrapper = Hack.into("android.content.ContextWrapper");
        sIsIgnoreFailure = true;
        Instrumentation = Hack.into("android.app.Instrumentation");
        sIsIgnoreFailure = false;
    }

    public static void allFields() throws HackAssertionException {

        ActivityThread_mInstrumentation = ActivityThread.field("mInstrumentation");
        ActivityThread_mInstrumentation.ofType(Instrumentation.class);
        ActivityThread_mAllApplications = ActivityThread.field("mAllApplications");
        ActivityThread_mAllApplications.ofGenericType(ArrayList.class);
        ActivityThread_mPackages = ActivityThread.field("mPackages");
        ActivityThread_mPackages.ofGenericType(Map.class);
        ActivityThread_sPackageManager = ActivityThread.staticField("sPackageManager").ofType(IPackageManager.getmClass());
        LoadedApk_mApplication = LoadedApk.field("mApplication");
        LoadedApk_mApplication.ofType(Application.class);
        LoadedApk_mResources = LoadedApk.field("mResources");
        LoadedApk_mResources.ofType(Resources.class);
        LoadedApk_mResDir = LoadedApk.field("mResDir");
        LoadedApk_mResDir.ofType(String.class);
        LoadedApk_mAppDir = LoadedApk.field("mAppDir");
        LoadedApk_mAppDir.ofType(String.class);
        ContextImpl_mResources = ContextImpl.field("mResources");
        ContextImpl_mResources.ofType(Resources.class);
        ContextImpl_mTheme = ContextImpl.field("mTheme");
        ContextImpl_mTheme.ofType(android.content.res.Resources.Theme.class);
        sIsIgnoreFailure = true;
        ContextThemeWrapper_mBase = ContextThemeWrapper.field("mBase");
        ContextThemeWrapper_mBase.ofType(Context.class);
        sIsIgnoreFailure = false;
        ContextThemeWrapper_mTheme = ContextThemeWrapper.field("mTheme");
        ContextThemeWrapper_mTheme.ofType(android.content.res.Resources.Theme.class);
        try {
            if (Build.VERSION.SDK_INT >= 17 && ContextThemeWrapper.getmClass().getDeclaredField("mResources") != null) {
                ContextThemeWrapper_mResources = ContextThemeWrapper.field("mResources");
                ContextThemeWrapper_mResources.ofType(Resources.class);
            }
        } catch (NoSuchFieldException e) {
            log.log("Not found ContextThemeWrapper.mResources on VERSION " + Build.VERSION.SDK_INT, Logger.LogLevel.WARN);
        }
        ContextWrapper_mBase = ContextWrapper.field("mBase");
        ContextWrapper_mBase.ofType(Context.class);
        Resources_mAssets = Resources.field("mAssets");
    }

    public static void allMethods() throws HackAssertionException {
        ActivityThread_currentActivityThread = ActivityThread.method("currentActivityThread", new Class[0]);
        AssetManager_addAssetPath = AssetManager.method("addAssetPath", String.class);
        Application_attach = Application.method("attach", Context.class);
    }

    public static void allConstructors() throws HackAssertionException {
    }

    public boolean onAssertionFailure(HackAssertionException hackAssertionException) {
        if (!sIsIgnoreFailure) {
            if (this.mExceptionArray == null) {
                this.mExceptionArray = new AssertionArrayException("Hack assert failed");
            }
            this.mExceptionArray.addException(hackAssertionException);
        }
        return true;
    }
}
