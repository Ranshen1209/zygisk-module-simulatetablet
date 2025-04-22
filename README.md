## zygisk-module-simulatetablet

A Zygisk-based module for simulating a tablet device on WeChat.

#### Features

- Enable WeChat tablet mode on Android devices.
- Directly based on Zygisk without using the Lsposed framework to avoid WeChat risk control.

#### Requirements

- Rooted Android device.
- Module manager with Zygisk enabled.

#### Installation

- Download the module from the [releases](https://github.com/Ranshen1209/zygisk-module-simulatetablet/releases) page.
- Open the module manager with Zygisk enabled and install.
- Restart the device after installation.

#### Compilation

- Clone the repository

```bash
git clone https://github.com/Ranshen1209/zygisk-module-simulatetablet.git
```

- Compile

```bash
cd zygisk-module-simulatetablet/module/jni && ndk-build
```

#### Credit

- [Magisk Zygisk Module Sample](https://github.com/topjohnwu/zygisk-module-sample)

- [I-Am-Pad](https://github.com/Houvven/I-Am-Pad)
