// Portico win32: the COM declaration macros (C++ interfaces as abstract classes, the layout Direct3D 9 headers expect),
// IUnknown, and __uuidof for the interfaces declared with __CRT_UUID_DECL.
#ifndef PORTICO_UNKNWN_H
#define PORTICO_UNKNWN_H

#include <guiddef.h>

#ifdef __cplusplus
#ifndef interface
#define interface struct
#endif
#define PURE = 0
#define THIS_
#define THIS void
#define DECLARE_INTERFACE(iface) interface DECLSPEC_NOVTABLE iface
#define DECLARE_INTERFACE_(iface, baseiface) interface DECLSPEC_NOVTABLE iface : public baseiface
#define DECLARE_INTERFACE_IID(iface, iid) interface DECLSPEC_UUID(iid) DECLSPEC_NOVTABLE iface
#define DECLARE_INTERFACE_IID_(iface, baseiface, iid) interface DECLSPEC_UUID(iid) DECLSPEC_NOVTABLE iface : public baseiface
#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#define STDMETHODV(method) virtual HRESULT STDMETHODVCALLTYPE method
#define STDMETHODV_(type, method) virtual type STDMETHODVCALLTYPE method
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#define BEGIN_INTERFACE
#define END_INTERFACE
#define STDAPI extern "C" HRESULT STDAPICALLTYPE
#define STDAPI_(type) extern "C" type STDAPICALLTYPE

// __uuidof(Interface): the IID that the interface's header gave with __CRT_UUID_DECL.
template <typename T> struct portico_uuidof;
#define __CRT_UUID_DECL(type, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern "C++" { template <> struct portico_uuidof<type> { \
        static constexpr GUID value = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}; }; }
#define __uuidof(type) (portico_uuidof<type>::value)

struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;
    virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
};
#else
#error "Portico's Win32 headers support C++ COM interfaces only"
#endif

typedef IUnknown *LPUNKNOWN;
DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

#endif  // PORTICO_UNKNWN_H
