package com.chris.video;

import java.io.FileDescriptor;
import java.io.IOException;

import android.os.ParcelFileDescriptor;

/**
 * Created by Chris on 11/24/15.
 */
public class RTMPPublisher {
	
	private ParcelFileDescriptor[] aPipeDes;
	private ParcelFileDescriptor[] vPipeDes;
	
	public RTMPPublisher() {
		try {
			aPipeDes = ParcelFileDescriptor.createPipe();
			vPipeDes = ParcelFileDescriptor.createPipe();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public void init() {
			setAudioPipeId(aPipeDes[0].getFd());
			setVideoPipeId(vPipeDes[0].getFd());
			publish();
	}
	
	public void release() {
		if (aPipeDes != null) {
			try {
				aPipeDes[0].close();
				aPipeDes[1].close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		if (vPipeDes != null) {
			try {
				vPipeDes[0].close();
				vPipeDes[1].close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
	}
	
	public FileDescriptor getVideoWritePipeId() {
		return vPipeDes[1].getFileDescriptor();
	}
	
	public FileDescriptor getAudioWritePipeId() {
		return aPipeDes[1].getFileDescriptor();
	}

	public native void setAudioPipeId(int audioPipeId);
	
	public native void setVideoPipeId(int videoPipeId);
	
    public native void publish();
    
    static {
    	System.loadLibrary("publisher");
    }
}
