"""Original primary D3D9 palette changes invalidate staged XR eyes before COM.

Only the original EXE may provide a scoped palette lineage. An EXE-foreign
primary mutation must disarm the physical Present even when its COM succeeds.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class PaletteBeforeComSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.bridge = (ROOT / "src/probe/verified_world_bridge.cpp").read_text("utf-8")

    @staticmethod
    def hook(source, name):
        return source.split(f"HRESULT STDMETHODCALLTYPE {name}(", 1)[1].split(
            "HRESULT STDMETHODCALLTYPE ", 1
        )[0]

    def test_real_palette_entries_reject_before_original_com(self):
        block = self.hook(self.source, "SetPaletteEntriesHook")
        self.assertEqual(block.count("const HRESULT hr=g_set_palette_entries(d,palette,entries);"), 1)
        self.assertLess(block.index("Model2VR_ObserveOriginalD3D9PaletteMutation("),
                        block.index("const HRESULT hr=g_set_palette_entries("))
        self.assertLess(block.index("Model2VR_ObserveOriginalD3D9PresentationMutation("),
                        block.index("const HRESULT hr=g_set_palette_entries("))
        self.assertIn("TrackFailedOriginalD3D9Surface(hr,31u,rva)", block)
        self.assertIn("if(rva && SUCCEEDED(hr))", block)
        self.assertIn("OriginalLineageKind::PaletteEntries", block)
        self.assertIn("if(!g_primary_device.Owns(d))", block)

    def test_original_palette_selection_reject_before_original_com(self):
        block = self.hook(self.source, "SetCurrentTexturePaletteHook")
        self.assertEqual(block.count("const HRESULT hr=g_set_current_texture_palette(d,palette);"), 1)
        self.assertLess(block.index("Model2VR_ObserveOriginalD3D9PaletteMutation("),
                        block.index("const HRESULT hr=g_set_current_texture_palette("))
        self.assertLess(block.index("Model2VR_ObserveOriginalD3D9PresentationMutation("),
                        block.index("const HRESULT hr=g_set_current_texture_palette("))
        self.assertIn("TrackFailedOriginalD3D9Surface(hr,32u,rva)", block)
        self.assertIn("if(rva && SUCCEEDED(hr))", block)
        self.assertIn("OriginalLineageKind::CurrentPalette", block)
        self.assertIn("if(!g_primary_device.Owns(d))", block)

    def test_untrusted_caller_disarms_physical_presentation(self):
        for name, method in (("SetPaletteEntriesHook", "1u"),
                             ("SetCurrentTexturePaletteHook", "2u")):
            block = self.hook(self.source, name)
            self.assertIn("const auto rva=CallerRva(origin);", block)
            self.assertIn("if(rva)Model2VR_ObserveOriginalD3D9PaletteMutation(", block)
            self.assertIn("g_original_external_presentation_unproven.store(", block)
            self.assertIn("true,std::memory_order_release", block)
            self.assertIn("g_present_count.load(std::memory_order_acquire)+1u,0u,0u,5u", block)
            self.assertIn("g_original_frame_draw_ordinal.load(std::memory_order_acquire),"+method, block)
        self.assertIn("g_original_external_presentation_unproven.load(", self.source)
        self.assertIn("BeforeOriginalPresentationControl(0u,5u)", self.source)
        self.assertIn("model2_d3d9_original_presentation_control_rejected", self.bridge)
        self.assertIn("m2vr::daytona::MarkOriginalLineageMismatch();", self.bridge)
        self.assertIn("m2vr::daytona::MarkOriginalPaletteMutationAfterEyes(method)", self.bridge)
        self.assertIn("g.source_epoch.Revoke(physical_present)", self.bridge)


if __name__ == "__main__":
    unittest.main()
