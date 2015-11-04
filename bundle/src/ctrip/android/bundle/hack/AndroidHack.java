package ctrip.android.bundle.hack;

import android.app.Activity;
import android.app.Application;
import android.app.Instrumentation;
import android.content.ContextWrapper;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;

import java.lang.ref.WeakReference;
import java.util.Map;

/**
 * Created by yb.wang on 15/1/5.
 * Android中的ClassLoader Hack
 */
public class AndroidHack {
    private static Object _mLoadedApk;
    private static Object _sActivityThread;

    static class ActivityThreadGetter implements Runnable {
        ActivityThreadGetter() {
        }

        public void run() {
            try {
                _sActivityThread = SysHacks.ActivityThread_currentActivityThread.invoke(SysHacks.ActivityThread.getmClass(), new Object[0]);
            } catch (Exception e) {
                e.printStackTrace();
            }
            synchronized (SysHacks.ActivityThread_currentActivityThread) {
                SysHacks.ActivityThread_currentActivityThread.notify();
            }
        }
    }

    static {
        _sActivityThread = null;
        _mLoadedApk = null;
    }

    public static Object getActivityThread() throws Exception {
        if (_sActivityThread == null) {
            if (Thread.currentThread().getId() == Looper.getMainLooper().getThread().getId()) {
                _sActivityThread = SysHacks.ActivityThread_currentActivityThread.invoke(null, new Object[0]);
            } else {
                Handler handler = new Handler(Looper.getMainLooper());
                synchronized (SysHacks.ActivityThread_currentActivityThread) {
                    handler.post(new ActivityThreadGetter());
                    SysHacks.ActivityThread_currentActivityThread.wait();
                }
            }
        }
        return _sActivityThread;
    }

    public static Object getLoadedApk(Object obj, String str) throws Exception {
        if (_mLoadedApk == null) {
            WeakReference weakReference = (WeakReference) ((Map) SysHacks.ActivityThread_mPackages.get(obj)).get(str);
            if (weakReference != null) {
                _mLoadedApk = weakReference.get();
            }
        }
        return _mLoadedApk;
    }

    public static void injectClassLoader(String str, ClassLoader classLoader) throws Exception {
        Object activityThread = getActivityThread();
        if (activityThread == null) {
            throw new Exception("Failed to get ActivityThread.sCurrentActivityThread");
        }
        activityThread = getLoadedApk(activityThread, str);
        if (activityThread == null) {
            throw new Exception("Failed to get ActivityThread.mLoadedApk");
        }
        SysHacks.LoadedApk_mClassLoader.set(activityThread, classLoader);
    }

    public static void injectResources(Application application, Resources resources) throws Exception {
        Object activityThread = getActivityThread();
        if (activityThread == null) {
            throw new Exception("Failed to get ActivityThread.sCurrentActivityThread");
        }
        Object loadedApk = getLoadedApk(activityThread, application.getPackageName());
        if (loadedApk == null) {
            throw new Exception("Failed to get ActivityThread.mLoadedApk");
        }
        SysHacks.LoadedApk_mResources.set(loadedApk, resources);
        SysHacks.ContextImpl_mResources.set(application.getBaseContext(), resources);
        SysHacks.ContextImpl_mTheme.set(application.getBaseContext(), null);
    }

    public static void injectActivityResources(Activity activity, Resources resources) throws Exception {
        Object activityThread = getActivityThread();
        if (activityThread == null) {
            throw new Exception("Failed to get ActivityThread.sCurrentActivityThread");
        }
        Object loadedApk = getLoadedApk(activityThread, activity.getPackageName());
        if (loadedApk == null) {
            throw new Exception("Failed to get ActivityThread.mLoadedApk");
        }
        SysHacks.LoadedApk_mResources.set(loadedApk, resources);
        SysHacks.ContextImpl_mResources.set(activity.getBaseContext(), resources);
        SysHacks.ContextImpl_mTheme.set(activity.getBaseContext(), null);
    }

    public static ClassLoader currentClassLoader(String str) throws Exception {
        Object activityThread = getActivityThread();
        if (activityThread == null) {
            throw new Exception("Failed to get ActivityThread.sCurrentActivityThread");
        }
        activityThread = getLoadedApk(activityThread, str);
        if (activityThread != null) {
            return SysHacks.LoadedApk_mClassLoader.get(activityThread);
        }
        throw new Exception("Failed to get ActivityThread.mLoadedApk");
    }

    public static Instrumentation getInstrumentation() throws Exception {
        Object activityThread = getActivityThread();
        if (activityThread != null) {
            return SysHacks.ActivityThread_mInstrumentation.get(activityThread);
        }
        throw new Exception("Failed to get ActivityThread.sCurrentActivityThread");
    }

    public static void injectInstrumentationHook(Instrumentation instrumentation) throws Exception {
        Object activityThread = getActivityThread();
        if (activityThread == null) {
            throw new Exception("Failed to get ActivityThread.sCurrentActivityThread");
        }
        SysHacks.ActivityThread_mInstrumentation.set(activityThread, instrumentation);
    }

    public static void injectContextHook(ContextWrapper contextWrapper, ContextWrapper contextWrapper2) {
        SysHacks.ContextWrapper_mBase.set(contextWrapper, contextWrapper2);
    }
}
