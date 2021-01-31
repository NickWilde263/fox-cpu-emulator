# fox-cpu-emulator

to add custom devices add following section to _config.ini
```ini
[Device_name]
EntryPoint = "YourEntryPoint"
Path = "Path to your .so file of the device"
Argument = "Argument passed to your device"
```

Argument is optional
  Will be passed like normal C++ main function

EntryPoint is required
  Pointing to void (int, char**) function

Device communicate with CPU using PortIO.h
Read is blocking for devices for CPU 50 ms is the timeout
Implementation make device not reading itself after writing to a port
