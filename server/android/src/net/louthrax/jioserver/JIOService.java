package net.louthrax.jioserver;

import android.Manifest;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

/*
 =======================================================================================================================
    Foreground service while the server is connected to the MSX (MainWindow.cpp: vSetLinkActive): without it, Android
    freezes the application in the background when the phone locks itself, and the communication stops. The service
    does nothing else: the server stays in the Qt code. Its notification shows that the server is running.
 =======================================================================================================================
 */
public class JIOService extends Service
{
    private static final String CHANNEL = "link";
    private static final int NOTIFICATION = 1;

    public static void start(Context _oContext)
    {
        try
        {
            _oContext.startForegroundService(new Intent(_oContext, JIOService.class));
        }
        catch (Exception e)                 // not allowed when started in the background (e.g. Bluetooth reconnection)
        {
            Log.w("JIOServer", "Foreground service not started: " + e);
        }
    }

    public static void stop(Context _oContext)
    {
        _oContext.stopService(new Intent(_oContext, JIOService.class));
    }

    // Android 13+: the notification of the service is only shown with this permission (the service runs without it)
    public static void requestNotifications(Context _oContext)
    {
        if ((Build.VERSION.SDK_INT >= 33) && (_oContext instanceof Activity)
            && (_oContext.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED))
            ((Activity) _oContext).requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS }, 1);
    }

    @Override
    public int onStartCommand(Intent _oIntent, int _iFlags, int _iStartId)
    {
        NotificationManager oManager = getSystemService(NotificationManager.class);
        oManager.createNotificationChannel(new NotificationChannel(CHANNEL, "MSX connection", NotificationManager.IMPORTANCE_LOW));

        Intent oOpen = getPackageManager().getLaunchIntentForPackage(getPackageName());
        Notification oNotification = new Notification.Builder(this, CHANNEL)
            .setSmallIcon(getApplicationInfo().icon)
            .setContentTitle("JIO Server")
            .setContentText("Connected to the MSX")
            .setContentIntent(PendingIntent.getActivity(this, 0, oOpen, PendingIntent.FLAG_IMMUTABLE))
            .setOngoing(true)
            .build();

        try
        {
            if (Build.VERSION.SDK_INT >= 29)
                startForeground(NOTIFICATION, oNotification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);
            else
                startForeground(NOTIFICATION, oNotification);
        }
        catch (Exception e)
        {
            Log.w("JIOServer", "startForeground failed: " + e);
            stopSelf();
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent _oIntent)
    {
        return null;
    }
}
