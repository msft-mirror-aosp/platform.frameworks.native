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

use crate::{
    binder::AsNative,
    error::{status_result, StatusCode},
    impl_deserialize_for_unstructured_parcelable, impl_serialize_for_unstructured_parcelable,
    parcel::{BorrowedParcel, UnstructuredParcelable},
};
use binder_ndk_sys::{
    APersistableBundle, APersistableBundle_delete, APersistableBundle_dup,
    APersistableBundle_erase, APersistableBundle_getBoolean, APersistableBundle_getDouble,
    APersistableBundle_getInt, APersistableBundle_getLong, APersistableBundle_isEqual,
    APersistableBundle_new, APersistableBundle_putBoolean, APersistableBundle_putDouble,
    APersistableBundle_putInt, APersistableBundle_putLong, APersistableBundle_readFromParcel,
    APersistableBundle_size, APersistableBundle_writeToParcel,
};
use std::ffi::{CString, NulError};
use std::ptr::{null_mut, NonNull};

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

    /// Removes any entry with the given key.
    ///
    /// Returns an error if the given key contains a NUL character, otherwise returns whether there
    /// was any entry to remove.
    pub fn remove(&mut self, key: &str) -> Result<bool, NulError> {
        let key = CString::new(key)?;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`.
        Ok(unsafe { APersistableBundle_erase(self.0.as_ptr(), key.as_ptr()) != 0 })
    }

    /// Inserts a key-value pair into the bundle.
    ///
    /// If the key is already present then its value will be overwritten by the given value.
    ///
    /// Returns an error if the key contains a NUL character.
    pub fn insert_bool(&mut self, key: &str, value: bool) -> Result<(), NulError> {
        let key = CString::new(key)?;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`.
        unsafe {
            APersistableBundle_putBoolean(self.0.as_ptr(), key.as_ptr(), value);
        }
        Ok(())
    }

    /// Inserts a key-value pair into the bundle.
    ///
    /// If the key is already present then its value will be overwritten by the given value.
    ///
    /// Returns an error if the key contains a NUL character.
    pub fn insert_int(&mut self, key: &str, value: i32) -> Result<(), NulError> {
        let key = CString::new(key)?;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`.
        unsafe {
            APersistableBundle_putInt(self.0.as_ptr(), key.as_ptr(), value);
        }
        Ok(())
    }

    /// Inserts a key-value pair into the bundle.
    ///
    /// If the key is already present then its value will be overwritten by the given value.
    ///
    /// Returns an error if the key contains a NUL character.
    pub fn insert_long(&mut self, key: &str, value: i64) -> Result<(), NulError> {
        let key = CString::new(key)?;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`.
        unsafe {
            APersistableBundle_putLong(self.0.as_ptr(), key.as_ptr(), value);
        }
        Ok(())
    }

    /// Inserts a key-value pair into the bundle.
    ///
    /// If the key is already present then its value will be overwritten by the given value.
    ///
    /// Returns an error if the key contains a NUL character.
    pub fn insert_double(&mut self, key: &str, value: f64) -> Result<(), NulError> {
        let key = CString::new(key)?;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`.
        unsafe {
            APersistableBundle_putDouble(self.0.as_ptr(), key.as_ptr(), value);
        }
        Ok(())
    }

    /// Gets the boolean value associated with the given key.
    ///
    /// Returns an error if the key contains a NUL character, or `Ok(None)` if the key doesn't exist
    /// in the bundle.
    pub fn get_bool(&self, key: &str) -> Result<Option<bool>, NulError> {
        let key = CString::new(key)?;
        let mut value = false;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`. The value pointer must be valid because it comes
        // from a reference.
        if unsafe { APersistableBundle_getBoolean(self.0.as_ptr(), key.as_ptr(), &mut value) } {
            Ok(Some(value))
        } else {
            Ok(None)
        }
    }

    /// Gets the i32 value associated with the given key.
    ///
    /// Returns an error if the key contains a NUL character, or `Ok(None)` if the key doesn't exist
    /// in the bundle.
    pub fn get_int(&self, key: &str) -> Result<Option<i32>, NulError> {
        let key = CString::new(key)?;
        let mut value = 0;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`. The value pointer must be valid because it comes
        // from a reference.
        if unsafe { APersistableBundle_getInt(self.0.as_ptr(), key.as_ptr(), &mut value) } {
            Ok(Some(value))
        } else {
            Ok(None)
        }
    }

    /// Gets the i64 value associated with the given key.
    ///
    /// Returns an error if the key contains a NUL character, or `Ok(None)` if the key doesn't exist
    /// in the bundle.
    pub fn get_long(&self, key: &str) -> Result<Option<i64>, NulError> {
        let key = CString::new(key)?;
        let mut value = 0;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`. The value pointer must be valid because it comes
        // from a reference.
        if unsafe { APersistableBundle_getLong(self.0.as_ptr(), key.as_ptr(), &mut value) } {
            Ok(Some(value))
        } else {
            Ok(None)
        }
    }

    /// Gets the f64 value associated with the given key.
    ///
    /// Returns an error if the key contains a NUL character, or `Ok(None)` if the key doesn't exist
    /// in the bundle.
    pub fn get_double(&self, key: &str) -> Result<Option<f64>, NulError> {
        let key = CString::new(key)?;
        let mut value = 0.0;
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. The pointer returned by `key.as_ptr()` is guaranteed
        // to be valid for the lifetime of `key`. The value pointer must be valid because it comes
        // from a reference.
        if unsafe { APersistableBundle_getDouble(self.0.as_ptr(), key.as_ptr(), &mut value) } {
            Ok(Some(value))
        } else {
            Ok(None)
        }
    }
}

// SAFETY: The underlying *APersistableBundle can be moved between threads.
unsafe impl Send for PersistableBundle {}

// SAFETY: The underlying *APersistableBundle can be read from multiple threads, and we require
// `&mut PersistableBundle` for any operations which mutate it.
unsafe impl Sync for PersistableBundle {}

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

impl UnstructuredParcelable for PersistableBundle {
    fn write_to_parcel(&self, parcel: &mut BorrowedParcel) -> Result<(), StatusCode> {
        let status =
        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. `parcel.as_native_mut()` always returns a valid
        // parcel pointer.
            unsafe { APersistableBundle_writeToParcel(self.0.as_ptr(), parcel.as_native_mut()) };
        status_result(status)
    }

    fn from_parcel(parcel: &BorrowedParcel) -> Result<Self, StatusCode> {
        let mut bundle = null_mut();

        // SAFETY: The wrapped `APersistableBundle` pointer is guaranteed to be valid for the
        // lifetime of the `PersistableBundle`. `parcel.as_native()` always returns a valid parcel
        // pointer.
        let status = unsafe { APersistableBundle_readFromParcel(parcel.as_native(), &mut bundle) };
        status_result(status)?;

        Ok(Self(NonNull::new(bundle).expect(
            "APersistableBundle_readFromParcel returned success but didn't allocate bundle",
        )))
    }
}

impl_deserialize_for_unstructured_parcelable!(PersistableBundle);
impl_serialize_for_unstructured_parcelable!(PersistableBundle);

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

    #[test]
    fn get_empty() {
        let bundle = PersistableBundle::new();
        assert_eq!(bundle.get_bool("foo"), Ok(None));
        assert_eq!(bundle.get_int("foo"), Ok(None));
        assert_eq!(bundle.get_long("foo"), Ok(None));
        assert_eq!(bundle.get_double("foo"), Ok(None));
    }

    #[test]
    fn remove_empty() {
        let mut bundle = PersistableBundle::new();
        assert_eq!(bundle.remove("foo"), Ok(false));
    }

    #[test]
    fn insert_get_primitives() {
        let mut bundle = PersistableBundle::new();

        assert_eq!(bundle.insert_bool("bool", true), Ok(()));
        assert_eq!(bundle.insert_int("int", 42), Ok(()));
        assert_eq!(bundle.insert_long("long", 66), Ok(()));
        assert_eq!(bundle.insert_double("double", 123.4), Ok(()));

        assert_eq!(bundle.get_bool("bool"), Ok(Some(true)));
        assert_eq!(bundle.get_int("int"), Ok(Some(42)));
        assert_eq!(bundle.get_long("long"), Ok(Some(66)));
        assert_eq!(bundle.get_double("double"), Ok(Some(123.4)));
        assert_eq!(bundle.size(), 4);

        // Getting the wrong type should return nothing.
        assert_eq!(bundle.get_int("bool"), Ok(None));
        assert_eq!(bundle.get_long("bool"), Ok(None));
        assert_eq!(bundle.get_double("bool"), Ok(None));
        assert_eq!(bundle.get_bool("int"), Ok(None));
        assert_eq!(bundle.get_long("int"), Ok(None));
        assert_eq!(bundle.get_double("int"), Ok(None));
        assert_eq!(bundle.get_bool("long"), Ok(None));
        assert_eq!(bundle.get_int("long"), Ok(None));
        assert_eq!(bundle.get_double("long"), Ok(None));
        assert_eq!(bundle.get_bool("double"), Ok(None));
        assert_eq!(bundle.get_int("double"), Ok(None));
        assert_eq!(bundle.get_long("double"), Ok(None));

        // If they are removed they should no longer be present.
        assert_eq!(bundle.remove("bool"), Ok(true));
        assert_eq!(bundle.remove("int"), Ok(true));
        assert_eq!(bundle.remove("long"), Ok(true));
        assert_eq!(bundle.remove("double"), Ok(true));
        assert_eq!(bundle.get_bool("bool"), Ok(None));
        assert_eq!(bundle.get_int("int"), Ok(None));
        assert_eq!(bundle.get_long("long"), Ok(None));
        assert_eq!(bundle.get_double("double"), Ok(None));
        assert_eq!(bundle.size(), 0);
    }
}
