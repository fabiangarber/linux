// SPDX-License-Identifier: GPL-2.0

//! A simple Rust character device using the C API.

use kernel::prelude::*;
use kernel::bindings;
use kernel::c_str;
use kernel::error::code;
use core::ffi::c_int;
use core::marker::PhantomData;

module! {
    type: VgpioRust,
    name: "vgpio_rust",
    author: "Fabian T. Garber",
    description: "A simple Rust character device using the C API",
    license: "GPL",
}

const DEVICE_NAME: &CStr = c_str!("vgpio_rust");
const CLASS_NAME: &CStr = c_str!("vgpio_rust_class");

/// A newtype wrapper around the C file_operations structure.
/// We assert that it is safe to share (Sync) because it is only used as a static
/// read-only table of function pointers.
#[repr(transparent)]
struct VgpioFops(kernel::bindings::file_operations);
unsafe impl Sync for VgpioFops {}

static VGPIO_FOPS: VgpioFops = VgpioFops(kernel::bindings::file_operations {
    owner: core::ptr::null_mut(), // Consider setting THIS_MODULE if needed
    open: Some(vgpio_open),
    release: Some(vgpio_release),
    read: Some(vgpio_read as unsafe extern "C" fn(_, _, _, _) -> _),
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
                0, // Dynamically allocate major number
                0, // baseminor
                1, // count
                DEVICE_NAME.as_char_ptr(),
                &VGPIO_FOPS.0, // Pass the inner file_operations pointer
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

        // Compute dev_t using the proper shift.
        // Typically, dev_t is computed with MKDEV(major, minor),
        // where the major number is shifted by MINORBITS (usually 20 on modern systems).
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
        // Compute dev_t the same way here
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
    _inode: *mut kernel::bindings::inode,
    _file: *mut kernel::bindings::file,
) -> c_int {
    pr_info!("vgpio_rust: device opened.\n");
    0
}

#[no_mangle]
pub extern "C" fn vgpio_release(
    _inode: *mut kernel::bindings::inode,
    _file: *mut kernel::bindings::file,
) -> c_int {
    pr_info!("vgpio_rust: device released.\n");
    0
}

#[no_mangle]
pub extern "C" fn vgpio_read(
    _file: *mut kernel::bindings::file,
    buf: *mut u8,
    count: usize,
    _pos: *mut kernel::bindings::loff_t,
) -> isize {
    pr_info!("vgpio_rust: read called with count={}\n", count);

    let msg = b"Hello from vgpio_rust!\n";
    let len = msg.len();
    let to_copy = count.min(len);

    if buf.is_null() {
        pr_err!("vgpio_rust: error: buf is null!\n");
        return -(kernel::bindings::EFAULT as isize);
    }

    let res = unsafe {
        bindings::copy_to_user(
            buf as *mut core::ffi::c_void,
            msg.as_ptr() as *const core::ffi::c_void,
            to_copy as u64,
        )
    };

    if res != 0 {
        pr_err!("vgpio_rust: error: copy_to_user() failed with {}\n", res);
        return -(kernel::bindings::EFAULT as isize);
    }

    pr_info!("vgpio_rust: successfully copied {} bytes\n", to_copy);
    to_copy as isize
}

