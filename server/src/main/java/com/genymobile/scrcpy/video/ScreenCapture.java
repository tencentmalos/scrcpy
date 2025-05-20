package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.control.PositionMapper;
import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Orientation;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.opengl.AffineOpenGLFilter;
import com.genymobile.scrcpy.opengl.MultiRegionOpenGLFilter;
import com.genymobile.scrcpy.opengl.OpenGLFilter;
import com.genymobile.scrcpy.opengl.OpenGLRunner;
import com.genymobile.scrcpy.util.AffineMatrix;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.IBinder;
import android.view.Surface;

import java.io.IOException;

public class ScreenCapture extends SurfaceCapture {

    private final VirtualDisplayListener vdListener;
    private final int displayId;
    private int maxSize;
    private final Rect crop;
    private final Rect crop2; // Added for second crop region
    private Orientation.Lock captureOrientationLock;
    private Orientation captureOrientation;
    private final float angle;

    private DisplayInfo displayInfo;
    private Size videoSize; // Will become the combined output size
    private Size videoSize1; // Size of the first cropped region after orientation
    private Size videoSize2; // Size of the second cropped region after orientation

    private final DisplaySizeMonitor displaySizeMonitor = new DisplaySizeMonitor();

    private IBinder display;
    private VirtualDisplay virtualDisplay;

    private AffineMatrix transform1; // Transform for the first region
    private AffineMatrix transform2; // Transform for the second region
    private OpenGLRunner glRunner;

    public ScreenCapture(VirtualDisplayListener vdListener, Options options) {
        this.vdListener = vdListener;
        this.displayId = options.getDisplayId();
        assert displayId != Device.DISPLAY_ID_NONE;
        this.maxSize = options.getMaxSize();
        this.crop = options.getCrop();
        this.crop2 = options.getCropRegion2(); // Initialize second crop region
        this.captureOrientationLock = options.getCaptureOrientationLock();
        this.captureOrientation = options.getCaptureOrientation();
        assert captureOrientationLock != null;
        assert captureOrientation != null;
        this.angle = options.getAngle();
    }

    @Override
    public void init() {
        displaySizeMonitor.start(displayId, this::invalidate);
    }

    @Override
    public void prepare() throws ConfigurationException {
        displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (displayInfo == null) {
            Ln.e("Display " + displayId + " not found\n" + LogUtils.buildDisplayListMessage());
            throw new ConfigurationException("Unknown display id: " + displayId);
        }

        if ((displayInfo.getFlags() & DisplayInfo.FLAG_SUPPORTS_PROTECTED_BUFFERS) == 0) {
            Ln.w("Display doesn't have FLAG_SUPPORTS_PROTECTED_BUFFERS flag, mirroring can be restricted");
        }

        Size displaySize = displayInfo.getSize();
        displaySizeMonitor.setSessionDisplaySize(displaySize);

        if (captureOrientationLock == Orientation.Lock.LockedInitial) {
            // The user requested to lock the video orientation to the current orientation
            captureOrientationLock = Orientation.Lock.LockedValue;
            captureOrientation = Orientation.fromRotation(displayInfo.getRotation());
        }

        boolean transposed = (displayInfo.getRotation() % 2) != 0;
        boolean locked = captureOrientationLock != Orientation.Lock.Unlocked;

        if (crop2 == null) {
            // Single region or no crop
            VideoFilter filter = new VideoFilter(displaySize);
            if (crop != null) {
                filter.addCrop(crop, transposed);
            }
            filter.addOrientation(displayInfo.getRotation(), locked, captureOrientation);
            filter.addAngle(angle);

            transform1 = filter.getInverseTransform(); // OpenGL needs inverse
            transform2 = null;
            videoSize1 = filter.getOutputSize(); // Store size before maxSize limit for potential later use
            videoSize = videoSize1.limit(maxSize).round8();
            videoSize2 = null;

            Ln.w(String.format("Crop2 not runing: %d x %d", videoSize.getWidth(), videoSize.getHeight()));
        } else {
            // Dual region crop
            // Process first crop region
            VideoFilter filter1 = new VideoFilter(displaySize);
            if (crop != null) {
                filter1.addCrop(crop, transposed);
            } else {
                // If crop1 is null but crop2 is not, we might default crop1 to full height, zero width,
                // or handle as an error. For now, assume crop1 is valid if crop2 is.
                // Or, more simply, if crop1 is null, its contribution to width/height is 0.
                // Let's assume if crop is null, it means it's not rendered or takes zero space.
                // For simplicity now, if crop is null, size1 will be 0x0.
                filter1.addCrop(new Rect(0, 0, 0, 0), transposed); // Effectively a zero-size crop
            }
            filter1.addOrientation(displayInfo.getRotation(), locked, captureOrientation);
            filter1.addAngle(angle); // Apply global angle to both
            videoSize1 = filter1.getOutputSize().round8();
            transform1 = filter1.getInverseTransform();

            // Process second crop region
            VideoFilter filter2 = new VideoFilter(displaySize);
            filter2.addCrop(crop2, transposed); // crop2 is guaranteed not null here
            filter2.addOrientation(displayInfo.getRotation(), locked, captureOrientation);
            filter2.addAngle(angle); // Apply global angle to both
            videoSize2 = filter2.getOutputSize().round8();
            transform2 = filter2.getInverseTransform();

            // Calculate combined videoSize for horizontal concatenation
            int combinedWidth = videoSize1.getWidth() + videoSize2.getWidth();
            int combinedHeight = Math.max(videoSize1.getHeight(), videoSize2.getHeight());
            if (combinedHeight == 0 && crop != null) { // if crop1 was null, but crop2 wasn't, use crop2's height
                combinedHeight = videoSize2.getHeight();
            }
            if (combinedHeight == 0 && crop2 != null && crop == null) { // if crop2 was null, but crop1 wasn't, use crop1's height
                combinedHeight = videoSize1.getHeight();
            }

            videoSize = new Size(combinedWidth, combinedHeight).round8();
            //videoSize = new Size(combinedWidth, combinedHeight).limit(maxSize).round8();

            Ln.w(String.format("Crop2 runing, displaySize: %d x %d", displaySize.getWidth(), displaySize.getHeight()));
            Ln.w(String.format("Crop2 runing, videoSize1: %d x %d", videoSize1.getWidth(), videoSize1.getHeight()));
            Ln.w(String.format("Crop2 runing, videoSize2: %d x %d", videoSize2.getWidth(), videoSize2.getHeight()));

            Ln.w(String.format("Crop2 runing, videoSize: %d x %d", videoSize.getWidth(), videoSize.getHeight()));
        }
    }

    @Override
    public void start(Surface surface) throws IOException {
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }

        Size inputSize;
        if (transform1 != null) { // If there is any filter
            inputSize = displayInfo.getSize();
            assert glRunner == null;

            if (transform2 != null && videoSize1 != null && videoSize2 != null) {
                // Dual crop mode: This will require a new or modified OpenGL filter and runner
                // For now, this will likely fail or render incorrectly as AffineOpenGLFilter expects one transform.
                // Placeholder for MultiRegionOpenGLFilter
                Ln.w("Dual crop rendering not fully implemented in OpenGL stage yet.");
                // We'll pass transform1 for now, which will render only the first region,
                // but the videoSize is the combined one. This will look weird but tests the pipeline.
                // OpenGLFilter glFilter = new AffineOpenGLFilter(transform1); // Old line
                OpenGLFilter glFilter = new MultiRegionOpenGLFilter(transform1, videoSize1, transform2, videoSize2);
                glRunner = new OpenGLRunner(glFilter);
                // The OpenGLRunner will render to the 'videoSize' (combined).
                // The glFilter (AffineOpenGLFilter) will use transform1 to source from inputSize.
                // The actual drawing area within videoSize for this first region would be videoSize1.
                // This setup is temporary.
                surface = glRunner.start(inputSize, videoSize, surface);
            } else {
                // Single crop mode (or no crop with orientation/angle)
                OpenGLFilter glFilter = new AffineOpenGLFilter(transform1);
                glRunner = new OpenGLRunner(glFilter);
                surface = glRunner.start(inputSize, videoSize, surface);
            }
        } else {
            // No filter (no crop, no orientation, no angle)
            inputSize = videoSize;
        }

        try {
            virtualDisplay = ServiceManager.getDisplayManager()
                    .createVirtualDisplay("scrcpy", inputSize.getWidth(), inputSize.getHeight(), displayId, surface);
            Ln.d("Display: using DisplayManager API");
        } catch (Exception displayManagerException) {
            try {
                display = createDisplay();

                Size deviceSize = displayInfo.getSize();
                int layerStack = displayInfo.getLayerStack();
                setDisplaySurface(display, surface, deviceSize.toRect(), inputSize.toRect(), layerStack);
                Ln.d("Display: using SurfaceControl API");
            } catch (Exception surfaceControlException) {
                Ln.e("Could not create display using DisplayManager", displayManagerException);
                Ln.e("Could not create display using SurfaceControl", surfaceControlException);
                throw new AssertionError("Could not create display");
            }
        }

        if (vdListener != null) {
            int virtualDisplayId;
            PositionMapper positionMapper;
            if (virtualDisplay == null || displayId == 0) {
                Size deviceSize = displayInfo.getSize();
                // TODO: PositionMapper needs to be aware of dual regions if transform2 is not null
                // For now, it will likely only work correctly for the first region or be incorrect.
                positionMapper = PositionMapper.create(videoSize, transform1, deviceSize);
                virtualDisplayId = displayId;
            } else {
                // TODO: PositionMapper needs to be aware of dual regions
                positionMapper = PositionMapper.create(videoSize, transform1, inputSize);
                virtualDisplayId = virtualDisplay.getDisplay().getDisplayId();
            }
            vdListener.onNewVirtualDisplay(virtualDisplayId, positionMapper);
        }
    }

    @Override
    public void stop() {
        if (glRunner != null) {
            glRunner.stopAndRelease();
            glRunner = null;
        }
    }

    @Override
    public void release() {
        displaySizeMonitor.stopAndRelease();

        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }
    }

    @Override
    public Size getSize() {
        return videoSize;
    }

    @Override
    public boolean setMaxSize(int newMaxSize) {
        maxSize = newMaxSize;
        return true;
    }

    private static IBinder createDisplay() throws Exception {
        // Since Android 12 (preview), secure displays could not be created with shell permissions anymore.
        // On Android 12 preview, SDK_INT is still R (not S), but CODENAME is "S".
        boolean secure = Build.VERSION.SDK_INT < AndroidVersions.API_30_ANDROID_11 || (Build.VERSION.SDK_INT == AndroidVersions.API_30_ANDROID_11
                && !"S".equals(Build.VERSION.CODENAME));
        return SurfaceControl.createDisplay("scrcpy", secure);
    }

    private static void setDisplaySurface(IBinder display, Surface surface, Rect deviceRect, Rect displayRect, int layerStack) {
        SurfaceControl.openTransaction();
        try {
            SurfaceControl.setDisplaySurface(display, surface);
            SurfaceControl.setDisplayProjection(display, 0, deviceRect, displayRect);
            SurfaceControl.setDisplayLayerStack(display, layerStack);
        } finally {
            SurfaceControl.closeTransaction();
        }
    }

    @Override
    public void requestInvalidate() {
        invalidate();
    }
}
