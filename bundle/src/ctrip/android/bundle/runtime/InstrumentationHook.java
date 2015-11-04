package ctrip.android.bundle.runtime;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.Application;
import android.app.Fragment;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.IBinder;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.util.List;

import ctrip.android.bundle.framework.Framework;
import ctrip.android.bundle.hack.SysHacks;
import ctrip.android.bundle.log.Logger;
import ctrip.android.bundle.log.LoggerFactory;
import ctrip.android.bundle.util.StringUtil;


/**
 * Created by yb.wang on 15/1/5.
 * 挂载在系统中的Instrumentation，以拦截相应的方法
 */
public class InstrumentationHook extends Instrumentation {
    static final Logger log;
    private Context context;
    private Instrumentation mBase;

    private static interface ExecStartActivityCallback {
        ActivityResult execStartActivity();
    }

    static {
        log = LoggerFactory.getLogcatLogger("InstrumentationHook");
    }

    public InstrumentationHook(Instrumentation instrumentation, Context context) {
        this.context = context;
        this.mBase = instrumentation;
    }

    public ActivityResult execStartActivity(final Context context, final IBinder iBinder, final IBinder iBinder2, final Activity activity, final Intent intent, final int i) {
        return execStartActivityInternal(this.context, intent, new ExecStartActivityCallback() {
            @Override
            public ActivityResult execStartActivity() {
                try {
                    return (ActivityResult) SysHacks.Instrumentation.method("execStartActivity", Context.class, IBinder.class, IBinder.class, Activity.class, Intent.class, int.class)
                            .invoke(mBase, context, iBinder, iBinder2, activity, intent, i);
                } catch (Throwable ex) {
                    ex.printStackTrace();
                    return null;
                }


            }
        });
    }

    @TargetApi(16)
    public ActivityResult execStartActivity(final Context context, final IBinder iBinder, final IBinder iBinder2, final Activity activity, final Intent intent, final int i, final Bundle bundle) {
        return execStartActivityInternal(this.context, intent, new ExecStartActivityCallback() {
            @Override
            public ActivityResult execStartActivity() {
                try {
                    Object result = SysHacks.Instrumentation.method("execStartActivity", Context.class, IBinder.class, IBinder.class, Activity.class, Intent.class, int.class, Bundle.class)
                            .invoke(mBase, context, iBinder, iBinder2, activity, intent, i, bundle);
                    if (result != null) return (ActivityResult) result;
                } catch (Throwable ex) {
                    ex.printStackTrace();

                }
                return null;
            }
        });
    }

    @TargetApi(14)
    public ActivityResult execStartActivity(final Context context, final IBinder iBinder, final IBinder iBinder2, final Fragment fragment, final Intent intent, final int i) {
        return execStartActivityInternal(this.context, intent, new ExecStartActivityCallback() {
            @Override
            public ActivityResult execStartActivity() {
                try {
                    return (ActivityResult) SysHacks.Instrumentation.method("execStartActivity", Context.class, IBinder.class, IBinder.class, Fragment.class, Intent.class, int.class)
                            .invoke(mBase, context, iBinder, iBinder2, fragment, intent, i);
                } catch (Throwable ex) {
                    ex.printStackTrace();
                    return null;
                }

            }
        });
    }

    @TargetApi(16)
    public ActivityResult execStartActivity(final Context context, final IBinder iBinder, final IBinder iBinder2, final Fragment fragment, final Intent intent, final int i, final Bundle bundle) {
        return execStartActivityInternal(this.context, intent, new ExecStartActivityCallback() {
            @Override
            public ActivityResult execStartActivity() {
                try {
                    return (ActivityResult) SysHacks.Instrumentation.method("execStartActivity", Context.class, IBinder.class, IBinder.class, Fragment.class, Intent.class, int.class, Bundle.class)
                            .invoke(mBase, context, iBinder, iBinder2, fragment, intent, i, bundle);
                } catch (Throwable ex) {
                    ex.printStackTrace();
                    return null;
                }
            }
        });
    }

    private ActivityResult execStartActivityInternal(Context context, Intent intent, ExecStartActivityCallback execStartActivityCallback) {
        String packageName;
        if (intent.getComponent() != null) {
            packageName = intent.getComponent().getPackageName();
        } else {
            ResolveInfo resolveActivity = context.getPackageManager().resolveActivity(intent, 0);
            if (resolveActivity == null || resolveActivity.activityInfo == null) {
                packageName = null;
            } else {
                packageName = resolveActivity.activityInfo.packageName;
            }
        }
        if (!StringUtil.equals(context.getPackageName(), packageName)) {
            return execStartActivityCallback.execStartActivity();
        }

        return execStartActivityCallback.execStartActivity();
    }

    public Activity newActivity(Class<?> cls, Context context, IBinder iBinder, Application application, Intent intent, ActivityInfo activityInfo, CharSequence charSequence, Activity activity, String str, Object obj) throws InstantiationException, IllegalAccessException {
        Activity newActivity = this.mBase.newActivity(cls, context, iBinder, application, intent, activityInfo, charSequence, activity, str, obj);
        if (RuntimeArgs.androidApplication.getPackageName().equals(activityInfo.packageName) && SysHacks.ContextThemeWrapper_mResources != null) {
            SysHacks.ContextThemeWrapper_mResources.set(newActivity, RuntimeArgs.delegateResources);
        }
        return newActivity;
    }

    public Activity newActivity(ClassLoader classLoader, String str, Intent intent) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        Activity newActivity;
        try {
            newActivity = this.mBase.newActivity(classLoader, str, intent);
            if (SysHacks.ContextThemeWrapper_mResources != null) {
                SysHacks.ContextThemeWrapper_mResources.set(newActivity, RuntimeArgs.delegateResources);
            }
        } catch (ClassNotFoundException e) {
            String property = Framework.getProperty("ctrip.android.bundle.welcome", "ctrip.android.view.home.CtripSplashActivity");
            if (StringUtil.isEmpty(property)) {
                throw e;
            } else {
                List runningTasks = ((ActivityManager) this.context.getSystemService(Context.ACTIVITY_SERVICE)).getRunningTasks(1);
                if (runningTasks != null && runningTasks.size() > 0 && ((ActivityManager.RunningTaskInfo) runningTasks.get(0)).numActivities > 1) {
                    if (intent.getComponent() == null) {
                        intent.setClassName(this.context, str);
                    }
                }
                log.log("Could not find activity class: " + str, Logger.LogLevel.WARN);
                log.log("Redirect to welcome activity: " + property, Logger.LogLevel.WARN);
                newActivity = this.mBase.newActivity(classLoader, property, intent);
            }
        }
        return newActivity;
    }

    public void callActivityOnCreate(Activity activity, Bundle bundle) {
        if (RuntimeArgs.androidApplication.getPackageName().equals(activity.getPackageName())) {
            ContextImplHook contextImplHook = new ContextImplHook(activity.getBaseContext());
            if (!(SysHacks.ContextThemeWrapper_mBase == null || SysHacks.ContextThemeWrapper_mBase.getField() == null)) {
                SysHacks.ContextThemeWrapper_mBase.set(activity, contextImplHook);
            }
            SysHacks.ContextWrapper_mBase.set(activity, contextImplHook);
        }
        this.mBase.callActivityOnCreate(activity, bundle);
    }

    @TargetApi(18)
    public UiAutomation getUiAutomation() {
        return this.mBase.getUiAutomation();
    }

    public void onCreate(Bundle bundle) {
        this.mBase.onCreate(bundle);
    }

    public void start() {
        this.mBase.start();
    }

    public void onStart() {
        this.mBase.onStart();
    }

    public boolean onException(Object obj, Throwable th) {
        return this.mBase.onException(obj, th);
    }

    public void sendStatus(int i, Bundle bundle) {
        this.mBase.sendStatus(i, bundle);
    }

    public void finish(int i, Bundle bundle) {
        this.mBase.finish(i, bundle);
    }

    public void setAutomaticPerformanceSnapshots() {
        this.mBase.setAutomaticPerformanceSnapshots();
    }

    public void startPerformanceSnapshot() {
        this.mBase.startPerformanceSnapshot();
    }

    public void endPerformanceSnapshot() {
        this.mBase.endPerformanceSnapshot();
    }

    public void onDestroy() {
        this.mBase.onDestroy();
    }

    public Context getContext() {
        return this.mBase.getContext();
    }

    public ComponentName getComponentName() {
        return this.mBase.getComponentName();
    }

    public Context getTargetContext() {
        return this.mBase.getTargetContext();
    }

    public boolean isProfiling() {
        return this.mBase.isProfiling();
    }

    public void startProfiling() {
        this.mBase.startProfiling();
    }

    public void stopProfiling() {
        this.mBase.stopProfiling();
    }

    public void setInTouchMode(boolean z) {
        this.mBase.setInTouchMode(z);
    }

    public void waitForIdle(Runnable runnable) {
        this.mBase.waitForIdle(runnable);
    }

    public void waitForIdleSync() {
        this.mBase.waitForIdleSync();
    }

    public void runOnMainSync(Runnable runnable) {
        this.mBase.runOnMainSync(runnable);
    }

    public Activity startActivitySync(Intent intent) {
        return this.mBase.startActivitySync(intent);
    }

    public void addMonitor(ActivityMonitor activityMonitor) {
        this.mBase.addMonitor(activityMonitor);
    }

    public ActivityMonitor addMonitor(IntentFilter intentFilter, ActivityResult activityResult, boolean z) {
        return this.mBase.addMonitor(intentFilter, activityResult, z);
    }

    public ActivityMonitor addMonitor(String str, ActivityResult activityResult, boolean z) {
        return this.mBase.addMonitor(str, activityResult, z);
    }

    public boolean checkMonitorHit(ActivityMonitor activityMonitor, int i) {
        return this.mBase.checkMonitorHit(activityMonitor, i);
    }

    public Activity waitForMonitor(ActivityMonitor activityMonitor) {
        return this.mBase.waitForMonitor(activityMonitor);
    }

    public Activity waitForMonitorWithTimeout(ActivityMonitor activityMonitor, long j) {
        return this.mBase.waitForMonitorWithTimeout(activityMonitor, j);
    }

    public void removeMonitor(ActivityMonitor activityMonitor) {
        this.mBase.removeMonitor(activityMonitor);
    }

    public boolean invokeMenuActionSync(Activity activity, int i, int i2) {
        return this.mBase.invokeMenuActionSync(activity, i, i2);
    }

    public boolean invokeContextMenuAction(Activity activity, int i, int i2) {
        return this.mBase.invokeContextMenuAction(activity, i, i2);
    }

    public void sendStringSync(String str) {
        this.mBase.sendStringSync(str);
    }

    public void sendKeySync(KeyEvent keyEvent) {
        this.mBase.sendKeySync(keyEvent);
    }

    public void sendKeyDownUpSync(int i) {
        this.mBase.sendKeyDownUpSync(i);
    }

    public void sendCharacterSync(int i) {
        this.mBase.sendCharacterSync(i);
    }

    public void sendPointerSync(MotionEvent motionEvent) {
        this.mBase.sendPointerSync(motionEvent);
    }

    public void sendTrackballEventSync(MotionEvent motionEvent) {
        this.mBase.sendTrackballEventSync(motionEvent);
    }

    public Application newApplication(ClassLoader classLoader, String str, Context context) throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        return this.mBase.newApplication(classLoader, str, context);
    }

    public void callApplicationOnCreate(Application application) {
        this.mBase.callApplicationOnCreate(application);
    }

    public void callActivityOnDestroy(Activity activity) {
        this.mBase.callActivityOnDestroy(activity);
    }

    public void callActivityOnRestoreInstanceState(Activity activity, Bundle bundle) {
        this.mBase.callActivityOnRestoreInstanceState(activity, bundle);
    }

    public void callActivityOnPostCreate(Activity activity, Bundle bundle) {
        this.mBase.callActivityOnPostCreate(activity, bundle);
    }

    public void callActivityOnNewIntent(Activity activity, Intent intent) {
        this.mBase.callActivityOnNewIntent(activity, intent);
    }

    public void callActivityOnStart(Activity activity) {
        this.mBase.callActivityOnStart(activity);
    }

    public void callActivityOnRestart(Activity activity) {
        this.mBase.callActivityOnRestart(activity);
    }

    public void callActivityOnResume(Activity activity) {
        this.mBase.callActivityOnResume(activity);
    }

    public void callActivityOnStop(Activity activity) {
        this.mBase.callActivityOnStop(activity);
    }

    public void callActivityOnSaveInstanceState(Activity activity, Bundle bundle) {
        this.mBase.callActivityOnSaveInstanceState(activity, bundle);
    }

    public void callActivityOnPause(Activity activity) {
        this.mBase.callActivityOnPause(activity);
    }

    public void callActivityOnUserLeaving(Activity activity) {
        this.mBase.callActivityOnUserLeaving(activity);
    }

    public void startAllocCounting() {
        this.mBase.startAllocCounting();
    }

    public void stopAllocCounting() {
        this.mBase.stopAllocCounting();
    }

    public Bundle getAllocCounts() {
        return this.mBase.getAllocCounts();
    }

    public Bundle getBinderCounts() {
        return this.mBase.getBinderCounts();
    }
}
