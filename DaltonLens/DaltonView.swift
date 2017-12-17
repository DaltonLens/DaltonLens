//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

import Cocoa
import Metal
import MetalKit

// For the CPU version, applies a function to a CGImage
func transformCGImage(inImage: CGImage,
                      by:(UnsafeMutablePointer<UInt8>, _:Int /* width */, _:Int /* height */) -> Void) -> CGImage? {
    
    // Create the bitmap context
    var cgctx = CreateARGBBitmapContext(width:inImage.width, height:inImage.height)
    if (cgctx == nil) {
        // error creating context
        return nil;
    }
    
    // Get image width, height. We'll use the entire image.
    let w = inImage.width;
    let h = inImage.height;
    let rect = CGRect(x:0, y:0, width:w, height:h)
    
    // Draw the image to the bitmap context. Once we draw, the memory
    // allocated for the context for rendering will then contain the
    // raw image data in the specified color space.
    cgctx!.draw(inImage, in:rect)
    
    // Now we can get a pointer to the image data associated with the bitmap
    // context.
    
    let data = cgctx!.data
    
    if (data != nil) {
        by (data!.assumingMemoryBound(to: UInt8.self), w, h)
    }
    
    let transformedImage = cgctx!.makeImage();
    
    cgctx = nil // release the context before releasing the data.
    
    if data != nil {
        free (data)
    }
    
    return transformedImage
}

func CreateARGBBitmapContext (width: Int, height: Int) -> CGContext?
{
    // Get image width, height. We'll use the entire image.
    let pixelsWide = width
    let pixelsHigh = height
    
    // Declare the number of bytes per row. Each pixel in the bitmap in this
    // example is represented by 4 bytes; 8 bits each of red, green, blue, and
    // alpha.
    let bitmapBytesPerRow = (pixelsWide * 4)
    let bitmapByteCount = (bitmapBytesPerRow * pixelsHigh)

    // Use the generic RGB color space.
    let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)
    if (colorSpace == nil) {
        NSLog("Error allocating color space\n");
        return nil;
    }

    // Allocate memory for image data. This is the destination in memory
    // where any drawing to the bitmap context will be rendered.
    let bitmapData = malloc( bitmapByteCount );
    if (bitmapData == nil)
    {
        NSLog ("Error: memory not allocated!");
        return nil;
    }
    
    // Create the bitmap context. We want pre-multiplied ARGB, 8-bits
    // per component. Regardless of what the source image format is
    // (CMYK, Grayscale, and so on) it will be converted over to the format
    // specified here by CGBitmapContextCreate.
    let context = CGContext (data: bitmapData,
                             width: pixelsWide,
                             height: pixelsHigh,
                             bitsPerComponent: 8,
                             bytesPerRow: bitmapBytesPerRow,
                             space: colorSpace!,
                             bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)
    
    if (context == nil) {
        free (bitmapData);
        NSLog ("Context not created!");
    }
    
    return context
}

class FPSMonitor {
    
    private var previousTime : Double = 0.0
    private var numSamples = 0
    private var accumulatedTime : Double = 0.0
    
    func tick () {
        
        let now = CACurrentMediaTime();
        
        if (previousTime == 0.0) {
            previousTime = now;
            accumulatedTime = 0
            numSamples = 0
            return;
        }
        
        let deltaT = (now-previousTime)
        accumulatedTime += deltaT;
        numSamples += 1
        previousTime = now;
        
        if (accumulatedTime > 1.0 && numSamples > 1) {
            debugPrint(String(format:"FPS: %.1f", Double(numSamples)/accumulatedTime))
            accumulatedTime = 0
            numSamples = 0
        }
    }
}

class PatchExtractorFromCGImage : NSObject {
    
    let cgContext : CGContext!
    let windowSize = 9
    let bytesPerPixel = 4
    let analyzer = DLColorUnderCursorAnalyzer.init()
    
    override init() {
        cgContext = CreateARGBBitmapContext(width: windowSize,height: windowSize)!
        cgContext.setShouldAntialias(false);
        cgContext.interpolationQuality = .none;
    }
    
    func rgbaValueNear (image:CGImage, p:CGPoint, screenToImageScale:CGFloat) -> DLRGBAPixel {
        
        // windowSizexwindowSize neighborhood.
        let croppedImage = image.cropping(to: CGRect.init(x: (p.x*screenToImageScale)-CGFloat(windowSize/2),
                                                          y: (p.y*screenToImageScale)-CGFloat(windowSize/2),
                                                          width: CGFloat(windowSize),
                                                          height: CGFloat(windowSize)))!
        
        let cgBounds = CGRect.init(x: 0, y: 0, width: windowSize, height: windowSize)
        
        cgContext.draw(croppedImage,
                       in: cgBounds,
                       byTiling: false)
        
        let data = cgContext.data!
        
        let buffer = data.assumingMemoryBound(to: UInt8.self)
        
        // Alternative. But darkest works best with a white background.
        //        let pixel = analyzer.dominantColor(inBuffer:buffer,
        //                                           width:cgContext.width,
        //                                           height:cgContext.height,
        //                                           bytesPerRow:cgContext.bytesPerRow)
        
        let pixel = analyzer.darkestColor(inBuffer:buffer,
                                          width:cgContext.width,
                                          height:cgContext.height,
                                          bytesPerRow:cgContext.bytesPerRow)
        
        
        
        return pixel;
    }
}

/**
 This view is going to be rendered on top of everything else. The content of the other windows
 is captured via CGWindowListCreateImage, which we load into an MTL texture, process, and render.
 */
class DaltonView: MTKView {

    private let fpsMonitor = FPSMonitor()
    
    private let cpuProcessor = DLCpuProcessor()
    
    // Useful to highlight the color under the cursor.
    private let patchExtractor = PatchExtractorFromCGImage.init()
    
    private var frameCount : Int32 = 0
    
    struct MetalData {

        let mtlRenderer : DLMetalRenderer
        let commandQueue : MTLCommandQueue
        let processor : DLMetalProcessor
        
        public init(_ device: MTLDevice) {
            commandQueue = device.makeCommandQueue()
            processor = DLMetalProcessor.init(device: device)
            mtlRenderer = DLMetalRenderer.init(processor: processor)
        }
    }
    var mtl : MetalData
    
    var processingMode: DLProcessingMode {
        get {
            return self.cpuProcessor.processingMode
        }
        set {
            self.cpuProcessor.processingMode = newValue
            self.mtl.processor.processingMode = newValue
            self.updateStateAndVisibility ()
        }
    }
    
    var blindnessType: DLBlindnessType {
        get {
            return self.cpuProcessor.blindnessType
        }
        set {
            self.cpuProcessor.blindnessType = newValue
            self.mtl.processor.blindnessType = newValue
            self.needsDisplay = true
        }
    }
    
    public init (frame frameRect: CGRect) {
        
        let defaultDevice = MTLCreateSystemDefaultDevice()
        mtl = MetalData.init(defaultDevice!)
        super.init (frame:frameRect, device:defaultDevice)
        
        // enableSetNeedsDisplay = true
        preferredFramesPerSecond = 30
    }
    
    public required init(coder: NSCoder) {
        let defaultDevice = MTLCreateSystemDefaultDevice()
        mtl = MetalData.init(defaultDevice!)
        super.init(coder: coder);
    }
    
    func updateStateAndVisibility () {
    
        let doingNothing = (cpuProcessor.processingMode == Nothing);
        self.isPaused = doingNothing;
        self.window?.setIsVisible(!doingNothing);
    }
    
    override func draw(_ dirtyRect: NSRect) {
        
        fpsMonitor.tick()
        
        if self.window == nil {
            return;
        }
        
        let rawWindowId = self.window!.windowNumber
        
        if rawWindowId <= 0 {
            NSLog("Invalid windowId \(rawWindowId)")
            return;
        }
        
        let windowId = CGWindowID(rawWindowId)
        
        let display = CGMainDisplayID()
        let displayRect = CGDisplayBounds(display)
        
        // Capture a screeshot of the other windows.
        var screenImage : CGImage? = CGWindowListCreateImage(displayRect,
                                                             [CGWindowListOption.optionOnScreenOnly],
                                                             // [CGWindowListOption.optionAll],
                                                             windowId, // kCGNullWindowID?
            []);
        
        if screenImage == nil {
            return
        }
        
        let mouseLocation = NSEvent.mouseLocation()
        let screenToImageScale = Float(screenImage!.width)/Float(displayRect.width)
        let mouseInImage = CGPoint.init(x: mouseLocation.x,
                                        y: (displayRect.height-mouseLocation.y))
        
        // Get the color under the cursor, used by some shaders.
        let cursorSRGBA = patchExtractor.rgbaValueNear(image:screenImage!,
                                                       p: mouseInImage,
                                                       screenToImageScale:CGFloat(screenToImageScale));
        
        let uniformsBuffer = mtl.mtlRenderer.uniformsBuffer!;
        uniformsBuffer.pointee.underCursorRgba.0 = Float(cursorSRGBA.r)/255.0;
        uniformsBuffer.pointee.underCursorRgba.1 = Float(cursorSRGBA.g)/255.0;
        uniformsBuffer.pointee.underCursorRgba.2 = Float(cursorSRGBA.b)/255.0;
        uniformsBuffer.pointee.underCursorRgba.3 = Float(cursorSRGBA.a)/255.0;
        uniformsBuffer.pointee.frameCount = frameCount;
    
        // Keeping this around, but we're now using Metal.
        let doCpuTransform = false;
        if (doCpuTransform)
        {
            screenImage = transformCGImage (inImage:screenImage!) {
                (srgbaBuffer, width, height) in
                self.cpuProcessor.transformSrgbaBuffer(srgbaBuffer, width:Int32(width), height:Int32(height))
            }
        }
        
        if let rpd = currentRenderPassDescriptor, let drawable = currentDrawable {
            rpd.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0)
            let commandBuffer = mtl.commandQueue.makeCommandBuffer()
            
            mtl.mtlRenderer.render(withScreenImage: screenImage!,
                                   commandBuffer: commandBuffer,
                                   renderPassDescriptor: rpd);
            
            commandBuffer.present(drawable)
            commandBuffer.commit()
        }
        
        frameCount = frameCount + 1;
    }
    
}
