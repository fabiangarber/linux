// SPDX-License-Identifier: GPL-2.0

//! A simple Rust character device using the C API.
//! This module creates a virtual GPIO with 8 pins that can be set to 0 or 1 via ioctl commands.

#![allow(missing_docs)]

use kernel::prelude::*;
use kernel::bindings;
use kernel::c_str;
use kernel::error::code;
use core::ffi::{c_int, c_long};
use core::marker::PhantomData;

module! {
    type: VgpioRust,
    name: "vgpio_rust",
    author: "Fabian T Garber",
    description: "A virtual Rust GPIO module with 8 pins",
    license: "GPL",
}

/// Name of the device node.
const DEVICE_NAME: &CStr = c_str!("vgpio_rust");
/// Name of the class for udev.
const CLASS_NAME: &CStr = c_str!("vgpio");

/// IOCTL command constants.
/// These values must match the ones used in userspace.
const GPIO_SET_VALUE: u32 = 0x40086701;
const GPIO_GET_VALUE: u32 = 0x80086702;

/// Structure for GPIO data passed through ioctl.
/// This structure is C-compatible.
#[repr(C)]
struct GpioData {
    pin: c_int,
    value: c_int,
}

/// Global virtual GPIO pin states (8 pins supported).
/// A value of 0 means low; 1 means high.
static mut VGPIO_PINS: [c_int; 8] = [0; 8];

/// A newtype wrapper around the C file_operations structure.
#[repr(transparent)]
struct VgpioFops(kernel::bindings::file_operations);
unsafe impl Sync for VgpioFops {}

static VGPIO_FOPS: VgpioFops = VgpioFops(kernel::bindings::file_operations {
    owner: core::ptr::null_mut(), // Optionally set THIS_MODULE if needed.
    open: Some(vgpio_open),
    release: Some(vgpio_release),
    read: Some(vgpio_read as unsafe extern "C" fn(_, _, _, _) -> _),
    unlocked_ioctl: Some(vgpio_ioctl), // Our ioctl handler.
    write: None,
    llseek: None,
    poll: None,
    mmap: None,
    flush: None,
    ..unsafe { core::mem::zeroed() }
});

struct VgpioRust {
    major: i32,
    class: *mut bindings::class,
    device: *mut bindings::device,
    _marker: PhantomData<*mut ()>,
}

unsafe impl Send for VgpioRust {}
unsafe impl Sync for VgpioRust {}

impl kernel::Module for VgpioRust {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        pr_info!("vgpio_rust: module loaded.\n");

        let major = unsafe {
            bindings::__register_chrdev(
                0, // Allocate a major number dynamically.
                0, // Base minor.
                1, // Count.
                DEVICE_NAME.as_char_ptr(),
                &VGPIO_FOPS.0, // File operations.
            )
        };
        if major < 0 {
            pr_err!("vgpio_rust: failed to register device: {}\n", major);
            return Err(code::EINVAL.into());
        }

        let class = unsafe { bindings::class_create(CLASS_NAME.as_char_ptr()) };
        if class.is_null() {
            pr_err!("vgpio_rust: failed to create class\n");
            unsafe {
                bindings::__unregister_chrdev(major as u32, 0, 1, DEVICE_NAME.as_char_ptr());
            }
            return Err(code::EINVAL.into());
        }

        // Compute dev_t (major shifted left by 20 bits).
        let dev = ((major as u32) << 20) | 0;
        let device = unsafe {
            bindings::device_create(
                class,
                core::ptr::null_mut(),
                dev,
                core::ptr::null_mut(),
                DEVICE_NAME.as_char_ptr(),
            )
        };
        if device.is_null() {
            pr_err!("vgpio_rust: failed to create device\n");
            unsafe {
                bindings::class_destroy(class);
                bindings::__unregister_chrdev(major as u32, 0, 1, DEVICE_NAME.as_char_ptr());
            }
            return Err(code::EINVAL.into());
        }

        pr_info!("vgpio_rust: registered character device under /dev/vgpio_rust.\n");

        Ok(VgpioRust {
            major,
            class,
            device,
            _marker: PhantomData,
        })
    }
}

impl Drop for VgpioRust {
    fn drop(&mut self) {
        let dev = ((self.major as u32) << 20) | 0;
        unsafe {
            if !self.device.is_null() {
                bindings::device_destroy(self.class, dev);
            }
            if !self.class.is_null() {
                bindings::class_destroy(self.class);
            }
            bindings::__unregister_chrdev(self.major as u32, 0, 1, DEVICE_NAME.as_char_ptr());
        }
        pr_info!("vgpio_rust: module unloaded.\n");
    }
}

#[no_mangle]
pub extern "C" fn vgpio_open(
    _inode: *mut bindings::inode,
    _file: *mut bindings::file,
) -> c_int {
    pr_info!("vgpio_rust: device opened.\n");
    0
}

#[no_mangle]
pub extern "C" fn vgpio_release(
    _inode: *mut bindings::inode,
    _file: *mut bindings::file,
) -> c_int {
    pr_info!("vgpio_rust: device released.\n");
    0
}

#[no_mangle]
pub extern "C" fn vgpio_read(
    _file: *mut bindings::file,
    buf: *mut u8,
    count: usize,
    pos: *mut bindings::loff_t,
) -> isize {
    pr_info!("vgpio_rust: read called with count={}\n", count);

    let msg = b"Hello from vgpio_rust!";
    let len = msg.len();

    // Get the current file offset.
    let offset = unsafe { *pos as usize };

    // If offset is >= message length, signal EOF.
    if offset >= len {
        return 0;
    }

    // Calculate number of bytes to copy.
    let bytes_left = len - offset;
    let to_copy = count.min(bytes_left);

    if buf.is_null() {
        pr_err!("vgpio_rust: error: buf is null!\n");
        return -(bindings::EFAULT as isize);
    }

    let res = unsafe {
        bindings::copy_to_user(
            buf as *mut core::ffi::c_void,
            msg[offset..offset + to_copy].as_ptr() as *const core::ffi::c_void,
            to_copy as u64,
        )
    };

    if res != 0 {
        pr_err!("vgpio_rust: error: copy_to_user() failed with {}\n", res);
        return -(bindings::EFAULT as isize);
    }

    // Update file offset.
    unsafe {
        *pos += to_copy as i64;
    }

    to_copy as isize
}

/// IOCTL handler for the virtual GPIO pins.
/// This example supports two commands:
/// - GPIO_SET_VALUE: sets the value of a specified virtual GPIO pin
/// - GPIO_GET_VALUE: gets the current value of a specified virtual GPIO pin
#[no_mangle]
pub extern "C" fn vgpio_ioctl(
    _file: *mut bindings::file,
    cmd: u32,
    arg: u64,  // Third parameter type must be u64
) -> c_long {
    // Prepare a GpioData structure.
    let mut data: GpioData = unsafe { core::mem::zeroed() };

    match cmd {
        GPIO_SET_VALUE => {
            let ret = unsafe {
                bindings::copy_from_user(
                    &mut data as *mut GpioData as *mut core::ffi::c_void,
                    arg as *const core::ffi::c_void,
                    core::mem::size_of::<GpioData>() as u64,
                )
            };
            if ret != 0 {
                return -(bindings::EFAULT as c_long);
            }
            // Only support pins 0-7.
            if data.pin < 0 || data.pin >= 8 {
                return -(bindings::EINVAL as c_long);
            }
            unsafe {
                VGPIO_PINS[data.pin as usize] = data.value;
            }
//            pr_info!("vgpio_rust: virtual GPIO pin {} set to {}\n", data.pin, data.value);
        },
        GPIO_GET_VALUE => {
            let ret = unsafe {
                bindings::copy_from_user(
                    &mut data as *mut GpioData as *mut core::ffi::c_void,
                    arg as *const core::ffi::c_void,
                    core::mem::size_of::<GpioData>() as u64,
                )
            };
            if ret != 0 {
                return -(bindings::EFAULT as c_long);
            }
            // Only support pins 0-7.
            if data.pin < 0 || data.pin >= 8 {
                return -(bindings::EINVAL as c_long);
            }
            unsafe {
                data.value = VGPIO_PINS[data.pin as usize];
            }
            let ret = unsafe {
                bindings::copy_to_user(
                    arg as *mut core::ffi::c_void,
                    &data as *const GpioData as *const core::ffi::c_void,
                    core::mem::size_of::<GpioData>() as u64,
                )
            };
            if ret != 0 {
                return -(bindings::EFAULT as c_long);
            }
        },
        _ => return -(bindings::EINVAL as c_long),
    }
    0
}

