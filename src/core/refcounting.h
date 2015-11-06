// Copyright (c) 2014-2015 Dropbox, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PYSTON_CORE_REFCOUNTING_H
#define PYSTON_CORE_REFCOUNTING_H

#include "core/common.h"

struct _typeobject;

// We have a few helper classes for checking refcounting semantics.
// Different ones should be used in different situations:
// - BorrowedReference: pretty straightforward, represents an explicitly-annotated borrewed reference.  C API safe.
// - StoredReference: specialized for being stored as part of other data structures (as opposed to say, being a local variable).  C API safe but only as data structure members (not as parameters or return values).
// - OwnedReference: how to represent an owned reference.  not C API safe.
// - PassedReference: C API way of representing an owned reference.  to conform to the C API certain checking has to be turned off, so this type of reference should never be held inside pyston code -- it should only be passed.  For C API code inside Pyston, it should be immediately converted to an OwnedReference.

// The non-Borrowed references all imply ownership of the reference.
// Stored and Owned references are very similar, but StoredReferences imply that you never want to hand off your ownership (which you can do for efficiency with an OwnedReference).
// (Maybe Owned should be called Passable or HandOffable or Giveable)

// The compiler can usually figure out what refcounting operations to add based
// on the source and destination types.
// Borrowed references can convert to any other type; converting to another borrowed reference is a noop, and converting to any other kind will cause an incref.
// StoredReferences can similarly be converted.  Being converted to one of the owned references will cause an incref.
// OwnedReference can be converted to a BorrowedReference.  OwnedReferences *cannot* be automatically converted to any of the owned references, since it is not clear if the ownership of the reference should be transferred.  To convert and pass the reference, call ->pass() on the OwnedReference and the result is convertable to the others, which will remove ownership from the original OwnedReference (no incref will occur).  To convert and not pass the reference, call ->borrow() on the OwnedReference, which will borrow the reference which will then automatically get converted.
// PassedReferences are not meant to be used in Pyston code.  It's expected that if we do need to use them (a C API function in our code, for example) that we convert them to an OwnedReference (or a raw pointer for C API functions).  They could be allowed to convert to BorrowedReferences, but that would imply usage of them that would have been converted to an OwnedReference, so even though we would know what operation to do, we still disallow it.

// Owned and Passed references need to follow this constraint:
// Every variable of those types must have exactly one call to pass() or release()
// [borrowed references are free to obey this to ease switching between them]
// OwnedReferences can check this via runtime flags, but not so for PassedReferences.

/*
 * Conversion table.  A = auto conversion, && = pass() conversion, X = disallowed
 * from \ to:   Borrow  Stored  Owned   Passed
 * Borrowed     A       A       A       A
 * Stored       A       A       A       A
 * Owned        A       &&      &&      &&
 * Passed       X       X       &&      A
 */

// Common functions:
// - ->release(): call on an owned reference (owned/passed) to decref and release the ownership.
// - ->disown(): manually set the owned reference to not be owned any more.
//
// - borrow(): return a raw borrowed reference
// - take(): return a raw owned reference, leaving the current ref disowned.

// Other trickiness:
// - It is tricky to pass references to variadic functions: you have to explicitly call borrow().

namespace pyston {

class Box;
class BoxedClass;

#ifndef NDEBUG
#define REFHANDLE_CHECKING
#endif

// Forward declarations:
template <typename B = Box, bool Nullable = false> struct BorrowedReference;
template <typename B = Box, bool Nullable = false> struct StoredReference;
template <typename B = Box, bool Nullable = false> struct OwnedReference;
template <typename B = Box, bool Nullable = false> struct PassedReference;

// XXX figure out how we want to reduce typing:
// typedef OwnedReference<Box, false> OwnedRef;
#define Owned OwnedReference

// For converting raw pointers to handles:
template <typename B> inline BorrowedReference<B, false> borrowed(B* b);
template <typename B> inline BorrowedReference<B, true> xborrowed(B* b);
template <typename B> inline OwnedReference<B, false> owned(B* b);
template <typename B> inline OwnedReference<B, true> xowned(B* b);

template <typename B, bool Nullable> struct BorrowedReference {
private:
    B* b;

    BorrowedReference(B* b, int unused) : b(b) {}

    template<typename _B, bool _Nullable> friend class BorrowedReference;
    template<typename _B, bool _Nullable> friend class StoredReference;
    template<typename _B, bool _Nullable> friend class OwnedReference;
    template<typename _B, bool _Nullable> friend class PassedReference;

public:
    // Disallowed conversions:
    // For now, turn type errors into runtime errors.  Once we annotate enough things we can switch to
    // having it enforced by the type system.
    BorrowedReference(B* b) { RELEASE_ASSERT(0, "Un-annotated ptr->borrowed"); }

    operator B*&() { RELEASE_ASSERT(0, "Un-annotated borrowed->ptr"); }

    BorrowedReference<B, Nullable>& operator=(B* b) { RELEASE_ASSERT(0, "Un-annotated borrowed=ptr"); }


    // Getting stuff out:
    B*& borrow() { return b; }

    B* operator->() { return b; }

    explicit operator bool() { return (bool)b; }

    template <typename B2> bool operator==(B2* b) { return b == this->b; }
    bool operator==(std::nullptr_t) { return b == nullptr; }
    bool operator==(const BorrowedReference<B, Nullable>& r) { return r.b == this->b; }
    template <typename B2> bool operator!=(B2* b) { return b != this->b; }
    bool operator!=(std::nullptr_t) { return b != nullptr; }
    bool operator!=(const BorrowedReference<B, Nullable>& r) { return r.b != this->b; }

    B* take() {
        return OwnedReference<B, Nullable>(*this).take();
    }

    // Conversions:
    BorrowedReference(const BorrowedReference<B, Nullable>& r) = default;

    template <typename B2, bool Nullable2>
    BorrowedReference(const BorrowedReference<B2, Nullable2>& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);
        b = r.b;
    }

    template <typename B2, bool Nullable2>
    BorrowedReference(const OwnedReference<B2, Nullable2>& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);
        b = r.b;
    }

    template <typename B2, bool Nullable2>
    BorrowedReference(const StoredReference<B2, Nullable2>& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);
        b = r.b;
    }

    // Constructors:
    BorrowedReference() = default; // not really wanted, but for POD

    // Factory function to actually create instances (use the borrowed() helper for less typing):
    static BorrowedReference<B, Nullable> fromBorrowed(B* b) { return BorrowedReference(b, 0); }
};
static_assert(std::is_trivial<BorrowedReference<Box, false>>::value, "");
static_assert(std::is_pod<BorrowedReference<Box, false>>::value, "");
static_assert(sizeof(BorrowedReference<Box, false>) == sizeof(Box*), "");


template <typename B, bool Nullable> struct StoredReference {
private:
    typedef StoredReference<B, Nullable> Self;

    B* b;

    StoredReference(B* b, bool already_owns) noexcept;

    template<typename _B, bool _Nullable> friend class BorrowedReference;
    template<typename _B, bool _Nullable> friend class StoredReference;
    template<typename _B, bool _Nullable> friend class OwnedReference;
    template<typename _B, bool _Nullable> friend class PassedReference;

    void incref();

public:
    /////
    // Disallowed conversions:
    // For now, turn type errors into runtime errors.  Once we annotate enough things we can switch to
    // having it enforced by the type system.
    // StoredReference(B* b) { RELEASE_ASSERT(0, "Un-annotated ptr->stored"); }

    Self& operator=(B* b) { RELEASE_ASSERT(0, "Un-annotated stored=ptr"); }

    operator B*&() { RELEASE_ASSERT(0, "Un-annotated stored->ptr"); }

    /////
    // Getting stuff out:
    B* operator->() { return b; }

    explicit operator bool() { return (bool)b; }
    explicit operator intptr_t() { return (intptr_t)b; }
    explicit operator unsigned long() { return (unsigned long)b; }

    template <typename B2> bool operator==(B2* b) { return b == this->b; }
    bool operator==(std::nullptr_t) { return b == nullptr; }
    bool operator==(const Self& r) { return r.b == this->b; }
    template <typename B2> bool operator!=(B2* b) { return b != this->b; }
    bool operator!=(std::nullptr_t) { return b != nullptr; }
    bool operator!=(const Self& r) { return r.b != this->b; }

    template <typename B2, bool _Nullable> bool operator==(BorrowedReference<B2, _Nullable> r) { return b == r.b; }

    B*& borrow() {
        return b;
    }

    //
    template <typename B2, bool Nullable2>
    Self& operator=(const BorrowedReference<B2, Nullable2>& r) {
        return *this = OwnedReference<B2, Nullable2>(r);
    }

    template <typename B2, bool Nullable2>
    Self& operator=(OwnedReference<B2, Nullable2>&& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);

        decref();
        b = r.b;
        r.disown();
        return *this;
    }

    // misc:

    StoredReference() = default;
    // This shouldn't be used:
    ~StoredReference() { RELEASE_ASSERT(0, ""); } // maybe assert(std::uncaught_exception());
    void decref();

    void init(OwnedReference<B, Nullable>&& r) {
        assert(!b);
        r.disown();
        b = r.b;
    }

    /////
    // Factory functions:
    static Self fromOwned(B* b) { return Self(b, true); }

    static Self fromUnowned(B* b) { return Self(b, false); }
};

template <typename B, bool Nullable> struct OwnedReference {
private:
    typedef OwnedReference<B, Nullable> Self;

    B* b;

    void incref() noexcept;

    OwnedReference(B* b, bool already_owns) noexcept : b(b) {
        if (!already_owns)
            incref();
    }


#ifdef REFHANDLE_CHECKING
    bool owns = true;
    void own() {
        assert(!owns);
        owns = true;
    }
    void assertOwned(bool expected = true) { assert(owns == expected); }
#else
#error "fill this in"
#endif

    template<typename _B, bool _Nullable> friend class BorrowedReference;
    template<typename _B, bool _Nullable> friend class StoredReference;
    template<typename _B, bool _Nullable> friend class OwnedReference;
    template<typename _B, bool _Nullable> friend class PassedReference;

public:
#ifdef REFHANDLE_CHECKING
    ~OwnedReference() { assertOwned(false); }
#endif

    /////
    // Disallowed conversions:
    // For now, turn type errors into runtime errors.  Once we annotate enough things we can switch to
    // having it enforced by the type system.
    OwnedReference(B* b) { RELEASE_ASSERT(0, "Un-annotated ptr->owned"); }

    Self& operator=(B* b) { RELEASE_ASSERT(0, "Un-annotated ptr->owned"); }

    operator B*&() { RELEASE_ASSERT(0, "Un-annotated owned->ptr"); }

    /////
    // Getting stuff out:
    B* operator->() { return b; }

    explicit operator bool() { return (bool)b; }

    template <typename B2> bool operator==(B2* b) { return b == this->b; }
    bool operator==(std::nullptr_t) { return b == nullptr; }
    bool operator==(const Self& r) { return r.b == this->b; }
    template <typename B2> bool operator!=(B2* b) { return b != this->b; }
    bool operator!=(std::nullptr_t) { return b != nullptr; }
    bool operator!=(const Self& r) { return r.b != this->b; }

    // These should be avoided as much as possible:
    B*& borrow() {
        this->assertOwned(); // theoretically possible, but likely that a use-after-decref is a bug
        return b;
    }
    B* take() {
        this->assertOwned(); // theoretically possible, but likely that a use-after-decref is a bug
        this->disown();
        return b;
    }

    /////
    // Allowable conversions:

#ifndef REFHANDLE_CHECKING
    OwnedReference() = default;
    OwnedReference(const OwnedReference<B, Nullable>& r) = default;
#endif

    template <typename B2, bool Nullable2>
    OwnedReference(const BorrowedReference<B2, Nullable2>& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);
        b = r.b;
        incref();
    }

    template <typename B2, bool Nullable2>
    OwnedReference(const StoredReference<B2, Nullable2>& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);
        b = r.b;
        incref();
    }

    template <typename B2, bool Nullable2>
    OwnedReference(OwnedReference<B2, Nullable2>&& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);
        r.disown();
        b = r.b;
    }

    template <typename B2, bool Nullable2>
    OwnedReference(PassedReference<B2, Nullable2>&& r) {
        if (Nullable2 && !Nullable)
            assert(r.b);
        r.disown();
        b = r.b;
    }

    /////
    // Disowning:
#ifdef REFHANDLE_CHECKING
    void disown() {
        assert(owns);
        owns = false;
    }
#else
    void disown() {
    }
#endif
     
    void release();

    Self&& pass() {
        this->assertOwned();
        return std::move(*this);
    }

    /////
    // Factory functions:
    static Self fromOwned(B* b) { return Self(b, true); }

    static Self fromUnowned(B* b) { return Self(b, false); }
};
#ifndef REFHANDLE_CHECKING
static_assert(std::is_trivial<OwnedReference<Box, false>>::value, "");
static_assert(std::is_pod<OwnedReference<Box, false>>::value, "");
#endif


template <typename B, bool Nullable> struct PassedReference {
private:
    B* b;

    typedef PassedReference<B, Nullable> Self;

    PassedReference(B* b, bool already_owns) noexcept;

    template<typename _B, bool _Nullable> friend class BorrowedReference;
    template<typename _B, bool _Nullable> friend class StoredReference;
    template<typename _B, bool _Nullable> friend class OwnedReference;
    template<typename _B, bool _Nullable> friend class PassedReference;

public:
    // Disallowed conversions:
    // For now, turn type errors into runtime errors.  Once we annotate enough things we can switch to
    // having it enforced by the type system.
    PassedReference(B* b) { RELEASE_ASSERT(0, "Un-annotated ptr->passed"); }

    operator B*&() { RELEASE_ASSERT(0, "Un-annotated passed->ptr"); }

    PassedReference<B, Nullable>& operator=(B* b) { RELEASE_ASSERT(0, "Un-annotated ptr->passed"); }

    /////
    // Getting stuff out:
    B* operator->() { return b; }

    explicit operator bool() { return (bool)b; }

    template <typename B2> bool operator==(B2* b) { return b == this->b; }
    bool operator==(std::nullptr_t) { return b == nullptr; }
    bool operator==(const Self& r) { return r.b == this->b; }
    template <typename B2> bool operator!=(B2* b) { return b != this->b; }
    bool operator!=(std::nullptr_t) { return b != nullptr; }
    bool operator!=(const Self& r) { return r.b != this->b; }

    // Allowed conversions:
    PassedReference() = default;
    PassedReference(const PassedReference&) = default;

    template <typename B2>
    PassedReference(const BorrowedReference<B2, Nullable>& r) {
        b = r.b;
    }
    template <typename B2>
    PassedReference(const StoredReference<B2, Nullable>& r) {
        b = r.b;
    }
    template <typename B2>
    PassedReference(OwnedReference<B2, Nullable>&& r) {
        r.disown();
        b = r.b;
    }
    template <typename B2>
    PassedReference(PassedReference<B2, Nullable>&& r) {
        r.disown();
        b = r.b;
    }


    void disown() {}
    void release();
};
static_assert(std::is_trivial<PassedReference<Box, false>>::value, "");
static_assert(std::is_pod<PassedReference<Box, false>>::value, "");
static_assert(sizeof(PassedReference<Box, false>) == sizeof(Box*), "");

template <typename B> inline BorrowedReference<B, false> borrowed(B* b) {
    return BorrowedReference<B, false>::fromBorrowed(b);
}
template <typename B> inline BorrowedReference<B, true> xborrowed(B* b) {
    return BorrowedReference<B, true>::fromBorrowed(b);
}
template <typename B> inline OwnedReference<B, false> owned(B* b) {
    return OwnedReference<B, false>::fromOwned(b);
}
template <typename B> inline OwnedReference<B, true> xowned(B* b) {
    return OwnedReference<B, true>::fromOwned(b);
}
}

#endif
