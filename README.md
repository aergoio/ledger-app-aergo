# ledger-app-aergo

Aergo application for Ledger Nano S

![ledger-app-aergo](https://user-images.githubusercontent.com/7624275/75844801-f048fb80-5db5-11ea-9957-3016f3970ccc.jpg)

The most secure method to protect your private key(s)

This Ledger application can be used to sign Aergo transactions, like:

* Transfer
* Stake & Unstake
* Votes (BP and DAO)
* Name System (create and update)
* Smart Contract function call

Support for Smart Contract deploy will come later.

## Requirements

To build and install the app on your Ledger Nano S you must set up the Ledger Nano S build environments.

Only Linux is supported to build the Ledger app so if you do not have one you can use a VM.

First set up the udev rules for the Ledger devices by executing this on a Linux terminal:

```
wget -q -O - https://raw.githubusercontent.com/LedgerHQ/udev-rules/master/add_udev_rules.sh | sudo bash
```

Then install the requirements in a virtual environnment by sourcing `prepare-devenv.sh`:

```
sudo apt install gcc-multilib g++-multilib python3-venv python3-dev libudev-dev libusb-1.0-0-dev
source prepare-devenv.sh
```

You can optionally follow the [Getting Started](https://ledger.readthedocs.io/en/latest/userspace/getting_started.html) instructions from Ledger.


## Installation

Connect the device to your computer and type:

```
make load
```


## Uninstall

```
make delete
```
