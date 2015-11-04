package ctrip.android.bundle.runtime;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.AssetManager;
import android.content.res.Resources;

import ctrip.android.bundle.log.Logger;
import ctrip.android.bundle.log.LoggerFactory;

/**
 * Created by yb.wang on 15/1/6.
 * Android Context Hook 挂载载系统的Context中，拦截相应的方法
 */
public class ContextImplHook extends ContextWrapper {
    static final Logger log;

    static {
        log = LoggerFactory.getLogcatLogger("ContextImplHook");
    }

    public ContextImplHook(Context context) {
        super(context);

    }

    @Override
    public Resources getResources() {
        log.log("getResources is invoke", Logger.LogLevel.INFO);
        return RuntimeArgs.delegateResources;
    }

    @Override
    public AssetManager getAssets() {
        log.log("getAssets is invoke", Logger.LogLevel.INFO);
        return RuntimeArgs.delegateResources.getAssets();
    }

}
