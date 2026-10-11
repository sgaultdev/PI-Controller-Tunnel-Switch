Setup (not tested)

Requires: 
1. RPI 4/5, or any with gadget functionality
2. "Dumb" Adapter that splits the type C cable between power and data but does not have its own USB controller hardware, with power adequate power supplied to the power side of the adapter.

OR

2. Powering the PI from the power headers rather than the type C port, leaving the type C port available.
3. A cable plugged from the switch/dock (type A or C) into the PI type C (adapter or just port, depending on 2.)

Running the startup.sh file should allow the pi to begin translating between devices plugged into its usb and the switch.

There should be 1-3 ms of added input delay
