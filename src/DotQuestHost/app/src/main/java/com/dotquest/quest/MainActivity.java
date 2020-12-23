package com.dotquest.quest;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;

import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;

import static android.system.Os.setenv;

@SuppressLint("SdCardPath")
public class MainActivity extends Activity implements SurfaceHolder.Callback {
	protected static final String TAG = "TAG1";

	private static final int READ_EXTERNAL_STORAGE_PERMISSION_ID = 1;
	private static final int WRITE_EXTERNAL_STORAGE_PERMISSION_ID = 2;
	// Main components
	protected static boolean LOADED = false;
	protected static MainActivity _singleton;

	// Audio
	protected static AudioTrack _audioTrack;
	protected static AudioRecord _audioRecord;

	private int _permissionCount = 0;
	private String _commandLineParams;
	private SurfaceView _view;
	private SurfaceHolder _surfaceHolder;
	private long _nativeHandle;

	public void initialize() {
		// The static nature of the singleton and Android quirkyness force us to initialize everything here
		// Otherwise, when exiting the app and returning to it, these variables *keep* their pre exit values
		_singleton = null;
		_audioTrack = null;
		_audioRecord = null;
		_nativeHandle = 0;
	}

	public void shutdown() {
		System.exit(0);
	}

	private void load() {
		if (LOADED)
			return;
		LOADED = true;
		PrintFolder("/data/data/com.dotquest.quest", true);
		Log.v(TAG, "----------------------------------------------------------------");
		String arch = System.getProperty("os.arch");
		if (arch.equals("aarch64")) {
			//load_asset("/data/data/com.dotquest.quest/app_lib", "aarch64/ld-musl-aarch64.so.1", false);
			//load_asset("/data/data/com.dotquest.quest/app_lib", "aarch64/libgcc_s.so.1", false);
			//load_asset("/data/data/com.dotquest.quest/app_lib", "aarch64/libstdc++.so.6.0.28", false);
		} else if (arch.equals("x86_64")) {
			//load_asset("/data/data/com.dotquest.quest/app_lib", "x86_64/ld-musl-x86_64.so.1", false);
			//load_asset("/data/data/com.dotquest.quest/app_lib", "x86_64/libgcc_s.so.1", false);
			//load_asset("/data/data/com.dotquest.quest/app_lib", "x86_64/libstdc++.so.6.0.28", false);
		} else {
			Log.v(TAG, "UNKNOWN: [" + arch + "]");
			System.exit(0);
		}
		//System.loadLibrary("c++_shared");
		System.loadLibrary("DotQuest");
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		load();
		Log.v(TAG, "----------------------------------------------------------------");
		Log.v(TAG, "MainActivity::onCreate()");
		super.onCreate(savedInstanceState);

		initialize();

		// So we can call stuff from static callbacks
		_singleton = this;

		_view = new SurfaceView(this);
		setContentView(_view);
		_view.getHolder().addCallback(this);

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

		// Force screen brightness to stay at maximum
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes(params);

		checkPermissionsAndInitialize();
	}

	// Initializes the Activity only if the permission has been granted.
	private void checkPermissionsAndInitialize() {
		// permission: WRITE_EXTERNAL_STORAGE
		if (ContextCompat.checkSelfPermission(this,
				Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
			ActivityCompat.requestPermissions(MainActivity.this,
					new String[] { Manifest.permission.WRITE_EXTERNAL_STORAGE }, WRITE_EXTERNAL_STORAGE_PERMISSION_ID);
		} else
			_permissionCount++;
		// permission: READ_EXTERNAL_STORAGE
		if (ContextCompat.checkSelfPermission(this,
				Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
			ActivityCompat.requestPermissions(MainActivity.this,
					new String[] { Manifest.permission.READ_EXTERNAL_STORAGE }, READ_EXTERNAL_STORAGE_PERMISSION_ID);
		} else
			_permissionCount++;
		// permission: READY
		if (_permissionCount == 2)
			create();
	}

	// Handles the user accepting the permission.
	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
		// permission: READ_EXTERNAL_STORAGE
		if (requestCode == READ_EXTERNAL_STORAGE_PERMISSION_ID) {
			if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED)
				_permissionCount++;
			else {
				Log.e(TAG, "adb shell pm grant com.dotquest.quest android.permission.READ_EXTERNAL_STORAGE");
				System.exit(0);
			}
		}
		// permission: WRITE_EXTERNAL_STORAGE
		if (requestCode == WRITE_EXTERNAL_STORAGE_PERMISSION_ID) {
			if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED)
				_permissionCount++;
			else {
				Log.e(TAG, "adb shell pm grant com.dotquest.quest android.permission.WRITE_EXTERNAL_STORAGE");
				System.exit(0);
			}
		}
		// permission: READY
		checkPermissionsAndInitialize();
	}

	public void create() {
		Log.v(TAG, "MainActivity::create()");
		// make the directories
		new File("/sdcard/DotQuest/Main").mkdirs();

		// copy assets
		copy_asset("/sdcard/DotQuest/Main", "main.cfg", false);

		// read these from a file and pass through
		_commandLineParams = new String("quest");

		// see if user is trying to use command line params
		if (new File("/sdcard/DotQuest/commandline.txt").exists()) { // should exist!
			BufferedReader br;
			try {
				br = new BufferedReader(new FileReader("/sdcard/DotQuest/commandline.txt"));
				String s;
				StringBuilder sb = new StringBuilder(0);
				while ((s = br.readLine()) != null)
					sb.append(s + " ");
				br.close();
				_commandLineParams = new String(sb.toString());
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			} catch (IOException e) {
				e.printStackTrace();
			}
		}

		try {
			setenv("DOTQUEST_LIBDIR", getApplicationInfo().nativeLibraryDir, true);
		} catch (Exception e) {
		}

		_nativeHandle = MainActivityJNI.onCreate(this, _commandLineParams);
	}

	@Override
	protected void onStart() {
		Log.v(TAG, "MainActivity::onStart()");
		super.onStart();
		MainActivityJNI.onStart(_nativeHandle, this);
	}

	@Override
	protected void onResume() {
		Log.v(TAG, "MainActivity::onResume()");
		super.onResume();
		MainActivityJNI.onResume(_nativeHandle);
	}

	@Override
	protected void onPause() {
		Log.v(TAG, "MainActivity::onPause()");
		MainActivityJNI.onPause(_nativeHandle);
		super.onPause();
	}

	@Override
	protected void onStop() {
		Log.v(TAG, "MainActivity::onStop()");
		MainActivityJNI.onStop(_nativeHandle);
		super.onStop();
	}

	@Override
	protected void onDestroy() {
		Log.v(TAG, "MainActivity::onDestroy()");
		if (_surfaceHolder != null) {
			MainActivityJNI.onSurfaceDestroyed(_nativeHandle);
		}
		MainActivityJNI.onDestroy(_nativeHandle);
		super.onDestroy();
		// Reset everything in case the user re-opens the app
		initialize();
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		Log.v(TAG, "MainActivity::surfaceCreated()");
		if (_nativeHandle != 0) {
			MainActivityJNI.onSurfaceCreated(_nativeHandle, holder.getSurface());
			_surfaceHolder = holder;
		}
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
		Log.v(TAG, "MainActivity::surfaceChanged()");
		if (_nativeHandle != 0) {
			MainActivityJNI.onSurfaceChanged(_nativeHandle, holder.getSurface());
			_surfaceHolder = holder;
		}
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		Log.v(TAG, "MainActivity::surfaceDestroyed()");
		if (_nativeHandle != 0) {
			MainActivityJNI.onSurfaceDestroyed(_nativeHandle);
			_surfaceHolder = null;
		}
	}

	// UTILS

	public void load_asset(String path, String name, boolean force) {
		copy_asset(path, name, force);
		Log.v(TAG, "LOAD: " + name);
		System.load(path + "/" + name);
	}

	public void copy_asset(String path, String name, boolean force) {
		File f = new File(path + "/" + name);
		if (!f.exists() || force) {
			String fullname = path + "/" + name;
			String directory = fullname.substring(0, fullname.lastIndexOf("/"));
			new File(directory).mkdirs();
			_copy_asset(name, path + "/" + name);
		}
	}

	public void _copy_asset(String name_in, String name_out) {
		AssetManager assets = this.getAssets();
		try {
			InputStream in = assets.open(name_in);
			OutputStream out = new FileOutputStream(name_out);
			copy_stream(in, out);
			out.close();
			in.close();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public static void copy_stream(InputStream in, OutputStream out) throws IOException {
		byte[] buf = new byte[1024];
		while (true) {
			int count = in.read(buf);
			if (count <= 0)
				break;
			out.write(buf, 0, count);
		}
	}

	private static void PrintFolder(String path, Boolean recurse) {
		File file = new File(path);
		File[] files = file.listFiles();
		if (files == null)
			return;
		if (files != null)
			for (int i = 0; i < files.length; ++i) {
				String subPath = files[i].getAbsolutePath();
				Log.v(TAG, subPath);
				if (recurse)
					PrintFolder(subPath, recurse);
			}
	}
}
