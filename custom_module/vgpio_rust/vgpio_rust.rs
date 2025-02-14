// SPDX-License-Identifier: GPL-2.0
#![no_std]
#![no_main]

// Import common kernel definitions.
use core::pin::Pin;
use kernel::prelude::*;
use kernel::{c_str, ThisModule};
use kernel::error::Error;

// The following imports assume your kernel tree supports these high-level APIs.
// If any are missing, you may need to update your kernel or adjust the paths.
use kernel::driver::Registration;
use kernel::file::{File, FileOperations, OpenContext, PollContext};
use kernel::sync::SpinLock;
use kernel::wait_queue::WaitQueue;
use kernel::user_ptr::{copy_from_user, copy_to_user};

// Debug flag: if nonzero, extra output is printed.
// (For simplicity, we use a mutable static variable.)
static mut DEBUG: i32 = 0;

// IOCTL command definitions (must match your C definitions)
const GPIO_SET_VALUE: u32 = 0x40086701;
const GPIO_GET_VALUE: u32 = 0x80086702;
const NUM_GPIO_PINS: usize = 8;

/// This structure must have the same layout as your C struct.
#[repr(C)]
#[derive(Default)]
pub struct GpioDataUser {
    pub pin: i32,
    pub value: i32,
}

/// Internal state for the virtual GPIO device.
struct GpioState {
    values: [bool; NUM_GPIO_PINS],
    changed: bool,
}

impl Default for GpioState {
    fn default() -> Self {
        Self {
            values: [false; NUM_GPIO_PINS],
            changed: false,
        }
    }
}

/// Per‑open device state.
struct VirtualGpio {
    state: SpinLock<GpioState>,
    waitq: WaitQueue,
}

impl VirtualGpio {
    fn new() -> Self {
        Self {
            state: SpinLock::new(GpioState::default()),
            waitq: WaitQueue::new(),
        }
    }
}

impl FileOperations for VirtualGpio {
    // (We omit the declare_file_operations macro if it’s not available.)
    fn open(_ctx: &OpenContext<Self>) -> Result<Self> {
        pr_info!("Virtual GPIO device opened\n");
        Ok(VirtualGpio::new())
    }

    fn release(self, _ctx: &OpenContext<Self>) {
        pr_info!("Virtual GPIO device closed\n");
    }

    fn unlocked_ioctl(
        &self,
        _file: &File,
        cmd: u32,
        arg: usize,
    ) -> Result<i32> {
        let mut data = GpioDataUser::default();
        // Copy the structure from user space.
        unsafe {
            copy_from_user(&mut data, arg as *const GpioDataUser)?;
        }
        match cmd {
            GPIO_SET_VALUE => {
                if data.pin < 0 || (data.pin as usize) >= NUM_GPIO_PINS {
                    return Err(Error::from_kernel_errno(kernel::error::EINVAL));
                }
                {
                    let mut state = self.state.lock();
                    if state.values[data.pin as usize] != (data.value != 0) {
                        state.values[data.pin as usize] = data.value != 0;
                        state.changed = true;
                    }
                }
                self.waitq.wake_all();
                unsafe {
                    if DEBUG != 0 {
                        pr_info!("GPIO[{}] set to {}\n", data.pin, data.value);
                    }
                }
            }
            GPIO_GET_VALUE => {
                if data.pin < 0 || (data.pin as usize) >= NUM_GPIO_PINS {
                    return Err(Error::from_kernel_errno(kernel::error::EINVAL));
                }
                {
                    let state = self.state.lock();
                    data.value = if state.values[data.pin as usize] { 1 } else { 0 };
                }
                unsafe {
                    copy_to_user(arg as *mut GpioDataUser, &data)?;
                }
            }
            _ => return Err(Error::from_kernel_errno(kernel::error::EINVAL)),
        }
        Ok(0)
    }

    fn poll(&self, _file: &File, ctx: &mut PollContext) -> u32 {
        ctx.poll_wait(&self.waitq);
        let mut mask = 0;
        {
            let mut state = self.state.lock();
            if state.changed {
                // These poll mask bits match those in your C code.
                const POLLIN: u32 = 0x0001;
                const POLLRDNORM: u32 = 0x0040;
                mask |= POLLIN | POLLRDNORM;
                state.changed = false;
            }
        }
        mask
    }
}

/// Module instance: registers our character device.
struct VgpioModule {
    _registration: Pin<Box<Registration<VirtualGpio>>>,
}

impl kernel::Module for VgpioModule {
    fn init(module: &'static ThisModule) -> Result<Self> {
        let reg = Registration::new_pinned(
            c_str!("vgpio_rust"),
            256, // Major number must match your C code
            module,
            VirtualGpio::open,
        )?;
        pr_info!("Virtual GPIO driver loaded\n");
        Ok(VgpioModule {
            _registration: reg,
        })
    }
}

impl Drop for VgpioModule {
    fn drop(&mut self) {
        pr_info!("Virtual GPIO driver unloaded\n");
    }
}

module! {
    type: VgpioModule,
    name: "vgpio_rust",
    author: "Fabian T Garber",
    description: "Virtual GPIO Driver with Interrupts & poll() in Rust",
    license: "GPL",
}
