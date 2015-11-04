package ctrip.android.bundle.hack;

import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import ctrip.android.bundle.framework.Framework;

/**
 * Created by yb.wang on 15/1/5.
 */
public class AssertionArrayException extends Exception {
    private static final long serialVersionUID = 1;
    private List<Hack.HackDeclaration.HackAssertionException> mAssertionErr;

    public AssertionArrayException(String str) {
        super(str);
        this.mAssertionErr = new ArrayList();
    }

    public void addException(Hack.HackDeclaration.HackAssertionException hackAssertionException) {
        this.mAssertionErr.add(hackAssertionException);
    }

    public void addException(List<Hack.HackDeclaration.HackAssertionException> list) {
        this.mAssertionErr.addAll(list);
    }

    public List<Hack.HackDeclaration.HackAssertionException> getExceptions() {
        return this.mAssertionErr;
    }

    public static AssertionArrayException mergeException(AssertionArrayException assertionArrayException, AssertionArrayException assertionArrayException2) {
        if (assertionArrayException == null) {
            return assertionArrayException2;
        }
        if (assertionArrayException2 == null) {
            return assertionArrayException;
        }
        AssertionArrayException assertionArrayException3 = new AssertionArrayException(assertionArrayException.getMessage() + Framework.SYMBOL_SEMICOLON + assertionArrayException2.getMessage());
        assertionArrayException3.addException(assertionArrayException.getExceptions());
        assertionArrayException3.addException(assertionArrayException2.getExceptions());
        return assertionArrayException3;
    }

    public String toString() {
        StringBuilder stringBuilder = new StringBuilder();
        for (Hack.HackDeclaration.HackAssertionException hackAssertionException : this.mAssertionErr) {
            stringBuilder.append(hackAssertionException.toString()).append(Framework.SYMBOL_SEMICOLON);
            try {
                if (hackAssertionException.getCause() instanceof NoSuchFieldException) {
                    Field[] declaredFields = hackAssertionException.getHackedClass().getDeclaredFields();
                    stringBuilder.append(hackAssertionException.getHackedClass().getName()).append(".").append(hackAssertionException.getHackedFieldName()).append(Framework.SYMBOL_SEMICOLON);
                    for (Field field : declaredFields) {
                        stringBuilder.append(field.getName()).append(File.separator);
                    }
                } else if (hackAssertionException.getCause() instanceof NoSuchMethodException) {
                    Method[] declaredMethods = hackAssertionException.getHackedClass().getDeclaredMethods();
                    stringBuilder.append(hackAssertionException.getHackedClass().getName()).append("->").append(hackAssertionException.getHackedMethodName()).append(Framework.SYMBOL_SEMICOLON);
                    for (int i = 0; i < declaredMethods.length; i++) {
                        if (hackAssertionException.getHackedMethodName().equals(declaredMethods[i].getName())) {
                            stringBuilder.append(declaredMethods[i].toGenericString()).append(File.separator);
                        }
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
            stringBuilder.append("@@@@");
        }
        return stringBuilder.toString();
    }
}
