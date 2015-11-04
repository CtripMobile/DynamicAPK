package ctrip.android.bundle.framework;

import java.io.InputStream;

public interface Bundle {


    long getBundleId();


    String getLocation();


    int getState();


    void update(InputStream inputStream) throws BundleException;
}
