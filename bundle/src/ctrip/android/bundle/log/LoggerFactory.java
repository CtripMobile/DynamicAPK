package ctrip.android.bundle.log;

/**
 * Created by yb.wang on 15/1/23.
 */
public class LoggerFactory {

    public static boolean isNeedLog;

    public static Logger.LogLevel minLevel;

    static {
        isNeedLog = false;
        minLevel = Logger.LogLevel.DBUG;
    }

    public static Logger getLogcatLogger(String tag) {
        return getLogcatLogger(tag, null);
    }

    public static Logger getLogcatLogger(Class<?> cls) {
        return getLogcatLogger(null, cls);
    }

    private static Logger getLogcatLogger(String tag, Class<?> cls) {
        return cls != null ? new LogcatLogger((Class) cls) : new LogcatLogger(tag);
    }

}
