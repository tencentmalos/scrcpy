package com.genymobile.scrcpy.util;

import android.os.Build;


public final class SpatialUtils {
    public static boolean isPicoDevice() {
        boolean isPico = false;
        String productModel = android.os.Build.MODEL;
        if(productModel.equals("A9210") || productModel.equals("A8110")) {
            return true;
        }
        return false;
    }
};

