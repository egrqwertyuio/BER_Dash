# BER_Dash

# Summary on build tooling research


1. Using PlatformIO with devcontainers 

The dev container approach involves allowing the docker container to use the USB passthrough to communicate with the microcontroller.

However the approach is quite challenging; It requires one to use the `usbipd` command since attached sessions are not conserved.
It is possible to script this and have it occur everytime on boot, but parsing the `usbipd` command everytime just to use Linux on a 
Windows system turned out to be more hassle than it was worth.



2. Using ESP32 SDK Directly

Requires you to make your own toolchain to compile the ESP32 software. Considering our time constraints, not worth our time.



3. Using PlatformIO on windows natively

This turned out to be the most straightforward way to program the denky32 esp MUC. It just requires `python 3.6` (which can be done with `python3-venv`) and thats it. VSCode makes it seamless.


4. Arduino IDE

Another straightforward solution with the cost of less control in configuration options for the esp. This should be used if PlatformIO 
isn't suitable



