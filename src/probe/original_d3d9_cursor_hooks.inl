// Genuine original Device9 cursor presentation state, COM slots 10/11/12.
// Never capture cursor bitmap pixels, coordinates, private game pointers or ROM.
// Only EXE-owned primary methods advance original Present lineage.
HRESULT STDMETHODCALLTYPE OriginalSetCursorPropertiesHook(
    IDirect3DDevice9* device,UINT hotspot_x,UINT hotspot_y,IDirect3DSurface9* bitmap){
    if(!g_original_set_cursor_properties)return D3DERR_INVALIDCALL;
    if(!g_primary_device.Owns(device))
        return g_original_set_cursor_properties(device,hotspot_x,hotspot_y,bitmap);
    const auto rva=CallerRva(_ReturnAddress());
    if(rva)BeforeOriginalPresentationControl(rva,6u);
    else {
        g_original_external_presentation_unproven.store(true,std::memory_order_release);
        BeforeOriginalPresentationControl(0u,5u);
    }
    const HRESULT hr=g_original_set_cursor_properties(device,hotspot_x,hotspot_y,bitmap);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,71u,rva);
    if(rva && SUCCEEDED(hr)){
        if(!g_original_cursor_visibility_known.load(std::memory_order_acquire))
            g_original_cursor_unknown.store(true,std::memory_order_release);
        g_original_cursor_controls[0].fetch_add(1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::DeviceCursorProperties,rva,hr);
    }
    return hr;
}
void STDMETHODCALLTYPE OriginalSetCursorPositionHook(
    IDirect3DDevice9* device,int x,int y,DWORD flags){
    if(!g_original_set_cursor_position)return;
    if(!g_primary_device.Owns(device)){
        g_original_set_cursor_position(device,x,y,flags);
        return;
    }
    const auto rva=CallerRva(_ReturnAddress());
    // A proven hidden cursor position has no visible output and must not
    // needlessly poison every racing eye pair during mouse movement.
    // First-ever or untrusted visibility still fails closed.
    const bool visibility_known=g_original_cursor_visibility_known.load(
        std::memory_order_acquire);
    const bool may_draw=!visibility_known ||
        g_original_cursor_visible.load(std::memory_order_acquire) ||
        g_original_cursor_unknown.load(std::memory_order_acquire);
    if(rva && may_draw)BeforeOriginalPresentationControl(rva,7u);
    else if(!rva) {
        g_original_external_presentation_unproven.store(true,std::memory_order_release);
        BeforeOriginalPresentationControl(0u,5u);
    }
    // Immediate cursor updates can change a hardware/GDI cursor mid-frame.
    g_original_set_cursor_position(device,x,y,flags);
    if(rva){
        if(!visibility_known)
            g_original_cursor_unknown.store(true,std::memory_order_release);
        g_original_cursor_controls[1].fetch_add(1u,std::memory_order_relaxed);
        if(may_draw)RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::DeviceCursorPosition,rva,S_OK);
    }
}
BOOL STDMETHODCALLTYPE OriginalShowCursorHook(IDirect3DDevice9* device,BOOL show){
    if(!g_original_show_cursor)return FALSE;
    if(!g_primary_device.Owns(device))return g_original_show_cursor(device,show);
    const auto rva=CallerRva(_ReturnAddress());
    if(rva)BeforeOriginalPresentationControl(rva,8u);
    else {
        g_original_external_presentation_unproven.store(true,std::memory_order_release);
        BeforeOriginalPresentationControl(0u,5u);
    }
    const bool known_before=g_original_cursor_visibility_known.load(std::memory_order_acquire);
    const bool visible_before=g_original_cursor_visible.load(std::memory_order_acquire);
    // The documented BOOL return is PREVIOUS visibility, not the request.
    const BOOL was_visible=g_original_show_cursor(device,show);
    if(rva){
        if(known_before && (was_visible!=FALSE)!=visible_before){
            g_original_external_presentation_unproven.store(true,std::memory_order_release);
            BeforeOriginalPresentationControl(0u,5u);
        }
        g_original_cursor_visibility_known.store(true,std::memory_order_release);
        g_original_cursor_visible.store(show!=FALSE,std::memory_order_release);
        g_original_cursor_unknown.store(false,std::memory_order_release);
        g_original_cursor_controls[2].fetch_add(1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::DeviceCursorVisibility,rva,S_OK);
    }
    return was_visible;
}
