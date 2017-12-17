# DaltonLens

This macOS program can apply color filters on top of your screen, such as the Daltonize algorithm. It is meant to help people with partial color blindness to see images differently and push the color information to the range of color that they can sense better.

It can apply the filters in real-time, using mostly GPU processing with Metal.

Here are the current filters:
- Simulate the three common types of color blindness
- Focus the color information on the most informative color channels via the Daltonize algorithm
- Switch red/blue green/blue
- Invert colors

## Getting started

The app is available in the app store. But to build it yourself, just clone the repository and open the project in Xcode 9. You will get a menu bar icon to activate the different modes. There are also global shortcuts `Ctrl+Alt+Cmd+[1-9]` to enable the different filters. `Ctrl+Alt+Cmd+0` disables filtering.

## License: BSD (with 2 clauses)

