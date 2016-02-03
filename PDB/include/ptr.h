#ifndef __PTR_INCLUDED__
#define __PTR_INCLUDED__

// UNDONE: this should move to ref.h and replace COMRefPtr as well in Orcas
template <class T, template <class S> class Trait > class AutoPtr {
public:
	AutoPtr(T* pt_ = NULL) {
		pt = pt_;
        if (pt != NULL) {
            Trait<T>::AddRef(pt);
        }
	}
    AutoPtr(AutoPtr<T, Trait> const& t) {
        pt = t.pt;
        if (pt != NULL) {
            Trait<T>::CopyAddRef(pt);
        }
    }

	~AutoPtr() {
        release();
	}

    AutoPtr<T, Trait>& operator=(T * pt_) {
        if (pt_ != NULL) {
            Trait<T>::AddRef(pt);
        }
        release();
        pt = pt_;
        return *this;
    }
    AutoPtr<T, Trait>& operator=(AutoPtr<T, Trait> const& t) {
        if (pt_ != NULL) {
            Trait<T>::CopyAddRef(pt);
        }
        release();
        pt = t.pt;
        return *this;
    }

    
    T ** operator&() {
        release();
        return &pt;
    }
    T* operator->() const { return pt; }
    operator T*() const { return pt; }
private:
    // close
    void release() {
        if (pt) {
            Trait<T>::Release(pt);
        }
        pt = NULL;
    }
	T* pt;
};

template <class T>
class AutoCloseTrait {
public:
    static void AddRef(T * pt) {};
    static void Release(T * pt) { pt->close(); }
};

template <class T>
class AutoReleaseTrait {
public:
    static void AddRef(T * pt) {};
    static void Release(T * pt) { pt->release(); }
};

template <class T> class AutoClosePtr : public AutoPtr<T, AutoCloseTrait> {};
template <class T> class AutoReleasePtr : public AutoPtr<T, AutoReleaseTrait> {};
#endif // !__PTR_INCLUDED__
