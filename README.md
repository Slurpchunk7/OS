# OS
## i was bored
---
### Build instructions

1. **Make sure you have required dependencies**
```
aarch64-none-elf-gcc
aarch64-none-elf-g++
aarch64-none-elf-ld
aarch64-none-elf-as
```

You can install [here.](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
**Make sure that you install the right one. (Bare Metal Target)**

Then unzip and put it in **PATH**

You will also need **QEMU**
`sudo apt install qemu-system-aarch64 qemu-utils`
Or install the full package.
`sudo apt install qemu qemu-system`

2. **Build**

Go into your terminal and navigate to the folder.
Run `make`

3. **Running it**

Run `make run` in terminal