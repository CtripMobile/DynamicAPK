package ctrip.android.bundle.framework;

public class BundleException extends Exception {
    private transient Throwable throwable;

    public BundleException(String str, Throwable th) {
        super(str);
        this.throwable = th;
    }

    public BundleException(String str) {
        super(str);
        this.throwable = null;
    }

    public Throwable getNestedException() {
        return this.throwable;
    }
}
