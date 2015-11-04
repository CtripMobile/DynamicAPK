package ctrip.android.bundle.util;

/**
 * Created by yb.wang on 14/12/31.
 */
public class StringUtil {
    private static final String EMPTY = "";

    public static boolean isEmpty(String source) {
        return source == null || source.length() == 0;
    }


    public static boolean equals(String str, String str2) {
        return str == null ? false : str.equals(str2);
    }

    public static String subStringBetween(String source, String start, String end) {
        if (source == null || start == null || end == null) {
            return null;
        }
        int indexOf = source.indexOf(start);
        if (indexOf == -1) return null;
        int indexOf2 = source.indexOf(end, start.length() + indexOf);
        return indexOf2 != -1 ? source.substring(start.length() + indexOf, indexOf2) : null;
    }

    public static String subStringAfter(String source, String prefix) {
        if (isEmpty(source)) return source;
        if (prefix == null) return EMPTY;
        int indexOf = source.indexOf(prefix);
        return indexOf != -1 ? source.substring(indexOf + prefix.length()) : EMPTY;

    }


    public static boolean isBlank(String str) {
        if (str != null) {
            int length = str.length();
            if (length != 0) {
                for (int i = 0; i < length; i++) {
                    if (!Character.isWhitespace(str.charAt(i))) {
                        return false;
                    }
                }
                return true;
            }
        }
        return true;
    }

    public static String join(Object[] objArr, String str) {
        return objArr == null ? null : join(objArr, str, 0, objArr.length);
    }

    public static String join(Object[] objArr, String str, int i, int i2) {
        if (objArr == null) {
            return null;
        }
        if (str == null) {
            str = EMPTY;
        }
        int i3 = i2 - i;
        if (i3 <= 0) {
            return EMPTY;
        }
        StringBuilder stringBuilder = new StringBuilder(((objArr[i] == null ? 128 : objArr[i].toString().length()) + str.length()) * i3);
        for (int i4 = i; i4 < i2; i4++) {
            if (i4 > i) {
                stringBuilder.append(str);
            }
            if (objArr[i4] != null) {
                stringBuilder.append(objArr[i4]);
            }
        }
        return stringBuilder.toString();
    }

}
