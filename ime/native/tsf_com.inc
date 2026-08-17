/* ───────── COM / TSF 실제 구현 (Windows 전용) ─────────
 *
 * tsf_textservice.cpp 에서 #include 된다. CUIME_TSF_STUB 미정의 시에만 컴파일.
 *
 * 이 파일이 DLL 의 실체다:
 *   - CheonjiinTextService : ITfTextInputProcessor + ITfKeyEventSink
 *                            + ITfCompositionSink + ITfThreadMgrEventSink
 *   - 편집 세션(ITfEditSession)에서 preedit/commit 을 실제 텍스트로 반영
 *   - DllGetClassObject / DllCanUnloadNow / DllRegisterServer / DllUnregisterServer
 *
 * 조합 로직은 한 줄도 여기 없다 — 전부 CompositionHost(=Shell=코어) 소관이다.
 */

#include <windows.h>
#include <msctf.h>
#include <olectl.h>
#include <new>

namespace cuime {
namespace tsf {

/* {A7B3C9D1-4E2F-4A5B-9C8D-1E2F3A4B5C6D} — 천지인 TSF 텍스트 서비스 */
static const CLSID CLSID_CuimeService =
    { 0xa7b3c9d1, 0x4e2f, 0x4a5b, { 0x9c, 0x8d, 0x1e, 0x2f, 0x3a, 0x4b, 0x5c, 0x6d } };
/* {B8C4DAE2-5F3A-4B6C-AD9E-2F3A4B5C6D7E} — 입력 프로파일 */
static const GUID GUID_CuimeProfile =
    { 0xb8c4dae2, 0x5f3a, 0x4b6c, { 0xad, 0x9e, 0x2f, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e } };

static const WCHAR kDesc[]     = L"\xCC9C\xC9C0\xC778";          /* 천지인 */
static const WCHAR kClsidKey[] = L"CLSID\\{A7B3C9D1-4E2F-4A5B-9C8D-1E2F3A4B5C6D}";
static const LANGID kLangKo    = 0x0412;                          /* ko-KR */

static HINSTANCE g_hInst = nullptr;
static LONG      g_refDll = 0;

/* ═══════════ 텍스트 서비스 ═══════════ */

class CheonjiinTextService final
    : public ITfTextInputProcessor
    , public ITfThreadMgrEventSink
    , public ITfKeyEventSink
    , public ITfCompositionSink
{
public:
    CheonjiinTextService() : ref_(1) { InterlockedIncrement(&g_refDll); }

    /* ── IUnknown ── */
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
            *ppv = static_cast<ITfTextInputProcessor*>(this);
        else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
            *ppv = static_cast<ITfThreadMgrEventSink*>(this);
        else if (IsEqualIID(riid, IID_ITfKeyEventSink))
            *ppv = static_cast<ITfKeyEventSink*>(this);
        else if (IsEqualIID(riid, IID_ITfCompositionSink))
            *ppv = static_cast<ITfCompositionSink*>(this);
        if (*ppv) { AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG c = InterlockedDecrement(&ref_);
        if (c == 0) { InterlockedDecrement(&g_refDll); delete this; }
        return c;
    }

    /* ── ITfTextInputProcessor ── */
    STDMETHODIMP Activate(ITfThreadMgr* tm, TfClientId cid) override {
        threadMgr_ = tm; if (tm) tm->AddRef();
        clientId_  = cid;

        /* 스레드 매니저 이벤트 싱크 */
        ITfSource* src = nullptr;
        if (threadMgr_ && SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, (void**)&src)) && src) {
            src->AdviseSink(IID_ITfThreadMgrEventSink,
                            static_cast<ITfThreadMgrEventSink*>(this), &cookieTM_);
            src->Release();
        }
        /* 키 이벤트 싱크 */
        ITfKeystrokeMgr* ksm = nullptr;
        if (threadMgr_ && SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&ksm)) && ksm) {
            ksm->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
            ksm->Release();
        }
        return S_OK;
    }

    STDMETHODIMP Deactivate() override {
        endComposition();
        if (threadMgr_) {
            ITfKeystrokeMgr* ksm = nullptr;
            if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&ksm)) && ksm) {
                ksm->UnadviseKeyEventSink(clientId_);
                ksm->Release();
            }
            ITfSource* src = nullptr;
            if (cookieTM_ != TF_INVALID_COOKIE &&
                SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, (void**)&src)) && src) {
                src->UnadviseSink(cookieTM_);
                src->Release();
                cookieTM_ = TF_INVALID_COOKIE;
            }
            threadMgr_->Release();
            threadMgr_ = nullptr;
        }
        return S_OK;
    }

    /* ── ITfThreadMgrEventSink (조합 무결성 유지용) ── */
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) override {
        /* 포커스 이동 시 조합 중이던 내용을 확정한다 (docs/02 §7.1) */
        host_.shell.core = press(host_.shell.core, "COMMIT");
        host_.sync();
        endComposition();
        return S_OK;
    }
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }

    /* ── ITfKeyEventSink ── */
    STDMETHODIMP OnSetFocus(BOOL) override { return S_OK; }

    STDMETHODIMP OnTestKeyDown(ITfContext*, WPARAM wp, LPARAM, BOOL* eaten) override {
        /* 실제로 처리하지 않고 "소비할 것인가"만 답한다 */
        *eaten = mapVirtualKey((UINT)wp, host_.padMode).empty() ? FALSE : TRUE;
        return S_OK;
    }
    STDMETHODIMP OnTestKeyUp(ITfContext*, WPARAM wp, LPARAM, BOOL* eaten) override {
        *eaten = (mapVirtualKey((UINT)wp, host_.padMode) == "HANJA") ? TRUE : FALSE;
        return S_OK;
    }

    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wp, LPARAM, BOOL* eaten) override {
        bool consumed = host_.onKeyDown((UINT)wp, GetKeyState(VK_SHIFT) < 0);
        *eaten = consumed ? TRUE : FALSE;
        if (consumed) requestEditSession(pic);
        else if (!host_.pendingCommit.empty()) requestEditSession(pic);
        return S_OK;
    }

    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wp, LPARAM, BOOL* eaten) override {
        /* 漢 키는 뗄 때 판정한다 (docs/00 §6) */
        curContext_ = pic;
        bool consumed = host_.onKeyUp((UINT)wp);
        curContext_ = nullptr;
        *eaten = consumed ? TRUE : FALSE;
        if (consumed) requestEditSession(pic);
        return S_OK;
    }

    STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override {
        *eaten = FALSE; return S_OK;
    }

    /* ── ITfCompositionSink ── */
    STDMETHODIMP OnCompositionTerminated(TfEditCookie, ITfComposition*) override {
        /* 앱이 조합을 강제 종료 — 코어도 확정시켜 상태를 맞춘다 */
        host_.shell.core = press(host_.shell.core, "COMMIT");
        host_.sync();
        if (composition_) { composition_->Release(); composition_ = nullptr; }
        return S_OK;
    }

    /* 편집 세션에서 호출되는 실제 반영 루틴 */
    HRESULT applyEdit(TfEditCookie ec, ITfContext* pic);

private:
    friend class EditSession;

    void endComposition() {
        if (composition_) { composition_->Release(); composition_ = nullptr; }
    }
    void requestEditSession(ITfContext* pic);

    LONG ref_;
    ITfThreadMgr*  threadMgr_ = nullptr;
    TfClientId     clientId_  = 0;   /* TF_CLIENTID_NULL (MinGW 헤더 미정의) */
    DWORD          cookieTM_  = TF_INVALID_COOKIE;
    ITfComposition* composition_ = nullptr;
    ITfContext*    curContext_ = nullptr;

public:
    CompositionHost host_;
};

/* ═══════════ 편집 세션 ═══════════ */

class EditSession final : public ITfEditSession {
public:
    EditSession(CheonjiinTextService* svc, ITfContext* pic)
        : ref_(1), svc_(svc), pic_(pic) { svc_->AddRef(); pic_->AddRef(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
            *ppv = static_cast<ITfEditSession*>(this);
        if (*ppv) { AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG c = InterlockedDecrement(&ref_);
        if (c == 0) { svc_->Release(); pic_->Release(); delete this; }
        return c;
    }
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        return svc_->applyEdit(ec, pic_);
    }
private:
    LONG ref_;
    CheonjiinTextService* svc_;
    ITfContext* pic_;
};

inline void CheonjiinTextService::requestEditSession(ITfContext* pic) {
    if (!pic) return;
    EditSession* es = new (std::nothrow) EditSession(this, pic);
    if (!es) return;
    HRESULT hr;
    pic->RequestEditSession(clientId_, es, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    es->Release();
}

inline HRESULT CheonjiinTextService::applyEdit(TfEditCookie ec, ITfContext* pic) {
    if (!pic) return E_INVALIDARG;

    /* 1) 확정 문자열 삽입 */
    std::string commit = host_.takeCommit();
    if (!commit.empty()) {
        ITfInsertAtSelection* ias = nullptr;
        if (SUCCEEDED(pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&ias)) && ias) {
            std::vector<WCHAR> w = toUtf16(commit);
            ITfRange* r = nullptr;
            if (SUCCEEDED(ias->InsertTextAtSelection(
                    ec, 0, w.empty() ? L"" : (const WCHAR*)w.data(),
                    (LONG)w.size(), &r)) && r) {
                r->Collapse(ec, TF_ANCHOR_END);
                ITfRange* sr = nullptr;
                if (SUCCEEDED(r->Clone(&sr)) && sr) {
                    TF_SELECTION sel;
                    sel.range = sr;
                    sel.style.ase = TF_AE_NONE;
                    sel.style.fInterimChar = FALSE;
                    pic->SetSelection(ec, 1, &sel);
                    sr->Release();
                }
                r->Release();
            }
            ias->Release();
        }
    }

    /* 2) preedit 갱신 */
    const std::string& pre = host_.lastPreedit;
    if (!pre.empty()) {
        if (!composition_) {
            ITfContextComposition* cc = nullptr;
            ITfInsertAtSelection*  ias = nullptr;
            if (SUCCEEDED(pic->QueryInterface(IID_ITfContextComposition, (void**)&cc)) && cc &&
                SUCCEEDED(pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&ias)) && ias) {
                ITfRange* r = nullptr;
                if (SUCCEEDED(ias->InsertTextAtSelection(
                        ec, TF_IAS_QUERYONLY, L"", 0, &r)) && r) {
                    cc->StartComposition(ec, r,
                        static_cast<ITfCompositionSink*>(this), &composition_);
                    r->Release();
                }
            }
            if (ias) ias->Release();
            if (cc)  cc->Release();
        }
        if (composition_) {
            ITfRange* cr = nullptr;
            if (SUCCEEDED(composition_->GetRange(&cr)) && cr) {
                std::vector<WCHAR> w = toUtf16(pre);
                cr->SetText(ec, 0, w.empty() ? L"" : (const WCHAR*)w.data(), (LONG)w.size());
                /* 커서를 조합 문자열 끝으로 */
                ITfRange* er = nullptr;
                if (SUCCEEDED(cr->Clone(&er)) && er) {
                    er->Collapse(ec, TF_ANCHOR_END);
                    TF_SELECTION sel;
                    sel.range = er;
                    sel.style.ase = TF_AE_NONE;
                    sel.style.fInterimChar = FALSE;
                    pic->SetSelection(ec, 1, &sel);
                    er->Release();
                }
                cr->Release();
            }
        }
    } else if (composition_) {
        composition_->EndComposition(ec);
        composition_->Release();
        composition_ = nullptr;
    }
    return S_OK;
}

/* ═══════════ 클래스 팩토리 ═══════════ */

class ClassFactory final : public IClassFactory {
public:
    ClassFactory() : ref_(1) { InterlockedIncrement(&g_refDll); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
            *ppv = static_cast<IClassFactory*>(this);
        if (*ppv) { AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG c = InterlockedDecrement(&ref_);
        if (c == 0) { InterlockedDecrement(&g_refDll); delete this; }
        return c;
    }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        CheonjiinTextService* svc = new (std::nothrow) CheonjiinTextService();
        if (!svc) return E_OUTOFMEMORY;
        HRESULT hr = svc->QueryInterface(riid, ppv);
        svc->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_refDll); else InterlockedDecrement(&g_refDll);
        return S_OK;
    }
private:
    LONG ref_;
};

} // namespace tsf
} // namespace cuime

/* ═══════════ DLL 진입점 ═══════════ */

using namespace cuime::tsf;

/* STDAPI 만으로는 내보내지지 않는다(실제로 겪음: 내보내기 테이블에 4종 누락).
 * regsvr32 / CoCreateInstance 가 찾으려면 명시적 dllexport 가 필요하다. */
#define CUIME_DLLAPI EXTERN_C __declspec(dllexport) HRESULT STDAPICALLTYPE

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInst = hInst;
        DisableThreadLibraryCalls(hInst);
    }
    return TRUE;
}

CUIME_DLLAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_CuimeService)) return CLASS_E_CLASSNOTAVAILABLE;
    ClassFactory* cf = new (std::nothrow) ClassFactory();
    if (!cf) return E_OUTOFMEMORY;
    HRESULT hr = cf->QueryInterface(riid, ppv);
    cf->Release();
    return hr;
}

CUIME_DLLAPI DllCanUnloadNow(void) {
    return (g_refDll == 0) ? S_OK : S_FALSE;
}

CUIME_DLLAPI DllRegisterServer(void) {
    /* 1) CLSID 등록 */
    WCHAR path[MAX_PATH];
    if (!GetModuleFileNameW(g_hInst, path, MAX_PATH)) return E_FAIL;

    HKEY hk = nullptr, hs = nullptr;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, kClsidKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hk, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE*)kDesc, (DWORD)sizeof(kDesc));
    if (RegCreateKeyExW(hk, L"InProcServer32", 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hs, nullptr) == ERROR_SUCCESS) {
        DWORD n = (DWORD)((lstrlenW(path) + 1) * sizeof(WCHAR));
        RegSetValueExW(hs, nullptr, 0, REG_SZ, (const BYTE*)path, n);
        RegSetValueExW(hs, L"ThreadingModel", 0, REG_SZ,
                       (const BYTE*)L"Apartment", (DWORD)(10 * sizeof(WCHAR)));
        RegCloseKey(hs);
    }
    RegCloseKey(hk);

    /* 2) TSF 입력 프로파일 등록 */
    HRESULT hr = CoInitialize(nullptr);
    bool inited = SUCCEEDED(hr);
    ITfInputProcessorProfiles* pf = nullptr;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITfInputProcessorProfiles, (void**)&pf);
    if (SUCCEEDED(hr) && pf) {
        pf->Register(CLSID_CuimeService);
        pf->AddLanguageProfile(CLSID_CuimeService, kLangKo, GUID_CuimeProfile,
                               kDesc, (ULONG)(sizeof(kDesc) / sizeof(WCHAR) - 1),
                               path, (ULONG)lstrlenW(path), 0);
        pf->Release();
    }

    /* 3) 카테고리 등록 — 키보드 TIP 으로 분류 */
    ITfCategoryMgr* cm = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfCategoryMgr, (void**)&cm)) && cm) {
        cm->RegisterCategory(CLSID_CuimeService, GUID_TFCAT_TIP_KEYBOARD, CLSID_CuimeService);
        cm->Release();
    }
    if (inited) CoUninitialize();
    return S_OK;
}

CUIME_DLLAPI DllUnregisterServer(void) {
    HRESULT hr = CoInitialize(nullptr);
    bool inited = SUCCEEDED(hr);
    ITfCategoryMgr* cm = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfCategoryMgr, (void**)&cm)) && cm) {
        cm->UnregisterCategory(CLSID_CuimeService, GUID_TFCAT_TIP_KEYBOARD, CLSID_CuimeService);
        cm->Release();
    }
    ITfInputProcessorProfiles* pf = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfiles, (void**)&pf)) && pf) {
        pf->Unregister(CLSID_CuimeService);
        pf->Release();
    }
    if (inited) CoUninitialize();
    RegDeleteTreeW(HKEY_CLASSES_ROOT, kClsidKey);
    return S_OK;
}
