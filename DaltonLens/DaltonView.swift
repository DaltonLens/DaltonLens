//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

import Cocoa
import Metal
import MetalKit

func copyCGImageRectToClipboard(inImage: CGImage, rect:CGRect) {
    
    let width = Int(rect.width)
    let height = Int(rect.height)
    
    // Create the bitmap context
    let cgctx = CreateARGBBitmapContext(width:width, height:height)
    
    let drawRect = CGRect(x:0, y:0, width:width, height:height)
    
    // Draw the image to the bitmap context. Once we draw, the memory
    // allocated for the context for rendering will then contain the
    // raw image data in the specified color space.
    cgctx!.draw(inImage.cropping(to:rect)!, in:drawRect)
    
    let bitmapFormat : NSBitmapImageRep.Format = [ .thirtyTwoBitLittleEndian ]
    let dataPointer = cgctx!.data!.assumingMemoryBound(to: UInt8.self)
    NSLog("dataPointer = %p", dataPointer.pointee)
    
    let planes = UnsafeMutablePointer<UnsafeMutablePointer<UInt8>?>.allocate(capacity: 1)
    planes[0] = dataPointer
    
    let bitmap = NSBitmapImageRep.init(bitmapDataPlanes: planes,
                                       pixelsWide: width,
                                       pixelsHigh: height,
                                       bitsPerSample: 8,
                                       samplesPerPixel: 4,
                                       hasAlpha: true,
                                       isPlanar: false,
                                       colorSpaceName: NSColorSpaceName.deviceRGB,
                                       bitmapFormat: bitmapFormat,
                                       bytesPerRow: width*4,
                                       bitsPerPixel: 32)

    NSLog("bitmap = %@", bitmap!)
    
    let data = bitmap!.tiffRepresentation(using: NSBitmapImageRep.TIFFCompression.none, factor: 1.0);

    NSLog("tiff data isEmpty = %d count = %d", data!.isEmpty, data!.count)
    
    let pasteboard = NSPasteboard.general;
    pasteboard.clearContents()
    let success = pasteboard.setData(data!, forType: NSPasteboard.PasteboardType.tiff)
    assert (success, "Could not set the data of the pasteboard.")
}

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
    
    private var frameCountWhenEnteredNothing : Int32 = 0
    
    struct GrabScreenData {
        var isDragging : Bool = false
        var firstPointInWindow : CGPoint = CGPoint(x:0, y:0)
        var secondPointInWindow : CGPoint = CGPoint(x:0, y:0)
        var rectIsValid : Bool = false
        var rectIsFinalized : Bool = false
    }
    var grabScreenData = GrabScreenData()
    
    struct MetalData {

        let mtlRenderer : DLMetalRenderer
        let commandQueue : MTLCommandQueue
        let processor : DLMetalProcessor
        
        public init(_ device: MTLDevice) {
            commandQueue = device.makeCommandQueue()!
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
            let prevMode = self.cpuProcessor.processingMode
            if (prevMode == GrabScreenArea)
            {
                self.window!.ignoresMouseEvents = true
                self.grabScreenData = GrabScreenData()
            }
            
            if (newValue == GrabScreenArea)
            {
                self.window!.ignoresMouseEvents = false
                // Make sure we grab the focus to be able to click and drag.
                self.window!.makeKey()
                NSApp.activate(ignoringOtherApps: true)
            }
            
            self.cpuProcessor.processingMode = newValue
            self.mtl.processor.processingMode = newValue
            self.updateStateAndVisibility ()
        }
    }
    
    weak var daltonAppDelegate : AppDelegate?
    
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
    
        let doingNothing = (cpuProcessor.processingMode == Nothing)
        self.isPaused = doingNothing
        self.window?.setIsVisible(!doingNothing)
        
        if (doingNothing)
        {
            self.window!.alphaValue = 0.0
            self.frameCountWhenEnteredNothing = self.frameCount
        }
    }
        
    override func mouseDown(with event: NSEvent) {
        if (processingMode != GrabScreenArea) {
            return
        }
        
        NSLog("Mouse DOWN!!! %d", NSEvent.pressedMouseButtons)
        let mouseLocation = event.locationInWindow
        grabScreenData = GrabScreenData()
        grabScreenData.isDragging = true
        grabScreenData.firstPointInWindow = mouseLocation
    }
    
    override func mouseDragged(with event: NSEvent) {
        if (processingMode != GrabScreenArea) {
            return
        }
        
        if (!grabScreenData.isDragging) {
            return;
        }
        
        grabScreenData.secondPointInWindow = event.locationInWindow
        grabScreenData.rectIsValid = true
    }
    
    override func mouseUp(with event: NSEvent) {
        if (processingMode != GrabScreenArea) {
            return
        }
        
        if (!grabScreenData.isDragging) {
            return;
        }
        
        NSLog("Mouse UP!!! %d", NSEvent.pressedMouseButtons)
        grabScreenData.isDragging = false
        grabScreenData.secondPointInWindow = event.locationInWindow
        grabScreenData.rectIsFinalized = true
    }
    
    override var acceptsFirstResponder: Bool { return true }
    
    override func keyDown(with event: NSEvent) {
        if (processingMode != GrabScreenArea) {
            return
        }
        
        if (event.keyCode == UInt(kVK_Escape))
        {
            daltonAppDelegate!.setProcessingMode (mode: Nothing)
        }
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
        
        let globalMouseLocation = NSEvent.mouseLocation
        let screenToImageScale = Float(screenImage!.width)/Float(displayRect.width)
        
        let windowToImage = { (w:CGPoint) -> CGPoint in
            return CGPoint.init(x: w.x,
                                y: (displayRect.height-w.y))
        }
        
        let windowToTexture = { (w:CGPoint) -> CGPoint in
            return CGPoint.init(x: w.x / displayRect.width,
                                y: w.y / displayRect.height)
        }
        
        let mouseInImage = windowToImage(globalMouseLocation)
        // NSLog("Mouse location %f %f", mouseInImage.x, mouseInImage.y)
        
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
        
        if (processingMode == GrabScreenArea && grabScreenData.rectIsValid)
        {
            NSLog("Got a rect to grab!");
            let p1Texture = windowToTexture(grabScreenData.firstPointInWindow)
            let p2Texture = windowToTexture(grabScreenData.secondPointInWindow)
            uniformsBuffer.pointee.grabScreenRectangle.0 = Float(min(p1Texture.x, p2Texture.x));
            uniformsBuffer.pointee.grabScreenRectangle.1 = Float(min(p1Texture.y, p2Texture.y));
            uniformsBuffer.pointee.grabScreenRectangle.2 = Float(max(p1Texture.x, p2Texture.x));
            uniformsBuffer.pointee.grabScreenRectangle.3 = Float(max(p1Texture.y, p2Texture.y));
        }
        else
        {
            uniformsBuffer.pointee.grabScreenRectangle.0 = -1
            uniformsBuffer.pointee.grabScreenRectangle.1 = -1
            uniformsBuffer.pointee.grabScreenRectangle.2 = -1
            uniformsBuffer.pointee.grabScreenRectangle.3 = -1
        }
    
        if (processingMode == GrabScreenArea && grabScreenData.rectIsFinalized)
        {
            NSLog("Grab finalized!");
            let p1Image = windowToImage(grabScreenData.firstPointInWindow)
            let p2Image = windowToImage(grabScreenData.secondPointInWindow)
            
            let xmin = round(Double(min(p1Image.x, p2Image.x)));
            let ymin = round(Double(min(p1Image.y, p2Image.y)));
            let xmax = round(Double(max(p1Image.x, p2Image.x)));
            let ymax = round(Double(max(p1Image.y, p2Image.y)));
            
            let rectInImage = CGRect(x:xmin, y:ymin, width:(xmax-xmin), height:(ymax-ymin))
            copyCGImageRectToClipboard(inImage:screenImage!, rect:rectInImage)
            
            let geometry = String(format: "%dx%d+%d+%d", Int32(xmax-xmin), Int32(ymax-ymin), Int32(xmin), Int32(ymin));
            daltonAppDelegate!.launchDaltonViewer(argc: 4, argv: ["DaltonViewer", "--paste", "--geometry", geometry])
            daltonAppDelegate!.setProcessingMode (mode: Nothing)
        }
        
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
            
            commandBuffer!.present(drawable)
            commandBuffer!.commit()
        }
        
        frameCount = frameCount + 1;
        
        // Eliminate the ugly artefact when moving away from Nothing. If we don't wait until
        // the drawables all get flushed, then we get flicker with old drawable showing up on
        // screen.
        if (frameCount == (frameCountWhenEnteredNothing + 3))
        {
            self.window!.alphaValue = 0.999
        }
    }
    
}
