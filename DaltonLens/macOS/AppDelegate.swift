//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

import Cocoa
import ServiceManagement
import GLKit

// @NSApplicationMain

class DaltonWindow: NSWindow {
    override var canBecomeKey: Bool {
            return true
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {

    private var glfwAppDelegate : NSApplicationDelegate?
    
    private var window : DaltonWindow?
    
    private var monitor: Any?
    
    private var daltonGUI : DaltonLensGUIMacOS
    
    var statusItem : NSStatusItem?
    
    let launchAtStartupMenuItem = NSMenuItem(title: "Launch at Startup",
                                             action: #selector(AppDelegate.toggleLaunchAtStartup(sender:)),
                                             keyEquivalent: "")
               
    override init () {
        
        let cmdAltCtrlMask = NSEvent.ModifierFlags(rawValue:
            NSEvent.ModifierFlags.command.rawValue
                | NSEvent.ModifierFlags.control.rawValue
                | NSEvent.ModifierFlags.option.rawValue)
        
        let controlShift = NSEvent.ModifierFlags(rawValue:
            NSEvent.ModifierFlags.control.rawValue
                | NSEvent.ModifierFlags.shift.rawValue)
                
        self.daltonGUI = DaltonLensGUIMacOS.init()
        
        super.init()
        
    }
    
    @objc func toggleLaunchAtStartup (sender: NSMenuItem) {
        
        let wasEnabled = (sender.state == NSControl.StateValue.on);
        let enabled = !wasEnabled;
        
        let changeApplied = SMLoginItemSetEnabled("org.daltonLens.DaltonLensLaunchAtLoginHelper" as CFString, enabled)
        
        if (changeApplied) {
            let defaults = UserDefaults.standard;
            defaults.set(enabled, forKey:"LaunchAtStartup")
            defaults.synchronize();
            
            sender.state = enabled ? NSControl.StateValue.on : NSControl.StateValue.off;
        }
        else
        {
            let alert = NSAlert()
            alert.window.title = "DaltonLens error";
            alert.alertStyle = NSAlert.Style.critical;
            alert.messageText = "Could not apply the change"
            alert.informativeText = "Please report the issue."
            alert.runModal();
        }
    }
        
    func createStatusBarItem () {
        
        // Do any additional setup after loading the view.
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        
        let icon = NSImage(named:"DaltonLensIcon_32x32")
        icon!.isTemplate = true
        
        statusItem!.image = icon!
        statusItem!.highlightMode = true
        
        let menu = NSMenu()
       
        let grabScreenItem = NSMenuItem(title: "Grab Screen Region", action: #selector(AppDelegate.grabScreenArea), keyEquivalent: " ")
        let cmdAltCtrlMask = NSEvent.ModifierFlags(rawValue:
            NSEvent.ModifierFlags.command.rawValue
                | NSEvent.ModifierFlags.control.rawValue
                | NSEvent.ModifierFlags.option.rawValue)
        grabScreenItem.keyEquivalentModifierMask = cmdAltCtrlMask
        menu.addItem(grabScreenItem)
        
        menu.addItem(NSMenuItem.separator())
        
        menu.addItem(NSMenuItem(title: "Help", action: #selector(AppDelegate.help), keyEquivalent: ""))
        menu.addItem(NSMenuItem.separator())
        
        menu.addItem(launchAtStartupMenuItem)
                
        menu.addItem(NSMenuItem.separator())
        menu.addItem(NSMenuItem(title: "Quit", action: #selector(AppDelegate.quit), keyEquivalent: "q"))
        
        statusItem!.menu = menu
    }
    
    func handleFlagsChangedEvent (event: NSEvent) -> NSEvent? {
        
//        let cmdControlAlt = NSEvent.ModifierFlags.command.rawValue |
//            NSEvent.ModifierFlags.control.rawValue |
//            NSEvent.ModifierFlags.option.rawValue
//
//        print("flags event detected: \(event)")
//
//        if ((event.modifierFlags.rawValue & (cmdControlAlt | NSEvent.ModifierFlags.shift.rawValue)) == cmdControlAlt) {
//            self.daltonPointerOverlay!.enabled = true
//            print ("CMD + CTRL + ALT!!!")
//        }
//        else {
//            self.daltonPointerOverlay!.enabled = false
//        }
        
        return event
    }
    
    func createGlobalShortcuts () {
                
        let cmdControlAlt = NSEvent.ModifierFlags.command.rawValue |
            NSEvent.ModifierFlags.control.rawValue |
            NSEvent.ModifierFlags.option.rawValue
        
        // https://stackoverflow.com/questions/47181412/monitoring-nsevents-using-addglobalmonitorforevents-missing-gesture-events
        let mask = NSEvent.EventTypeMask.flagsChanged;
        self.monitor = NSEvent.addGlobalMonitorForEvents(matching: mask) { event in let _ = self.handleFlagsChangedEvent(event:event); }
        NSEvent.addLocalMonitorForEvents(matching: mask, handler: self.handleFlagsChangedEvent)
    }
    
    func makeClosableWindow () {
        
        window = DaltonWindow.init(contentRect: NSMakeRect(300, 300, 640, 400),
                               styleMask: [NSWindow.StyleMask.titled,
                                           NSWindow.StyleMask.resizable],
                               backing: NSWindow.BackingStoreType.buffered,
                               defer: true);
        
        window!.makeKeyAndOrderFront(self);
        window!.isOpaque = true
    }
    
    func makeAssistiveWindow () {
        
        self.window = DaltonWindow.init(contentRect: NSScreen.main!.frame,
                                    styleMask: [NSWindow.StyleMask.titled],
                                    backing: NSWindow.BackingStoreType.buffered,
                                    defer: true);
        
        if let window = self.window {
        
            window.styleMask = NSWindow.StyleMask.borderless;
            window.level = NSWindow.Level(rawValue: Int(CGWindowLevelKey.assistiveTechHighWindow.rawValue));
            window.preferredBackingLocation = NSWindow.BackingLocation.videoMemory;
            window.collectionBehavior = [NSWindow.CollectionBehavior.stationary,
                                         // NSWindowCollectionBehavior.canJoinAllSpaces,
                                        NSWindow.CollectionBehavior.moveToActiveSpace,
                                        NSWindow.CollectionBehavior.ignoresCycle];
            
            // window.backgroundColor = NSColor.clear;
            // Using a tiny alpha value to make sure that windows below this window get refreshes.
            // Apps like Google Chrome or spotify won't redraw otherwise.
            window.alphaValue = 0.999;
            window.isOpaque = false;
            // window.orderFront (0);
            window.ignoresMouseEvents = true;
        }
    }
    
    func launchDaltonLensGUI () {
        assert (NSApp.delegate === self)
        self.daltonGUI.initialize ()
        assert (NSApp.delegate !== self)
        
        self.glfwAppDelegate = NSApp.delegate
        NSApp.delegate = self
    }
    
    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Insert code here to initialize your application
        
        createStatusBarItem ()
        
        createGlobalShortcuts ()
        
        let defaults = UserDefaults.standard;
        
        if (defaults.value(forKey: "LaunchAtStartup") != nil) {
            let enabled = defaults.bool(forKey:"LaunchAtStartup")
            launchAtStartupMenuItem.state = enabled ? NSControl.StateValue.on : NSControl.StateValue.off;
        }
        
        launchDaltonLensGUI()
        
        // Quick Solution: Place this in the AppDelegate in applicationDidFinishLaunching
        // https://stackoverflow.com/questions/5060345/keeping-window-on-top-when-switching-spaces
        NSWorkspace.shared.notificationCenter.addObserver(
            self,
            selector: #selector(notifySpaceChanged),
            name: NSWorkspace.activeSpaceDidChangeNotification,
            object: nil
        )
    }
    
    // and this in the same class
    @objc func notifySpaceChanged() {
        self.daltonGUI.notifySpaceChanged()
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }
    
    func applicationDidChangeScreenParameters(_ notification: Notification)
    {
        if let glfwAppDelegate = self.glfwAppDelegate {
            if glfwAppDelegate.applicationDidChangeScreenParameters != nil {
                glfwAppDelegate.applicationDidChangeScreenParameters!(notification)
            }
        }
    }

    func applicationDidHide(_ notification: Notification)
    {
        if let glfwAppDelegate = self.glfwAppDelegate
        {
            if glfwAppDelegate.applicationDidHide != nil {
                glfwAppDelegate.applicationDidHide!(notification)
            }
        }
    }

    @objc func grabScreenArea () {
        daltonGUI.grabScreenArea ();
    }
    
    @objc func help () {
        daltonGUI.helpRequested();
    }
    
    @objc func quit () {
        NSApplication.shared.terminate (self);
    }

}
