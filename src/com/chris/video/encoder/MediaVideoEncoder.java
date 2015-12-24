package com.chris.video.encoder;

/*
 * AudioVideoRecordingSample
 * Sample project to cature audio and video from internal mic/camera and save as MPEG4 file.
 *
 * Copyright (c) 2014-2015 saki t_saki@serenegiant.com
 *
 * File name: MediaVideoEncoder.java
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

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.opengl.EGLContext;
import android.util.Log;
import android.view.Surface;

import com.chris.video.FFmpegMuxer;
import com.chris.video.glutils.RenderHandler;

public class MediaVideoEncoder extends MediaEncoder {
	private static final boolean DEBUG = false; // TODO set false on release
	private static final String TAG = "MediaVideoEncoder";

	private static final String MIME_TYPE = "video/avc";
	// parameters for recording
	private static final int FRAME_RATE = 25;
	private static final float BPP = 0.05f;

	private final int mWidth;
	private final int mHeight;
	private RenderHandler mRenderHandler;
	private Surface mSurface;
//	private ParcelFileDescriptor[] pipeDes;
//	private FileOutputStream outputStream;
	private boolean firstFrame;
	private ExecutorService mService;

	public MediaVideoEncoder(FFmpegMuxer ffmpegMuxer,
			final MediaMuxerWrapper muxer, final MediaEncoderListener listener,
			final int width, final int height) {
		super(ffmpegMuxer, muxer, listener);
		if (DEBUG)
			Log.i(TAG, "MediaVideoEncoder: ");
		mWidth = width;
		mHeight = height;
		mRenderHandler = RenderHandler.createHandler(TAG);
	}

	public boolean frameAvailableSoon(final float[] tex_matrix) {
		boolean result;
		if (result = super.frameAvailableSoon())
			mRenderHandler.draw(tex_matrix);
		return result;
	}

	@Override
	public boolean frameAvailableSoon() {
		boolean result;
		if (result = super.frameAvailableSoon())
			mRenderHandler.draw(null);
		return result;
	}

	@Override
	protected void prepare() throws IOException {
		if (DEBUG)
			Log.i(TAG, "prepare: ");
		mTrackIndex = -1;
		mMuxerStarted = mIsEOS = false;
		firstFrame = true;
		mService = Executors.newSingleThreadExecutor();

		final MediaCodecInfo videoCodecInfo = selectVideoCodec(MIME_TYPE);
		if (videoCodecInfo == null) {
			Log.e(TAG, "Unable to find an appropriate codec for " + MIME_TYPE);
			return;
		}
		if (DEBUG)
			Log.i(TAG, "selected codec: " + videoCodecInfo.getName());

//		try {
//			pipeDes = ParcelFileDescriptor.createPipe();
//		} catch (IOException e) {
//			e.printStackTrace();
//		}
//		mPublisher.setVideoPipeId(pipeDes[0].getFd());
//		Log.e("Chris","---------pipeDes[0].getFd() = " + pipeDes[0].getFd());
//		mPublisher.publish();
//
//		outputStream = null;
//		String fileName = "/sdcard/test2017.h264";
//		try {
//			outputStream = new FileOutputStream(mPublisher.getVideoWritePipeId());
//			Log.d(TAG, "encoded output will be saved as " + fileName);
//		} catch (Exception ioe) {
//			Log.w(TAG, "Unable to create debug output file " + fileName);
//			throw new RuntimeException(ioe);
//		}

		final MediaFormat format = MediaFormat.createVideoFormat(MIME_TYPE,
				mWidth, mHeight);
		format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
				MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface); // API >=
																		// 18
		format.setInteger(MediaFormat.KEY_BIT_RATE, calcBitRate());
		Log.e("Chris","--------calcBitRate() = " + calcBitRate());
		format.setInteger(MediaFormat.KEY_FRAME_RATE, FRAME_RATE);
		format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 5);
		if (DEBUG)
			Log.i(TAG, "format: " + format);

		mMediaCodec = MediaCodec.createEncoderByType(MIME_TYPE);
		mMediaCodec.configure(format, null, null,
				MediaCodec.CONFIGURE_FLAG_ENCODE);
		// get Surface for encoder input
		// this method only can call between #configure and #start
		mSurface = mMediaCodec.createInputSurface(); // API >= 18
		mMediaCodec.start();
		if (DEBUG)
			Log.i(TAG, "prepare finishing");
		if (mListener != null) {
			try {
				mListener.onPrepared(this);
			} catch (final Exception e) {
				Log.e(TAG, "prepare:", e);
			}
		}
	}

	public void setEglContext(final EGLContext shared_context, final int tex_id) {
		mRenderHandler.setEglContext(shared_context, tex_id, mSurface, true);
	}

	@Override
	protected void release() {
		if (DEBUG)
			Log.i(TAG, "release:");
		if (mSurface != null) {
			mSurface.release();
			mSurface = null;
		}
		if (mRenderHandler != null) {
			mRenderHandler.release();
			mRenderHandler = null;
		}
//		try {
//			if (outputStream != null) {
//				outputStream.close();
//			}
//		} catch (IOException e) {
//			// TODO Auto-generated catch block
//			e.printStackTrace();
//		}
		super.release();
	}

	private int calcBitRate() {
		final int bitrate = (int) (BPP * FRAME_RATE * mWidth * mHeight);
		Log.i(TAG,
				String.format("bitrate=%5.2f[Mbps]", bitrate / 1024f / 1024f));
		return 1000000;//bitrate;
	}

	/**
	 * select the first codec that match a specific MIME type
	 * 
	 * @param mimeType
	 * @return null if no codec matched
	 */
	protected static final MediaCodecInfo selectVideoCodec(final String mimeType) {
		if (DEBUG)
			Log.v(TAG, "selectVideoCodec:");

		// get the list of available codecs
		final int numCodecs = MediaCodecList.getCodecCount();
		for (int i = 0; i < numCodecs; i++) {
			final MediaCodecInfo codecInfo = MediaCodecList.getCodecInfoAt(i);

			if (!codecInfo.isEncoder()) { // skipp decoder
				continue;
			}
			// select first codec that match a specific MIME type and color
			// format
			final String[] types = codecInfo.getSupportedTypes();
			for (int j = 0; j < types.length; j++) {
				if (types[j].equalsIgnoreCase(mimeType)) {
					if (DEBUG)
						Log.i(TAG, "codec:" + codecInfo.getName() + ",MIME="
								+ types[j]);
					final int format = selectColorFormat(codecInfo, mimeType);
					if (format > 0) {
						return codecInfo;
					}
				}
			}
		}
		return null;
	}

	/**
	 * select color format available on specific codec and we can use.
	 * 
	 * @return 0 if no colorFormat is matched
	 */
	protected static final int selectColorFormat(
			final MediaCodecInfo codecInfo, final String mimeType) {
		if (DEBUG)
			Log.i(TAG, "selectColorFormat: ");
		int result = 0;
		final MediaCodecInfo.CodecCapabilities caps;
		try {
			Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
			caps = codecInfo.getCapabilitiesForType(mimeType);
		} finally {
			Thread.currentThread().setPriority(Thread.NORM_PRIORITY);
		}
		int colorFormat;
		for (int i = 0; i < caps.colorFormats.length; i++) {
			colorFormat = caps.colorFormats[i];
			if (isRecognizedViewoFormat(colorFormat)) {
				if (result == 0)
					result = colorFormat;
				break;
			}
		}
		if (result == 0)
			Log.e(TAG,
					"couldn't find a good color format for "
							+ codecInfo.getName() + " / " + mimeType);
		return result;
	}

	/**
	 * color formats that we can use in this class
	 */
	protected static int[] recognizedFormats;
	static {
		recognizedFormats = new int[] {
		// MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar,
		// MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
		// MediaCodecInfo.CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
		MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface, };
	}

	private static final boolean isRecognizedViewoFormat(final int colorFormat) {
		if (DEBUG)
			Log.i(TAG, "isRecognizedViewoFormat:colorFormat=" + colorFormat);
		final int n = recognizedFormats != null ? recognizedFormats.length : 0;
		for (int i = 0; i < n; i++) {
			if (recognizedFormats[i] == colorFormat) {
				return true;
			}
		}
		return false;
	}

	@Override
	protected void signalEndOfInputStream() {
		if (DEBUG)
			Log.d(TAG, "sending EOS to encoder");
		mMediaCodec.signalEndOfInputStream(); // API >= 18
//		if (outputStream != null) {
//			try {
//				outputStream.close();
//			} catch (IOException ioe) {
//				Log.w(TAG, "failed closing debug file");
//				throw new RuntimeException(ioe);
//			}
//		}
		mIsEOS = true;
//		mService.shutdown();
	}

	@Override
	protected void postProcessEncodedData(final ByteBuffer byteBuffer,
			final BufferInfo bufferInfo) {
		mService.submit(new Runnable() {

			public void run() {
		        if (((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0)) {
		                // Capture H.264 SPS + PPS Data
		        		if (mWeakFFmpegMuxer.get() != null) {
		        			mWeakFFmpegMuxer.get().captureH264MetaData(byteBuffer, bufferInfo);
//		                releaseOutputBufer(encoder, encodedData, bufferIndex, trackIndex);
		        		}
		                return;
		        }
				if (firstFrame) {
					String avcC = "000000016742801FDA02D0286806D0A1350000000168CE06E2";

					//TODO Chris
//					try {
//						outputStream.write(hexStr2Bytes(avcC));
//					} catch (IOException e) {
//						// TODO Auto-generated catch block
//						e.printStackTrace();
//					}
					Log.e("Chris","-------write avcc");
					firstFrame = false;
				}

				if (bufferInfo.size > 0) {
					byte[] data = new byte[bufferInfo.size];
					byteBuffer.get(data);
					byteBuffer.position(bufferInfo.offset);
//					try {
//						outputStream.write(data);
//					} catch (IOException ioe) {
//						Log.w(TAG, "failed writing debug data to file");
//						throw new RuntimeException(ioe);
//					}
				}
			}
		});

	}

	public static byte[] hexStr2Bytes(String src) {
		/* 对输入值进行规范化整理 */
		src = src.trim().replace(" ", "").toUpperCase(Locale.US);
		// 处理值初始化
		int m = 0, n = 0;
		int iLen = src.length() / 2; // 计算长度
		byte[] ret = new byte[iLen]; // 分配存储空间

		for (int i = 0; i < iLen; i++) {

			m = i * 2 + 1;
			n = m + 1;
			ret[i] = (byte) (Integer.decode("0x" + src.substring(i * 2, m)
					+ src.substring(m, n)) & 0xFF);
		}
		return ret;
	}

	static {
		System.loadLibrary("publisher");
	}
}
