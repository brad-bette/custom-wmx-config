
# My Custom wmx configuration

## What is wmx?

Wmx is based on [wm2](https://www.all-day-breakfast.com/wm2/), and it retains a similar look and feel, but it provides: virtual desktops, minimal memory footprint, minimal menu's, and a very unique window border.
  
## What does my configuration add?

My configuration adds the following to wmx:

| Features | Keyboard shortcut's |
|----------------|-------------------------------|
|Keyboard virtual desktop navigation | `Alt-F#` each F key is represented by the virutal desktop #.
|Click to focus  |`N/a`  |
|Click to exit  |`Right click anywhere on the desktop and exit from wmx`|
  
## Building wmx

To build wmx, your going to need the following dependencies:
* libX11, libXext, libXmu, libXt, libSM, and libICE.
* basic build essentials
	* Ubuntu> `build essentials`
	* RHEL> `groupinstall "Development tools"`

## Configuration

By default from the original author of [wmx](https://www.all-day-breakfast.com/wmx/), only provides configuration via the provided `Config.h` file, and provides no support for startup scripts. Please do browse the `Config.h` file, and changes where you see fit. I highly recommend changing this line to a terminal emulator of your choice:
```
// What to run to get a new window (from the "New" menu option)
#define CONFIG_NEW_WINDOW_COMMAND "konsole"
``` 

In order to make this work, I have made my own startup script called `wmx-start.sh`. The contents of this file is listed bellow:

```bash
compton & # a compositor
nitrogen --restore & # background manager
xfce4-panel --disable-wm-check & # a useful dock
wmx # the window manager.
```
This file is called when selected by the lightdm session manager. The configuration for this window manager is displayed bellow (wmx.desktop):
```
[Desktop Entry]
Name=wmx
Comment=Log into using the wmx window manager
Exec=/usr/local/bin/wmx-start.sh
Type=Application
```

## Usage:

Aside from some of its limitations, it actually has some very unique navigation features. Virtual desktops are represented as TV-channels. For the purpose of following along, these "channels" will be referred to as virtual desktops. These virtual desktops can be navigated by:

*  rolling the mouse wheel on the desktop
* by pressing the `alt F#` key
	* The number of the function key is used in relation the virtual desktop number.
* by pressing `alt left or right arrow key`

By default there will always be a new virtual desktop created when an application is in the adjacent virtual desktop. (Think of it like Elementary OS, where it auto creates/deletes virtual desktops for you).

To move a window to another virtual desktop, all you have to do is hover the mouse on the title bar of the window, and roll the mouse wheel in the desired location.

### Window control

Windows can be maximized by adjusting the window size from the bottom right corner. It does minimize by clicking on the title bar square. These can be re-accessed by right clicking on the desktop and selecting the application on the menu. For more information, please see resources. 

### Keyboard control
You can also use the keyboard driven shortcut `alt-tab` to switch windows. 

Bellow is a list of key combinations that can be used with wmx. All of these are found in the `Config.h` file. There are plenty more (and a bit too much to list). But here is just a few:

* Raise key  `alt-Up`
* Lower key  `alt-Down`
* Normal window height `alt-Next`
* Maximize window `alt-Home`
* Unmaximize window `alt-End`

## Resources

There seems to be a limited amount of resources available for wmx. Its last update was in 2014, and while that is nearly 10 years ago....There just doesn't seem like a huge community out there using it. So here is a list of documentation resources that you can use to help make wmx more true to your needs:

* [Some good documentation on how to use wmx](https://github.com/bbidulock/wmx/blob/master/README)
* [A small write up on wmx](https://thornton2.com/unix/minimalism/wmx.html)
