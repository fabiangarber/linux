// SPDX-License-Identifier: GPL-2.0

//! Simple Rust Character Device Driver (Structured like NVMe).
//!
//! This module registers a character device with major number 359,
//! logs messages when the module is loaded, accessed, and unloaded.

use kernel::{
    device::Device,
    driver::{self, Registration},
    error::code::*,
    file::{File, Operations},
    prelude::*,
    sync::Mutex,
    types::ARef,
};

module! {
    type: CharDeviceModule,
    name: "rchar_dev",
    author: "Your Name",
    description: "Character Device Structured like NVMe",
    license: "GPL",
}

struct CharDevice {
    data: Mutex<u8>,
}

impl CharDevice {
    fn new() -> Result<ARef<Self>> {
        ARef::try_new(Self {
            data: Mutex::new(0),
        })
    }
}

impl Operations for CharDevice {
    fn open(_ctx: &Registration<Self>, _file: &File) -> Result<Self::Wrapper> {
        pr_info!("Character device opened!\n");
        Ok(ARef::try_new(CharDevice {
            data: Mutex::new(0),
        })?)
    }

    fn release(_ctx: &Registration<Self>, _file: &File) {
        pr_info!("Character device closed!\n");
    }
}

struct CharDeviceModule {
    _registration: Pin<Box<Registration<Device>>>,
}

impl kernel::Module for CharDeviceModule {
    fn init(_name: &'static CStr, module: &'static ThisModule) -> Result<Self> {
        pr_info!("Character device module loaded!\n");

        // Register as a generic device, similar to NVMe.
        let reg = Registration::new_pinned(c_str!("rchar_dev"), module)?;
        pr_info!("Device registered!\n");

        Ok(Self { _registration: reg })
    }
}

impl Drop for CharDeviceModule {
    fn drop(&mut self) {
        pr_info!("Character device module unloaded!\n");
    }
}
