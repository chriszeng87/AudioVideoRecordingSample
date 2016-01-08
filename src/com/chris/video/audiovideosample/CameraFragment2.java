package com.chris.video.audiovideosample;
/*
 * AudioVideoRecordingSample
 * Sample project to cature audio and video from internal mic/camera and save as MPEG4 file.
 *
 * Copyright (c) 2014-2015 saki t_saki@serenegiant.com
 *
 * File name: CameraFragment.java
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * All files in the folder are under this Apache License, Version 2.0.
*/

import java.io.IOException;

import android.app.Fragment;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageButton;

import com.chris.video.Broadcaster;
import com.chris.video.SessionConfig;
import com.chris.video.view.GLCameraEncoderView;
import com.serenegiant.mediaaudiotest.R;

public class CameraFragment2 extends Fragment {
	private static final boolean DEBUG = false;	// TODO set false on release
	private static final String TAG = "CameraFragment2";

	/**
	 * for camera preview display
	 */
	private GLCameraEncoderView mCameraView;
	/**
	 * button for start/stop recording
	 */
	private ImageButton mRecordButton;
	/**
	 * muxer for audio/video recording
	 */
//	private MediaMuxerWrapper mMuxer;
//	
//	private FFmpegMuxer mFFmpegMuxer;
	
    private static Broadcaster mBroadcaster;        // Make static to survive Fragment re-creation

	public CameraFragment2() {
		// need default constructor
	}
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//        if (!Kickflip.readyToBroadcast()) {
//            Log.e(TAG, "Kickflip not properly prepared by BroadcastFragment's onCreate. SessionConfig: " + Kickflip.getSessionConfig() + " key " + Kickflip.getApiKey() + " secret " + Kickflip.getApiSecret());
//        } else {
            setupBroadcaster();
//        }
    }

	@Override
	public View onCreateView(final LayoutInflater inflater, final ViewGroup container, final Bundle savedInstanceState) {
		final View rootView = inflater.inflate(R.layout.fragment_main2, container, false);
		mCameraView = (GLCameraEncoderView)rootView.findViewById(R.id.cameraView2);
		mCameraView.setKeepScreenOn(true);
//		mCameraView.setVideoSize(1280, 720);
		mCameraView.setOnClickListener(mOnClickListener);
		mBroadcaster.setPreviewDisplay(mCameraView);
		mRecordButton = (ImageButton)rootView.findViewById(R.id.record_button2);
		mRecordButton.setOnClickListener(mOnClickListener);
		return rootView;
	}

	@Override
	public void onResume() {
		super.onResume();
		if (DEBUG) Log.v(TAG, "onResume:");
//		mCameraView.onResume();
        if (mBroadcaster != null)
            mBroadcaster.onHostActivityResumed();
	}

	@Override
	public void onPause() {
		if (DEBUG) Log.v(TAG, "onPause:");
//		stopRecording();
        if (mBroadcaster != null)
            mBroadcaster.onHostActivityPaused();
//		mCameraView.onPause();
		super.onPause();
	}
	
	

	@Override
	public void onDestroy() {
		// TODO Auto-generated method stub
		super.onDestroy();        
		if (mBroadcaster != null && !mBroadcaster.isRecording())
            mBroadcaster.release();
	}



	/**
	 * method when touch record button
	 */
	private final OnClickListener mOnClickListener = new OnClickListener() {
		@Override
		public void onClick(final View view) {
			switch (view.getId()) {
			case R.id.cameraView2:
//				final int scale_mode = (mCameraView.getScaleMode() + 1) % 4;
//				mCameraView.setScaleMode(scale_mode);
				break;
			case R.id.record_button2:
//				if (mMuxer == null && mFFmpegMuxer == null)
//					startRecording();
//				else
//					stopRecording();
	            if (mBroadcaster.isRecording()) {
	            	mRecordButton.setColorFilter(0);
	                mBroadcaster.stopRecording();
//	                hideLiveBanner();
	            } else {
	            	mRecordButton.setColorFilter(0xffff0000);
	                mBroadcaster.startRecording();
	                //stopMonitoringOrientation();
//	                v.setBackgroundResource(R.drawable.red_dot_stop);
	            }
				break;
			}
		}
	};
	
    protected void setupBroadcaster() {
        // By making the recorder static we can allow
        // recording to continue beyond this fragment's
        // lifecycle! That means the user can minimize the app
        // or even turn off the screen without interrupting the recording!
        // If you don't want this behavior, call stopRecording
        // on your Fragment/Activity's onStop()
//        if (getActivity().getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
            if (mBroadcaster == null) {
//                if (VERBOSE)
//                    Log.i(TAG, "Setting up Broadcaster for output " + Kickflip.getSessionConfig().getOutputPath() + " client key: " + Kickflip.getApiKey() + " secret: " + Kickflip.getApiSecret());
                // TODO: Don't start recording until stream start response, so we can determine stream type...
                Context context = getActivity().getApplicationContext();
                try {
                	SessionConfig sessionConfig = new SessionConfig.Builder("rtmp://120.132.75.127/demo/chris")
	                    .withVideoBitrate(1000 * 1000)
	                    .withPrivateVisibility(false)
	                    .withLocation(true)
	                    .withVideoResolution(720, 1280)
	                    .build();
                    mBroadcaster = new Broadcaster(context, sessionConfig);
//                    mBroadcaster.getEventBus().register(this);
//                    mBroadcaster.setBroadcastListener(Kickflip.getBroadcastListener());
//                    Kickflip.clearSessionConfig();
                } catch (IOException e) {
                    Log.e(TAG, "Unable to create Broadcaster. Could be trouble creating MediaCodec encoder.");
                    e.printStackTrace();
                }

            }
//        }
    }

//	/**
//	 * start resorcing
//	 * This is a sample project and call this on UI thread to avoid being complicated
//	 * but basically this should be called on private thread because prepareing
//	 * of encoder is heavy work
//	 */
//	private void startRecording() {
//		if (DEBUG) Log.v(TAG, "startRecording:");
//		try {
//			mRecordButton.setColorFilter(0xffff0000);	// turn red
//			mFFmpegMuxer = FFmpegMuxer.create("rtmp://120.132.75.127/demo/chris",FORMAT.RTMP);
////			mPublisher.init();
//			mMuxer = new MediaMuxerWrapper(".mp4");	// if you record audio only, ".m4a" is also OK.
//			if (true) {
//				// for video capturing
//				new MediaVideoEncoder(mFFmpegMuxer, mMuxer, mMediaEncoderListener, mCameraView.getVideoWidth(), mCameraView.getVideoHeight());
//			}
//			if (true) {
//				// for audio capturing
//				new MediaAudioEncoder(mFFmpegMuxer, mMuxer, mMediaEncoderListener);
//			}
//			mMuxer.prepare();
//			mMuxer.startRecording();
//		} catch (final IOException e) {
//			mRecordButton.setColorFilter(0);
//			Log.e(TAG, "startCapture:", e);
//		}
//	}
//
//	/**
//	 * request stop recording
//	 */
//	private void stopRecording() {
//		if (DEBUG) Log.v(TAG, "stopRecording:mMuxer=" + mMuxer);
//		mRecordButton.setColorFilter(0);	// return to default color
//		if (mMuxer != null) {
//			mMuxer.stopRecording();
//			mMuxer = null;
//			// you should not wait here
//		}
//		if (mFFmpegMuxer != null) {
//			mFFmpegMuxer.release();
//		}
//	}
	
    /**
     * Force this fragment to stop broadcasting.
     * Useful if your application wants to stop broadcasting
     * when a user leaves the Activity hosting this fragment
     */
    public void stopBroadcasting() {
        if (mBroadcaster.isRecording()) {
            mBroadcaster.stopRecording();
            mBroadcaster.release();
        }
    }

//	/**
//	 * callback methods from encoder
//	 */
//	private final MediaEncoder.MediaEncoderListener mMediaEncoderListener = new MediaEncoder.MediaEncoderListener() {
//		@Override
//		public void onPrepared(final MediaEncoder encoder) {
//			if (DEBUG) Log.v(TAG, "onPrepared:encoder=" + encoder);
//			if (encoder instanceof MediaVideoEncoder)
//				mCameraView.setVideoEncoder((MediaVideoEncoder)encoder);
//		}
//
//		@Override
//		public void onStopped(final MediaEncoder encoder) {
//			if (DEBUG) Log.v(TAG, "onStopped:encoder=" + encoder);
//			if (encoder instanceof MediaVideoEncoder)
//				mCameraView.setVideoEncoder(null);
//		}
//	};
}
