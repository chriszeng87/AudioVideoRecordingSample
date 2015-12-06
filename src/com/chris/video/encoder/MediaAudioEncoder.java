package com.chris.video.encoder;

/*
 * AudioVideoRecordingSample
 * Sample project to cature audio and video from internal mic/camera and save as MPEG4 file.
 *
 * Copyright (c) 2014-2015 saki t_saki@serenegiant.com
 *
 * File name: MediaAudioEncoder.java
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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.media.MediaRecorder;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import com.chris.video.RTMPPublisher;

public class MediaAudioEncoder extends MediaEncoder {
	private static final boolean DEBUG = false; // TODO set false on release
	private static final String TAG = "MediaAudioEncoder";

	private static final String MIME_TYPE = "audio/mp4a-latm";
	private static final int SAMPLE_RATE = 44100; // 44.1[KHz] is only setting
													// guaranteed to be
													// available on all devices.
	private static final int BIT_RATE = 64000;
	public static final int SAMPLES_PER_FRAME = 1024; // AAC,
														// bytes/frame/channel
	public static final int FRAMES_PER_BUFFER = 25; // AAC, frame/buffer/sec

	private ParcelFileDescriptor[] pipeDes;
	private FileOutputStream outputStream;
	private RTMPPublisher mPublisher;
	private ExecutorService mService;

	private AudioThread mAudioThread = null;

	public MediaAudioEncoder(RTMPPublisher publisher,
			final MediaMuxerWrapper muxer, final MediaEncoderListener listener) {
		super(muxer, listener);
		mPublisher = publisher;
	}

	@Override
	protected void prepare() throws IOException {
		if (DEBUG)
			Log.v(TAG, "prepare:");
		mTrackIndex = -1;
		mMuxerStarted = mIsEOS = false;
		mService = Executors.newSingleThreadExecutor();
		// prepare MediaCodec for AAC encoding of audio data from inernal mic.
		final MediaCodecInfo audioCodecInfo = selectAudioCodec(MIME_TYPE);
		if (audioCodecInfo == null) {
			Log.e(TAG, "Unable to find an appropriate codec for " + MIME_TYPE);
			return;
		}
		if (DEBUG)
			Log.i(TAG, "selected codec: " + audioCodecInfo.getName());

		try {
			pipeDes = ParcelFileDescriptor.createPipe();
		} catch (IOException e) {
			e.printStackTrace();
		}
		Log.e("Chris","----------pipeDes[0].getFd() = " + pipeDes[0].getFd());
		mPublisher.setAudioPipeId(pipeDes[0].getFd());

		outputStream = null;
		String fileName = "/sdcard/test2016.aac";
		try {
			outputStream = new FileOutputStream(pipeDes[1].getFileDescriptor());
			Log.d(TAG, "encoded output will be saved as " + fileName);
		} catch (Exception ioe) {
			Log.w(TAG, "Unable to create debug output file " + fileName);
			throw new RuntimeException(ioe);
		}
		mPublisher.publish();

		final MediaFormat audioFormat = MediaFormat.createAudioFormat(
				MIME_TYPE, SAMPLE_RATE, 1);
		audioFormat.setInteger(MediaFormat.KEY_AAC_PROFILE,
				MediaCodecInfo.CodecProfileLevel.AACObjectLC);
		audioFormat.setInteger(MediaFormat.KEY_CHANNEL_MASK,
				AudioFormat.CHANNEL_IN_MONO);
		audioFormat.setInteger(MediaFormat.KEY_BIT_RATE, BIT_RATE);
		audioFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT, 1);
		// audioFormat.setLong(MediaFormat.KEY_MAX_INPUT_SIZE,
		// inputFile.length());
		// audioFormat.setLong(MediaFormat.KEY_DURATION, (long)durationInMs );
		// byte[] header = new byte[4];
		// header[0] = (byte)0xAE;
		// header[1] = (byte)0x00;
		// header[2] = (byte)0x13;
		// header[3] = (byte)0x90;
		//
		// outputStream.write(header);

		if (DEBUG)
			Log.i(TAG, "format: " + audioFormat);
		mMediaCodec = MediaCodec.createEncoderByType(MIME_TYPE);
		mMediaCodec.configure(audioFormat, null, null,
				MediaCodec.CONFIGURE_FLAG_ENCODE);
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

	@Override
	protected void startRecording() {
		super.startRecording();
		// create and execute audio capturing thread using internal mic
		if (mAudioThread == null) {
			mAudioThread = new AudioThread();
			mAudioThread.start();
		}
	}

	@Override
	protected void release() {
		if (outputStream != null) {
			try {
				outputStream.close();
			} catch (IOException ioe) {
				Log.w(TAG, "failed closing debug file");
				throw new RuntimeException(ioe);
			}
		}
		mService.shutdown();
		try {
			pipeDes[0].close();
			pipeDes[1].close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		mAudioThread = null;
		super.release();
	}

	private static final int[] AUDIO_SOURCES = new int[] {
			MediaRecorder.AudioSource.MIC, MediaRecorder.AudioSource.DEFAULT,
			MediaRecorder.AudioSource.CAMCORDER,
			MediaRecorder.AudioSource.VOICE_COMMUNICATION,
			MediaRecorder.AudioSource.VOICE_RECOGNITION, };

	/**
	 * Thread to capture audio data from internal mic as uncompressed 16bit PCM
	 * data and write them to the MediaCodec encoder
	 */
	private class AudioThread extends Thread {
		@Override
		public void run() {
			android.os.Process
					.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
			try {
				final int min_buffer_size = AudioRecord.getMinBufferSize(
						SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO,
						AudioFormat.ENCODING_PCM_16BIT);
				int buffer_size = SAMPLES_PER_FRAME * FRAMES_PER_BUFFER;
				if (buffer_size < min_buffer_size)
					buffer_size = ((min_buffer_size / SAMPLES_PER_FRAME) + 1)
							* SAMPLES_PER_FRAME * 2;

				AudioRecord audioRecord = null;
				for (final int source : AUDIO_SOURCES) {
					try {
						audioRecord = new AudioRecord(source, SAMPLE_RATE,
								AudioFormat.CHANNEL_IN_MONO,
								AudioFormat.ENCODING_PCM_16BIT, buffer_size);
						if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED)
							audioRecord = null;
					} catch (final Exception e) {
						audioRecord = null;
					}
					if (audioRecord != null)
						break;
				}
				if (audioRecord != null) {
					try {
						if (mIsCapturing) {
							if (DEBUG)
								Log.v(TAG, "AudioThread:start audio recording");
							final ByteBuffer buf = ByteBuffer
									.allocateDirect(SAMPLES_PER_FRAME);
							int readBytes;
							audioRecord.startRecording();
							try {
								for (; mIsCapturing && !mRequestStop && !mIsEOS;) {
									// read audio data from internal mic
									buf.clear();
									readBytes = audioRecord.read(buf,
											SAMPLES_PER_FRAME);
									if (readBytes > 0) {
										// set audio data to encoder
										buf.position(readBytes);
										buf.flip();
										encode(buf, readBytes, getPTSUs());
										frameAvailableSoon();
									}
								}
								frameAvailableSoon();
							} finally {
								audioRecord.stop();
							}
						}
					} finally {
						audioRecord.release();
					}
				} else {
					Log.e(TAG, "failed to initialize AudioRecord");
				}
			} catch (final Exception e) {
				Log.e(TAG, "AudioThread#run", e);
			}
			if (DEBUG)
				Log.v(TAG, "AudioThread:finished");
		}
	}

	/**
	 * select the first codec that match a specific MIME type
	 * 
	 * @param mimeType
	 * @return
	 */
	private static final MediaCodecInfo selectAudioCodec(final String mimeType) {
		if (DEBUG)
			Log.v(TAG, "selectAudioCodec:");

		MediaCodecInfo result = null;
		// get the list of available codecs
		final int numCodecs = MediaCodecList.getCodecCount();
		LOOP: for (int i = 0; i < numCodecs; i++) {
			final MediaCodecInfo codecInfo = MediaCodecList.getCodecInfoAt(i);
			if (!codecInfo.isEncoder()) { // skipp decoder
				continue;
			}
			final String[] types = codecInfo.getSupportedTypes();
			for (int j = 0; j < types.length; j++) {
				if (DEBUG)
					Log.i(TAG, "supportedType:" + codecInfo.getName()
							+ ",MIME=" + types[j]);
				if (types[j].equalsIgnoreCase(mimeType)) {
					if (result == null) {
						result = codecInfo;
						break LOOP;
					}
				}
			}
		}
		return result;
	}

	static int m_channel = 2; // 双声道
	static int m_profile = 1; // AAC(Version 4) LC

	// static byte[] add_adts_header() {
	// byte[] adts_header = new byte[14];
	// 0101
	// 00
	// fff1 504
	// *p++ = 0xff; //syncword (0xfff, high_8bits)
	// *p = 0xf0; //syncword (0xfff, low_4bits)
	// *p |= (0 << 3); //ID (0, 1bit)
	// *p |= (0 << 1); //layer (0, 2bits)
	// *p |= 1; //protection_absent (1, 1bit)
	// p++;
	// *p = (unsigned char) ((m_profile & 0x3) << 6); //profile (profile, 2bits)
	// 01
	// *p |= ((m_sampleRateIndex & 0xf) << 2); //sampling_frequency_index
	// (sam_idx, 4bits) 0100
	// *p |= (0 << 1); //private_bit (0, 1bit)
	// *p |= ((m_channel & 0x4) >> 2); //channel_configuration (channel,
	// high_1bit) 0
	// p++;
	// *p = ((m_channel & 0x3) << 6); //channel_configuration (channel,
	// low_2bits) 10
	// *p |= (0 << 5); //original/copy (0, 1bit)
	// *p |= (0 << 4); //home (0, 1bit);
	// *p |= (0 << 3); //copyright_identification_bit (0, 1bit)
	// *p |= (0 << 2); //copyright_identification_start (0, 1bit)
	// *p |= ((frame_len & 0x1800) >> 11); //frame_length (value, high_2bits)
	// p++;
	// *p++ = (unsigned char) ((frame_len & 0x7f8) >> 3); //frame_length (value,
	// middle_8bits)
	// *p = (unsigned char) ((frame_len & 0x7) << 5); //frame_length (value,
	// low_3bits)
	// *p |= 0x1f; //adts_buffer_fullness (0x7ff, high_5bits)
	// p++;
	// *p = 0xfc; //adts_buffer_fullness (0x7ff, low_6bits)
	// *p |= 0; //number_of_raw_data_blocks_in_frame (0, 2bits);
	// p++;
	// }

	protected void postProcessEncodedData(final ByteBuffer byteBuffer,
			final BufferInfo bufferInfo) {
		mService.submit(new Runnable() {

			public void run() {
				if (outputStream != null) {
					if (bufferInfo.size > 0) {
						byte[] data = new byte[bufferInfo.size];
						byteBuffer.get(data);
						byteBuffer.position(bufferInfo.offset);
						byte[] header = new byte[7];
						header[0] = (byte) 0xFF;
						header[1] = (byte) 0xF1;
						header[2] = (byte) 0x50;
						header[3] = (byte) 0x40;
						;
						int frame_len = bufferInfo.size + 7;
						header[3] |= (byte) ((frame_len & 0x1800) >> 11);
						header[4] |= (byte) ((frame_len & 0x7f8) >> 3);
						header[5] = (byte) ((frame_len & 0x7) << 5);
						header[5] |= (byte) 0x1F;
						header[6] = (byte) 0xFC;
						header[6] |= 0;
						// byte[] packetHeader = new byte[2];
						// packetHeader[0] =(byte) 0xAE;
						// packetHeader[1] = (byte) 0x01;

						try {
							if (outputStream != null) {
								outputStream.write(header);
								outputStream.write(data);
							}
						} catch (IOException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
					}
				}
			}
		});
	}

	static {
		System.loadLibrary("publisher");
	}

}
