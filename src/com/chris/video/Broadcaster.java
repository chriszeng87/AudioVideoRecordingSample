package com.chris.video;

import java.io.File;
import java.io.IOException;
import java.util.ArrayDeque;

import android.content.Context;
import android.util.Log;
import android.util.Pair;

import com.chris.video.event.MuxerFinishedEvent;

import de.greenrobot.event.EventBus;

/**
 * Broadcasts HLS video and audio to <a href="https://kickflip.io">Kickflip.io</a>.
 * The lifetime of this class correspond to a single broadcast. When performing multiple broadcasts,
 * ensure reference to only one {@link io.kickflip.sdk.av.Broadcaster} is held at any one time.
 * {@link io.kickflip.sdk.fragment.BroadcastFragment} illustrates how to use Broadcaster in this pattern.
 * <p/>
 * Example Usage:
 * <p/>
 * <ol>
 * <li>Construct {@link Broadcaster()} with your Kickflip.io Client ID and Secret</li>
 * <li>Call {@link Broadcaster#setPreviewDisplay(io.kickflip.sdk.view.GLCameraView)} to assign a
 * {@link io.kickflip.sdk.view.GLCameraView} for displaying the live Camera feed.</li>
 * <li>Call {@link io.kickflip.sdk.av.Broadcaster#startRecording()} to begin broadcasting</li>
 * <li>Call {@link io.kickflip.sdk.av.Broadcaster#stopRecording()} to end the broadcast.</li>
 * </ol>
 *
 * @hide
 */
// TODO: Make HLS / RTMP Agnostic
public class Broadcaster extends AVRecorder {
    private static final String TAG = "Broadcaster";
    private static final boolean VERBOSE = false;
    private static final int MIN_BITRATE = 3 * 100 * 1000;              // 300 kbps
//    private final String VOD_FILENAME = "vod.m3u8";
    private Context mContext;
    private ArrayDeque<Pair<String, File>> mUploadQueue;
    private SessionConfig mConfig;
//    private BroadcastListener mBroadcastListener;
    private EventBus mEventBus;
//    private boolean mReadyToBroadcast;                                  // Kickflip user registered and endpoint ready
    private boolean mSentBroadcastLiveEvent;
    private int mVideoBitrate;
//    private File mManifestSnapshotDir;                                  // Directory where manifest snapshots are stored

    /**
     * Construct a Broadcaster with Session settings and Kickflip credentials
     *
     * @param context       the host application {@link android.content.Context}.
     * @param config        the Session configuration. Specifies bitrates, resolution etc.
     * @param CLIENT_ID     the Client ID available from your Kickflip.io dashboard.
     * @param CLIENT_SECRET the Client Secret available from your Kickflip.io dashboard.
     */
    public Broadcaster(Context context, SessionConfig config) throws IOException {
        super(config);
        init();
        mContext = context;
        mConfig = config;
        mConfig.getMuxer().setEventBus(mEventBus);
        mVideoBitrate = mConfig.getVideoBitrate();
        if (VERBOSE) Log.i(TAG, "Initial video bitrate : " + mVideoBitrate);
//        mManifestSnapshotDir = new File(mConfig.getOutputPath().substring(0, mConfig.getOutputPath().lastIndexOf("/") + 1), "m3u8");
//        mManifestSnapshotDir.mkdir();
//        mVodManifest = new File(mManifestSnapshotDir, VOD_FILENAME);
//        writeEventManifestHeader(mConfig.getHlsSegmentDuration());
//
//        mReadyToBroadcast = false;

    }

    private void init() {
//        mDeleteAfterUploading = true;
//        mLastRealizedBandwidthBytesPerSec = 0;
//        mNumSegmentsWritten = 0;
        mSentBroadcastLiveEvent = false;
        mEventBus = new EventBus();//"Broadcaster");
//        mEventBus.register(this);
    }

    /**
     * Set whether local recording files be deleted after successful upload. Default is true.
     * <p/>
     * Must be called before recording begins. Otherwise this method has no effect.
     *
     * @param doDelete whether local recording files be deleted after successful upload.
     */
    public void setDeleteLocalFilesAfterUpload(boolean doDelete) {
        if (!isRecording()) {
//            mDeleteAfterUploading = doDelete;
        }
    }

//    /**
//     * Set a Listener to be notified of basic Broadcast events relevant to
//     * updating a broadcasting UI.
//     * e.g: Broadcast begun, went live, stopped, or encountered an error.
//     * <p/>
//     * See {@link io.kickflip.sdk.av.BroadcastListener}
//     *
//     * @param listener
//     */
//    public void setBroadcastListener(BroadcastListener listener) {
//        mBroadcastListener = listener;
//    }

    /**
     * Set an {@link com.google.common.eventbus.EventBus} to be notified
     * of events between {@link io.kickflip.sdk.av.Broadcaster},
     * {@link io.kickflip.sdk.av.HlsFileObserver}, {@link io.kickflip.sdk.api.s3.S3BroadcastManager}
     * e.g: A HLS MPEG-TS segment or .m3u8 Manifest was written to disk, or uploaded.
     * See a list of events in {@link io.kickflip.sdk.event}
     *
     * @return
     */
    public EventBus getEventBus() {
        return mEventBus;
    }

    /**
     * Start broadcasting.
     * <p/>
     * Must be called after {@link Broadcaster#setPreviewDisplay(io.kickflip.sdk.view.GLCameraView)}
     */
    @Override
    public void startRecording() {
        super.startRecording();
//        mKickflip.startStream(mConfig.getStream(), new KickflipCallback() {
//            @Override
//            public void onSuccess(Response response) {
//                mCamEncoder.requestThumbnailOnDeltaFrameWithScaling(10, 1);
//                Log.i(TAG, "got StartStreamResponse");
//                checkArgument(response instanceof HlsStream, "Got unexpected StartStream Response");
//                onGotStreamResponse((HlsStream) response);
//            }
//
//            @Override
//            public void onError(KickflipException error) {
//                Log.w(TAG, "Error getting start stream response! " + error);
//            }
//        });
    }



    /**
     * Check if the broadcast has gone live
     *
     * @return
     */
    public boolean isLive() {
        return mSentBroadcastLiveEvent;
    }

    /**
     * Stop broadcasting and release resources.
     * After this call this Broadcaster can no longer be used.
     */
    @Override
    public void stopRecording() {
        super.stopRecording();
        mSentBroadcastLiveEvent = false;
//        if (mStream != null) {
//            if (VERBOSE) Log.i(TAG, "Stopping Stream");
//            mKickflip.stopStream(mStream, new KickflipCallback() {
//                @Override
//                public void onSuccess(Response response) {
//                    if (VERBOSE) Log.i(TAG, "Got stop stream response " + response);
//                }
//
//                @Override
//                public void onError(KickflipException error) {
//                    Log.w(TAG, "Error getting stop stream response! " + error);
//                }
//            });
//        }
    }


//    @Subscribe
    public void onMuxerFinished(MuxerFinishedEvent e) {
        // TODO: Broadcaster uses AVRecorder reset()
        // this seems better than nulling and recreating Broadcaster
        // since it should be usable as a static object for
        // bg recording
    }

//    private void sendStreamMetaData() {
////        if (mStream != null) {
////            mKickflip.setStreamInfo(mStream, null);
////        }
//    }
//
//    /**
//     * Construct an S3 Key for a given filename
//     *
//     */
//    private String keyForFilename(String fileName) {
//        return mStream.getAwsS3Prefix() + fileName;
//    }
//
//    /**
//     * Handle an upload, either submitting to the S3 client
//     * or queueing for submission once credentials are ready
//     *
//     * @param key  destination key
//     * @param file local file
//     */
//    private void queueOrSubmitUpload(String key, File file) {
//        if (mReadyToBroadcast) {
//            submitUpload(key, file);
//        } else {
//            if (VERBOSE) Log.i(TAG, "queueing " + key + " until S3 Credentials available");
//            queueUpload(key, file);
//        }
//    }
//
//    /**
//     * Queue an upload for later submission to S3
//     *
//     * @param key  destination key
//     * @param file local file
//     */
//    private void queueUpload(String key, File file) {
//        if (mUploadQueue == null)
//            mUploadQueue = new ArrayDeque<>();
//        mUploadQueue.add(new Pair<>(key, file));
//    }
//
//    /**
//     * Submit all queued uploads to the S3 client
//     */
//    private void submitQueuedUploadsToS3() {
//        if (mUploadQueue == null) return;
//        for (Pair<String, File> pair : mUploadQueue) {
//            submitUpload(pair.first, pair.second);
//        }
//    }
//
//    private void submitUpload(final String key, final File file) {
//        submitUpload(key, file, false);
//    }
//
//    private void submitUpload(final String key, final File file, boolean lastUpload) {
////        mS3Manager.queueUpload(mStream.getAwsS3Bucket(), key, file, lastUpload);
//    }
//
//    /**
//     * An S3 Upload completed.
//     * <p/>
//     * Called on a background thread
//     */
//    public void onS3UploadComplete(S3UploadEvent uploadEvent) {
//        if (VERBOSE) Log.i(TAG, "Upload completed for " + uploadEvent.getDestinationUrl());
//        if (uploadEvent.getDestinationUrl().contains(".m3u8")) {
//            onManifestUploaded(uploadEvent);
//        } else if (uploadEvent.getDestinationUrl().contains(".ts")) {
//            onSegmentUploaded(uploadEvent);
//        } else if (uploadEvent.getDestinationUrl().contains(".jpg")) {
//            onThumbnailUploaded(uploadEvent);
//        }
//    }

    public SessionConfig getSessionConfig() {
        return mConfig;
    }

//    private void writeEventManifestHeader(int targetDuration) {
////        FileUtils.writeStringToFile(
////                String.format("#EXTM3U\n" +
////                        "#EXT-X-PLAYLIST-TYPE:VOD\n" +
////                        "#EXT-X-VERSION:3\n" +
////                        "#EXT-X-MEDIA-SEQUENCE:0\n" +
////                        "#EXT-X-TARGETDURATION:%d\n", targetDuration + 1),
////                mVodManifest, false
////        );
//    }
//
//    private void appendLastManifestEntryToEventManifest(File sourceManifest, boolean lastEntry) {
////        String result = FileUtils.tail2(sourceManifest, lastEntry ? 3 : 2);
////        FileUtils.writeStringToFile(result, mVodManifest, true);
////        if (lastEntry) {
////            submitUpload(keyForFilename("vod.m3u8"), mVodManifest, true);
////            if (VERBOSE) Log.i(TAG, "Queued master manifest " + mVodManifest.getAbsolutePath());
////        }
//    }
//
//    S3BroadcastManager.S3RequestInterceptor mS3RequestInterceptor = new S3BroadcastManager.S3RequestInterceptor() {
//        @Override
//        public void interceptRequest(PutObjectRequest request) {
//            if (request.getKey().contains("index.m3u8")) {
//                if (mS3ManifestMeta == null) {
//                    mS3ManifestMeta = new ObjectMetadata();
//                    mS3ManifestMeta.setCacheControl("max-age=0");
//                }
//                request.setMetadata(mS3ManifestMeta);
//            }
//        }
//    };

}
