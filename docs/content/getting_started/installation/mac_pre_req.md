## Prerequisites

##### (1) Xcode Command Line Tools

To install Command Line Tools (CLT) on your machine, simply open a terminal and run the following command.

```bash
xcode-select --install
```

If you do not have CLT installed, you will be presented with a dialog box allowing you to install CLT.

##### (2) XQuartz 2.7.9: [XQuartz-2.7.9.dmg](https://dl.bintray.com/xquartz/downloads/XQuartz-2.7.9.dmg)
<p></p>

##### (3) MOOSE Environment package (choose one):
  * Sierra 10.12: !moosepackage arch=osx10.12 return=link!
  * El Capitan 10.11: !moosepackage arch=osx10.11 return=link!

!!!warning "IMPORTANT: Close Open Terminals"
    If you have any opened terminals at this point, you must close and re-open them to use the MOOSE environment. The following instructions will ultimately fail if you do not.
