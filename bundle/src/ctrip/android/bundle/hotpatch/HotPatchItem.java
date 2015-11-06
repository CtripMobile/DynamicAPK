package ctrip.android.bundle.hotpatch;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import ctrip.android.bundle.loader.BundlePathLoader;
import ctrip.android.bundle.log.Logger;
import ctrip.android.bundle.log.LoggerFactory;
import ctrip.android.bundle.runtime.RuntimeArgs;
import ctrip.android.bundle.util.APKUtil;

/**
 * Created by yb.wang on 15/7/30.
 */
public class HotPatchItem {
    private static final Logger log;
    private static final  String HOTPATCH_FILE_NAME="hotfix.zip";
    private File hotFixFile;
    private File storageDir;
    private String hotPatchId;
    static {
        log= LoggerFactory.getLogcatLogger("HotPatchItem");
    }

    HotPatchItem(File storeDir,InputStream inputStream) throws IOException{
        this.storageDir=storeDir;
        if(!storageDir.exists()){
            storageDir.mkdirs();
        }
        this.hotPatchId=storeDir.getName();
        this.hotFixFile=new File(storeDir,HOTPATCH_FILE_NAME);
        APKUtil.copyInputStreamToFile(inputStream, this.hotFixFile);
    }

    HotPatchItem(File storeDir){

        this.storageDir=storeDir;
        if(!storageDir.exists()){
            storageDir.mkdirs();
        }
        this.hotPatchId=storeDir.getName();
        this.hotFixFile=new File(storeDir,HOTPATCH_FILE_NAME);
    }


    public String getHotPatchId(){
        return  this.hotPatchId;
    }

    public boolean isPatchInstalled(){
        if(hotFixFile.exists()){
            return verifyZipFile(hotFixFile);
        }
        return false;
    }

    private boolean verifyZipFile(File file) {
        try {
            ZipFile zipFile = new ZipFile(file);
            try {
                zipFile.close();
                return true;
            } catch (IOException e) {
                log.log("Failed to close zip file: " + file.getAbsolutePath(), Logger.LogLevel.ERROR, e);
            }
        } catch (ZipException ex) {
            log.log("File " + file.getAbsolutePath() + " is not a valid zip file.", Logger.LogLevel.ERROR, ex);
        } catch (IOException ex) {
            log.log("Got an IOException trying to open zip file: " + file.getAbsolutePath(), Logger.LogLevel.ERROR, ex);
        }
        return false;
    }

    public void optDexFile() throws Exception{
        List<File> files = new ArrayList<File>();
        files.add(this.hotFixFile);
        BundlePathLoader.installBundleDexs(RuntimeArgs.androidApplication.getClassLoader(), storageDir, files, false);
    }

    public void optHotFixDexFile() throws Exception{
        List<File> files = new ArrayList<File>();
        files.add(this.hotFixFile);
        BundlePathLoader.installBundleDexs(RuntimeArgs.androidApplication.getClassLoader(), storageDir, files, true);
    }

    public  void  purge(){

        deleteDirectory(storageDir);
    }

    private   void deleteDirectory(File file) {
        File[] listFiles = file.listFiles();
        for (int i = 0; i < listFiles.length; i++) {
            if (listFiles[i].isDirectory()) {
                deleteDirectory(listFiles[i]);
            } else {
                listFiles[i].delete();
            }
        }
        file.delete();
    }

}
