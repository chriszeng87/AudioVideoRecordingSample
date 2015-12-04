package com.chris.video;

/**
 * Created by Chris on 11/24/15.
 */
public class RTMPPublisher {

	public native void setAudioPipeId(int audioPipeId);
	
	public native void setVideoPipeId(int videoPipeId);
	
    public native void publish();
}
