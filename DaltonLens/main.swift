//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

import Cocoa

let delegate = AppDelegate() //alloc main app's delegate class
NSApplication.shared().delegate = delegate //set as app's delegate

// Old versions:
// NSApplicationMain(C_ARGC, C_ARGV)
let errorCode = NSApplicationMain(CommandLine.argc, CommandLine.unsafeArgv);  //start of run loop
