package ctrip.android.bundle.framework.storage;


import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.SortedMap;
import java.util.TreeMap;

import ctrip.android.bundle.framework.Framework;
import ctrip.android.bundle.util.StringUtil;

/**
 * Created by yb.wang on 14/12/31.
 * <p/>
 * Bundle 目录结构：version_1,version_2
 */
public class BundleAchive implements Archive {

    public static final String REVISION_DIREXTORY = "version";
    private static final Long BENGIN_VERSION = 1L;
    private File bundleDir;
    private final BundleArchiveRevision currentRevision;
    private final SortedMap<Long, BundleArchiveRevision> revisionSortedMap;

    public BundleAchive(File file) throws IOException {
        this.revisionSortedMap = new TreeMap();
        String[] lists = file.list();
        if (lists != null) {
            for (String str : lists) {
                if (str.startsWith(REVISION_DIREXTORY)) {
                    long parseLong = Long.parseLong(StringUtil.subStringAfter(str, "_"));
                    if (parseLong > 0) {
                        this.revisionSortedMap.put(parseLong, null);
                    }
                }
            }

        }
        if (revisionSortedMap.isEmpty()) {
            throw new IOException("No Valid revisions in bundle archive directory");
        }
        this.bundleDir = file;
        long longValue = this.revisionSortedMap.lastKey();
        BundleArchiveRevision bundleArchiveRevision = new BundleArchiveRevision(longValue, new File(file, REVISION_DIREXTORY + "_" + String.valueOf(longValue)));
        this.revisionSortedMap.put(longValue, bundleArchiveRevision);
        this.currentRevision = bundleArchiveRevision;
    }

    public BundleAchive(File file, InputStream inputStream) throws IOException {
        this.revisionSortedMap = new TreeMap();
        this.bundleDir = file;
        BundleArchiveRevision bundleArchiveRevision = new BundleArchiveRevision(BENGIN_VERSION, new File(file, REVISION_DIREXTORY + "_" + String.valueOf(BENGIN_VERSION)), inputStream);
        this.revisionSortedMap.put(BENGIN_VERSION, bundleArchiveRevision);
        this.currentRevision = bundleArchiveRevision;
    }


    @Override
    public BundleArchiveRevision newRevision(File storageFile, InputStream inputStream) throws IOException {
        long version = this.revisionSortedMap.lastKey() + 1;
        BundleArchiveRevision bundleArchiveRevision = new BundleArchiveRevision(version, new File(storageFile, REVISION_DIREXTORY + "_" + String.valueOf(version)), inputStream);
        this.revisionSortedMap.put(version, bundleArchiveRevision);
        return bundleArchiveRevision;

    }


    public BundleArchiveRevision getCurrentRevision() {
        return this.currentRevision;
    }

    public File getBundleDir() {
        return this.bundleDir;
    }

    @Override
    public void close() {

    }

    @Override
    public File getArchiveFile() {
        return this.currentRevision.getRevisionFile();
    }


    @Override
    public boolean isBundleInstalled() {
        return this.currentRevision.isBundleInstalled();
    }

    @Override
    public boolean isDexOpted() {
        return this.currentRevision.isDexOpted();
    }

    @Override
    public void optDexFile() throws Exception {
        this.currentRevision.optDexFile();
    }

    @Override
    public void purge() throws Exception {

        Framework.deleteDirectory(this.currentRevision.getRevisionDir());
        long l = this.revisionSortedMap.lastKey();
        this.revisionSortedMap.clear();
        if (l < 1) {
            this.revisionSortedMap.put(0L, this.currentRevision);
        } else {
            this.revisionSortedMap.put(l - 1, this.currentRevision);
        }
    }


    @Override
    public InputStream openAssetInputStream(String fileName) throws IOException {
        return this.currentRevision.openAssetInputStream(fileName);
    }

    @Override
    public InputStream openNonAssetInputStream(String fileName) throws IOException {
        return this.currentRevision.openNonAssetInputStream(fileName);
    }

}
