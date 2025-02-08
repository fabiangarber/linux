// SPDX-License-Identifier: GPL-2.0

//! Simple Hello World Rust kernel module.

use kernel::prelude::*;

module! {
    type: HelloWorld,
    name: "hello_world_rust",
    author: "Fabian T. Garber",
    description: "A simple Hello World Rust kernel module",
    license: "GPL",
}

struct HelloWorld;

impl kernel::Module for HelloWorld {
    fn init(_module: &'static ThisModule) -> Result<Self> {
        pr_info!("Hello, World! From a Rust module.\n");
        Ok(HelloWorld)
    }
}

impl Drop for HelloWorld {
    fn drop(&mut self) {
        pr_info!("Goodbye, World! From Rust a module.\n");
    }
}

