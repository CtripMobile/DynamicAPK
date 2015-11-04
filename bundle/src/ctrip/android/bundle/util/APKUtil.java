package ctrip.android.bundle.util;


import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

/**
 * Created by yb.wang on 14/12/31.
 */
public class APKUtil {


    public static void copyInputStreamToFile(InputStream inputStream, File file) throws IOException {
        FileChannel fileChannel = null;
        FileOutputStream fileOutputStream = null;
        try {
            fileOutputStream = new FileOutputStream(file);
            fileChannel = fileOutputStream.getChannel();
            byte[] buffer = new byte[1024];
            while (true) {
                int read = inputStream.read(buffer);
                if (read <= 0) {
                    break;
                }
                fileChannel.write(ByteBuffer.wrap(buffer, 0, read));
            }

        } catch (IOException ex) {
            throw ex;
        } finally {
            if (inputStream != null)
                inputStream.close();
            if (fileChannel != null)
                fileChannel.close();
            if (fileOutputStream != null)
                fileOutputStream.close();
        }

    }

}
