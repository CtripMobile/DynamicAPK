package ctrip.android.bundle.log;

import android.util.Log;

/**
 * Created by yb.wang on 15/1/23.
 */
public class LogcatLogger implements Logger {

    private final String tag;

    public LogcatLogger(String _tag) {
        this.tag = _tag;
    }

    public LogcatLogger(Class<?> classType) {
        this(classType.getSimpleName());
    }

    @Override
    public void log(String msg, LogLevel level) {
        if (!LoggerFactory.isNeedLog) return;
        if (level.getLevel() < LoggerFactory.minLevel.getLevel()) return;
        switch (level) {
            case DBUG:
                Log.d(tag, msg);
                break;
            case INFO:
                Log.i(tag, msg);
                break;
            case WARN:
                Log.w(tag, msg);
                break;
            case ERROR:
                Log.e(tag, msg);
                break;
            default:
                break;
        }
    }

    @Override
    public void log(String msg, LogLevel level, Throwable th) {
        if (!LoggerFactory.isNeedLog) return;
        if (level.getLevel() < LoggerFactory.minLevel.getLevel()) return;
        switch (level) {
            case DBUG:
                Log.d(tag, msg, th);
                break;
            case INFO:
                Log.i(tag, msg, th);
                break;
            case WARN:
                Log.w(tag, msg, th);
                break;
            case ERROR:
                Log.e(tag, msg, th);
                break;
            default:
                break;
        }
    }


}
