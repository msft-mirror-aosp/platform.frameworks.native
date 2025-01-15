/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use binder_ndk_sys::{
    APersistableBundle, APersistableBundle_delete, APersistableBundle_dup,
    APersistableBundle_isEqual, APersistableBundle_new, APersistableBundle_size,
};
use std::ptr::NonNull;

/// A mapping from string keys to values of various types.
#[derive(Debug)]
pub struct PersistableBundle(NonNull<APersistableBundle>);

impl PersistableBundle {
    /// Creates a new `PersistableBundle`.
    pub fn new() -> Self {
        // SAFETY: APersistableBundle_new doesn't actually have any safety requirements.
        let bundle = unsafe { APersistableBundle_new() };
        Self(NonNull::new(bundle).expect("Allocated APersistableBundle was null"))
    }

    /// Returns the number of mappings in the bundle.
    pub fn size(&self) -> usize {
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`.
        unsafe { APersistableBundle_size(self.0.as_ptr()) }
            .try_into()
            .expect("APersistableBundle_size returned a negative size")
    }
}

impl Default for PersistableBundle {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for PersistableBundle {
    fn drop(&mut self) {
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of this `PersistableBundle`.
        unsafe { APersistableBundle_delete(self.0.as_ptr()) };
    }
}

impl Clone for PersistableBundle {
    fn clone(&self) -> Self {
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`.
        let duplicate = unsafe { APersistableBundle_dup(self.0.as_ptr()) };
        Self(NonNull::new(duplicate).expect("Duplicated APersistableBundle was null"))
    }
}

impl PartialEq for PersistableBundle {
    fn eq(&self, other: &Self) -> bool {
        // SAFETY: The wrapped `APersistableBundle` pointers are guaranteed to be valid for the
        // lifetime of the `PersistableBundle`s.
        unsafe { APersistableBundle_isEqual(self.0.as_ptr(), other.0.as_ptr()) }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn create_delete() {
        let bundle = PersistableBundle::new();
        drop(bundle);
    }

    #[test]
    fn duplicate_equal() {
        let bundle = PersistableBundle::new();
        let duplicate = bundle.clone();
        assert_eq!(bundle, duplicate);
    }
}
